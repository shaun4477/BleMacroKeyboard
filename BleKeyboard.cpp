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

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "BleKeyboard.h"

const char *deviceName = DEFAULT_KEYBOARD_NAME;
const char *manufacturerName = KEYBOARD_MANUFACTURER;
esp_ble_auth_req_t mainKeyboardAuthMode = ESP_LE_AUTH_REQ_SC_MITM_BOND;

static BLEHIDDevice* hid;
static BLECharacteristic* input;
static BLECharacteristic* output;

static BLEServer *pKeyServer = NULL;
static int connectedCount = 0;
static void (*mainOnInitialized)() = NULL;
static void (*mainOnConnect)() = NULL;
static void (*mainOnDisconnect)() = NULL;
static void (*mainOnPassKeyNotify)(uint32_t pass_key) = NULL;
static bool mainAllowMultiConnect = false;
static BLEAddress peerAddress("00:00:00:00:00:00");
static std::map<uint16_t, conn_info_t> connectedClientsMap;
const char *LOG_TAG = "blekeyboard"; 

BleKeyboardHandler BleKeyboard;

class MySecurity : public BLESecurityCallbacks {
  bool onConfirmPIN(uint32_t pin){
    return false;
  }
  
  uint32_t onPassKeyRequest(){
    ESP_LOGI(LOG_TAG, "On PassKeyRequest");
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key){
    ESP_LOGI(LOG_TAG, "On passkey Notify number:%d", pass_key);
    if (mainOnPassKeyNotify)
      mainOnPassKeyNotify(pass_key);
  }

  bool onSecurityRequest(){
    ESP_LOGI(LOG_TAG, "On Security Request");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl){
    ESP_LOGI(LOG_TAG, "Authentication complete, starting BLE work!");
    if(cmpl.success){
      uint16_t length;
      esp_ble_gap_get_whitelist_size(&length);
      ESP_LOGD(LOG_TAG, "Whitelist size now: %d", length);
    }
  }
};

class MyCallbacks : public BLEServerCallbacks {
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
    ESP_LOGI(LOG_TAG, "Special keys in output report: %d", *value);
  }
};

static void handle_gatts_event(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
  /* NOTE: This assumes that there is only one GATT server running in the ESP32, there is no 
   * way to get the gatts_if from the BLEServer class to check. 
   * 
   * We could also process ESP_GATTS_REG_EVT to get the interface, but this is overkill */

  BLE2902* desc = NULL;

  Serial.printf("BLE event %d\n", event);

  switch (event) {
    case ESP_GATTS_CONNECT_EVT:
      connectedCount++;
      
      conn_info_t conn_info;
      memcpy(conn_info.peer, param->connect.remote_bda, sizeof(conn_info.peer));
      connectedClientsMap.insert(std::pair<uint16_t, conn_info_t>(param->connect.conn_id, conn_info));

      Serial.printf("BLE keyboard connected, connection id %d\n", param->connect.conn_id);

      // Connected count is incremented AFTER this callback is invoked
      Serial.printf("BLE keyboard connected\n");

      desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
      
      Serial.printf("Connection from %02x:%02x:%02x:%02x:%02x:%02x with connection id %d, count now %d\n",
                   param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                   param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5],
                   param->connect.conn_id,
                   pKeyServer->getConnectedCount());

      peerAddress = BLEAddress(param->connect.remote_bda);
      
      if (mainOnConnect)
        mainOnConnect();    

      if (mainAllowMultiConnect)
        BLEDevice::startAdvertising();      

      break;
    case ESP_GATTS_DISCONNECT_EVT:
      if (mainOnDisconnect)
        mainOnDisconnect();    

      Serial.printf("Disconnect from %02x:%02x:%02x:%02x:%02x:%02x with connection id %d, count now %d\n",
                   param->disconnect.remote_bda[0], param->disconnect.remote_bda[1], param->disconnect.remote_bda[2],
                   param->disconnect.remote_bda[3], param->disconnect.remote_bda[4], param->disconnect.remote_bda[5],
                   param->disconnect.conn_id,
                   pKeyServer->getConnectedCount());
      
      connectedClientsMap.erase(param->disconnect.conn_id);
      connectedCount--;
      if (connectedCount <= 0) {
        desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        desc->setNotifications(false);

        BLEDevice::startAdvertising();      
      }
      
      
      break;
    default:
      break;
  }
}

