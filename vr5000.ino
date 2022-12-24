/*
 * VR5000 Display Replacement
 * STM32F407 + SPI TFT 320x480 with ILI9488
 *
 * Copyright (C) 2022 Michal Krumnikl
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
 
/*
DEV board description https://github.com/mcauser/MCUDEV_DEVEBOX_F407VGT6
TFT/OLED (J4)
    1 3V3
    2 GND
    3 PB15 MOSI
    4 PB13 SCK
    5 PB12 CS
    6 PB14 MISO
    7 PC5 RS
    8 PB1 BLK
 */

#include <SPI.h>
#include <TFT_eSPI.h>  // Hardware-specific library  https://github.com/Bodmer/TFT_eSPI

TFT_eSPI tft = TFT_eSPI();  // Invoke TFT library

//#define SERIAL_DEBUG 1
//#define DEBUG_DIGITS 1

#define HSTEP 16
#define STATE_INIT 0
#define STATE_CMD 1
#define STATE_DATA 2

#define RING_SIZE 32768
volatile unsigned short ring_head;
volatile unsigned short ring_tail;
volatile unsigned short ring_data[RING_SIZE];
unsigned short next_head;

struct sLcdLabels {
  unsigned short bank_id;
  unsigned short pos_id;
  unsigned short bit_id;
  char text[6];
  unsigned short posx;
  unsigned short posy;
  unsigned short fg_color;
  unsigned short bg_color;
};

struct sLcd7Digits {
  unsigned short bank_id;
  unsigned short pos_id;
  unsigned short bit_id;
  unsigned short posx;
  unsigned short posy;
  unsigned short w;
  unsigned short h;
};

#ifdef DEBUG_DIGITS
unsigned char tmp_bb2[16*8];
unsigned char tmp_bb4[16*8];
#endif

// LCD labels cmd[0], cmd[1], cmd[2], cmd[3], text, x, y, foreground, background
const struct sLcdLabels LcdLabels[] = {

  { 4, 0, 9, "F", 3, 2, TFT_BLACK, TFT_WHITE },         //OK
  { 4, 0, 0xA, "BUSY", 33, 2, TFT_BLACK, TFT_GREEN },   //OK
                                                        // {0, 0, "DSP",   93, 2, 0, TFT_BLACK, TFT_WHITE},
  { 4, 2, 0xD, "ATT", 148, 2, TFT_BLACK, TFT_YELLOW },  //OK
  { 4, 3, 2, "NB", 184, 2, TFT_WHITE, TFT_BLACK },      //OK
  { 2, 5, 1, "STEP", 230, 2, TFT_WHITE, TFT_BLACK},     // visible same as Hz
                                                        //{ 2, 4, 0xE, "DELAY", 381, 2, TFT_WHITE, TFT_BLACK},
  { 4, 7, 5, "HOLD", 430, 2, TFT_WHITE, TFT_BLACK},

  { 2, 0, 9, "LOCK", 3, 23, TFT_BLACK, TFT_RED },  //OK
                                                   // {0, 0, "PROG",  40, 23, TFT_BLACK, TFT_WHITE},
                                                   // {0, 0, "SLEEP", 93, 23, TFT_BLACK, TFT_WHITE},
                                                   // {0, 0, "ON",   148, 23, TFT_BLACK, TFT_WHITE},
                                                   // {0, 0, "REC",  184, 23, TFT_BLACK, TFT_WHITE},
  { 2, 4, 4, "AUTO", 230, 23, TFT_WHITE, TFT_BLACK },
  { 2, 5, 0,    "K", 346, 23, TFT_WHITE, TFT_BLACK},
  { 2, 5, 1,   "Hz", 358, 23, TFT_WHITE, TFT_BLACK},
  { 2, 4,0xE, "PAUSE",385, 23, TFT_WHITE, TFT_BLACK},
                                                   // {0, 0, "VCS+", 430, 23, TFT_BLACK, TFT_WHITE},
  { 2, 0, 4, "LSB", 3, 43, TFT_WHITE, TFT_BLACK },     //OK
  { 2, 0, 0xF, "USB", 45, 43, TFT_WHITE, TFT_BLACK },  //OK
  { 2, 1, 8, "CW", 88, 43, TFT_WHITE, TFT_BLACK },     //OK
  { 2, 2, 2, "W", 124, 43, TFT_WHITE, TFT_BLACK },     //OK
  { 2, 2, 7, "AM", 143, 43, TFT_WHITE, TFT_BLACK },    //OK
  { 2, 2, 0xD, "-N", 174, 43, TFT_WHITE, TFT_BLACK },  //OK
  { 2, 3, 6, "W", 205, 43, TFT_WHITE, TFT_BLACK },     //OK
  { 2, 3, 0xA, "FM", 224, 43, TFT_WHITE, TFT_BLACK },  //OK
  { 2, 3, 0xA, "-N", 255, 43, TFT_WHITE, TFT_BLACK }   // ?
  //  {0, 0, "CAT",  322, 48, TFT_WHITE, TFT_BLACK },

  //  {0, 0, "BANK",   373, 55, TFT_WHITE, TFT_BLACK },
  //  {0, 0, "LINK",   375, 73, TFT_WHITE, TFT_BLACK },

  //  {0, 0, "SKIP>",   369, 99, TFT_WHITE, TFT_BLACK },
  //  {0, 0, "SEL>",   375, 115, TFT_WHITE, TFT_BLACK },
  //  {0, 0, "PRI>",   374, 131, TFT_WHITE, TFT_BLACK }

};

