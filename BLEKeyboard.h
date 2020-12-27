#ifndef BLEKeyboard_h
#define BLEKeyboard_h

void startKeyboard(void (*onInitialized_p)(), void (*onConnect_p)(esp_ble_gatts_cb_param_t *param));
void sendKey(uint8_t modifier, uint8_t key, uint8_t key2);
bool keyboardConnected();

#endif
