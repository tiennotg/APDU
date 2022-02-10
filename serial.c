/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * serial.c: Low-level UART management.
 * 
 * This file is part of APDU.
 * 
 * APDU is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * APDU is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with APDU.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "serial.h"
#include "mycodes.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

bool serialInitialize(int serial_port)
{
	struct termios tty;
	if (tcgetattr(serial_port, &tty) != 0)
	{
		perror("Error while getting serial port attributes : ");
		return false;
	}
	
	tty.c_cflag &= ~PARENB; // No parity bit
	tty.c_cflag &= ~CSTOPB; // One stop bit
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;     // byte size: 8
	tty.c_cflag &= ~CRTSCTS;// disable RTS/CTS hardware flow control
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
	
	tty.c_lflag &= ~ICANON; // Disable line per line mode
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	// Disable any special handling of received bytes
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
	
	tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;
	
	// Set baud rate to 115200
	cfsetspeed(&tty, B115200);
	
	if (tcsetattr(serial_port, TCSANOW, &tty) != 0)
	{
		perror("Error while setting serial port attributes : ");
		return false;
	}
	return true;
}

void sendCommand(int serial_port, uint8_t *buffer, uint8_t len)
{
	if (len == 0)
		return;
	
	uint8_t *cmdbuffer = (uint8_t*) malloc(sizeof(uint8_t) * (len+2));
	if (cmdbuffer == NULL)
	{
		fprintf(stderr, "Memory allocation error!\n");
		return;
	}
	cmdbuffer[0] = MYTERM_COMMAND;
	cmdbuffer[1] = len;
	memcpy(cmdbuffer+2, buffer, len);
	
	write(serial_port, cmdbuffer, (len+2));
	free(cmdbuffer);
	return;
}

int waitResponse(int serial_port, uint8_t *buffer, uint8_t *len)
{
	uint8_t tmp_buffer[BUFFER_SIZE];
	uint8_t data_length = 0;
	uint8_t res_code = 0;
	uint8_t max_size = *len;
	bool incoming = false;
	
	int n = 0;
	
	if (buffer == NULL)
		return 0;
	
	while (n == 0)
	{
		n = read(serial_port, &tmp_buffer, BUFFER_SIZE);
		if (n < 0)
		  return n;
		else if (n > 0)
		{
			#ifdef LOGLEVEL_DEBUG
			printf("Debug message: Serial port received %d bytes.\n\n",n);
			printf("\tBuffer content:\n");
			printf("\t");
			for (int x=0; x<n; x++)
				printf("0x%02X ",tmp_buffer[x]);
			printf("\n\n");
			#endif
			
			// First time, get the expected data length
			// and start filling the buffer
			if (!incoming)
			{
				incoming = true;
				if (n < 2) // malformed frame
					return -1;
				data_length = tmp_buffer[1];
				res_code = tmp_buffer[0];
			
				if (n-2 <= max_size) // Is the buffer too small ?
				{
					// no, everything is in order
					// otherwise, copy only *len data.
					
					*len = n-2; // remove response code and data length
					if (*len < data_length) // Some data is missing,
					// set n to stay in the while loop.
						n = 0;
				}
				
				if (*len > 0)
					memcpy(buffer, tmp_buffer+2, *len);
			}
			// Some data incoming, that are appended in the buffer
			else
			{
				uint8_t start_pos = *len;
				*len += (uint8_t) n;
				
				// avoid buffer overflow
				if (*len > max_size)
					n -= (int) *len - max_size;
				
				memcpy(buffer+start_pos, tmp_buffer, n);
				
				// is it finished ?
				if (*len < data_length)
					n = 0; // No, stay in the loop
				else
					n = 1; // Yes, exit!
			}
		}
	}
	return (int) res_code;
}
