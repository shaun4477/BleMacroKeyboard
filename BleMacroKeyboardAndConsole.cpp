#ifdef ARDUINO_M5Stack_Core_ESP32
#include <M5Stack.h>
#elif defined(ARDUINO_M5Stick_C)
#include <M5StickC.h>
#else
#error "This code works on m5stick-c or m5stack core"
#endif

#include <WiFi.h>
#include <StreamString.h>
#include "HIDKeyboardTypes.h"
#include "M5Util.h"
#include "BleMacroKeyboard.h"
#include "GvmLightControl.h"

int lcd_off = 0;
unsigned long last_button_millis = 0;

#define INACTIVE_SCREEN_OFF_MILLIS   10000 // Milliseconds since last button press to power down LCD backlight
#define INACTIVE_POWER_OFF_MILLIS    20000 // Milliseconds since last button press to switch off
// #define INACTIVE_OFF_WHEN_PLUGGED_IN 1     // Whether to do inactive off when plugged in
#define INACTIVE_SCREEN_OFF_WHEN_PLUGGED_IN 1
#define INACTIVE_OFF_WHEN_PLUGGED_IN 0

#define MODE_SUMMARY        -1
#define MODE_SET_ON_OFF      0
#define MODE_SET_CHANNEL     1
#define MODE_SET_HUE         2
#define MODE_SET_BRIGHTNESS  3
#define MODE_SET_CCT         4
#define MODE_SET_SATURATION  5
#define MODE_KEYBOARD_TEST   6

const int8_t mode_set[] = { MODE_SUMMARY, 
                            MODE_KEYBOARD_TEST,
                            MODE_SET_ON_OFF, 
                            // MODE_SET_CHANNEL, MODE_SET_HUE, 
                            MODE_SET_BRIGHTNESS, MODE_SET_CCT,
                            // MODE_SET_SATURATION 
                            };

static void onWiFiConnectAttempt(uint8_t *bssid, int attempt) {
  setScreenText("Trying\nBSSID %02x:%02x:%02x:%02x:%02x:%02x\nAttempt %d\n",
                bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                attempt);
}

void update_screen_status();
void serialEvent();

static void onStatusUpdated() {
  update_screen_status();
}

void onKeyboardConnect() {
  setScreenText("BLE Keyboard connected\nPeer: %s", BleMacroKeyboard.getPeerAddress().toString().c_str());
}

void onKeyboardInitialized() {
  esp_bd_addr_t *pLocalAddr = BLEDevice::getAddress().getNative();

  setScreenText("BLE initialized\nLocal %02x:%02x:%02x:%02x:%02x:%02x\nWaiting",
                (*pLocalAddr)[0], (*pLocalAddr)[1], (*pLocalAddr)[2],
                (*pLocalAddr)[3], (*pLocalAddr)[4], (*pLocalAddr)[5]);
}

void set_screen_text(String newText, int textFont = 2) {
  static String lastText; 
  if (newText == lastText)
    return;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.setTextFont(textFont);
  M5.Lcd.print(newText);
  lastText = newText;      
}

#define PAIR_MAX_DEVICES 20

