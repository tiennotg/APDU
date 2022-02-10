/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "serial.h"
#include "mycodes.h"
#include "apdu.h"
#include "tlv.h"
#include "main.h"


void printBuffer(uint8_t *buffer, uint8_t len)
{
	if (len > 0)
	{
		for (uint8_t j=0; j<len; j++)
			printf("%02x",buffer[j]);
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Usage: %s <serial_port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	int serial_port = open(argv[1],O_RDWR);
	if (serial_port < 0)
	{
		perror("Error while opening serial port : ");
		return EXIT_FAILURE;
	}
	
	if (!serialInitialize(serial_port))
	{
		close(serial_port);
		return EXIT_FAILURE;
	}
	
	if (!apduInitialize(serial_port))
	{
		close(serial_port);
		return EXIT_FAILURE;
	}
	
	while (1)
	{
		uint8_t buffer[BUFFER_SIZE];
		uint8_t buflen = BUFFER_SIZE;
		
		apduWaitForCard(serial_port);
		
		apduSendCommand(serial_port,0x00,0xA4,0x04,0x00,0x0E,"2PAY.SYS.DDF01",0x00,true);
		apduWaitForResponse(serial_port, buffer, &buflen, NULL, NULL);
		
		// Look for FCI. This tag contains the application templates, with the AID.
		struct TLVobject *d = tlvParseData(buffer, buflen);
		d = tlvParseData(buffer, buflen);
		
		struct TLVobject *fci = tlvObjectLookForTag(d, 0xBF0C);
		if (fci == NULL)
		{
			printf("Error: No FCI found.\n");
			return EXIT_FAILURE;
		}
		
		
		bool data_found = false;
		
		for (int i=0; i<255; i++)
		{
			if (fci->data[i] == NULL)
				break;
			
			struct TLVobject *aid = tlvObjectLookForTag(fci->data[i], 0x4F);
			
			if (aid != NULL)
			{			
				// Select AID
				buflen = BUFFER_SIZE;
				apduSendCommand(serial_port,0x00,0xA4,0x04,0x00,aid->length,aid->data[0],0x00,true);
				apduWaitForResponse(serial_port, buffer, &buflen, NULL, NULL);
				
				// Try to retrieve data reading record by record, and sfi by sfi
				for (uint8_t sfi=1; sfi<16; sfi++)
				{
					for (uint8_t record_number=1; record_number<32; record_number++)
					{
						uint8_t sw1, sw2;
						buflen = BUFFER_SIZE;
						apduSendCommand(serial_port,0x00,0xB2,record_number,(sfi << 3)|04,0x00,NULL,0x00,true);
						apduWaitForResponse(serial_port, buffer, &buflen, &sw1, &sw2);
						if (sw1 == 0x90 && sw2 == 00)
						{
							struct TLVobject *rec = tlvParseData(buffer, buflen);
							
							struct TLVobject *card_number = tlvObjectLookForTag(rec, 0x5a);
							struct TLVobject *expiration_date = tlvObjectLookForTag(rec, 0x5f24);
							
							if (card_number != NULL)
							{
								printf("### Card number ###\n");
								printBuffer((uint8_t*) card_number->data[0], card_number->length);
								printf("\n");
								data_found = true;
							}
							
							if (expiration_date != NULL)
							{
								printf("### Expiration date ###\n");
								printf("%02x/%02x\n\n", ((uint8_t*) expiration_date->data[0])[1],
								((uint8_t*) expiration_date->data[0])[0]);
								data_found = true;
							}
							tlvObjectFree(rec);
						}
						else
							break;
						if (data_found) break;				
					}
					if (data_found) break;
				}
			}
			if (data_found) break;
		}
		
		tlvObjectFree(d);
		int r = 0;
		while (r != MYTERM_TIMEOUT)
		{
			buflen = BUFFER_SIZE;
			r = apduWaitForResponse(serial_port, buffer, &buflen, NULL, NULL);
			
			if (r != MYTERM_TIMEOUT && r != MYTERM_OK)
				return EXIT_FAILURE;
		}
	}

	close(serial_port);
	return EXIT_SUCCESS;
}
