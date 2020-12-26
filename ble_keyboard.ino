#include <M5StickC.h>
#include <BLEDevice.h>
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "M5Util.h"
#include "BLEKeyboard.h"

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

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Do not reinitialize Serial in M5.begin otherwise ESP32 
  // debug logging will stop working
  M5.begin(true, true, false);

  M5.Lcd.setRotation(3);

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
      sendKey(msg, sizeof(msg));
      hello++;

      // Send HID report showing key back up
      uint8_t msg1[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
      sendKey(msg1, sizeof(msg1));
      delay(10);
    }
  }
  delay(50);
}

IRAM_ATTR void clickHome(){
  sendString = 1;
}
