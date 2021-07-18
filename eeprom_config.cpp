#include <Arduino.h>
#include <EEPROM.h>

#include "SerialUtil.h":
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

  Serial.printf("Global EEPROM at %p with size %d\n", &EEPROM, EEPROM.length());
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

    Serial.printf("Setting pin %d to pull up\n", pin);
    pinMode(pin, INPUT_PULLUP);    
    pinsToWatch |= (WATCH_TYPE) 1 << (pin - FIRST_INPUT_PIN);
    pinsLast    |= (WATCH_TYPE) 1 << (pin - FIRST_INPUT_PIN);
  }  

  Serial.printf("Pins to watch %llx\n", pinsToWatch);
}

uint8_t checkPinChange(uint8_t pin, uint8_t *newValue) {
  uint8_t pinRead = digitalRead(pin);
  WATCH_TYPE pinNew = (WATCH_TYPE) pinRead << (pin - FIRST_INPUT_PIN);
  WATCH_TYPE pinOld = pinsLast & ((WATCH_TYPE) 1 << (pin - FIRST_INPUT_PIN));

  pinsLast = (pinsLast & (~((WATCH_TYPE) 1 << (pin - FIRST_INPUT_PIN)))) | pinNew;
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

int readModifierAndCode(uint8_t *modifier_p, uint8_t *code_p, char *terminator_p) {
  int rc;
  
  rc = serialTimedReadNum(modifier_p, terminator_p, true);
  if (rc) {
    Serial.println("Timeout reading modifier");
    return -1;
  } else if (*terminator_p != ' ') {
    Serial.println("No space after modifier");
    return -1;      
  }

  rc = serialTimedReadNum(code_p, terminator_p, true);
  if (rc) {
    Serial.println("Timeout reading keycode");
    return -1;
  } else if (!(*terminator_p == ' ' || *terminator_p == ';')) {
    Serial.println("No terminator after keycode");
    return -1;      
  }  

  return 0;
}

int readPinConfigUpdateFromSerial() {
  uint8_t pin;
  int rc;
  char terminator;
  
  if ((rc = serialTimedReadNum(&pin, &terminator, false)) || terminator != ' ') {
    Serial.println("Invalid pin"); 
    return -1; 
  }

  if (pin < 0 || pin < FIRST_INPUT_PIN || pin > LAST_INPUT_PIN) {
    Serial.print("Invalid input pin ");
    Serial.print(pin);
    Serial.println("");
    return -1;
  }

  Serial.print("Updating pin ");
  Serial.println(pin);

  uint8_t keystrokeIdx = 0;
  while (keystrokeIdx < MAX_KEYSTROKES - 1) {
    uint8_t modifier, keycode;

    rc = serialTimedSkipWhitespace(&terminator);
    if (rc) {
      Serial.println("Timeout reading key");
      return -1;
    } else if (terminator == ';') {
      Serial.read();
      break;      
    }

    rc = readModifierAndCode(&modifier, &keycode, &terminator);

    Serial.print("Read modifier ");
    serialPrintHex(modifier);
    Serial.print(" ");

    Serial.print("keycode ");
    serialPrintHex(keycode);
    Serial.println();

    updateKey(pin, keystrokeIdx, modifier, keycode);
    keystrokeIdx++;   
  }  

  if (keystrokeIdx < MAX_KEYSTROKES - 1)
    updateKey(pin, keystrokeIdx, 0, 0);  

  Serial.println("Updated keycode, reloaded config");
  readAndProcessConfig();
  return 0;
}

int readSerialKeysAndCallback(void (*sendKey)(uint8_t modifier, uint8_t key, uint8_t key2)) {
  uint8_t pin;
  int rc;
  char terminator;

  while (true) {
    uint8_t modifier, keycode;

    rc = serialTimedSkipWhitespace(&terminator);
    if (rc) {
      Serial.println("Timeout reading key");
      return -1;
    } else if (terminator == ';') {
      Serial.read();
      break;      
    }

    rc = readModifierAndCode(&modifier, &keycode, &terminator);

    Serial.print("Read modifier ");
    serialPrintHex(modifier);
    Serial.print(" ");

    Serial.print("keycode ");
    serialPrintHex(keycode);
    Serial.println();

    sendKey(modifier, keycode, 0x0);
  }  

  return 0;
}

void checkPinsAndCallback(void (*sendKey)(uint8_t modifier, uint8_t key, uint8_t key2)) {
  for (uint8_t pin = FIRST_INPUT_PIN; pin <= LAST_INPUT_PIN; pin++) {
    if (!WATCH_PIN(pin))
      continue;

    uint8_t pinChanged, pinValue;
    pinChanged = checkPinChange(pin, &pinValue);

    if (!pinChanged)
      continue;

    Serial.print("Pin ");
    Serial.print(pin);
    Serial.print(" change: ");
    Serial.println(pinValue);

    if (!pinValue) {
      for (uint8_t keystrokeIdx = 0; keystrokeIdx < MAX_KEYSTROKES; keystrokeIdx++) {
        uint8_t modifier = GET_KEY_MODIFIER(pin, keystrokeIdx);
        uint8_t code     = GET_KEY_CODE(pin, keystrokeIdx);

        if (!code)
          break;

        Serial.print("Send key ");
        serialPrintHex(modifier);
        Serial.print(" ");
        serialPrintHex(code);
        Serial.println("");

        sendKey(modifier, code, 0x0);
      }
    }
  }  
}