char *bda2str(const uint8_t* bda, char *str, size_t size)
{
  if (bda == NULL || str == NULL || size < 18)
    return NULL;
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

void dump_bluetooth_info() {
  esp_ble_bond_dev_t pairedDeviceBtAddr[PAIR_MAX_DEVICES];
  char bda_str[18];

  // Get the numbers of bonded/paired devices in the BT module
  int count = esp_ble_get_bond_device_num();
  if(!count) {
    Serial.println("No bonded device found.");
  } else {
    Serial.print("Bonded device count: "); Serial.println(count);
    if(PAIR_MAX_DEVICES < count) {
      count = PAIR_MAX_DEVICES;
      Serial.print("Reset bonded device count: "); Serial.println(count);
    }
    esp_err_t tError =  esp_ble_get_bond_device_list(&count, pairedDeviceBtAddr);
    if(ESP_OK == tError) {
      for(int i = 0; i < count; i++) {
        Serial.print("Bonded device # "); Serial.print(i); Serial.print(" -> ");
        Serial.println(bda2str(pairedDeviceBtAddr[i].bd_addr, bda_str, 18));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE + GVM Light console...\n");
  Serial.printf("Log level set to %d\n", ARDUHAL_LOG_LEVEL);

  // Do not reinitialize Serial in M5.begin otherwise ESP32 
  // debug logging will stop working
  M5.begin(true, true, false);

#ifdef ARDUINO_M5Stick_C
  M5.Lcd.setRotation(3);
#endif

  screen_on();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);

  setScreenText("Starting BLE + GVM");

#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setTextSize(2); // 15px
#endif

#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Power.begin();
  Serial.printf("Currently charging %d\n", battery_power());
#endif

  BleMacroKeyboard.loadConfig();

  // Starting bluetooth will cause a spurious interrupt on PIN 39, 
  // be sure to ignore it
  setScreenText("Initializing BLE Keyboard...");
  BleMacroKeyboard.startKeyboard(onKeyboardInitialized, onKeyboardConnect, NULL, false, NULL, 
                                 "Meeting Keyboard", ESP_LE_AUTH_BOND);

  GVM.debugOn();

  GVM.callbackOnWiFiConnectAttempt(onWiFiConnectAttempt);
  GVM.callbackOnStatusUpdated(onStatusUpdated);

  int networks_found = 0;
  int join_failed = 0;
  int join_attempted = 0;
  while ((join_failed = GVM.find_and_join_light_wifi(&networks_found))) {
    if (networks_found) {
      Serial.println("Couldn't connect to any light, trying again");
      set_screen_text("Couldn't connect to any light, trying again");
      delay(1000);
    } else {
      Serial.println("No lights found, trying again in 5 seconds");
      set_screen_text("No lights found, trying again in 5 seconds");
      delay(5000);
    }
    join_attempted++;
    if (join_attempted > 2)
      break;    
  }

  // Write info in serial logs.
  if (!join_failed) {
    Serial.print("Connected to the WiFi network. IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Base station is: %s\n", WiFi.BSSIDstr().c_str());
    Serial.printf("Receive strength is: %d\n", WiFi.RSSI());
  }

  // Write info to LCD.
  update_screen_status();

  last_button_millis = millis();

  dump_bluetooth_info();
  Serial.printf("Setup complete\n");
}

int screen_mode = 0;

void update_screen_status() {
  StreamString o;
  LightStatus light_status = GVM.getLightStatus();

  switch (mode_set[screen_mode]) {
    case MODE_SUMMARY:
      o.printf("BLE: ");
      if (BleMacroKeyboard.keyboardConnected()) {
        o.printf("%s", BleMacroKeyboard.getPeerAddress().toString().c_str());
      }
      else 
        o.printf("Waiting");
      o.printf("\n");
        
      o.printf("Light: %s\n", WiFi.BSSIDstr().c_str());
      if (light_status.on_off != -1) 
        o.printf("On %d ", light_status.on_off);
      if (light_status.hue != -1) 
        o.printf("Hue %d ", light_status.hue * 5);
      if (light_status.brightness != -1) 
        o.printf("Bright. %d", light_status.brightness);
      o.println();
      if (light_status.cct != -1) 
        o.printf("CCT %d ", light_status.cct * 100);
      if (light_status.saturation != -1) 
        o.printf("Sat. %d", light_status.saturation);
      o.println(); 
      o.printf("Battery %0.1f %%\n", getBatteryLevel() * 100);
      break;
    case MODE_SET_ON_OFF:   
      o.printf("Light On\n");
      if (light_status.on_off != -1) 
        o.printf("%d", light_status.on_off);
      break;
    case MODE_SET_CHANNEL:
      o.printf("Channel\n");
      if (light_status.channel != -1) 
        o.printf("%d", light_status.channel - 1);
      break;    
    case MODE_SET_BRIGHTNESS:
      o.printf("Brightness\n");
      if (light_status.brightness != -1) 
        o.printf("%d%%", light_status.brightness);
      break;    
    case MODE_SET_CCT:
      o.printf("CCT\n");    
      if (light_status.cct != -1) 
        o.printf("%d", light_status.cct * 100);
      break;    
    case MODE_SET_HUE:
      o.printf("Hue\n");    
      if (light_status.hue != -1) 
        o.printf("%d", light_status.hue * 5);
      break;    
    case MODE_SET_SATURATION: 
      o.printf("Saturation\n");    
      if (light_status.saturation != -1) 
        o.printf("%d%%", light_status.saturation);
      break;    
    case MODE_KEYBOARD_TEST: {
      o.printf("BLE Keyboard\n");
      esp_bd_addr_t *pLocalAddr = BLEDevice::getAddress().getNative();
    
      o.printf("Local:  %02x:%02x:%02x:%02x:%02x:%02x\n",
               (*pLocalAddr)[0], (*pLocalAddr)[1], (*pLocalAddr)[2],
               (*pLocalAddr)[3], (*pLocalAddr)[4], (*pLocalAddr)[5]);

      o.printf("Connected #: %d\n", BleMacroKeyboard.getConnectedCount());
      o.printf("Remote: ");
      if (BleMacroKeyboard.keyboardConnected()) {
        o.printf("%s", BleMacroKeyboard.getPeerAddress().toString().c_str());
      }
      else 
        o.printf("Waiting");
      o.printf("\n");      
    }
  }

  set_screen_text(o, mode_set[screen_mode] == MODE_SUMMARY || mode_set[screen_mode] == MODE_KEYBOARD_TEST ? 2 : 4); 
}

void test_screen_idle_off() {
  int onBattery = battery_power();
  if (millis() - last_button_millis > INACTIVE_SCREEN_OFF_MILLIS && 
      !lcd_off && 
      (INACTIVE_SCREEN_OFF_WHEN_PLUGGED_IN || onBattery)) {
    screen_off();
    lcd_off = 1;
    Serial.printf("** Screen off **\nBattery %% is %f %d\n", getBatteryLevel(), onBattery);
  }  
  if (millis() - last_button_millis > INACTIVE_POWER_OFF_MILLIS && 
      (INACTIVE_OFF_WHEN_PLUGGED_IN || onBattery)) {
    Serial.printf("** Powering off ** %d %d\n", INACTIVE_OFF_WHEN_PLUGGED_IN, onBattery);
    power_off();
  }
}

int button_screen_on() {
  if (lcd_off) {
    screen_on();
    lcd_off = 0;
    return 1;
  }
  return 0;
}

void loop() {
  /* Check if any pins should trigger keys to be sent */
  BleMacroKeyboard.checkPins();

  GVM.process_messages();

  int button_pressed = 0xff;

  test_screen_idle_off();

  // Read buttons before processing state
  M5.update();
  
  if (home_pressed()) {
    button_pressed = 0;
    Serial.println("Home button pressed");
    Serial.printf("Battery %% is %f\n", getBatteryLevel());
  }

  if (down_pressed()) {
    button_pressed = -1;
    Serial.println("Power button pressed");
  }

  if (up_pressed()) {
    button_pressed = 1;
    Serial.println("Side button pressed");
    if (mode_set[screen_mode] == MODE_SUMMARY) 
      GVM.send_hello_msg();
  }

  if (button_pressed != 0xff) {
    last_button_millis = millis();
    if (!button_screen_on()) {
      int change = button_pressed;
      if (button_pressed == 0) {
        screen_mode = screen_mode + 1 >= (sizeof(mode_set) / sizeof(mode_set[0])) ? 0 : screen_mode + 1;
        Serial.printf("New mode %d == %d\n", screen_mode, mode_set[screen_mode]);
        update_screen_status();          
      } else if (change && mode_set[screen_mode] != MODE_SUMMARY) {
        switch (mode_set[screen_mode]) {
          case MODE_SET_ON_OFF:  
            GVM.setOnOff(GVM.getOnOff() + change); 
            break;
          case MODE_SET_CHANNEL:   
            GVM.setChannel(GVM.getChannel() + change); 
            break;
          case MODE_SET_BRIGHTNESS:
            GVM.setBrightness(GVM.getBrightness() + change); 
            break;    
          case MODE_SET_CCT:
            GVM.setCct(GVM.getCct() + change); 
            break;    
          case MODE_SET_HUE:
            GVM.setHue(GVM.getHue() + change);
            break;    
          case MODE_SET_SATURATION: 
            GVM.setSaturation(GVM.getSaturation() + change);
            break;          
          case MODE_KEYBOARD_TEST:             
            if (change == -1) {
              // Send 'Hello'
              BleMacroKeyboard.sendString("Hello");
            } else if (change == 1) {
              BleMacroKeyboard.sendString("World");
            }
            break;          
        }
        update_screen_status();
      }
    }
  }

#ifdef ESP32
  if (Serial.available()) 
    serialEvent();
#endif

  GVM.wait_msg_or_timeout();
}

IRAM_ATTR void clickHome(){
  Serial.println("CLK");
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
      case 'o': {
        int rc = GVM.send_set_cmd(LIGHT_VAR_ON_OFF, 1);
        Serial.print("Send on ");
        Serial.println(rc);
        break;
      }
      case 'O': {
        int rc = GVM.send_set_cmd(LIGHT_VAR_ON_OFF, 0);
        Serial.print("Send off ");
        Serial.println(rc);
        break;
      }
      case 'b': {
        int rc = GVM.send_set_cmd(LIGHT_VAR_BRIGHTNESS, 11);
        Serial.print("Send bright 11 ");
        Serial.println(rc);
        break;   
      }
      case 'r': {
        String toSend = Serial.readStringUntil(';');
        int rc = GVM.broadcast_udp(toSend.c_str(), toSend.length());
        Serial.printf("Send %d '%s' %d\n", toSend.length(), toSend.c_str(), rc);
        break;   
      }
      case 'R': {
        String toSend = Serial.readStringUntil(';');
        Serial.printf("Send with CRC length %d\n", toSend.length());
        unsigned short crc = calcCrcFromHexStr(toSend.c_str(), toSend.length());
        char crc_str[5];
        shortToHex(crc, crc_str);
        crc_str[4] = '\0';
        toSend += crc_str;
        int rc = GVM.broadcast_udp(toSend.c_str(), toSend.length());
        Serial.printf("Send %d '%s' %d\n", toSend.length(), toSend.c_str(), rc);
        break;   
      }      
      case 'c': {
        String toCalc = Serial.readStringUntil(';');
        Serial.printf("Calc %d %s %d\n", toCalc.length(), toCalc.c_str(), calcCrcFromHexStr(toCalc.c_str(), toCalc.length()));
        break;   
      }      
      case 'X': {
        Serial.println("Restarting");
        ESP.restart();
      }        
      case 'p': {
        Serial.printf("Battery on is %d\n", battery_power());
        break;
      }
      default:
        Serial.print(F("Unknown command '"));
        Serial.print(inChar);
        Serial.println("'");
    }
  }
}