// 7segment digits 
const struct sLcd7Digits Lcd7Digits[] = {
  {4, 4, 5  , 282   , 2+3 , 3, 13 },  // f 1st step
  {4, 4, 6  , 282+3 , 2+16, 11, 3 },  // g 1st step
  {4, 4, 7  , 282+14, 2+3 , 3, 13 },  // b 1st step
  {4, 5, 8  , 282+3 , 2   , 11, 3 },  // a 1st step
  {2, 4, 6  , 282+3 , 2+32, 11, 3 },  // d 1st step
  {2, 4, 7  , 282+14, 2+19, 3 ,13 },  // c 1st step
  {2, 4, 5  , 282   , 2+19, 3 ,13 },  // e 1st step

  {2, 4, 8  , 282+18, 2+34, 3 , 3 },  // dot 1st step

  {4, 4, 9  , 304   , 2+3 , 3, 13 },  // f 2nd step
  {4, 4, 0xA, 304+3 , 2+16, 11, 3 },  // g 2nd step
  {4, 4, 0xB, 304+14, 2+3 , 3, 13 },  // b 2nd step
  {4, 5, 1  , 304+3 , 2   , 11, 3 },  // a 2nd step
  {2, 4, 0xA, 304+3 , 2+32, 11, 3 },  // d 2nd step
  {2, 4, 0xB, 304+14, 2+19, 3 ,13 },  // c 2nd step
  {2, 4, 9  , 304   , 2+19, 3 ,13 },  // e 2nd step

  {2, 4, 0xC, 304+18, 2+34, 3 , 3 },  // dot 2nd step

  {4, 5, 0  , 326   , 2+3 , 3, 13 },  // f 3rd step
  {4, 4, 0xE, 326+3 , 2+16, 11, 3 },  // g 3rd step
  {4, 4, 0xF, 326+14, 2+3 , 3, 13 },  // b 3rd step
  {4, 5, 0  , 326+3 , 2   , 11, 3 },  // a 3rd step
  {4, 4, 0xD, 326+3 , 2+32, 11, 3 },  // d 3rd step
  {2, 4, 0xF, 326+14, 2+19, 3 ,13 },  // c 3rd step
  {2, 4, 0xD, 326   , 2+19, 3 ,13 }   // e 3rd step
};

// edit  packages/STMicroelectronics/hardware/stm32/2.3.0/libraries/SrcWrapper/src/stm32/interrupt.cpp

extern "C" {
  void EXTI0_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);   // /WR[1]
    ring_data[ring_head] = GPIOD->IDR;

    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_2) != RESET) {   // A[0]
      __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
      ring_data[ring_head] |= 0x1000;
    }

    ring_head = (ring_head + 1) % RING_SIZE;
  }

  void EXTI2_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
  }
}

