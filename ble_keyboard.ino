#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif

#include <BLEDevice.h>
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "M5Util.h"
#include "BLEKeyboard.h"
#include "eeprom_config.h"
#include "SerialUtil.h"

static volatile uint8_t sendString = 0;
const char *helloStr = "AaBbCcDd";

void onKeyboardConnect(esp_ble_gatts_cb_param_t *param) {
  setScreenText("BLE Keyboard connected\nPeer: %02x:%02x:%02x:%02x:%02x:%02x",
                param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);    
}

void onKeyboardInitialized() {
  esp_bd_addr_t *pLocalAddr = BLEDevice::getAddress().getNative();

  setScreenText("BLE initialized\nLocal %02x:%02x:%02x:%02x:%02x:%02x\nWaiting",
                (*pLocalAddr)[0], (*pLocalAddr)[1], (*pLocalAddr)[2],
                (*pLocalAddr)[3], (*pLocalAddr)[4], (*pLocalAddr)[5]);
}

int battery_power() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return !M5.Power.isCharging();
#else
  return !M5.Axp.GetIusbinData();
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Do not reinitialize Serial in M5.begin otherwise ESP32 
  // debug logging will stop working
  M5.begin(true, true, false);

#ifdef ARDUINO_M5Stick_C
  M5.Lcd.setRotation(3);
#endif

#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setTextSize(2); // 15px
#endif

#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Power.begin();
  Serial.printf("Currently charging %d\n", battery_power());
#endif

  setScreenText("Initializing BLE Keyboard...");
  startKeyboard(onKeyboardInitialized, onKeyboardConnect);
  
#if 0
  pinMode(12, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(12), clickNumLock, CHANGE);   // Num Lock
  pinMode(14, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(13), clickCapsLock, CHANGE);   // Caps Lock
  pinMode(32, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(32), clickScrollLock, CHANGE);   // Scroll Lock
#endif

  Serial.println(F("Arduino keyboard sender (https://github.com/shaun4477/arduino-uno-r3-usb-keyboard)"));
  readAndProcessConfig();

  // The home button on the M5Stick sends text
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_A_PIN), clickHome, FALLING);
}

void loop() {
  
  if (keyboardConnected() && sendString){
    Serial.println("Sending string");
    sendString = 0;
    const char* hello = helloStr;

    while (*hello){
      KEYMAP map = keymap[(uint8_t)*hello];
      Serial.printf("Sending %c with %02x %02x\n", *hello, map.modifier, map.usage);

      // Send HID report for key down
      uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
      sendKey(map.modifier, map.usage, 0x00);
      hello++;
      delay(10);
    }
  }

  /* Check if any pins should trigger keys to be sent */
  checkPins();

#ifdef ESP32
  if (Serial.available()) 
    serialEvent();
#endif

  delay(50);
}

void checkPins() {
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

IRAM_ATTR void clickHome(){
  sendString = 1;
}

int readPinConfigUpdateFromSerial() {
  uint8_t pin;
  int rc;
  char terminator;
  
  if (rc = serialTimedReadNum(&pin, &terminator, false) || terminator != ' ') {
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
      Serial.println("Timeout looking for modifier");
      return -1;
    } else if (terminator == ';') {
      Serial.read();
      break;      
    }

    rc = serialTimedReadNum(&modifier, &terminator, true);
    if (rc) {
      Serial.println("Timeout reading modifier");
      return -1;
    } else if (terminator != ' ') {
      Serial.println("No space after modifier");
      return -1;      
    }

    rc = serialTimedReadNum(&keycode, &terminator, true);
    if (rc) {
      Serial.println("Timeout reading keycode");
      return -1;
    } else if (!(terminator == ' ' || terminator == ';')) {
      Serial.println("No terminator after keycode");
      return -1;      
    }

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

void serialEvent() {
  if (Serial.available()) {         
    char inChar = Serial.read();         

    Serial.print("Received: ");  
    Serial.println(inChar);         

    switch (inChar) {
      case 's': {
        // Send a keystroke, the actual key data should never 
        // appear in the console since it will become a HID key
        Serial.print("key:");       // print the string "key:"
        sendKey(0x00, 0x04, 0x00);
        Serial.println(""); 
        break;
      }
      case 'F':
        // Format EEPROM then re-read config
        formatEeprom();
      case 'l':
        // Read and list config
        readAndProcessConfig();
        break;
      case 'u':
        // Update a pin to trigger some keystrokes
        readPinConfigUpdateFromSerial();
        break;        
      case '\n':
      case '\r':
      case ' ':
        // Ignore whitespace
        break;
      default:
        Serial.print(F("Unknown command '"));
        Serial.print(inChar);
        Serial.println("'");
    }
  }
}
