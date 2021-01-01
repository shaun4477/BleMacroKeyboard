#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif
#include "M5Util.h"

int setScreenText(const char *format, ...) {
  char loc_buf[64];
  char * temp = loc_buf;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
  va_end(copy);
  if(len < 0) {
      va_end(arg);
      return 0;
  };
  if(len >= sizeof(loc_buf)){
      temp = (char*) malloc(len+1);
      if(temp == NULL) {
          va_end(arg);
          return 0;
      }
      len = vsnprintf(temp, len+1, format, arg);
  }
  va_end(arg);
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  len = M5.Lcd.print((const char *) temp);

  Serial.print("Screen: ");
  Serial.println(temp);
  
  if(temp != loc_buf){
      free(temp);
  }
  return len;  
}


void screen_off() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setBrightness(0);
#else
  M5.Axp.SetLDO2(false);  
#endif
}

void screen_on() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setBrightness(255);
#else  
  // Turn on tft backlight voltage output
  M5.Axp.SetLDO2(true);  
#endif
}

int down_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnA.wasPressed();
#else
  // 0x01 long press(1s), 0x02 short press
  return M5.Axp.GetBtnPress() == 0x02;
#endif  
}

int up_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnC.wasPressed();
#else
  // 0x01 long press(1s), 0x02 short press
  return M5.BtnB.wasPressed();
#endif    
}

int home_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnB.wasPressed();
#else
  return M5.BtnA.wasPressed();
#endif
}

int battery_power() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return !M5.Power.isCharging();
#else 
  return !M5.Axp.GetIusbinData();
#endif  
}

void power_off() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Power.powerOFF();
#else 
  M5.Axp.PowerOff();
#endif    
}

float getBatteryLevel() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return (float) M5.Power.getBatteryLevel() / 100.0f;
#else  
  return getStickBatteryLevel(M5.Axp.GetBatVoltage());
#endif
}
