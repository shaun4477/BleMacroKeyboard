include("${CMAKE_CURRENT_LIST_DIR}/util.cmake")

set(ARDUINO_LIB_SRC_DIR "$ENV{HOME}/src/microprocessors/esp32")
list(APPEND ARDUINO_SRC_LIBS "GvmLightControl")
__get_sources_from_subdirs("${ARDUINO_SRC_LIBS}" "${ARDUINO_LIB_SRC_DIR}" sources include_dirs)

list(APPEND sources "../../BleMacroKeyboardAndConsole.cpp" "../../BLEKeyboard.cpp" "../../BleMacroKeyboard.cpp" "../../M5Util.cpp" "../../SerialUtil.cpp" "../../eeprom_config.cpp")
list(APPEND include_dirs "../..")

#idf_component_register(SRCS "${sources}" INCLUDE_DIRS "${include_dirs}" PRIV_REQUIRES "arduino" "M5Stack")
idf_component_register(SRCS "${sources}" INCLUDE_DIRS "${include_dirs}" PRIV_REQUIRES "arduino" "M5Stack")

#target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO_M5Stick_C -DUS_KEYBOARD)
target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO_M5Stack_Core_ESP32 -DUS_KEYBOARD -Wno-error=unused-const-variable)
