/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * apdu.c: High-level interface for sending APDU commands over UART.
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

#include "apdu.h"
#include "serial.h"
#include "mycodes.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool apduInitialize(int serialPort)
{
	uint8_t buffer[BUFFER_SIZE];
	uint8_t buflen = BUFFER_SIZE;
	int res = 0;
	
	// When starting, Arduino checks the module connectivity,
	// and returns an error if not found. Otherwise, it sends
	// the chip model and version number.
	switch (res = waitResponse(serialPort, buffer, &buflen))
	{
		case MYTERM_NOTFOUND:
			fprintf(stderr,"No NFC module detected.\n");
			return false;
		break;
		case MYTERM_OK:
			printf("Found a PN5%02x chip. ", buffer[0]);
			printf("Version %d.%d.\n", buffer[1], buffer[2]);
		break;
		default:
			mycodesPrintStr(res,NULL);
			return false;
		break;
	}
	return true;
}

bool apduWaitForCard(int serialPort)
{
	uint8_t buffer[BUFFER_SIZE];
	uint8_t buflen = BUFFER_SIZE;
	int res;
	switch (res = waitResponse(serialPort, buffer, &buflen))
	{
		case MYTERM_CARDFOUND:
			printf("Card detected! UID: ");
			for (uint8_t i=0; i<buflen; i++)
				printf("%02x",buffer[i]);
			printf("\n");
		break;
		default:
			mycodesPrintStr(res,NULL);
			return false;
		break;
	}
	return true;
}

void apduSendCommand(int serialPort, uint8_t cla, uint8_t ins, uint8_t p1,
uint8_t p2, uint8_t lc, uint8_t *data, uint8_t le, bool isLePresent)
{
	const uint8_t PN532_WRITE_CMD[2] = {0x40,0x01};
	int cmdlen = sizeof(PN532_WRITE_CMD)+4+lc;
	if (isLePresent) cmdlen++;
	if (lc > 0) cmdlen++;

	uint8_t *buffer = (uint8_t*) malloc(sizeof(uint8_t)*cmdlen);
	if (buffer == NULL)
		return;
	uint8_t *bufferApduCmd = buffer+sizeof(PN532_WRITE_CMD);

	memcpy(buffer,PN532_WRITE_CMD,sizeof(PN532_WRITE_CMD));
	bufferApduCmd[0] = cla;
	bufferApduCmd[1] = ins;
	bufferApduCmd[2] = p1;
	bufferApduCmd[3] = p2;

	if (lc > 0 && data != NULL)
	{
		bufferApduCmd[4] = lc;
		memcpy(bufferApduCmd + 5, data, lc);
	}

	if (isLePresent)
		buffer[cmdlen-1] = le;

	#ifdef LOGLEVEL_DEBUG
	if (cmdlen == 0)
		printf("Debug message: apduSendCommand buffer is empty!\n\n");
	else
	{
		printf("Debug message: apduSendCommand buffer content\n\n");
		printf("\t");
		for (int x=0; x<cmdlen; x++)
			printf("0x%02X ",buffer[x]);
		printf("\n\n");
	}
	#endif
	
	sendCommand(serialPort, buffer, cmdlen);
	free(buffer);
}

int apduWaitForResponse(int serialPort, uint8_t *resdata, uint8_t *reslen, uint8_t *sw1, uint8_t *sw2)
{
	uint8_t buffer[BUFFER_SIZE];
	uint8_t buflen = BUFFER_SIZE;
	int res = waitResponse(serialPort, buffer, &buflen);
	
	#ifdef LOGLEVEL_DEBUG
	if (buflen == 0)
		printf("Debug message: apduWaitForResponse buffer is empty!\n\n");
	else
	{
		printf("Debug message: apduWaitForResponse buffer content\n\n");
		printf("\t");
		for (uint8_t j=0; j<buflen; j++)
			printf("0x%02X ",buffer[j]);
		printf("\n\n");
	}
	#endif
	
	if (res != MYTERM_OK)
	{
		mycodesPrintStr(res,NULL);
		if (reslen != NULL) *reslen = 0;
		return res;
	}
	
	// APDU response code follows the data
	uint8_t _sw1 = buffer[buflen-2];
	uint8_t _sw2 = buffer[buflen-1];
	
	if (_sw1 != APDU_SW1_OK || _sw2 != APDU_SW2_OK)
		apduPrintError(_sw1,_sw2);
	
	// Copy the response code if needed
	if (sw1 != NULL)
		*sw1 = _sw1;
	if (sw2 != NULL)
		*sw2 = _sw2;
	
	if (reslen != NULL && resdata != NULL && *reslen >= buflen-2)
	{
		*reslen = buflen-2;
		memcpy(resdata, buffer, *reslen);
	}
	return res;
}

