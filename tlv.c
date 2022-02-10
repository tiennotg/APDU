/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * tlv.c: High-level interface for TLV object manipulation (TLV stands
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

#include "tlv.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct TLVobject* tlvParseData(uint8_t *data, uint8_t length)
{
	if (length < 2 || data == NULL) return NULL;
		
	struct TLVobject *ptr = (struct TLVobject*) malloc(sizeof(struct TLVobject));
	if (ptr == NULL) return NULL;
	uint8_t pindex = 0;
	
	ptr->oclass = (data[pindex] & 0xC0) >> 6;
	ptr->constructed = ((data[pindex] & 0x20) == 0x20) ? true : false;
	
	ptr->tag = data[pindex++];
	// if all of the 5 lsb are high, tag is coded on more than one byte.
	if ((ptr->tag & 0x1F) == 0x1F)
	{
		// if msb is high, another tag byte is following
		while ((data[pindex] & 0x80) == 0x80)
		{
			ptr->tag = (ptr->tag << 8) + data[pindex++];
			if (pindex > length) // expecting another byte, but data seems to be incomplete, exiting
			{
				free(ptr);
				return NULL;
			}
		}
		// don't forget to add the last byte with msb low :-)
		ptr->tag = (ptr->tag << 8) + data[pindex++];
	}
	
	if (data[pindex] < 0x80)
		ptr->length = data[pindex++];
	else
	{
		uint8_t count = data[pindex++] & 0x7F;
		ptr->length = 0;
		for (int i=pindex; i<pindex+count; i++)
			ptr->length = (ptr->length << 8) + data[i];
		pindex += count;
	}
	
	if (ptr->length > length-pindex) // data length is shorter than expected. Exiting
	{
		free(ptr);
		return NULL;
	}
	
	
	if (ptr->constructed)
	{
		int record_count = 0;
		
		ptr->data = malloc(sizeof(void*));
		if (ptr->data == NULL)
		{
			free(ptr);
			return NULL;
		}
		
		while (pindex < length)
		{
			uint8_t rstart = pindex;
			unsigned int rlen = 0; // length of subrecord
			
			// Skip tag
			if ((data[pindex++] & 0x1F) == 0x1F)
				while ((data[pindex++] & 0x80) == 0x80);
			
			// calculate length
			if (data[pindex] < 0x80)
				rlen = data[pindex++];
			else
			{
				uint8_t count = data[pindex++] & 0x7F;
				rlen = 0;
				for (int i=pindex; i<pindex+count; i++)
					rlen = (rlen << 8) + data[i];
				pindex += count;
			}
			
			// point on start of next record
			pindex += rlen;
			
			ptr->data[record_count++] = (void*) tlvParseData(data+rstart, pindex-rstart);
			ptr->data = realloc(ptr->data, sizeof(void*)*(record_count+1));
			ptr->data[record_count] = NULL;
		}
	}
	else
	{
		ptr->data = malloc(sizeof(void*)*2);
		if (ptr->data == NULL)
		{
			free(ptr);
			return NULL;
		}
		
		ptr->data[1] = NULL;
		ptr->data[0] = malloc(sizeof(uint8_t)*ptr->length);
		if (ptr->data[0] == NULL)
		{
			free(ptr->data);
			free(ptr);
			return NULL;
		}
		
		memcpy(ptr->data[0], data+pindex, ptr->length);
	}
	
	return ptr;
}

struct TLVobject* tlvObjectLookForTag(struct TLVobject *obj, unsigned int tag)
{
	if (obj == NULL) return NULL;
	if (obj->tag == tag)
		return obj;
	else
	{
		if (obj->constructed)
		{
			int i=0;
			while (obj->data[i] != NULL)
			{
				struct TLVobject *ret = NULL;
				ret = tlvObjectLookForTag((struct TLVobject*) obj->data[i++], tag);
				if (ret != NULL)
					return ret;
			}
			return NULL;
		}
		else
			return NULL;
	}
}

void tlvObjectPrintIndented(struct TLVobject *obj, unsigned int indentLevel)
{
	char *padd = malloc(sizeof(char)*(indentLevel*2+1));
	if (padd == NULL) return;
	
	padd[indentLevel*2] = '\0';
	memset(padd,' ',indentLevel*2);
	
	if (obj == NULL)
	{
		printf("%sNo data.\n",padd);
		return;
	}
	
	printf("%sTag: 0x%x\n", padd, obj->tag);
	printf("%sLength: %d\n", padd, obj->length);
	
	printf("%sClass: ", padd);
	switch (obj->oclass)
	{
		case TLV_APPLICATION_CLASS:
		printf("application\n");
		break;
		case TLV_CONTEXT_SPECIFIC_CLASS:
		printf("context specific\n");
		break;
		case TLV_PRIVATE_CLASS:
		printf("private\n");
		break;
		case TLV_UNIVERSAL_CLASS:
		printf("universal\n");
		break;
	}
	
	if (obj->constructed)
	{
		printf("%sSubobjects:\n\n", padd);
		
		int i=0;
		while (obj->data[i] != NULL)
			tlvObjectPrintIndented((struct TLVobject*) obj->data[i++], indentLevel+1);
	}
	else
	{
		bool printable = true;
		printf("%sData: ", padd);
		for (uint8_t j=0; j<obj->length; j++)
		{
			printf("%02x",((uint8_t*)obj->data[0])[j]);
			if (((uint8_t*)obj->data[0])[j] > 0x7F || ((uint8_t*)obj->data[0])[j] < 0x20)
				printable = false;
		}
		printf("\n");
		
		if (printable)
		{
			printf("%sStr: ", padd);
			for (uint8_t j=0; j<obj->length; j++)
			{
				printf("%c",((uint8_t*)obj->data[0])[j]);
			}
			printf("\n");
		}
		printf("\n");
	}
}

void tlvObjectPrint(struct TLVobject *obj)
{
	tlvObjectPrintIndented(obj,0);
}

void tlvObjectFree(struct TLVobject *obj)
{
	if (obj == NULL)
		return;
	
	if (obj->constructed)
	{
		int i=0;
		while (obj->data[i] != NULL)
			tlvObjectFree(obj->data[i++]);
	}
	else
	{
		free(obj->data[0]);
		free(obj->data);
	}
	free(obj);
}
