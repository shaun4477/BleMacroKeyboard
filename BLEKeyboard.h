#ifndef BleKeyboard_h
#define BleKeyboard_h

#include <BLEDevice.h>

class BleKeyboardHandler {
  public:
    BleKeyboardHandler();
    void startKeyboard(void (*onInitialized_p)(), void (*onConnect_p)());
    bool keyboardConnected();  
    int getConnectedCount();
    uint8_t *getPeerAddress();
    void sendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    void sendString(char *str);

  protected:
    static void directSendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    static void directSendMsg(uint8_t *msg, int len);

  private:
    void sendMsg(uint8_t *msg, int len);
};

extern BleKeyboardHandler BleKeyboard;

#endif
