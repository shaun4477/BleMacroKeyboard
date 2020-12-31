#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif

#include "HIDKeyboardTypes.h"
#include "M5Util.h"
#include "BleMacroKeyboard.h"
#include "GvmLightControl.h"

static volatile uint8_t sendString = 0;
const char *helloStr = "AaBbCcDd";

void onKeyboardConnect() {
  uint8_t *peerAddress = BleMacroKeyboard.getPeerAddress();
  setScreenText("BLE Keyboard connected\nPeer: %02x:%02x:%02x:%02x:%02x:%02x",
                peerAddress[0], peerAddress[1], peerAddress[2],
                peerAddress[3], peerAddress[4], peerAddress[5]);    
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
  BleMacroKeyboard.loadConfig();

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
  BleMacroKeyboard.startKeyboard(onKeyboardInitialized, onKeyboardConnect);

  GVM.debugOn();
  int networks_found;
  GVM.find_and_join_light_wifi(&networks_found);
  
  Serial.printf("Setup complete\n");
}

void loop() {

  if (sendString) {
    // Ignore any sendString presses until the keyboard is connected, 
    // this is important since BT power up will cause a spurious interrupt 
    // on PIN 39 (and possibly others)
    if (!BleMacroKeyboard.keyboardConnected())
      sendString = 0;
    else {
      GVM.setOnOff(GVM.getOnOff() + 1);
      
      Serial.printf("Sending string, sendString %d\n", sendString);
      sendString = 0;
      const char* hello = helloStr;
  
      while (*hello){
        KEYMAP map = keymap[(uint8_t)*hello];
        Serial.printf("Sending %c with %02x %02x\n", *hello, map.modifier, map.usage);
  
        // Send HID report for key down
        uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
        BleMacroKeyboard.sendKey(map.modifier, map.usage, 0x00);
        hello++;
        delay(10);
      }    
    }
  }

  /* Check if any pins should trigger keys to be sent */
  BleMacroKeyboard.checkPins();

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
        BleMacroKeyboard.sendKey(0x00, 0x04, 0x00);
        Serial.println(""); 
        break;
      }
      case 'S': {
        // Send keystrokes, reads a string of hex pairs (modifier, 
        // code) and sends them via HID
        BleMacroKeyboard.readSerialKeysAndSend();
        break;
      }      
      case 'F':
        // Format EEPROM then re-read config
        BleMacroKeyboard.resetConfig();
      case 'l':
        // Read and list config
        BleMacroKeyboard.loadConfig();
        break;
      case 'u':
        // Update a pin to trigger some keystrokes
        BleMacroKeyboard.readSerialPinConfigUpdate();
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
