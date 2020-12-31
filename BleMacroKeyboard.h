#ifndef BleMacroKeyboard_h
#define BleMacroKeyboard_h

#include "BleKeyboard.h"

class BleMacroKeyboardHandler : public BleKeyboardHandler {
  public:
    void loadConfig();
    void resetConfig();
    void checkPins();

    void readSerialKeysAndSend();
    void readSerialPinConfigUpdate();
};

extern BleMacroKeyboardHandler BleMacroKeyboard;

#endif
