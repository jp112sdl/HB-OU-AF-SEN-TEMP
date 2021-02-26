//- -----------------------------------------------------------------------------------------------------------------------
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2021-02-26 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef LCDDISPLAY_H_
#define LCDDISPLAY_H_


#include "TM12864_LCD.h"

namespace as {

class LCDDisplay : public Alarm {
  class BacklightAlarm : public Alarm {
    LCDDisplay& lcd;
    public:
      BacklightAlarm (LCDDisplay& l) : Alarm (0), lcd(l) {}
      virtual ~BacklightAlarm () {}

      void trigger (__attribute__ ((unused)) AlarmClock& clock)  {
        lcd.disableBacklight();
      }
    } lcdBacklightTimer;

private:
  U8G2_ST7565_64128N_F_4W_HW_SPI display;
  TM12864<LCD_RED, LCD_GREEN, LCD_BLUE> tm12864;
  uint8_t cursorPos;
  uint8_t backlightOnTime;
  int16_t temperature;
  uint8_t stepperpos;
  uint8_t numdigits(uint32_t i) {
    return (i < 10) ? 1: (uint8_t)(log10((uint32_t)i)) + 1;
  }

public:
  enum LED_COLORS {WHITE=0, RED, GREEN, BLUE };

  LCDDisplay () : lcdBacklightTimer(*this), display(U8G2_R0, LCD_CS, LCD_DC, LCD_RST), tm12864(display), cursorPos(0), backlightOnTime(4), temperature(0), stepperpos(0) {}
  virtual ~LCDDisplay () {}

  void setBackLightOnTime(uint8_t t) {
    if (backlightOnTime == 0)
      disableBacklight();

    backlightOnTime = t;

    if (backlightOnTime == 255)
      enableBacklight();
  }

  void disableBacklight() {
    tm12864.setALL(LED_OFF);
  }

  void enableBacklight(uint8_t color=WHITE) {
    if (backlightOnTime > 0) {
      if (color == WHITE)    tm12864.setALL(LED_ON);
      if (color == RED  )  { tm12864.setALL(LED_OFF); tm12864.setRED  (true); }
      if (color == GREEN)  { tm12864.setALL(LED_OFF); tm12864.setGREEN(true); }
      if (color == BLUE )  { tm12864.setALL(LED_OFF); tm12864.setBLUE (true); }
      if (backlightOnTime < 255) {
        sysclock.cancel(lcdBacklightTimer);
        lcdBacklightTimer.set(seconds2ticks(backlightOnTime));
        sysclock.add(lcdBacklightTimer);
      }
    } else {
      disableBacklight();
    }
  }

  void trigger (__attribute__ ((unused)) AlarmClock& clock)  {
    update();
    set(millis2ticks(LCD_UPDATE_CYCLE));
    clock.add(*this);
  }

  void startCyclicUpdate() {
    sysclock.cancel(*this);
    set(millis2ticks(LCD_UPDATE_CYCLE));
    sysclock.add(*this);
  }

  void stopCyclicUpdate() {
    sysclock.cancel(*this);
  }

  void clearBuffer() {
    display.clearBuffer();
  }

  void setCursor(uint8_t x, uint8_t y) {
    display.setCursor(x, y);
  }

  void setFont(const uint8_t * font) {
    display.setFont(font);
  }

  uint16_t centerPosition(const char * text) {
    return tm12864.centerPosition(text);
  }

  void print(const char str[]){
    display.print(str);
    display.sendBuffer();
  }

  void setTemperature(int16_t t) {
    temperature = t;
  }

  void setPosition(uint8_t p) {
    stepperpos = p;
  }

  void startupScreen(bool hasMaster, bool scValid) {
     tm12864.initLCD();
     //display.setDisplayRotation(U8G2_R0);
     enableBacklight(scValid ? GREEN:RED);
     display.setFontMode(false);

     const char * title        PROGMEM = "HB-OU-AF-SEN-TEMP";
     const char * asksinpp     PROGMEM = "AskSin++ V" ASKSIN_PLUS_PLUS_VERSION_STR;
     const char * compiledMsg  PROGMEM = "compiled on";
     const char * compiledDate PROGMEM = __DATE__ " " __TIME__;
     const char * noMaster     PROGMEM = "-keine Zentrale-";

     display.setFont(u8g2_font_6x13_tr);
     display.setCursor(centerPosition(title), 10);
     display.print(title);

     display.setFont(u8g2_font_6x10_tr);
     display.setCursor(centerPosition(asksinpp), 30);
     display.print(asksinpp);

     display.setFont(u8g2_font_5x8_tr);
     if (hasMaster == false) {
       display.setCursor(centerPosition(noMaster), 42);
       display.print(noMaster);
     }

     display.setCursor(centerPosition(compiledMsg), 54);
     display.print(compiledMsg);
     display.setCursor(centerPosition(compiledDate), 64);
     display.print(compiledDate);
     display.sendBuffer();
   }

   void update() {
     display.clearBuffer();
     const char * title PROGMEM = "Kamin-O-Mat";
     display.setFont(u8g2_font_6x13_tr);
     display.setCursor(centerPosition(title), 10);
     display.print(title);
     display.setCursor(32, 16);
     display.print("-----------");

     display.setFont(u8g2_font_6x12_tr);
     display.setCursor(0,26);
     display.print("Temperatur C:");

     display.setFont(u8g2_font_ncenB08_tf);
     display.drawUTF8(62, 26, "°");
     display.setFont(u8g2_font_6x12_tr);

     display.setCursor(85,26);
     display.print(temperature / 10);

     display.setCursor(0,40);
     display.print("Position   %:");
     display.setCursor(85,40);
     display.print(stepperpos);

     display.drawRFrame(5, 48, 118, 12, 5);

     if (stepperpos > 0)
       display.drawRBox(5, 48, max(((118*stepperpos)/100),11), 12, 5);

     display.sendBuffer();
   }

 };

}


#endif /* LCDDISPLAY_H_ */
