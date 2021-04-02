#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif
#include "M5Util.h"

#ifdef ARDUINO_M5Stick_C
float getStickBatteryLevel(float voltage);
#endif

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

/* From https://github.com/eggfly/M5StickCProjects/blob/master/StickWatch2/battery.h */
static const float levels[] = {4.13, 4.06, 3.98, 3.92, 3.87, 3.82, 3.79, 3.77, 3.74, 3.68, 3.45, 3.00};

float getStickBatteryLevel(float voltage) {
  float level = 1;
  if (voltage >= levels[0]) {
    level = 1;
  } else if (voltage >= levels[1]) {
    level = 0.9;
    level += 0.1 * (voltage - levels[1]) / (levels[0] - levels[1]);
  } else if (voltage >= levels[2]) {
    level = 0.8;
    level += 0.1 * (voltage - levels[2]) / (levels[1] - levels[2]);
  } else if (voltage >= levels[3]) {
    level = 0.7;
    level += 0.1 * (voltage - levels[3]) / (levels[2] - levels[3]);
  } else if (voltage >= levels[4]) {
    level = 0.6;
    level += 0.1 * (voltage - levels[4]) / (levels[3] - levels[4]);
  } else if (voltage >= levels[5]) {
    level = 0.5;
    level += 0.1 * (voltage - levels[5]) / (levels[4] - levels[5]);
  } else if (voltage >= levels[6]) {
    level = 0.4;
    level += 0.1 * (voltage - levels[6]) / (levels[5] - levels[6]);
  } else if (voltage >= levels[7]) {
    level = 0.3;
    level += 0.1 * (voltage - levels[7]) / (levels[6] - levels[7]);
  } else if (voltage >= levels[8]) {
    level = 0.2;
    level += 0.1 * (voltage - levels[8]) / (levels[7] - levels[8]);
  } else if (voltage >= levels[9]) {
    level = 0.1;
    level += 0.1 * (voltage - levels[9]) / (levels[8] - levels[9]);
  } else if (voltage >= levels[10]) {
    level = 0.05;
    level += 0.05 * (voltage - levels[10]) / (levels[9] - levels[10]);
  } else if (voltage >= levels[11]) {
    level = 0.00;
    level += 0.05 * (voltage - levels[11]) / (levels[10] - levels[11]);
  } else {
    level = 0.00;
  }
  return level;
}