unsigned short removeFromRing(void) {
  if (ring_head != ring_tail) {
    unsigned short c = ring_data[ring_tail];
    ring_tail = (ring_tail + 1) % RING_SIZE;
    return c;
  } else {
    return 0xFFFF;
  }
}

void setup(void) {
  // pins connected to VR5000 display pads
  pinMode(PA0, INPUT_PULLDOWN);   // /WR[1]
  pinMode(PA2, INPUT_PULLDOWN);   // /A[0]
  pinMode(PD0, INPUT_PULLDOWN);   // D[0]
  pinMode(PD1, INPUT_PULLDOWN);   // D[1]
  pinMode(PD2, INPUT_PULLDOWN);   // D[2]
  pinMode(PD3, INPUT_PULLDOWN);   // D[3]
  pinMode(PD4, INPUT_PULLDOWN);   // D[4]
  pinMode(PD5, INPUT_PULLDOWN);   // D[5]
  pinMode(PD6, INPUT_PULLDOWN);   // D[6]
  pinMode(PD7, INPUT_PULLDOWN);   // D[7]
  pinMode(PD8, INPUT_PULLDOWN);   // /RD[1] not used
  pinMode(PD9, INPUT_PULLDOWN);   // /WR[1]
  pinMode(PD10, INPUT_PULLDOWN);  // /A[0]
  pinMode(PD11, INPUT_PULLDOWN);  // CS1X not used
  pinMode(PD12, INPUT_PULLDOWN);  // not used
  pinMode(PD13, INPUT_PULLDOWN);  // not used
  pinMode(PD14, INPUT_PULLDOWN);  // not used
  pinMode(PD15, INPUT_PULLDOWN);  // not used

  attachInterrupt(digitalPinToInterrupt(PA0), NULL, FALLING);
  attachInterrupt(digitalPinToInterrupt(PA2), NULL, FALLING);

#ifdef SERIAL_DEBUG
  Serial.begin(921600);
  Serial.println("Waiting ...");
  Serial.print("CPU SPEED: ");
  Serial.println(F_CPU);
#endif
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
}

unsigned short cmd[8];
unsigned char cmd_idx = 0;
unsigned short data_idx = 0;

volatile int state = STATE_INIT;
unsigned int posx = 0;
unsigned int offx = 0;
unsigned int offy = 0;

// big / small pixels
inline void mFillRect(int16_t x0, int16_t y0, uint16_t w, uint16_t h, uint16_t color) {
  tft.fillRect(x0, y0, w, h, color);
  //tft.drawPixel(x0,y0,color);
}

