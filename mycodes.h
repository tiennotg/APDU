/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * mycodes.h: Custom response codes of the Arduino board.
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

#ifndef MYCODES_H
#define MYCODES_H

// Opcodes / rescodes
#define MYTERM_TIMEOUT    0x11
#define MYTERM_NOTFOUND   0x12
#define MYTERM_READERROR  0x21
#define MYTERM_WRITEERROR 0x22
#define MYTERM_UNDEFERROR 0xF0

#define MYTERM_OK         0x0A
#define MYTERM_CARDFOUND  0x0B
#define MYTERM_COMMAND    0x0C

#define OK_STR			 "OK"
#define CARDFOUND_STR	 "Card detected"
#define TIMEOUT_STR		 "Timeout"
#define NOTFOUND_STR		 "Chip/card not found"
#define READERROR_STR	 "Read error"
#define WRITEERROR_STR	 "Write error"
#define UNDEFERROR_STR	 "Undefined error"

#define BUFFER_SIZE 255

void mycodesPrintStr(int code, char *preStr);

#endif