// APDU response code to readable string

void apduPrintError(uint8_t sw1, uint8_t sw2)
{
	printf("0x%02x%02x ",sw1,sw2);
	switch (sw1)
	{
		case 0x06:
			printf("Error: Class not supported.\n");
		break;
		case 0x61:
			printf("Command successfully executed. %d bytes of data are available and can be requested using GET RESPONSE.", sw2);
		break;
		case 0x62:
			switch (sw2)
			{
				case 0x01:
				printf("Warning: NV-Ram not changed 1.\n");
				break;
				case 0x81:
				printf("Warning: Part of returned data may be corrupted.\n");
				break;
				case 0x82:
				printf("Warning: End of file/record reached before reading Le bytes.\n");
				break;
				case 0x83:
				printf("Warning: Selected file invalidated.\n");
				break;
				case 0x84:
				printf("Warning: Selected file is not valid. FCI not formated according to ISO.\n");
				break;
				case 0x85:
				printf("Warning: No input data available from a sensor on the card. No Purse Engine enslaved for R3bc.\n");
				break;
				case 0xA2:
				printf("Warning: Wrong R-MAC.\n");
				break;
				case 0xA4:
				printf("Warning: Card locked (during reset).\n");
				break;
				case 0xF1:
				printf("Warning: Wrong C-MAC.\n");
				break;
				case 0xF3:
				printf("Warning: Internal reset.\n");
				break;
				case 0xF5:
				printf("Warning: Default agent locked.\n");
				break;
				case 0xF7:
				printf("Warning: Cardholder locked.\n");
				break;
				case 0xF8:
				printf("Warning: Basement is current agent.\n");
				break;
				case 0xF9:
				printf("Warning: CALC Key Set not unblocked.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x63:
			switch (sw2)
			{
				case 0x00:
				printf("Warning: No information given (NV-Ram changed).\n");
				break;
				case 0x81:
				printf("Warning: File filled up by the last write. Loading/updating is not allowed.\n");
				break;
				case 0x82:
				printf("Warning: Card key not supported.\n");
				break;
				case 0x83:
				printf("Warning: Reader key not supported.\n");
				break;
				case 0x84:
				printf("Warning: Plaintext transmission not supported.\n");
				break;
				case 0x85:
				printf("Warning: Secured transmission not supported.\n");
				break;
				case 0x86:
				printf("Warning: Volatile memory is not available.\n");
				break;
				case 0x87:
				printf("Warning: Non-volatile memory is not available.\n");
				break;
				case 0x88:
				printf("Warning: Key number not valid.\n");
				break;
				case 0x89:
				printf("Warning: Key length is not correct.\n");
				break;
				case 0xC0:
				printf("Warning: Verify fail, no try left.\n");
				break;
				case 0xC1:
				printf("Warning: Verify fail, 1 try left.\n");
				break;
				case 0xC2:
				printf("Warning: Verify fail, 2 tries left.\n");
				break;
				case 0xC3:
				printf("Warning: Verify fail, 3 tries left.\n");
				break;
				case 0xF1:
				printf("Warning: More data expected.\n");
				break;
				case 0xF2:
				printf("Warning: More data expected and proactive command pending.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x64:
			switch (sw2)
			{
				case 0x00:
				printf("Error: No information given (NV-Ram not changed).\n");
				break;
				case 0x01:
				printf("Error: Command timeout. Immediate response required by the card.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x65:
			switch (sw2)
			{
				case 0x00:
				printf("Error: No information given.\n");
				break;
				case 0x01:
				printf("Error: Write error. Memory failure. There have been problems in writing or reading the EEPROM. Other hardware problems may also bring this error.\n");
				break;
				case 0x81:
				printf("Error: Memory failure.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x66:
			switch (sw2)
			{
				case 0x00:
					printf("Security: Error while receiving (timeout).\n");
				break;
				case 0x01:
					printf("Security: Error while receiving (character parity error).\n");
				break;
				case 0x02:
					printf("Security: Wrong checksum.\n");
				break;
				case 0x03:
					printf("Security: The current DF file without FCI.\n");
				break;
				case 0x04:
					printf("Security: No SF or KF under the current DF.\n");
				break;
				case 0x69:
					printf("Security: Incorrect Encryption/Decryption Padding.\n");
				break;
				default:
					
				break;
			}
		break;
		case 0x67:
			if (sw2 == 0x00)
				printf("Error: Wrong length.\n");
			else
				printf("Error: Length incorrect (procedure)(ISO 7816-3).\n");
		break;
		case 0x68:
			switch (sw2)
			{
				case 0x00:
					printf("Error: No information given (The request function is not supported by the card).\n");
				break;
				case 0x81:
					printf("Error: Logical channel not supported.\n");
				break;
				case 0x82:
					printf("Error: Secure messaging not supported.\n");
				break;
				case 0x83:
					printf("Error: Last command of the chain expected.\n");
				break;
				case 0x84:
					printf("Error: Command chaining not supported.\n");
				break;
				default:
					printf("Unknown code.\n");
				break;
			}
		break;
		case 0x69:
			switch (sw2)
			{
				case 0x00:
					printf("Error: Command not allowed.\n");
				break;
				case 0x01:
					printf("Error: Command not accepted (inactive state).\n");
				break;
				case 0x81:
					printf("Error: Command incompatible with file structure.\n");
				break;
				case 0x82:
					printf("Error: Security condition not satisfied.\n");
				break;
				case 0x83:
					printf("Error: Authentication method blocked.\n");
				break;
				case 0x84:
					printf("Error: Referenced data reversibly blocked (invalidated).\n");
				break;
				case 0x85:
					printf("Error: Conditions of use not satisfied.\n");
				break;
				case 0x86:
					printf("Error: Command not allowed (no current EF).\n");
				break;
				case 0x87:
					printf("Error: Expected secure messaging (SM) object missing.\n");
				break;
				case 0x88:
					printf("Error: Incorrect secure messaging (SM) data object.\n");
				break;
				case 0x96:
					printf("Error: Data must be updated again.\n");
				break;
				case 0xE1:
					printf("Error: POL1 of the currently Enabled Profile prevents this action.\n");
				break;
				case 0xF0:
					printf("Error: Permission Denied.\n");
				break;
				case 0xF1:
					printf("Error: Permission Denied – Missing Privilege.\n");
				break;
				default:
					printf("Unknown code.\n");
				break;
			}
		break;
		case 0x6A:
			switch (sw2)
			{
				case 0x00:
					printf("Error: No information given (Bytes P1 and/or P2 are incorrect).\n");
				break;
				case 0x80:
					printf("Error: The parameters in the data field are incorrect.\n");
				break;
				case 0x81:
					printf("Error: Function not supported.\n");
				break;
				case 0x82:
					printf("Error: File not found.\n");
				break;
				case 0x83:
					printf("Error: Record not found.\n");
				break;
				case 0x84:
					printf("Error: There is insufficient memory space in record or file.\n");
				break;
				case 0x85:
					printf("Error: Lc inconsistent with TLV structure.\n");
				break;
				case 0x86:
					printf("Error: Incorrect P1 or P2 parameter.\n");
				break;
				case 0x87:
					printf("Error: Lc inconsistent with P1-P2.\n");
				break;
				case 0x88:
					printf("Error: Referenced data not found.\n");
				break;
				case 0x89:
					printf("Error: File already exists.\n");
				break;
				case 0x8A:
					printf("Error: DF name already exists.\n");
				break;
				case 0xF0:
					printf("Error: Wrong parameter value.\n");
				break;
				default:
					printf("Unknown code.\n");
				break;
			}
		break;
		case 0x6B:
			if (sw2 == 0x00)
				printf("Error: Wrong parameter(s) P1-P2.\n");
			else
				printf("Error: Reference incorrect (procedure byte), (ISO 7816-3).\n");
		break;
		case 0x6C:
			if (sw2 == 0x00)
				printf("Error: Incorrect P3 length.\n");
			else
				printf("Error: Bad length value in Le; 0x%02x is the correct exact Le.\n",sw2);
		break;
		case 0x6D:
			if (sw2 == 0x00)
				printf("Error: Instruction code not supported or invalid.\n");
			else
				printf("Error: Instruction code not programmed or invalid (procedure byte), (ISO 7816-3).\n");
		break;
		case 0x6E:
			if (sw2 == 0x00)
				printf("Error: Class not supported.\n");
			else
				printf("Instruction class not supported (procedure byte), (ISO 7816-3)");
		break;
		case 0x6F:
			if (sw2 == 0x00)
				printf("Error: Command aborted – more exact diagnosis not possible (e.g., operating system error).\n");
			else if (sw2 == 0xFF)
				printf("Error: Card dead (overuse, …).\n");
			else
				printf("Error: Internal Exception. No precise diagnosis (procedure byte), (ISO 7816-3).\n");
		break;
		case 0x90:
			switch (sw2)
			{
				case 0x00:
					printf("Information: Command successfully executed (OK).\n");
				break;
				case 0x04:
					printf("Warning: PIN not succesfully verified, 3 or more PIN tries left.\n");
				break;
				case 0x08:
					printf("Key/file not found.\n");
				break;
				case 0x80:
					printf("Warning: Unblock Try Counter has reached zero.\n");
				break;
				default:
					printf("Unknown code.\n");
				break;
			}
		break;
		case 0x91:
			switch (sw2)
			{
				case 0x00:
				printf("OK.\n");
				break;
				case 0x01:
				printf("States.activity, States.lock Status or States.lockable has wrong value.\n");
				break;
				case 0x02:
				printf("Transaction number reached its limit.\n");
				break;
				case 0x0C:
				printf("No changes.\n");
				break;
				case 0x0E:
				printf("Insufficient NV-Memory to complete command.\n");
				break;
				case 0x1C:
				printf("Command code not supported.\n");
				break;
				case 0x1E:
				printf("CRC or MAC does not match data.\n");
				break;
				case 0x40:
				printf("Invalid key number specified.\n");
				break;
				case 0x7E:
				printf("Length of command string invalid.\n");
				break;
				case 0x9D:
				printf("Not allow the requested command.\n");
				break;
				case 0x9E:
				printf("Value of the parameter invalid.\n");
				break;
				case 0xA0:
				printf("Requested AID not present on PICC.\n");
				break;
				case 0xA1:
				printf("Unrecoverable error within application.\n");
				break;
				case 0xAE:
				printf("Authentication status does not allow the requested command.\n");
				break;
				case 0xAF:
				printf("Additional data frame is expected to be sent.\n");
				break;
				case 0xBE:
				printf("Out of boundary.\n");
				break;
				case 0xC1:
				printf("Unrecoverable error within PICC.\n");
				break;
				case 0xCA:
				printf("Previous Command was not fully completed.\n");
				break;
				case 0xCD:
				printf("PICC was disabled by an unrecoverable error.\n");
				break;
				case 0xCE:
				printf("Number of Applications limited to 28.\n");
				break;
				case 0xDE:
				printf("File or application already exists.\n");
				break;
				case 0xEE:
				printf("Could not complete NV-write operation due to loss of power.\n");
				break;
				case 0xF0:
				printf("Specified file number does not exist.\n");
				break;
				case 0xF1:
				printf("Unrecoverable error within file.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		case 0x92:
			if (sw2 < 0x10)
				printf("Information: Writing to EEPROM successful after %d attempts.\n", sw2 & 0xF);
			else
				switch (sw2)
				{
					case 0x10:
					printf("Error: Insufficient memory. No more storage available.\n");
					break;
					case 0x40:
					printf("Error: Writing to EEPROM not successful.\n");
					break;
					default:
					printf("Unknown code.\n");
					break;
				}
		break;
		case 0x93:
			switch (sw2)
			{
				case 0x01:
				printf("Integrity error.\n");
				break;
				case 0x02:
				printf("Candidate S2 invalid.\n");
				break;
				case 0x03:
				printf("Error: Application is permanently locked.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x94:
			switch (sw2)
			{
				case 0x00:
				printf("Error: No EF selected.\n");
				break;
				case 0x01:
				printf("Candidate currency code does not match purse currency.\n");
				break;
				case 0x02:
				printf("Candidate amount too high.\n");
				break;
				case 0x03:
				printf("Candidate amount too low.\n");
				break;
				case 0x04:
				printf("Error: FID not found, record not found or comparison pattern not found.\n");
				break;
				case 0x05:
				printf("Problems in the data field.\n");
				break;
				case 0x06:
				printf("Error: Required MAC unavailable.\n");
				break;
				case 0x07:
				printf("Bad currency : purse engine has no slot with R3bc currency.\n");
				break;
				case 0x08:
				printf("Error: Selected file type does not match command.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x95:
			if (sw2 == 0x80)
				printf("Bad sequence.\n");
			else
				printf("Unknown code.\n");
		break;
		case 0x96:
			if (sw2 == 0x81)
				printf("Slave not found.\n");
			else
				printf("Unknown code.\n");
		break;
		case 0x97:
			switch (sw2)
			{
				case 0x00:
				printf("PIN blocked and Unblock Try Counter is 1 or 2.\n");
				break;
				case 0x02:
				printf("Main keys are blocked.\n");
				break;
				case 0x04:
				printf("PIN not succesfully verified, 3 or more PIN tries left.\n");
				break;
				case 0x84:
				printf("Base key.\n");
				break;
				case 0x85:
				printf("Limit exceeded – C-MAC key.\n");
				break;
				case 0x86:
				printf("SM error – Limit exceeded – R-MAC key.\n");
				break;
				case 0x87:
				printf("Limit exceeded – sequence counter.\n");
				break;
				case 0x88:
				printf("Limit exceeded – R-MAC length.\n");
				break;
				case 0x89:
				printf("Service not available.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x98:
			switch (sw2)
			{
				case 0x02:
				printf("Error: No PIN defined.\n");
				break;
				case 0x04:
				printf("Error: Access conditions not satisfied, authentication failed.\n");
				break;
				case 0x35:
				printf("Error: ASK RANDOM or GIVE RANDOM not executed.\n");
				break;
				case 0x40:
				printf("Error: PIN verification not successful.\n");
				break;
				case 0x50:
				printf("Error: INCREASE or DECREASE could not be executed because a limit has been reached.\n");
				break;
				case 0x62:
				printf("Error: Authentication Error, application specific (incorrect MAC).\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x99:
			switch (sw2)
			{
				case 0x00:
				printf("1 PIN try left.\n");
				break;
				case 0x04:
				printf("PIN not succesfully verified, 1 PIN try left.\n");
				break;
				case 0x85:
				printf("Wrong status – Cardholder lock.\n");
				break;
				case 0x86:
				printf("Error: Missing privilege.\n");
				break;
				case 0x87:
				printf("PIN is not installed.\n");
				break;
				case 0x88:
				printf("Wrong status – R-MAC state.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x9A:
			switch (sw2)
			{
				case 0x00:
				printf("2 PIN try left.\n");
				break;
				case 0x04:
				printf("PIN not succesfully verified, 2 PIN try left.\n");
				break;
				case 0x71:
				printf("Wrong parameter value – Double agent AID.\n");
				break;
				case 0x72:
				printf("Wrong parameter value – Double agent Type.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x9D:
			switch (sw2)
			{
				case 0x05:
				printf("Error: Incorrect certificate type.\n");
				break;
				case 0x07:
				printf("Error: Incorrect session data size.\n");
				break;
				case 0x08:
				printf("Error: Incorrect DIR file record size.\n");
				break;
				case 0x09:
				printf("Error: Incorrect FCI record size.\n");
				break;
				case 0x0A:
				printf("Error: Incorrect code size.\n");
				break;
				case 0x10:
				printf("Error: Insufficient memory to load application.\n");
				break;
				case 0x11:
				printf("Error: Invalid AID.\n");
				break;
				case 0x12:
				printf("Error: Duplicate AID.\n");
				break;
				case 0x13:
				printf("Error: Application previously loaded.\n");
				break;
				case 0x14:
				printf("Error: Application history list full.\n");
				break;
				case 0x15:
				printf("Error: Application not open.\n");
				break;
				case 0x17:
				printf("Error: Invalid offset.\n");
				break;
				case 0x18:
				printf("Error: Application already loaded.\n");
				break;
				case 0x19:
				printf("Error: Invalid certificate.\n");
				break;
				case 0x1A:
				printf("Error: Invalid signature.\n");
				break;
				case 0x1B:
				printf("Error: Invalid KTU.\n");
				break;
				case 0x1D:
				printf("Error: MSM controls not set.\n");
				break;
				case 0x1E:
				printf("Error: Application signature does not exist.\n");
				break;
				case 0x1F:
				printf("Error: KTU does not exist.\n");
				break;
				case 0x20:
				printf("Error: Application not loaded.\n");
				break;
				case 0x21:
				printf("Error: Invalid Open command data length.\n");
				break;
				case 0x30:
				printf("Error: Check data parameter is incorrect (invalid start address).\n");
				break;
				case 0x31:
				printf("Error: Check data parameter is incorrect (invalid length).\n");
				break;
				case 0x32:
				printf("Error: Check data parameter is incorrect (illegal memory check area).\n");
				break;
				case 0x40:
				printf("Error: Invalid MSM Controls ciphertext.\n");
				break;
				case 0x41:
				printf("Error: MSM controls already set.\n");
				break;
				case 0x42:
				printf("Error: Set MSM Controls data length less than 2 bytes.\n");
				break;
				case 0x43:
				printf("Error: Invalid MSM Controls data length.\n");
				break;
				case 0x44:
				printf("Error: Excess MSM Controls ciphertext.\n");
				break;
				case 0x45:
				printf("Error: Verification of MSM Controls data failed.\n");
				break;
				case 0x50:
				printf("Error: Invalid MCD Issuer production ID.\n");
				break;
				case 0x51:
				printf("Error: Invalid MCD Issuer ID.\n");
				break;
				case 0x52:
				printf("Error: Invalid set MSM controls data date.\n");
				break;
				case 0x53:
				printf("Error: Invalid MCD number.\n");
				break;
				case 0x60:
				printf("Error: MAC verification failed.\n");
				break;
				case 0x61:
				printf("Error: Maximum number of unblocks reached.\n");
				break;
				case 0x62:
				printf("Error: Card was not blocked.\n");
				break;
				case 0x63:
				printf("Error: Crypto functions not available.\n");
				break;
				case 0x64:
				printf("Error: No application loaded.\n");
				break;
				default:
				printf("Unknown code.\n");
				break;
			}
		break;
		case 0x9E:
			if (sw2 == 0x00)
				printf("PIN not installed.\n");
			else if (sw2 == 0x04)
				printf("PIN not succesfully verified, PIN not installed.\n");
			else
				printf("Unknown code.\n");
		break;
		case 0x9F:
			if (sw2 == 0x00)
				printf("PIN blocked and Unblock Try Counter is 3.\n");
			else if (sw2 == 0x04)
				printf("PIN not succesfully verified, PIN blocked and Unblock Try Counter is 3.\n");
			else
				printf(": Command successfully executed; %d bytes of data are available and can be requested using GET RESPONSE.\n", sw2);
		break;
		default:
			printf("Unknown code.\n");
		break;
	}
}
