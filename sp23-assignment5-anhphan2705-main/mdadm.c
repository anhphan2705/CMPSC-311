#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "cache.h"
#include "net.h"

int is_mounted = 0;


uint32_t encode_operation(int cmd, int disk_num, int block_num) {
	return (cmd << 26) | (disk_num << 22) | (block_num) ;
}

int mdadm_mount(void) {
	uint32_t op = encode_operation(JBOD_MOUNT, 0, 0);
	int mount = jbod_client_operation(op, NULL);
	if (mount == 0) {
		is_mounted = 1;
		return 1;
	}
  return -1;
}

int mdadm_unmount(void) {
	uint32_t op = encode_operation(JBOD_UNMOUNT, 0, 0);
	int mount = jbod_client_operation(op, NULL);

	if (mount == 0) {
		is_mounted = 0;
		return 1;
	}

  return -1;
}


int min(int a, int b) 
{
  return (a<b)? a : b;
}


void translate_address(uint32_t address, int *disk_num, int *block_num, int*offset) {
	*disk_num = address/ JBOD_DISK_SIZE;
 	*block_num = (address % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
 	*offset = (address % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
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
  jbod_client_operation(op_disk, NULL);
  jbod_client_operation(op_block, NULL);

  // Loop until the entire read length has been processed
  while (len > 0) {
    // Read the next block into a temporary buffer
    uint8_t sbuf[256];
    uint32_t op_read = encode_operation(JBOD_READ_BLOCK, 0, 0);
    jbod_client_operation(op_read, sbuf);

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
      jbod_client_operation(op_disk, NULL);
      jbod_client_operation(op_block, NULL);
    }
    offset = 0;
  }

  // Return the number of bytes read
  return num_read;
}



int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  // Check if the disks are mounted
  if (!is_mounted) {
      return -1;
  }

  // Check if the data length is too large
  if (len > 1024) {
      return -1;
  }

  // Check if the data buffer is null but length is non-zero
  if (buf == NULL && len > 0) {
      return -1;
  }

  // Check if the data goes beyond the disk size
  if (addr + len > 1048576) {
      return -1;
  }

  // Initialize variables
  int temp_len = len;
  int write_count = 0;
  int disk_num = 0;
  int block_num = 0;
  int offset = 0;

  // Loop through the data to write
  while (write_count < len) {
      // Translate the address to disk, block, and offset
      translate_address(addr, &disk_num, &block_num, &offset);

      // Read the block data from the disk
      uint8_t mybuf[256];
      uint32_t op_disk = encode_operation(JBOD_SEEK_TO_DISK, disk_num, 0);
      uint32_t op_block = encode_operation(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
      uint32_t op_read = encode_operation(JBOD_READ_BLOCK, disk_num, block_num);
      jbod_client_operation(op_disk, NULL);
      jbod_client_operation(op_block, NULL);
      jbod_client_operation(op_read, mybuf);

      // Copy the data to write into the block buffer
      int num_bytes = min(temp_len, 256 - offset);
      memcpy(mybuf + offset, buf + write_count, num_bytes);

      // Write the block data back to the disk
      uint32_t op_write = encode_operation(JBOD_WRITE_BLOCK, disk_num, block_num);
      jbod_client_operation(op_disk, NULL);
      jbod_client_operation(op_block, NULL);
      jbod_client_operation(op_write, mybuf);

      // If we've written to the last block on the disk, move to the next disk
      if (block_num == 256) {
          disk_num++;
          block_num = 0;
          jbod_client_operation(op_disk, NULL);
          jbod_client_operation(op_block, NULL);
      }

      // Reset the offset and update the counters
      offset = 0;
      temp_len -= num_bytes;
      write_count += num_bytes;
      addr += num_bytes;
  }

  // Return the number of bytes written
  return len;
}