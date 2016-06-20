#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include "inc/ascii.h"


uint8_t lcd_buffer[1024];


void spiSendChar(uint8_t send_char)
{
  SPDR = send_char;
  while (!(SPSR & (1<<SPIF))); // ждем пока флаг прерывания (SPIF) регистра контроля SPI (SPSR) встанет в 1
}

void sendData(uint8_t data)
{
  spiSendChar(0xFA);
  spiSendChar(data & 0xF0);
  spiSendChar((data<<4));
  _delay_us(25);
}

void sendCode(uint8_t code)
{
  spiSendChar(0xF8);
  spiSendChar(code & 0xF0);
  spiSendChar((code<<4));
  _delay_us(25);
}


void GLCDInit()
{
  _delay_ms(50);          // Short delay after resetting.

  sendCode(0b00100000);// 4-bit mode.
  _delay_ms(1);
  sendCode(0b00100000);// 4-bit mode again.
  _delay_ms(1);
  sendCode(0b00001100);// Cursor and blinking cursor disabled.
  _delay_ms(1);;
  sendCode(0b00000001);// Clears screen.
  _delay_ms(10);
  sendCode(0b00000110);// Cursor moves right, no display shift. 
  sendCode(0b00000010);// Returns to home. Cursor moves to starting point.
}

void GLCDEnableGraphics()
{
  sendCode(0x20);    
  _delay_ms(1);
  sendCode(0x24);      // Switch to extended instruction mode. 
  _delay_ms(1);
  sendCode(0x26);      // Enable graphics mode.
  _delay_ms(1);
}

void GLCDClearGraphics()
{
  // This function performs similar to the SKPIC32_GLCDFillScreenGraphic but
  // only zeros are sent into the screen instead of data from an array.
  unsigned char x, y;
  for(y = 0; y < 64; y++)
  {
    if(y < 32)
    {
      sendCode(0x80 | y);
      sendCode(0x80);
    }
    else
    {
      sendCode(0x80 | (y-32));
      sendCode(0x88);
    }
    for(x = 0; x < 8; x++)
    {
      sendCode(0x00);
      sendCode(0x00);
    }
  }
}

void GLCDDisableGraphics()
{
  sendCode(0x20);      // Graphics and extended instruction mode turned off.
  _delay_ms(1);
}



void GLCDFillScreenGraphic(const unsigned char* graphic)
{
  unsigned char x, y;

  for(y = 0; y < 64; y++)
  {

    if(y < 32)
    {
      sendCode(0x80 | y);
      sendCode(0x80 | 0);
    } 
    else
    {
        sendCode(0x80 | y-32);
        sendCode(0x88 | 0); 
    }

    for(x = 0; x < 16; x++)
    { 

      sendData(graphic[x + 16*y]);

    }
    
  }
}

void buffer_setPoint(int x, int y, uint8_t *g_buffer){

  int first_bt_line = (y-1)*16;

  int bt_in_line = (int) (x-1)/8;

  g_buffer[first_bt_line + bt_in_line] |= (0b10000000 >> (x-1)%8);

}

void buffer_setPointVar(int x, int y, uint8_t *g_buffer, int i){

  int first_bt_line = (y-1)*16;

  int bt_in_line = (int) (x-1)/8;

  if(i)
  {
    g_buffer[first_bt_line + bt_in_line] |= (0b10000000 >> (x-1)%8);
  } 
  else
  {
    g_buffer[first_bt_line + bt_in_line] |= 0b00000000;
  }
  
  
}

