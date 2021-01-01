#ifndef M5Util_h
#define M5Util_h

int setScreenText(const char *format, ...);
void screen_off();
void screen_on();
int down_pressed();
int up_pressed();
int home_pressed();
int battery_power();
void power_off();
float getBatteryLevel();

#endif
