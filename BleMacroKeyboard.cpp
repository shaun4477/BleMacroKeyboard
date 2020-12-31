#include "BleMacroKeyboard.h"
#include "eeprom_config.h"

BleMacroKeyboardHandler BleMacroKeyboard;

void BleMacroKeyboardHandler::checkPins() {
  checkPinsAndCallback(directSendKey);
}

void BleMacroKeyboardHandler::readSerialKeysAndSend() {
  readSerialKeysAndCallback(directSendKey);
}

void BleMacroKeyboardHandler::resetConfig() {
  formatEeprom();
}

void BleMacroKeyboardHandler::loadConfig() {
  readAndProcessConfig();
}

void BleMacroKeyboardHandler::readSerialPinConfigUpdate() {
  readPinConfigUpdateFromSerial();
}
