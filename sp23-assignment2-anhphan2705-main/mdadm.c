#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

uint32_t is_mounted = 0;

uint32_t encode_operation(jbod_cmd_t cmd, int disk_num, int block_num)
{
  uint32_t op = cmd << 26 | disk_num << 22 | block_num;
  return op;
}

int mdadm_mount(void) 
{
  if (is_mounted) return -1;
  jbod_error_t err = jbod_operation(JBOD_MOUNT, NULL);
  int ans = (err == JBOD_ALREADY_MOUNTED) ? -1 : 1;
  is_mounted = 1;
  return ans;
}

int mdadm_unmount(void) 
{
  if(!is_mounted) return -1;
  jbod_error_t err = jbod_operation(JBOD_UNMOUNT, NULL);
  int ans = (err == JBOD_ALREADY_UNMOUNTED)?-1:1;
  is_mounted = 0;
  return ans;
}

void translate_address(uint32_t linear_address, int *disk_num, int *block_num, int*offset)
{
  *disk_num = linear_address / JBOD_DISK_SIZE;
  *block_num = (linear_address % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  *offset = (linear_address % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

int min(int a, int b) 
{
  return (a<b)? a : b;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) 
{
  if (!is_mounted) {
      return -1;
  }
  if (len > 1024) {
      return -1;
  }
  if (buf == NULL && len > 0) {
      return -1;
  }
  if (addr + len > 1048576) {
      return -1;
  }

  int disk_num = 0;
  int block_num = 0;
  int offset = 0;
  int temp_addr = addr;
  int num_read = 0;

  translate_address(temp_addr, &disk_num, &block_num, &offset);
  uint32_t op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, block_num);
  uint32_t op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
  jbod_operation(op_disk, NULL);
  jbod_operation(op_block, NULL);

  while (len > 0) {
    uint8_t sbuf[256];
    uint32_t op_read = encode_operation(JBOD_READ_BLOCK, 0, 0);
    jbod_operation(op_read, sbuf);

    int bytes_read = min(len, min(256, 256 - offset));
    memcpy(buf + num_read, sbuf + offset, len);
    num_read += bytes_read;
    len -= bytes_read;
    block_num += 1;

    if (block_num == 256) {
      block_num = 0;
      disk_num++;
      op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, block_num);
      op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
      jbod_operation(op_disk, NULL);
      jbod_operation(op_block, NULL);
    }
    offset = 0;
  }
  return num_read;
}