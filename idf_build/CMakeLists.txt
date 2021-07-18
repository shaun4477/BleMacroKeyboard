# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# message(INFO "${CMAKE_CURRENT_LIST_DIR}")
#set(EXTRA_COMPONENT_DIRS "$ENV{HOME}/src/microprocessors/esp32/arduino-esp32" "$ENV{HOME}/src/microprocessors/esp32/m5stack/M5Stack")
# set(EXTRA_COMPONENT_DIRS "$ENV{HOME}/src/microprocessors/esp32/m5stack/M5Stack")
# message(INFO "${EXTRA_COMPONENT_DIRS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(BleMacroKeyboardAndConsole)