void taskServer(void*){
  Serial.println("Initialize BLE");

  Serial.println("Init device");
  BLEDevice::init(deviceName);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new MySecurity());

  Serial.println("Create BLE server");
  pKeyServer = BLEDevice::createServer();

  // We need a callbacks object even if it's empty because
  // otherwise we'll get a null pointer dereference for 
  // m_pServerCallbacks->onMtuChanged
  pKeyServer->setCallbacks(new MyCallbacks());

  // Unfortunately the GATTS handler in BLEServer doesn't
  // give us the connection id on disconnect, so we register
  // our own handler too 
  BLEDevice::setCustomGattsHandler(handle_gatts_event);

  Serial.printf("Created BLE server at %p\n", (void *) pKeyServer);

  hid = new BLEHIDDevice(pKeyServer);
  input = hid->inputReport(1); // <-- input REPORTID from report map
  output = hid->outputReport(1); // <-- output REPORTID from report map

  output->setCallbacks(new MyOutputCallbacks());

  std::string name = manufacturerName;
  hid->manufacturer()->setValue(name);

  // Plug and Play IDs, Vendor ID source (0x02 = USB Implementers forum), 
  // Vendor ID 0xe502 little endian = 0x2e5 = Unknown, 
  // Product ID 0xa111 little endian = 0x11a1, 
  // Product Version 0x0210 little endian = 0x1002
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);

  // Country = 0x00, Flags = 0x02
  hid->hidInfo(0x00,0x02);

  BLESecurity *pSecurity = new BLESecurity();
  // pSecurity->setKeySize();
  // pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  // pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  // Require authentication, prevent MITM and bond the devices after pairing 
  // pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND); 
  // During negotiation set this device up to show the passcode 
  pSecurity->setAuthenticationMode(mainKeyboardAuthMode); 
  if (mainKeyboardAuthMode == ESP_LE_AUTH_REQ_SC_MITM_BOND || 
      mainKeyboardAuthMode == ESP_LE_AUTH_REQ_SC_ONLY) 
    pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

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

  BLEAdvertising *pAdvertising = pKeyServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
  hid->setBatteryLevel(7);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  
  if (mainOnInitialized) 
    mainOnInitialized();  

  // Wait forever
  delay(portMAX_DELAY);  
}

BleKeyboardHandler::BleKeyboardHandler() {
  connectedCount = 0;
}

BLEAddress BleKeyboardHandler::getPeerAddress() {
  return peerAddress;
}

bool BleKeyboardHandler::keyboardConnected() {
  return connectedCount > 0;
}

void BleKeyboardHandler::startKeyboard(void (*onInitialized_p)(),
                                       void (*onConnect_p)(),
                                       void (*onPassKeyNotify_p)(uint32_t),
                                       bool allowMultiConnect,
                                       void (*onDisconnect_p)(),
                                       const char *keyboardName,
                                       esp_ble_auth_req_t authMode) {
  mainOnInitialized = onInitialized_p;
  mainOnConnect = onConnect_p;
  mainOnPassKeyNotify = onPassKeyNotify_p;
  mainAllowMultiConnect = allowMultiConnect;
  mainOnDisconnect = onDisconnect_p;
  mainKeyboardAuthMode = authMode;
  if (keyboardName)
    deviceName = strdup(keyboardName);
  Serial.printf("Starting keyboard task, on init callback %p on connect callback %p\n", mainOnInitialized, mainOnConnect);
  delay(10);
  xTaskCreate(taskServer, "server", 20000, NULL, 5, NULL);
}

std::map<uint16_t, conn_info_t> BleKeyboardHandler::getConnectedClients() {
  return connectedClientsMap;
}

/* Static method */
void BleKeyboardHandler::directSendMsg(uint8_t *msg, int len) {
  if (connectedCount > 0) {
    input->setValue(msg, len);
    input->notify();  
    vTaskDelay(3);
  }  
}

/* Static method */
void BleKeyboardHandler::directSendKey(uint8_t modifier, uint8_t key, uint8_t key2) {
  // The HID report descriptor defined above has one byte of modifier, 
  // a reserved byte then up to 6 key codes
  uint8_t msg[] = {modifier, 0x0, key, key2, 0x0, 0x0, 0x0, 0x0};
  directSendMsg(msg, sizeof(msg));

  // Send the matching key up
  uint8_t blank[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  directSendMsg(blank, sizeof(blank));    
}

void BleKeyboardHandler::sendMsg(uint8_t *msg, int len) {
  BleKeyboardHandler::directSendMsg(msg, len);
}

void BleKeyboardHandler::sendKey(uint8_t modifier, uint8_t key, uint8_t key2) {
  BleKeyboardHandler::directSendKey(modifier, key, key2);
}

int BleKeyboardHandler::getConnectedCount() {
  return connectedCount;
}

void BleKeyboardHandler::sendString(const char *str) {
  while (*str) {
    KEYMAP map = keymap[(uint8_t)*str];
    // Serial.printf("Send %d %d for '%c' %d\n", map.modifier, map.usage, *str, (uint8_t) *str);
    sendKey(map.modifier, map.usage, 0x0);
    str++;
  }
}
