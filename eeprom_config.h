#ifndef eeprom_config_h
#define eeprom_config_h

#include <EEPROM.h>

#define MAX_KEYSTROKES   64     // Set to the maximum number of keystrokes available to be sent for each input

#define EEPROM_CHECK_BYTE_1 0x61
#define EEPROM_CHECK_BYTE_2 0x42
#define EEPROM_HEADER_SIZE 2

#if !defined(E2END) && defined(ESP32)
/* The ESP32 compatibility library doesn't have a strict size, similate 3k bytes */
#define E2END ((2 * 1024) - 1)
#endif

#define EEPROM_SIZE      (E2END + 1)

#ifdef ESP32
#define WATCH_TYPE unsigned int
#else
#define WATCH_TYPE uint8_t
#endif

extern WATCH_TYPE pinsToWatch; 
extern WATCH_TYPE pinsLast;

// Set to first input pin that can be used to trigger a set of keystrokes 
// when taken to ground
#ifdef ESP32
#define FIRST_INPUT_PIN  25      
#else 
#define FIRST_INPUT_PIN  4
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// The maximum number of pins that can be watched is based 
// Up to 8 pins can be watched because they're stored in an 8 bit set of flags
#define MAX_INPUT_PINS   MIN((((EEPROM_SIZE - EEPROM_HEADER_SIZE) / (MAX_KEYSTROKES * 2))|0), sizeof(pinsToWatch) * 8)

#define LAST_INPUT_PIN   (FIRST_INPUT_PIN + MAX_INPUT_PINS - 1)

#ifndef ESP32
#define DEFAULT_INPUT_PIN 7
#define DEFAULT_INPUT_STRING "Hello World!"
#endif

#define EEPROM_OFFSET(pin) (2 + ((pin - FIRST_INPUT_PIN) * (MAX_KEYSTROKES * 2)))
#define WATCH_PIN(pin) (pinsToWatch & (1 << (pin - FIRST_INPUT_PIN)))
#define GET_KEY_MODIFIER(pin, keyNo) EEPROM.read(EEPROM_OFFSET(pin) + (keyNo * 2));
#define GET_KEY_CODE(pin, keyNo)     EEPROM.read(EEPROM_OFFSET(pin) + (keyNo * 2) + 1);

void formatEeprom();
void readAndProcessConfig();
uint8_t checkPinChange(uint8_t pin, uint8_t *newValue);
void updateKey(uint8_t pin, uint8_t stroke, uint8_t modifier, uint8_t code);

#endif 
