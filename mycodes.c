/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * mycodes.c: Custom response codes of the Arduino board.
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

#include "mycodes.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mycodesPrintStr(int code, char *preStr)
{
	char *errorStr = NULL;
	switch (code)
	{
		case MYTERM_TIMEOUT:
			errorStr = TIMEOUT_STR;
			break;
		case MYTERM_NOTFOUND:
			errorStr = NOTFOUND_STR;
			break;
		case MYTERM_READERROR:
			errorStr = READERROR_STR;
			break;
		case MYTERM_WRITEERROR:
			errorStr = WRITEERROR_STR;
			break;
		case MYTERM_UNDEFERROR:
			errorStr = UNDEFERROR_STR;
			break;
		case MYTERM_OK:
			errorStr = OK_STR;
			break;
		case MYTERM_CARDFOUND:
			errorStr = CARDFOUND_STR;
			break;
	}
	
	if (errorStr == NULL)
		fprintf(stderr, "Error 0x%02x\n", code);
	else if (preStr == NULL)
		fprintf(stderr, "Error 0x%02x: %s\n", code, errorStr);
	else
		fprintf(stderr, "%s %s\n", preStr, errorStr);
}
