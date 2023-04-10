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
  // Check if the disk is mounted
  if (!is_mounted) {
      return -1;
  }

  // Check if the read length is too long
  if (len > 1024) {
      return -1;
  }

  // Check if the buffer is null but the length is positive
  if (buf == NULL && len > 0) {
      return -1;
  }

  // Check if the read extends beyond the disk size
  if (addr + len > 1048576) {
      return -1;
  }

  // Initialize variables for disk, block, and offset
  int disk_num = 0;
  int block_num = 0;
  int offset = 0;
  int temp_addr = addr;
  int num_read = 0;

  // Translate the starting address to disk, block, and offset values
  translate_address(temp_addr, &disk_num, &block_num, &offset);

  // Seek to the correct disk and block
  uint32_t op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, block_num);
  uint32_t op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
  jbod_operation(op_disk, NULL);
  jbod_operation(op_block, NULL);

  // Loop until the entire read length has been processed
  while (len > 0) {
    // Read the next block into a temporary buffer
    uint8_t sbuf[256];
    uint32_t op_read = encode_operation(JBOD_READ_BLOCK, 0, 0);
    jbod_operation(op_read, sbuf);

    // Determine how many bytes can be read from the current block
    int bytes_read = min(len, min(256, 256 - offset));

    // Copy the bytes from the temporary buffer to the output buffer
    memcpy(buf + num_read, sbuf + offset, len);

    // Update the read counters and seek to the next block if necessary
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

  // Return the number of bytes read
  return num_read;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) 
{
  // Check if the disk is mounted
  if (!is_mounted) {
      return -1;
  }
  
  // Check the input length is valid
  if (len > 1024) {
      return -1;
  }
  
  // Check if the input buffer is valid
  if (buf == NULL && len > 0) {
      return -1;
  }
  
  // Check if the write operation goes beyond the disk size
  if (addr + len > 1048576) {
      return -1;
  }

  int disk_num = 0;
  int block_num = 0;
  int offset = 0;
  int temp_addr = addr;
  int num_written = 0;

  // Translate the logical address to physical disk and block numbers
  translate_address(temp_addr, &disk_num, &block_num, &offset);
  
  // Seek to the correct disk and block numbers
  uint32_t op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, block_num);
  uint32_t op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
  jbod_operation(op_disk, NULL);
  jbod_operation(op_block, NULL);

  while (len > 0) {
    uint8_t sbuf[256];
    
    // Write data to the JBOD
    uint32_t op_write = encode_operation(JBOD_WRITE_BLOCK, 0, 0);
    int bytes_written = min(len, min(256, 256 - offset));
    memcpy(sbuf + offset, buf + num_written, bytes_written);
    jbod_operation(op_write, sbuf);

    num_written += bytes_written;
    len -= bytes_written;
    block_num += 1;

    if (block_num == 256) {
      block_num = 0;
      disk_num++;
      
      // Seek to the next disk and block numbers
      op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, block_num);
      op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
      jbod_operation(op_disk, NULL);
      jbod_operation(op_block, NULL);
    }
    offset = 0;
  }
  
  return num_written;
}