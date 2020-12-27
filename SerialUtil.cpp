#include <Arduino.h>
#include "SerialUtil.h"

int serialTimedPeek() {
  int c;
  unsigned long _startMillis;

  _startMillis = millis();
  do {
    if (Serial.available()) {
      c = Serial.peek();
      return c;
    }
  } while(millis() - _startMillis < SERIAL_TIMEOUT_MS);
  return -1;     // -1 indicates timeout
}

int serialTimedSkipWhitespace(char *terminator) {
  while (1) {
    char nextChar = serialTimedPeek();
    if (nextChar == -1)
      return -1;
    else if (nextChar == ' ' || nextChar == '\t')
      Serial.read();
    else {
      if (terminator)
        *terminator = nextChar;
      return 0;
    }
  }
}

int serialTimedReadNum(uint8_t *out, char *terminator, bool hex) {
  bool firstChar = true;

  if (serialTimedSkipWhitespace(NULL) == -1) {
    Serial.println("Timeout 2");
    return -1;
  }

  *out = 0;

  while (1) {
    char nextChar = serialTimedPeek();
    if (nextChar == -1) {
      return -1;
    }

    uint8_t charVal;

    if (nextChar >= '0' && nextChar <= '9')
      charVal = nextChar - '0';
    else if (hex && nextChar >= 'a' && nextChar <= 'f')
      charVal = nextChar - 'a' + 10;
    else if (hex && nextChar >= 'A' && nextChar <= 'F')
      charVal = nextChar - 'A' + 10;
    else {
      if (firstChar)
        return -1;
      *terminator = nextChar;
      return 0;
    }

    Serial.read();

    *out *= hex ? 16 : 10;
    *out += charVal;
    firstChar = false;
  }
}

void serialPrintHex(long num) {
  if (num < 16)
    Serial.print("0");
  Serial.print(num, HEX);
}
