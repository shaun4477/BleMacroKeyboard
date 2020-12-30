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

#if 0
  pinMode(12, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(12), clickNumLock, CHANGE);   // Num Lock
  pinMode(14, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(13), clickCapsLock, CHANGE);   // Caps Lock
  pinMode(32, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(32), clickScrollLock, CHANGE);   // Scroll Lock
#endif

  Serial.println(F("Arduino keyboard sender (https://github.com/shaun4477/arduino-uno-r3-usb-keyboard)"));
  MacroKeyboard.loadConfig();

  // The home button on the M5Stick or the left button on the M5Stack sends text
  Serial.printf("Setting pin to pullup\n");
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  
  Serial.printf("Attaching interrupt\n");
  attachInterrupt(digitalPinToInterrupt(BUTTON_A_PIN), clickHome, FALLING);
  Serial.printf("Interrupt attached\n");

  Serial.printf("Monitoring pin %d send string %d\n", BUTTON_A_PIN, sendString);

  // Starting bluetooth will cause a spurious interrupt on PIN 39, 
  // be sure to ignore it
  setScreenText("Initializing BLE Keyboard...");
  MacroKeyboard.startKeyboard(onKeyboardInitialized, onKeyboardConnect);
  
  Serial.printf("Setup complete\n");
}

void loop() {

  if (sendString) {
    // Ignore any sendString presses until the keyboard is connected, 
    // this is important since BT power up will cause a spurious interrupt 
    // on PIN 39 (and possibly others)
    if (!MacroKeyboard.keyboardConnected())
      sendString = 0;
    else {
      Serial.printf("Sending string, sendString %d\n", sendString);
      sendString = 0;
      const char* hello = helloStr;
  
      while (*hello){
        KEYMAP map = keymap[(uint8_t)*hello];
        Serial.printf("Sending %c with %02x %02x\n", *hello, map.modifier, map.usage);
  
        // Send HID report for key down
        uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
        MacroKeyboard.sendKey(map.modifier, map.usage, 0x00);
        hello++;
        delay(10);
      }    
    }
  }

  /* Check if any pins should trigger keys to be sent */
  MacroKeyboard.checkPins();

#ifdef ESP32
  if (Serial.available()) 
    serialEvent();
#endif

  delay(50);
}

IRAM_ATTR void clickHome(){
  // Serial.println("CLK");
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
        MacroKeyboard.sendKey(0x00, 0x04, 0x00);
        Serial.println(""); 
        break;
      }
      case 'S': {
        // Send keystrokes, reads a string of hex pairs (modifier, 
        // code) and sends them via HID
        MacroKeyboard.readSerialKeysAndSend();
        break;
      }      
      case 'F':
        // Format EEPROM then re-read config
        MacroKeyboard.resetConfig();
      case 'l':
        // Read and list config
        MacroKeyboard.loadConfig();
        break;
      case 'u':
        // Update a pin to trigger some keystrokes
        MacroKeyboard.readSerialPinConfigUpdate();
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