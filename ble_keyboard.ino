#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif

#include <BLEDevice.h>
#include "BLEHIDDevice.h"
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
  checkPinsAndSend(sendKey);

#ifdef ESP32
  if (Serial.available()) 
    serialEvent();
#endif

  delay(50);
}


IRAM_ATTR void clickHome(){
  sendString = 1;
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
      case 'S': {
        // Send keystrokes, reads a string of hex pairs (modifier, 
        // code) and sends them via HID
        readSerialKeysAndSend(sendKey);
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
