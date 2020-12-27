#include <Arduino.h>
#include <EEPROM.h>

#include "eeprom_config.h"

WATCH_TYPE pinsToWatch = 0; 
WATCH_TYPE pinsLast = 0;

void initEeprom() {
  // Initialize the EEPROM library, only needed for the ESP32 compatibility library 
#ifdef ESP32
  if (!EEPROM.length())
    EEPROM.begin(E2END + 1);
#endif
}

inline void updateEeprom(uint16_t address, uint8_t value) {
#ifdef ESP32
  if (EEPROM.read(address) != value) 
    EEPROM.write(address, value);
#else
  EEPROM.update(address, value);
#endif
}

void formatEeprom() {
  Serial.println(F("Formatting EEPROM"));

  Serial.printf("Global EEPROM at 0x%08x with size %d\n", EEPROM, EEPROM.length());
  Serial.printf("Current check bytes 0x%02x 0x%02x\n", EEPROM.read(0), EEPROM.read(1));
  updateEeprom(0, EEPROM_CHECK_BYTE_1);
  updateEeprom(1, EEPROM_CHECK_BYTE_2);
  Serial.printf("New check bytes 0x%02x 0x%02x\n", EEPROM.read(0), EEPROM.read(1));
  
  for (uint8_t pin = FIRST_INPUT_PIN; pin <= LAST_INPUT_PIN; pin++) {
    uint16_t eepromOffset = EEPROM_OFFSET(pin);

#ifdef DEFAULT_INPUT_PIN
    if (pin == DEFAULT_INPUT_PIN) {
      for (char *defaultString = DEFAULT_INPUT_STRING; *defaultString; defaultString++) {
        uint8_t code, modifier;
        code = asciiToKey(*defaultString, &modifier);
        updateEeprom(eepromOffset++, modifier);
        updateEeprom(eepromOffset++, code);
      }      
    } 
#endif
    
    updateEeprom(eepromOffset, 0);
    updateEeprom(eepromOffset + 1, 0);
  }

#ifdef ESP32
  EEPROM.commit();
#endif
}

void readAndProcessConfig() {
  initEeprom();
  
  if (EEPROM.read(0) != EEPROM_CHECK_BYTE_1 || EEPROM.read(1) != EEPROM_CHECK_BYTE_2)
    formatEeprom();

  Serial.printf("Bits in watch set %d\nMaximum input pins %d\n", sizeof(pinsToWatch) * 8, MAX_INPUT_PINS);

  pinsToWatch = 0;
    
  for (uint8_t pin = FIRST_INPUT_PIN; pin <= LAST_INPUT_PIN; pin++) {
    uint16_t eepromOffset = EEPROM_OFFSET(pin);

    Serial.print("Pin ");
    Serial.print(pin);
    Serial.print(": ");
    
    if (!EEPROM.read(eepromOffset + 1)) {
      Serial.println("off");
      continue;
    }

    for (uint8_t keystrokeIdx = 0; keystrokeIdx < MAX_KEYSTROKES; keystrokeIdx++) {
      uint8_t modifier = EEPROM.read(eepromOffset + (keystrokeIdx * 2));
      uint8_t code     = EEPROM.read(eepromOffset + (keystrokeIdx * 2) + 1);
      
      if (!code)
        break;
        
      if (keystrokeIdx > 0)
        Serial.print(" ");
        
      if (modifier < 16) 
        Serial.print("0");
      Serial.print(modifier, HEX);
      
      Serial.print(" ");
      
      if (code < 16) 
        Serial.print("0");
      Serial.print(code, HEX);
    }
    Serial.println("");

    pinMode(pin, INPUT_PULLUP);    
    pinsToWatch |= 1 << (pin - FIRST_INPUT_PIN);
    pinsLast |= 1 << (pin - FIRST_INPUT_PIN);
  }  
}

uint8_t checkPinChange(uint8_t pin, uint8_t *newValue) {
  uint8_t pinRead = digitalRead(pin);
  WATCH_TYPE pinNew = pinRead << (pin - FIRST_INPUT_PIN);
  WATCH_TYPE pinOld = pinsLast & (1 << (pin - FIRST_INPUT_PIN));

  pinsLast = (pinsLast & (~(1 << (pin - FIRST_INPUT_PIN)))) | pinNew;
  *newValue = pinRead; 

  return pinNew != pinOld;
} 

void updateKey(uint8_t pin, uint8_t stroke, uint8_t modifier, uint8_t code) {
  updateEeprom(EEPROM_OFFSET(pin) + (stroke * 2), modifier);
  updateEeprom(EEPROM_OFFSET(pin) + (stroke * 2) + 1, code);
#ifdef ESP32
  EEPROM.commit();
#endif
}
