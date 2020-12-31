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
#define INACTIVE_OFF_WHEN_PLUGGED_IN 0

static void onWiFiConnectAttempt(uint8_t *bssid, int attempt) {
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("Trying\nBSSID %02x:%02x:%02x:%02x:%02x:%02x\nAttempt %d\n",
                bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                attempt);
}

static void onStatusUpdated() {
  update_screen_status();
}

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

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting BLE + GVM Light console...\n");
  Serial.printf("Log level set to %d\n", ARDUHAL_LOG_LEVEL);

  // For some reason I keep getting Serial driver errors unless I
  // add this delay
  delay(100);

  // Do not reinitialize Serial in M5.begin otherwise ESP32 
  // debug logging will stop working
  M5.begin(true, true, false);

#ifdef ARDUINO_M5Stick_C
  M5.Lcd.setRotation(3);
#endif

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);

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
  BleMacroKeyboard.startKeyboard(onKeyboardInitialized, onKeyboardConnect);

  GVM.debugOn();

  GVM.callbackOnWiFiConnectAttempt(onWiFiConnectAttempt);
  GVM.callbackOnStatusUpdated(onStatusUpdated);

  int networks_found = 0;
  while (GVM.find_and_join_light_wifi(&networks_found)) {
    if (networks_found) {
      Serial.println("Couldn't connect to any light, trying again");
      delay(1000);
    } else {
      Serial.println("No lights found, trying again in 20 seconds");
      delay(20000);
    }
  }

  // Write info in serial logs.
  Serial.print("Connected to the WiFi network. IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Base station is: %s\n", WiFi.BSSIDstr().c_str());
  Serial.printf("Receive strength is: %d\n", WiFi.RSSI());

  // Write info to LCD.
  update_screen_status();

  last_button_millis = millis();

  Serial.printf("Setup complete\n");
}

int screen_mode = -1;

float getBatteryLevel() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return (float) M5.Power.getBatteryLevel() / 100.0f;
#else  
  return getStickBatteryLevel(M5.Axp.GetBatVoltage());
#endif
}

void update_screen_status() {
  StreamString o;
  static String lastUpdate;
  LightStatus light_status = GVM.getLightStatus();

  switch (screen_mode) {
    case -1:
      o.printf("BLE: ");
      if (BleMacroKeyboard.keyboardConnected()) {
        uint8_t *peerAddress = BleMacroKeyboard.getPeerAddress();
        o.printf("%02x:%02x:%02x:%02x:%02x:%02x",
                 peerAddress[0], peerAddress[1], peerAddress[2],
                 peerAddress[3], peerAddress[4], peerAddress[5]);    
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
    case LIGHT_VAR_ON_OFF:   
      if (light_status.on_off != -1) 
        o.printf("Light On\n%d", light_status.on_off);
      break;
    case LIGHT_VAR_CHANNEL:
      if (light_status.channel != -1) 
        o.printf("Channel\n%d", light_status.channel - 1);
      break;    
    case LIGHT_VAR_BRIGHTNESS:
      if (light_status.brightness != -1) 
        o.printf("Brightness\n%d%%", light_status.brightness);
      break;    
    case LIGHT_VAR_CCT:
      if (light_status.cct != -1) 
        o.printf("CCT\n%d", light_status.cct * 100);
      break;    
    case LIGHT_VAR_HUE:
      if (light_status.hue != -1) 
        o.printf("Hue\n%d", light_status.hue * 5);
      break;    
    case LIGHT_VAR_SATURATION: 
      if (light_status.saturation != -1) 
        o.printf("Saturation\n%d%%", light_status.saturation);
      break;    
  }

  if (lastUpdate != o) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.setTextFont(screen_mode == -1 ? 2 : 4);
    M5.Lcd.print(o);    
  }
  
  lastUpdate = o;
}

void screen_off() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setBrightness(0);
#else
  M5.Axp.SetLDO2(false);  
#endif
}

void screen_on() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Lcd.setBrightness(255);
#else  
  // Turn on tft backlight voltage output
  M5.Axp.SetLDO2(true);  
#endif
}

int down_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnA.wasPressed();
#else
  // 0x01 long press(1s), 0x02 short press
  return M5.Axp.GetBtnPress() == 0x02;
#endif  
}

int up_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnC.wasPressed();
#else
  // 0x01 long press(1s), 0x02 short press
  return M5.BtnB.wasPressed();
#endif    
}

int home_pressed() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return M5.BtnB.wasPressed();
#else
  return M5.BtnA.wasPressed();
#endif
}

int battery_power() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  return !M5.Power.isCharging();
#else 
  return !M5.Axp.GetIusbinData();
#endif  
}

void power_off() {
#ifdef ARDUINO_M5Stack_Core_ESP32
  M5.Power.powerOFF();
#else 
  M5.Axp.PowerOff();
#endif    
}

void test_screen_idle_off() {
  if (millis() - last_button_millis > INACTIVE_SCREEN_OFF_MILLIS && 
      !lcd_off && 
      (INACTIVE_OFF_WHEN_PLUGGED_IN || battery_power())) {
    screen_off();
    lcd_off = 1;
    Serial.printf("** Screen off **\nBattery %% is %f\n", getBatteryLevel());
  }  
  if (millis() - last_button_millis > INACTIVE_POWER_OFF_MILLIS && 
      (INACTIVE_OFF_WHEN_PLUGGED_IN || battery_power())) {
    Serial.printf("** Powering off **\n");
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
    if (screen_mode == -1) 
      GVM.send_hello_msg();
  }

  if (button_pressed != 0xff) {
    last_button_millis = millis();
    if (!button_screen_on()) {
      int change = button_pressed;
      if (button_pressed == 0) {
        screen_mode++;
        if (screen_mode > 5)
          screen_mode = -1;
        update_screen_status();          
      } else if (change && screen_mode != -1) {
        int8_t newVal;
        switch (screen_mode) {
          case LIGHT_VAR_ON_OFF:  
            GVM.setOnOff(GVM.getOnOff() + change); 
            break;
          case LIGHT_VAR_CHANNEL:   
            GVM.setChannel(GVM.getChannel() + change); 
            break;
          case LIGHT_VAR_BRIGHTNESS:
            GVM.setBrightness(GVM.getBrightness() + change); 
            break;    
          case LIGHT_VAR_CCT:
            GVM.setCct(GVM.getCct() + change); 
            break;    
          case LIGHT_VAR_HUE:
            GVM.setHue(GVM.getHue() + change);
            break;    
          case LIGHT_VAR_SATURATION: 
            GVM.setSaturation(GVM.getSaturation() + change);
            break;          
        }
        update_screen_status();
      }
    }
  }

#ifdef ESP32
  // Serial.println("CHK");
  if (Serial.available()) 
    serialEvent();
  // Serial.println("DN");
#endif
  delay(10);

  // Serial.println("WT");Aa
  GVM.wait_msg_or_timeout();
  // Serial.println("DN");

  delay(10);
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
      default:
        Serial.print(F("Unknown command '"));
        Serial.print(inChar);
        Serial.println("'");
    }
  }
}
