#ifndef SerialUtil_h
#define SerialUtil_h

#define SERIAL_TIMEOUT_MS 500

int serialTimedPeek();
int serialTimedSkipWhitespace(char *terminator);
int serialTimedReadNum(uint8_t *out, char *terminator, bool hex);
void serialPrintHex(long num);

#endif
