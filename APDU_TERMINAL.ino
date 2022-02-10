/*
 * Copyright 2021-2022 Guilhem Tiennot
 * 
 * Little example program that retrieves credit card number in NFC with
 * a PN532-based module. It can communicate with most of the Arduino
 * boards over UART.
 * 
 * APDU_TERMINAL.ino
 * 
 * Arduino part of the project. Works with a PN532-based module,
 * in SPI mode.
 * 
 * Dependancy: Adafruit PN532 library
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

#include <Adafruit_PN532.h>

// SPI Pinout
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

#define UID_LENGTH  7
#define TIMEOUT     1000 // ms
#define READ_BUFFER_LEN 255 // max 255
#define ACK_TIMEOUT 10000 // ms

/*
 * Frame format:
 * - 1 byte: opcode / rescode
 * - 1 byte: data length (may be 0)
 * - xx bytes: data
 */

// Opcodes / rescodes
#define MYTERM_TIMEOUT    0x11 // Card communication timed out
#define MYTERM_NOTFOUND   0x12 // No PN532 chip found
#define MYTERM_READERROR  0x21 // Card answer data wasn't read successfully
#define MYTERM_WRITEERROR 0x22 // Card didn't ack the sent command
#define MYTERM_UNDEFERROR 0xF0 // Generic error

#define MYTERM_OK         0x0A // Everything is all right!
#define MYTERM_CARDFOUND  0x0B // When the chip detects a new card
#define MYTERM_COMMAND    0x0C // Computer wants to send a command to the card

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

unsigned long timeEllapsed = 0;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    uint8_t buf[2] = {MYTERM_NOTFOUND,0x00};
    Serial.write(buf ,2);
    while(1);
  }

  // We do not use SAM, so init it with no parameters.
  nfc.SAMConfig();

  uint8_t verbuf[5];
  verbuf[0] = MYTERM_OK;
  verbuf[1] = 0x03; // data length
  verbuf[2] = (versiondata>>24) & 0xFF;
  verbuf[3] = (versiondata>>16) & 0xFF;
  verbuf[4] = (versiondata>>8) & 0xFF;
  Serial.write(verbuf, 5);
}

void loop(void)
{
  uint8_t uid[UID_LENGTH];
  uint8_t uidLength = UID_LENGTH;
  
  bool waitForLength = false;
  uint8_t dataLength = 0;
  uint8_t *dataBuffer = NULL;

  // This function actually works also for credit cards
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength))
  {
    timeEllapsed = millis();
    Serial.write(MYTERM_CARDFOUND);
    Serial.write(UID_LENGTH);
    Serial.write(uid, UID_LENGTH);

    // Timeout loop
    while (millis()-timeEllapsed < TIMEOUT)
    {
      // Wait for commands. Read serial port one byte after another.
      int r = Serial.read();
      if (r >= 0)
      {
        uint8_t c = r & 0xFF;
        
        // reset timeout
        timeEllapsed = millis();

        // if we don't have received the command length,
        // and computer start sending a new command...
        if (!waitForLength && c == MYTERM_COMMAND)
          waitForLength = true;
        else if (waitForLength)
        {
          dataLength = c;
          waitForLength = false;
  
          if (dataLength > 0)
          {
            dataBuffer = (uint8_t*) malloc(sizeof(uint8_t)*dataLength);
            if (dataBuffer == NULL)
            {
              dataLength = 0;
              Serial.write(MYTERM_UNDEFERROR);
              Serial.write(0x00);
              return;
            }
            
            int n = Serial.readBytes(dataBuffer, dataLength);

            // We receive all the data, start transmitting to the card!
            if (n == dataLength)
            {
              // If the card don't ack, raise an error
              if (!nfc.sendCommandCheckAck(dataBuffer, dataLength, ACK_TIMEOUT))
              {
                Serial.write(MYTERM_WRITEERROR);
                Serial.write(0x00);
                return;
              }
              // Command was successfully sent, wait for answer.
              else
              {
                uint8_t readBufferLen = READ_BUFFER_LEN;
                uint8_t *readBuffer = (uint8_t*) malloc(sizeof(uint8_t)*readBufferLen);
                if (readBuffer == NULL)
                {
                  dataLength = 0;
                  Serial.write(MYTERM_UNDEFERROR);
                  Serial.write(0x00);
                  return;
                }
                if (!PN532ReadData(readBuffer, &readBufferLen))
                {
                  Serial.write(MYTERM_READERROR);
                  Serial.write(0x00);
                  free(readBuffer);
                  return;
                }
                Serial.write(MYTERM_OK);
                Serial.write(readBufferLen);
                Serial.write(readBuffer,readBufferLen);
                dataLength = 0;
                free(readBuffer);
              }
            }
          }
        }
      }
    }

    // Timeout is over. Send an error.
    Serial.write(MYTERM_TIMEOUT);
    Serial.write(0x00);
  }
  if (dataBuffer != NULL)
    free(dataBuffer);
}

bool PN532ReadData(uint8_t *buffer, uint8_t *len)
{
  if (*len == 0) return false;
  uint8_t *tmpbuf = (uint8_t*) malloc(*len);
  if (tmpbuf == NULL) return false;

  nfc.readdata(tmpbuf, *len);

  // The 8 first bytes are related to the PN532 communication protocol.
  // Useless for us, except the last one: if it is not zero, there were
  // a communication error with the card.
  if (tmpbuf[7] != 0)
  {
    free(tmpbuf);
    return false;
  }

  // Data starts at position 8.
  // tmpbuf[3] is the length of the frame, including the command code byte,
  // the frame identifier byte and the status code byte.
  // So, adding 4 to this field gives us the data end index.
  // See the PN532 user manual, section 6.2.1.1, for more details.
  uint8_t start_index = 8;
  uint8_t stop_index = tmpbuf[3] + 4;

  if (stop_index < start_index)
    *len = 0;
  else
    *len = stop_index-start_index+1;
  if (*len > 0)
    memcpy(buffer, tmpbuf+start_index, *len);
  free(tmpbuf);
  
  return true;
}
