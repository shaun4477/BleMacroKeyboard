/* From https://gist.githubusercontent.com/sabas1080/93115fb66e09c9b40e5857a19f3e7787/raw/b59f18a2acc4c1d0b0b826642b189d02d9123988/ESP32_HID.ino */

/*
  Copyright (c) 2014-2020 Electronic Cats SAPI de CV.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>

const char *helloStr = "AaBbCcDdEeFfGg - Hello from BLE Keyboard";
const char *deviceName = "Meeting Keyboard";
const char *manufacturerName = "SMC";

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

volatile uint8_t sendString = 0;
bool connected = false;

class MyCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param){
    connected = true;
    
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.printf("BLE Keyboard connected");

    Serial.printf("Connection ID %d Peer %02x:%02x:%02x:%02x:%02x:%02x\n", 
                  param->connect.conn_id, 
                  param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                  param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
                  
    Serial.printf("BLE keyboard connected, count %d\n", pServer->getConnectedCount());
    for (auto const &it : pServer->getPeerDevices(false)) {
      Serial.printf("Server Connection Id %d Data 0x%08x\n", it.first, it.second);
    }
    for (auto const &it : pServer->getPeerDevices(true)) {
      Serial.printf("Client Connection Id %d Data 0x%08x\n", it.first, it.second);
    }
    
    desc->setNotifications(true);
  }

  void onDisconnect(BLEServer* pServer){
    connected = false;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.printf("BLE Keyboard DISCONNECTED");
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
  }
};

/*
 * This callback is connect with output report. In keyboard output report report special keys changes, like CAPSLOCK, NUMLOCK
 * We can add digital pins with LED to show status
 * bit 0 - NUM LOCK
 * bit 1 - CAPS LOCK
 * bit 2 - SCROLL LOCK
 */
 class MyOutputCallbacks : public BLECharacteristicCallbacks {
 void onWrite(BLECharacteristic* me){
    uint8_t* value = (uint8_t*)(me->getValue().c_str());
    ESP_LOGI(LOG_TAG, "special keys: %d", *value);
  }
};

void taskServer(void*){
  BLEDevice::init(deviceName);

  esp_bd_addr_t *pLocalAddr = BLEDevice::getAddress().getNative();
  
  Serial.printf("BLE initialized, address %02x:%02x:%02x:%02x:%02x:%02x\n", 
                (*pLocalAddr)[0], (*pLocalAddr)[1], (*pLocalAddr)[2], 
                (*pLocalAddr)[3], (*pLocalAddr)[4], (*pLocalAddr)[5]);
                
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallbacks());
  Serial.printf("Created BLE server at 0x%08x\n", pServer);

  hid = new BLEHIDDevice(pServer);
  input = hid->inputReport(1); // <-- input REPORTID from report map
  output = hid->outputReport(1); // <-- output REPORTID from report map

  output->setCallbacks(new MyOutputCallbacks());

  std::string name = manufacturerName;
  hid->manufacturer()->setValue(name);

  // Plug and Play IDs, Vendor ID source (0x02 = USB Implementers forum), 
  // Vendor ID 0xe502 = Unknown, Product ID 0xa111, Product Version = 0x0210
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00,0x02);

  BLESecurity *pSecurity = new BLESecurity();
//  pSecurity->setKeySize();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  const uint8_t report[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,        //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
    USAGE_MINIMUM(1),   0xE0,
    USAGE_MAXIMUM(1),   0xE7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1),     0x01,       //   1 byte (Modifier)
    REPORT_COUNT(1),    0x08,
    HIDINPUT(1),           0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x01,       //   1 byte (Reserved)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),           0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),           0x00,       //   Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    HIDOUTPUT(1),          0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),          0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    END_COLLECTION(0)
  };

  hid->reportMap((uint8_t*)report, sizeof(report));
  hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
  hid->setBatteryLevel(7);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  delay(portMAX_DELAY);  
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Do not reinitialize Serial in M5.begin otherwise ESP32 
  // debug logging will stop working
  M5.begin(true, true, false);

  M5.Lcd.setRotation(3);
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("Initializing BLE Keyboard...");
  
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

  xTaskCreate(taskServer, "server", 20000, NULL, 5, NULL);
}

void loop() {
  
  if (connected && sendString){
    Serial.println("Sending string");
    sendString = 0;
    const char* hello = helloStr;

    while (*hello){
      KEYMAP map = keymap[(uint8_t)*hello];
      Serial.printf("Sending %c with %02x %02x\n", *hello, map.modifier, map.usage);

      // Send HID report for key down
      uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
      input->setValue(msg, sizeof(msg));
      input->notify();
      hello++;

      // Send HID report showing key back up
      uint8_t msg1[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

      input->setValue(msg1, sizeof(msg1));
      input->notify();
      delay(10);
    }
  }
  delay(50);
}

IRAM_ATTR void clickHome(){
  sendString = 1;
}
