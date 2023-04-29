#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
	int byte_read_total = 0;
	int byte_read_current = 0;
	
	// Keep reading from the file descriptor until we've read len bytes
	while (byte_read_total < len)
	{
		// Read len - byte_read_total bytes from the file descriptor into buf
		byte_read_current = read(fd, buf + byte_read_total, len - byte_read_total);
		
		// If an error occurred during the read, return false
		if (byte_read_current == -1)
		{
			return false;
		}
		
		// Otherwise, update the total bytes read and continue the loop
		byte_read_total += byte_read_current;
	}
	
	return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
	int byte_written_total = 0;
	int byte_written_current = 0;
	
	// Keep writing to the file descriptor until we've written len bytes
	while (byte_written_total < len)
	{
		// Write len - byte_written_total bytes from buf to the file descriptor
		byte_written_current = write(fd, buf + byte_written_total, len - byte_written_total);
		
		// If an error occurred during the write, return false
		if (byte_written_current == -1)
		{
			return false;
		}
		
		// Otherwise, update the total bytes written and continue the loop
		byte_written_total += byte_written_current;
	}
	
	return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
	// Buffer for storing the packet header
	uint8_t packet[HEADER_LEN];
	
	// Read the packet header from the file descriptor
	if (nread(fd, HEADER_LEN, packet) == false)
	{
		return false;
	}
	
	// Extract the length, operation code, and return code from the packet header
	uint16_t length;
	memcpy(&length, packet, 2);
	memcpy(op, packet + 2, 4);
	memcpy(ret, packet + 6, 2);
	
	// Convert the length, operation code, and return code to network byte order
	length = htons(length);
	*op = htonl(*op);
	*ret = htons(*ret);
	
	// If the packet includes a data block, read it into the provided buffer
	if (length == (HEADER_LEN + JBOD_BLOCK_SIZE))
	{
		if (nread(fd, JBOD_BLOCK_SIZE, block) == true) 
		{
			return true;
		}
		else
		{
			return false;
		}
		
	}
	
	// Otherwise, the packet does not include a data block, so just return true
	return true;
}

uint8_t * create_packet(uint16_t length, uint32_t opCode, uint16_t returnCode, uint8_t *block){
	// Determine the size of the packet based on the provided length
	int packet_size = length;
	
	// Allocate memory for the packet and initialize a pointer to the start of the memory block
	uint8_t packet[packet_size];
	uint8_t* packet_ptr = packet;
	
	// Convert the values of length, opcode, and return code to network byte order
	length = ntohs(length);
	opCode = ntohl(opCode);
	returnCode = ntohs(returnCode);
	
	// Copy the length, opcode, and return code into the packet byte array
	memcpy(packet, &length, 2);
	memcpy(packet + 2, &opCode, 4);
	memcpy(packet + 6, &returnCode, 2);
	
	// If a block of data was provided, copy it into the packet byte array
	if (packet_size == 264)
	{
		memcpy(packet + 8, block, JBOD_BLOCK_SIZE);
	}
	
	// Return a pointer to the start of the packet byte array
	return packet_ptr;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
	uint32_t length;
	uint16_t returnCode = 0;
	
	// Determine packet length based on operation code
	if ((op >> 26) == JBOD_WRITE_BLOCK)
	{
		length = HEADER_LEN + 256;
	}
	else
	{
		length = HEADER_LEN;
	}
	
	// Create packet with given parameters
	uint8_t *packet;
	packet = create_packet(length, op, returnCode, block);
	
	// Send packet over socket
	if (nwrite(sd, length, packet) == false)
	{
		return false;
	}
	
	return true;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
	struct sockaddr_in caddr;
	
	// Create socket for the connection
	cli_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (cli_sd == -1)
	{
		return false;
	}
	
	// Set address and port for the connection
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);
	if (inet_aton(ip, &caddr.sin_addr) == 0) 
	{
		return false;
	}
	
	// Connect to the server
	if (connect(cli_sd, (const struct sockaddr *) &caddr, sizeof(caddr)) == -1) 
	{
		return false;
	} 
	
	return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd);
	cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t returnValue;
  // send packet with op and block to server
  send_packet(cli_sd, op, block);
  // receive packet from server containing the return value and update the returnValue variable
  recv_packet(cli_sd, &op, &returnValue, block);

  return returnValue;
}
