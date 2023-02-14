
// Modified from the excellent code here: https://github.com/bitbank2/oled_turbo
// Shrink-ified to fit onto the ATtiny85 by [ShepherdingElectrons] 2020

#include <avr/io.h>
#include <string.h> /* memcpy */

#include "oled_lib.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Transmit a byte and ack bit
//
inline void i2cByteOut(uint8_t b) // This is only called from i2Cbegin, before tones are started so don't need to worry about
{
  uint8_t i;

  CLEAR_SCL;

  for (i = 0; i < 8; i++)
  {
    CLEAR_SDA;
    if (b & 0x80)
      SET_SDA;

    SET_SCL;
    CLEAR_SCL;
    
    b <<= 1;
  } // for i
  // ack bit

  CLEAR_SDA;
  SET_SCL;
  CLEAR_SCL;

} /* i2cByteOut() */

void i2cBegin(uint8_t addr)
{
  SET_SDA;
  SET_SCL;

  OUTPUT_SDA;
  OUTPUT_SCL;

  CLEAR_SDA;
  CLEAR_SCL;

  i2cByteOut(addr << 1); // send the slave address
} /* i2cBegin() */

void i2cWrite(uint8_t *pData, uint8_t bLen)
{
  uint8_t i, b;

  while (bLen--)
  {
    b = *pData++;
    if (b == 0 || b == 0xff) // special case can save time
    {
      CLEAR_SDA;

      if (b & 0x80)
        SET_SDA;
		
      CLEAR_SCL;

      for (i = 0; i < 8; i++)
      {
        SET_SCL;
        CLEAR_SCL;
      } // for i
    }
    else // normal byte needs every bit tested
    {
      CLEAR_SCL;

      for (i = 0; i < 8; i++)
      {
        CLEAR_SDA;
        if (b & 0x80)
          SET_SDA;

        SET_SCL;
        CLEAR_SCL;

        b <<= 1;
      } // for i

    }
    
    CLEAR_SDA;
    SET_SCL;
    CLEAR_SCL;