To build this project with ESP-IDF 

- Symlink components/arduino to a clone of https://github.com/shaun4477/arduino-esp32 (a 
  version of https://github.com/espressif/arduino-esp32 minorly modified to compile 
  correctly)

  For example:
    `ln -s ~/src/microprocessors/esp32/arduino-esp32 components/arduino`

- Symlink components/M5Stack to a clone of https://github.com/shaun4477/M5Stack (a 
  version of https://github.com/m5stack/M5Stack minorly modified to compile correctly)

  For example:
    `ln -s ~/src/microprocessors/esp32/m5stack/M5Stack components/M5Stack`

- Put https://github.com/shaun4477/GvmLightControl in ~/src/microprocessors/esp32