void parseNext(unsigned short c) {

  if ((state == STATE_INIT) && ((c & 0x1000) == 0)) {
    state = STATE_CMD;
    for (int i = 0; i < 8; i++) cmd[i] = 0;
    cmd[0] = c;
    cmd_idx = 0;
    posx = 0;
    data_idx = 0;
    return;
  }
  if ((state == STATE_CMD) && ((c & 0x1000) == 0)) {
    cmd[++cmd_idx] = c;
    return;
  }
  if ((state == STATE_CMD) && ((c & 0x1000) != 0)) {
    state = STATE_DATA;
    cmd[cmd_idx + 1] = c;
    data_idx = 0;
    return;
  }
  if (state == STATE_DATA) {
    
    
    // start with display symbols
    if ((cmd[0] == 0x0BB4 || cmd[0] == 0x0BB2) && cmd_idx == 3) {
      // labels
      for (int i = 0; i < sizeof(LcdLabels); i++) {
        if (((cmd[0] & 0x000F) == LcdLabels[i].bank_id) && ((cmd[1] & 0x000F) == LcdLabels[i].pos_id) && ((cmd[2] & 0x000F) == LcdLabels[i].bit_id)) {
          tft.setCursor(LcdLabels[i].posx, LcdLabels[i].posy);
          if ((cmd[4] & 0x0001) != 0) {
            tft.setTextColor(LcdLabels[i].fg_color, LcdLabels[i].bg_color);
          } else {
            tft.setTextColor(0x0000, 0x0001);
          }
          tft.print(LcdLabels[i].text);
        }
      }
      // 7digits
      for (int i = 0; i < sizeof(Lcd7Digits); i++) {
        if (((cmd[0] & 0x000F) == Lcd7Digits[i].bank_id) && ((cmd[1] & 0x000F) == Lcd7Digits[i].pos_id) && ((cmd[2] & 0x000F) == Lcd7Digits[i].bit_id)) {
          if ((cmd[4] & 0x0001) != 0) {
            mFillRect(Lcd7Digits[i].posx, Lcd7Digits[i].posy, Lcd7Digits[i].w, Lcd7Digits[i].h, TFT_WHITE);
          } else {
            mFillRect(Lcd7Digits[i].posx, Lcd7Digits[i].posy, Lcd7Digits[i].w, Lcd7Digits[i].h, TFT_BLACK);
          }
        }
      }
    }
    
#ifdef DEBUG_DIGITS    
    if ((cmd[0] == 0x0BB4 || cmd[0] == 0x0BB2) && cmd_idx == 3) {
        if (cmd[0] == 0x0BB2) {
          tmp_bb2[((cmd[1] & 0x000F)*16 + (cmd[2] & 0x000F))] = cmd[4];
          Serial.print("BB2 ");
          for (int i = 16*4; i < 16*8; i++) {
            if (i % 16 == 0) Serial.print(" | ");
            Serial.print(tmp_bb2[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }
        if (cmd[0] == 0x0BB4) {
          tmp_bb4[((cmd[1] & 0x000F)*16 + (cmd[2] & 0x000F))] = cmd[4];
           Serial.print("BB4 ");
           for (int i = 16*4; i < 16*8; i++) {
            if (i % 16 == 0) Serial.print(" | ");
            Serial.print(tmp_bb4[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }  
    }
#endif

    if (c == 0x0BEE && cmd_idx > 1) {
      state = STATE_INIT;
      return;
    }

    // signal graph pixel data (without BE0 - BEE)
    if (cmd_idx == 2) {
      if (cmd[0] == 0x0BB3) {
        if (posx == 0) posx = ((cmd[1] & 0x000F) * HSTEP + (cmd[2] & 0x000F));
        for (unsigned short y = 0; y < 16; y++) {
          if ((cmd[3] & (0x0080 >> y / 2)) != 0)
            mFillRect(posx * 4, 192 + y * 4, 3, 3, TFT_WHITE);
          else
            mFillRect(posx * 4, 192 + y * 4, 3, 3, TFT_BLACK);
        }
      } 
      if (cmd[0] == 0x0BB0) {
        if (posx == 0) posx = ((cmd[0] & 0x000F) * HSTEP + (cmd[1] & 0x000F));
        for (unsigned short y = 0; y < 16; y++) {
          if ((cmd[3] & (0x0080 >> y / 2)) != 0)
            mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_WHITE);
          else
            mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_BLACK);
        }
      }
      state = STATE_INIT;
      return;
    }

    // process matrix part
    // BBx   B1x  B0x  BE0  1Bxx .... BEE
    // part  pos  pos START pixels    STOP

    /*  never seen ? */
    
    if (cmd[0] == 0x0BB4 && cmd[3] == 0x0BE0 && cmd_idx == 3) {
      if (posx == 0) posx = ((cmd[1] & 0x0000F) * HSTEP + (cmd[2] & 0x000F)) + 1; 
        /*
        Serial.println("?");
        Serial.print(cmd_idx,HEX);
        Serial.print(" ");
        Serial.print(cmd[0],HEX);
        Serial.print(" ");
        Serial.print(cmd[1],HEX);
        Serial.print(" ");
        Serial.print(cmd[2],HEX);
        Serial.print(" ");
        Serial.print(cmd[3],HEX);
        Serial.print(" ");
        Serial.print(c,HEX);
        */
      posx++;
      data_idx++;
      return;
    }
    
    if (c < 0x1000) { return;}
    if (cmd[0] == 0x0BB3 && cmd[3] == 0x0BE0 && cmd_idx == 3) {
      if (posx == 0) posx = ((cmd[1] & 0x0000F) * HSTEP + (cmd[2] & 0x000F)) + 1;
      if (data_idx == 0) {
        for (unsigned short y = 0; y < 16; y++) {
          if ((cmd[4] & (0x0080u >> y / 2)) != 0)
            mFillRect(posx * 4, 200 + y * 4, 3, 3, TFT_WHITE);
          else
            mFillRect(posx * 4, 200 + y * 4, 3, 3, TFT_BLACK);
        }
        posx++;
      }

      for (unsigned short y = 0; y < 16; y++) {
        if ((c & (0x0080 >> y / 2)) != 0)
          mFillRect(posx * 4, 200 + y * 4, 3, 3, TFT_WHITE);
        else
          mFillRect(posx * 4, 200 + y * 4, 3, 3, TFT_BLACK);
      }
      posx++;
      data_idx++;
      return;
    }

    if (cmd[0] == 0x0BB2 && cmd[3] == 0x0BE0 && cmd_idx == 3) {
      if (posx == 0) posx = ((cmd[1] & 0x000F) * HSTEP + (cmd[2] & 0x000F)) + 1;
      if (data_idx == 0) {
        for (unsigned short y = 0; y < 8; y++) {
          if ((cmd[4] & (0x0080u >> y)) != 0)
            mFillRect(posx * 4, 263 + y * 4, 3, 3, TFT_WHITE);
          else
            mFillRect(posx * 4, 263 + y * 4, 3, 3, TFT_BLACK);
        }
        posx++;
      }

      for (unsigned short y = 0; y < 8; y++) {
        if ((c & (0x0080 >> y)) != 0)
          mFillRect(posx * 4, 263 + y * 4, 3, 3, TFT_WHITE);
        else
          mFillRect(posx * 4, 263 + y * 4, 3, 3, TFT_BLACK);
      }
      posx++;
      data_idx++;
      return;
    }

    if (cmd[0] == 0x0BB1 && cmd[3] == 0x0BE0 && cmd_idx == 3) {
      if (posx == 0) posx = ((cmd[1] & 0x0000F) * HSTEP + (cmd[2] & 0x000F)) + 1;
      if (data_idx == 0) {
        for (unsigned short y = 0; y < 8; y++) {
          if ((cmd[4] & (0x0080u >> y)) != 0)
            mFillRect(posx * 4, 60 + y * 9, 3, 8, TFT_WHITE);
          else
            mFillRect(posx * 4, 60 + y * 9, 3, 8, TFT_BLACK);
        }
        posx++;
      }

      for (unsigned short y = 0; y < 8; y++) {
        if ((c & (0x0080 >> y)) != 0)
          mFillRect(posx * 4, 60 + y * 9, 3, 8, TFT_WHITE);
        else
          mFillRect(posx * 4, 60 + y * 9, 3, 8, TFT_BLACK);
      }
      posx++;
      data_idx++;
      return;
    }

    if (cmd[0] == 0x0BB0 && cmd[3] == 0x0BE0 && cmd_idx == 3) {
      if (posx == 0) posx = ((cmd[1] & 0x0000F) * HSTEP + (cmd[2] & 0x000F)) + 1;
      if (data_idx == 0) {
        for (unsigned short y = 0; y < 16; y++) {
          if ((cmd[4] & (0x0080u >> y / 2)) != 0)
            mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_WHITE);
          else
            mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_BLACK);
        }
        posx++;
      }

      for (unsigned short y = 0; y < 16; y++) {
        if ((c & (0x0080 >> y / 2)) != 0)
          mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_WHITE);
        else
          mFillRect(posx * 4, 136 + y * 4, 3, 3, TFT_BLACK);
      }
      posx++;
      data_idx++;
      return;
    }

#ifdef SERIAL_DEBUG    
      Serial.println("!");
      Serial.print(cmd_idx,HEX);
      Serial.print(" ");
      Serial.print(cmd[0],HEX);
      Serial.print(" ");
      Serial.print(cmd[1],HEX);
      Serial.print(" ");
      Serial.print(cmd[2],HEX);
      Serial.print(" ");
      Serial.print(cmd[3],HEX);
      Serial.print(" ");
      Serial.print(c,HEX);
#endif
  }
}


void loop() {
  // process loop of received data in ring buffer
  
  unsigned short c;
  while (true) {
    c = removeFromRing();
    //Serial.println(c, HEX);
    if (c != 0xFFFF) parseNext(c);
  }
}
