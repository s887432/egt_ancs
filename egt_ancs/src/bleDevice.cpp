#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

#include "uartFunc.h"
#include "bleDevice.h"
// ******************************************
// global variables
// ******************************************
// ******************************************
// internal constant
// ******************************************
#define CMD_HEADER_1	0xEF
#define CMD_HEADER_2	0x12

#define HEADER_OFFSET	0
#define HEADER_SIZE		2

#define COMMAND_OFFSET	2
#define COMMAND_SIZE	1

#define LENGTH_OFFSET	3
#define LENGTH_SIZE		1

#define PAYLOAD_OFFSET	4

#define RECEIVE_BUFFER_SIZE	512
// ******************************************
// internal functions
// ******************************************
int makeBufHeader(unsigned char *buf)
{
    buf[0] = CMD_HEADER_1;
    buf[1] = CMD_HEADER_2;
    
    return 2;
}

unsigned char makeBufChecksum(unsigned char *buf, int size)
{
    unsigned int sum = 0;
    int i;
    
    for(i=0; i<size; i++)
    {
        sum += buf[i];
    }
    
    return (sum&0xFF);
}

int makeCmdNaviDir(unsigned char *buf, unsigned char val)
{
    int offset = 0;
    
    // header
    offset += makeBufHeader(buf);
    
	// command
    buf[offset++] = COMMAND_NAVI_DIRECTION;

	// length
    buf[offset++] = 4+4;	// TODO... what is the name?
    
    // payload: distance
    buf[offset++] = 1;
    buf[offset++] = 2;
    buf[offset++] = 3;
    
    // payload: direction
    buf[offset++] = val;
    
    // payload: name
    buf[offset++] = 'P';
    buf[offset++] = 'a';
    buf[offset++] = 't';
    buf[offset++] = 'o';
    
    buf[offset] = makeBufChecksum(buf, offset);
    offset++;
    
    return offset;
}

static unsigned char cmd_checksum(unsigned char *buffer, unsigned char size)
{
	unsigned char i;
	unsigned int sum=0;
	
	for(i=0; i<size; i++)
	{
		sum += buffer[i];
	}
	
	return (sum&0xFF);
}

// re-position the buffer if 0xEF 0x12 is not in beginning of buffer
int bleCheckCommand(unsigned char *recBuf, int length, unsigned char *cmdBuf)
{
	int cmdSize = 0;
	int header = -1;
	int tail = 0;
	int index = 0;

	int package_size = 0;

	// find header
	for(int i=0; i<length-1; i++)
	{
		if( recBuf[i] == CMD_HEADER_1 && recBuf[i+1] == CMD_HEADER_2 )
		{
			header = i;
			index = i+HEADER_SIZE;
			break;
		}
	}

	if( header >= 0 )
	{
		// header found
		if( (index + COMMAND_SIZE) <= length )
		{
			// command existed
			index += COMMAND_SIZE;
		}
		else
		{
			// command package is not complete
			return cmdSize;
		}

		// command found
		if( (index + LENGTH_SIZE) <= length )
		{
			// length existed
			package_size = recBuf[index];
			index += COMMAND_SIZE;			
		}
		else
		{
			// command package is not complete
			return cmdSize;
		}

		// package size found
		if( (index + package_size) <= length )
		{
			// payload existed
			index += package_size;
		}
		else
		{
			// command package is not complete
			return cmdSize;
		}

		// payload found
		if( (index + 2) <= length )
		{
			// tail existed
			index += 2;
			tail = index -1;
		}
		else
		{
			// command package is not complete
			return cmdSize;
		}

		// command is complete
		memcpy(cmdBuf, recBuf+header, index);
		cmdSize = tail - header;

		// move buffer
		if( length >= index )
		{
			memcpy(recBuf, recBuf+index, length-index-2);
		}
	}

	if( header >= 0 && tail >=0 )
	{
		// command package is complete
		
	}

	return cmdSize;
}

// ******************************************
// extern functions
// ******************************************

// ******************************************
// send data to BLE device via UART port
// 
// Paramaters
//    cmd: the command which defined at bleProtocol.h
//    param: paramaters
//
// Return value
//    0: success
//    others: fail
// ******************************************
int bleSend(int fd, BLE_COMMANDS cmd, int param)
{
	unsigned char buf[64];
    int size = 0;

	switch( cmd )
	{
		case COMMAND_NAVI_DIRECTION:
			size = makeCmdNaviDir(buf, param);
			break;

		default:
			break;
	}

	if( size > 0 )
	{
#if 1	
		printf("(%d}", fd);
		for(int i=0; i<size; i++)
		{
			printf("%02X, ", buf[i]);
		}
		printf("\r\n");
#endif		
		write(fd, buf, size);
	}

	return 0;
}

void bleStop(void)
{
}

int bleNaviReconstruct(unsigned char *cmdBuf, pNaviStr navi)
{
	if( cmdBuf[HEADER_OFFSET] != CMD_HEADER_1 || cmdBuf[HEADER_OFFSET+1] != CMD_HEADER_2 )
	{
		return -2;
	}
	
	if( (cmdBuf[COMMAND_OFFSET] != COMMAND_NAVI_DIRECTION) )
	{
		return -1;
	}
	
	navi->distance = cmdBuf[PAYLOAD_OFFSET]*256*256 + cmdBuf[PAYLOAD_OFFSET+1]*256 + cmdBuf[PAYLOAD_OFFSET+2];
	navi->direction = (NAVI_DIRECTIONS)cmdBuf[PAYLOAD_OFFSET+3];
	
	return 0;
}

BLE_COMMANDS bleGetCommand(unsigned char *cmdBuf)
{
	if( cmdBuf[HEADER_OFFSET] != CMD_HEADER_1 || cmdBuf[HEADER_OFFSET+1] != CMD_HEADER_2 )
	{
		return COMMAND_NONE;
	}
	
	return (BLE_COMMANDS)cmdBuf[COMMAND_OFFSET];
}

int bleConnect(int fd)
{
	int ret = 0;
	char buf[64];
	
	if( fd < 0 )
	{
		perror("UART device is not opened\r\n");
		return -1;
	}
	
	// enable command mode
	sprintf(buf, "$$$");
	write(fd, buf, 3);
	usleep(100*1000);
	
	// connect to dvice
	sprintf(buf, "c\n");
	write(fd, buf, 2);
	usleep(100*1000);
	
	return ret;	
}

int bleDisconnect(int fd)
{
	int ret = 0;
	char buf[64];
	
	if( fd < 0 )
	{
		perror("UART device is not opened\r\n");
		return -1;
	}
	
	// enable command mode
	sprintf(buf, "$$$");
	write(fd, buf, 3);
	usleep(100*1000);
	
	// disconnect link
	sprintf(buf, "k,1\n");
	write(fd, buf, 4);
	
	// reboot device
	sprintf(buf, "r,1\n");
	write(fd, buf, 4);
	
	return ret;		
}

// end of file