void GLCDDrawLine(int x1, int y1, int x2, int y2, uint8_t *g_buffer) 
{
    const int deltaX = abs(x2 - x1);
    const int deltaY = abs(y2 - y1);
    const int signX = x1 < x2 ? 1 : -1;
    const int signY = y1 < y2 ? 1 : -1;
    //
    int error = deltaX - deltaY;
    //
    buffer_setPoint(x2, y2, g_buffer); 
    while(x1 != x2 || y1 != y2) {
        buffer_setPoint(x1, y1, g_buffer); 
        const int error2 = error * 2;
        //
        if(error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        if(error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }

}

void GLCDDrawCircle(int x0, int y0, int radius, uint8_t *g_buffer) 
{
  int x = 0;
  int y = radius;
  int delta = 1 - 2 * radius;
  int error = 0;
  while(y >= 0) {
          
    buffer_setPoint(x0 + x, y0 + y, g_buffer);
    buffer_setPoint(x0 + x, y0 - y, g_buffer);
    buffer_setPoint(x0 - x, y0 + y, g_buffer);
    buffer_setPoint(x0 - x, y0 - y, g_buffer);
    
    error = 2 * (delta + y) - 1;
    if(delta < 0 && error <= 0) {
            ++x;
            delta += 2 * x + 1;
            continue;
    }
    error = 2 * (delta - x) - 1;
    if(delta > 0 && error > 0) {
            --y;
            delta += 1 - 2 * y;
            continue;
    }
    ++x;
    delta += 2 * (x - y);
    --y;
  }
}

void GLCDDrawRect(int x1, int y1, int x2, int y2, uint8_t *g_buffer)
{

  GLCDDrawLine(x1, y1, x2, y1, g_buffer);
  GLCDDrawLine(x1, y2, x2, y2, g_buffer);
  GLCDDrawLine(x1, y1, x1, y2, g_buffer);
  GLCDDrawLine(x2, y1, x2, y2, g_buffer);

}


void GLCDDrawChar(int x, int y, int asciiCode, uint8_t *g_buffer) 
{

  //asciiCode = asciiCode - 32;

  //asciiCode = 0;

  int i;
  for(i = 0; i < 5; i++)
  {
    int e;
    for(e = 8; e >= 0; e--)
    {
      //buffer_setPointVar(i+1+x, e+1+y, g_buffer, font[asciiCode*4+e] >> e);
      buffer_setPointVar(i+1+x, e+1+y, g_buffer, font[asciiCode*5+i]&(1 << e));
    }
  }

}

void DLCDDrawString(int x, int y, char cMyCharacter[], uint8_t *g_buffer){

  int i;
  for(i = 0; i < strlen(cMyCharacter); i++)
  {
    int iMyAsciiValue = (int)cMyCharacter[i];
    
    if(x >= 128-7)
    {

      y = y + 8;
      x = 1;

    }

    GLCDDrawChar(x, y, iMyAsciiValue, g_buffer);

    x = x + 6;


  }


}

void SPI_init(void){
  DDRB|=0x8E;   // 0b10001110 -> PB7(CS) PB1(SCK) PB2(MOSI), PB3(MISO)
  PORTB|=0x8E;  // 0b10001110
  // SPI initialization
  // SPI Type: Master
  // SPI Clock Rate: 2*1843.200 kHz
  // SPI Clock Phase: Cycle Half
  // SPI Clock Polarity: Low
  // SPI Data Order: MSB First
  SPCR=0x50;  // 0b01010000 включаем модуль SPI, выбираем режим работы 1 - ведущий
  SPSR=0x01;  // 0b00000001
}

int main(void)
{

  SPI_init();
  GLCDInit();
  GLCDEnableGraphics();

  int i;
  for(i = 0; i < 1024; i++)
  {
    lcd_buffer[i] = 0x00;
  }



  //buffer_setPoint(128, 64, &lcd_buffer);

  //GLCDDrawLine(10, 10, 128, 64, &lcd_buffer);

  //GLCDDrawCircle(64, 32, 10, &lcd_buffer);

  //GLCDDrawRect(10, 10, 90, 50, &lcd_buffer);

  //GLCDDrawChar(20, 20, 48, &lcd_buffer);

  DLCDDrawString(1, 1, "1234567890", &lcd_buffer);

  GLCDFillScreenGraphic(lcd_buffer);


  while(1)
  {

  }

}