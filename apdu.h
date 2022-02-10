/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * apdu.h: High-level interface for sending APDU commands over UART.
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

#ifndef APDU_H
#define APDU_H

#include <stdint.h>
#include <stdbool.h>

#define APDU_SW1_OK 0x90
#define APDU_SW2_OK 0x00

bool apduInitialize(int serialPort);
bool apduWaitForCard(int serialPort);
void apduSendCommand(int serialPort, uint8_t cla, uint8_t ins, uint8_t p1,
uint8_t p2,uint8_t lc, uint8_t *data, uint8_t le, bool isLePresent);
int apduWaitForResponse(int serialPort, uint8_t *resdata, uint8_t *reslen, uint8_t *sw1, uint8_t *sw2);

void apduPrintError(uint8_t sw1, uint8_t sw2);

#endif
