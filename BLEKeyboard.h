#ifndef BleKeyboard_h
#define BleKeyboard_h

#ifdef IDF_VER
/* If we're being compiled in the ESP IDF environment 
 * make sure bluedroid is enabled */
#ifndef CONFIG_BLUEDROID_ENABLED
#error Looks like Bluetooth is not enabled in menuconfig
#endif
#endif

#include <BLEAddress.h>
#include <BLEDevice.h>

#ifndef DEFAULT_KEYBOARD_NAME
#define DEFAULT_KEYBOARD_NAME "Custom Keyboard"
#endif

#ifndef KEYBOARD_MANUFACTURER
#define KEYBOARD_MANUFACTURER "SMC"
#endif

typedef struct {
  esp_bd_addr_t peer;
} conn_info_t;

class BleKeyboardHandler {
  public:
    BleKeyboardHandler();
    void startKeyboard(void (*onInitialized_p)() = NULL, 
                       void (*onConnect_p)() = NULL, 
                       void (*onPassKeyNotify_p)(uint32_t) = NULL,
                       bool allowMultiConnect = false, 
                       void (*onDisconnect_p)() = NULL,
                       const char *keyboardName = NULL,
                       esp_ble_auth_req_t authMode = ESP_LE_AUTH_REQ_SC_MITM_BOND);
    bool keyboardConnected();  
    int getConnectedCount();
    BLEAddress getPeerAddress();
    void sendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    void sendString(const char *str);
    std::map<uint16_t, conn_info_t> getConnectedClients();

  protected:
    static void directSendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    static void directSendMsg(uint8_t *msg, int len);

  private:
    void sendMsg(uint8_t *msg, int len);
};

extern BleKeyboardHandler BleKeyboard;

#endif
