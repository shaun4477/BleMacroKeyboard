#ifndef BLEKeyboard_h
#define BLEKeyboard_h

class BleMacroKeyboard {
  public:
    BleMacroKeyboard();
    void startKeyboard(void (*onInitialized_p)(), void (*onConnect_p)(esp_ble_gatts_cb_param_t *param));
    bool keyboardConnected();  

    void loadConfig();
    void resetConfig();
    void checkPins();

    void sendKey(uint8_t modifier, uint8_t key, uint8_t key2);
    void readSerialKeysAndSend();
    void readSerialPinConfigUpdate();

  private:
    void sendMsg(uint8_t *msg, int len);
};

extern BleMacroKeyboard MacroKeyboard;

#endif
