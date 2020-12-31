#ifndef BleKeyboard_h
#define BleKeyboard_h

#include <BLEDevice.h>

class BleKeyboardHandler {
  public:
    BleKeyboardHandler();
    void startKeyboard(void (*onInitialized_p)(), void (*onConnect_p)(esp_ble_gatts_cb_param_t *param));
    bool keyboardConnected();  
    void sendKey(uint8_t modifier, uint8_t key, uint8_t key2);

  protected:
    static void directSendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    static void directSendMsg(uint8_t *msg, int len);

  private:
    void sendMsg(uint8_t *msg, int len);
};

extern BleKeyboardHandler BleKeyboard;

#endif
