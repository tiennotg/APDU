/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * tlv.h: High-level interface for TLV object manipulation (TLV stands
 * for Tag-Length-Value, see the EMV Book for more details).
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

#ifndef TLV_H
#define TLV_H

#include <stdbool.h>
#include <stdint.h>

enum
{
	TLV_UNIVERSAL_CLASS,
	TLV_APPLICATION_CLASS,
	TLV_CONTEXT_SPECIFIC_CLASS,
	TLV_PRIVATE_CLASS
};

struct TLVobject
{
	unsigned int tag;		// ID
	bool constructed;		// true if TLVobject contains another TLVobject(s)
	unsigned int length;	// if constructed == false, length of **data. Otherwise unused.
	uint8_t oclass;			// class of TLVobject (see EMV 4.3 Book 3, Annex B1)
	
	void **data;			// NULL-terminated array
							// type TLVobject** if constructed == true,
							// uint8_t** otherwise
};

struct TLVobject* tlvParseData(uint8_t *data, uint8_t length);
struct TLVobject* tlvObjectLookForTag(struct TLVobject* obj, unsigned int tag);
void tlvObjectPrint(struct TLVobject* obj);
void tlvObjectFree(struct TLVobject* obj);

#endif
