cmake_minimum_required(VERSION 3.16)
project(wac_native_tests C)
enable_testing()
find_package(Threads REQUIRED)

# Platform declarations live in the approved support header; forwarding headers
# are disposable native-build outputs, never added to firmware sources.
set(platform_headers "${CMAKE_CURRENT_BINARY_DIR}/platform")
file(MAKE_DIRECTORY "${platform_headers}/freertos")
foreach(header esp_err.h esp_system.h esp_wifi.h nvs.h freertos/FreeRTOS.h freertos/semphr.h freertos/task.h)
  file(WRITE "${platform_headers}/${header}" "#include \"test_support.h\"\n")
endforeach()

add_executable(test_device_config
  test_device_config.c test_support.c
  ../../main/device_config.c ../../main/timezone_table.c)
target_include_directories(test_device_config PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../../main)
target_compile_features(test_device_config PRIVATE c_std_11)
target_compile_options(test_device_config PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_device_config PRIVATE Threads::Threads m)
add_test(NAME device_config COMMAND test_device_config)
set_tests_properties(device_config PROPERTIES TIMEOUT 15)

add_executable(test_time test_time.c ../../main/wac/time.c ../../main/timezone_table.c)
target_include_directories(test_time PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../../main)
target_compile_features(test_time PRIVATE c_std_11)
target_compile_options(test_time PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME time_timezone COMMAND test_time)

# Exercise production controls with the existing LVGL dependency, rather than
# a second implementation of its object/event behavior.
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_BINARY_DIR}/lv_conf.h" CACHE PATH "" FORCE)
file(WRITE "${LV_BUILD_CONF_PATH}" "#pragma once\n#define LV_CONF_H\n#define LV_MEM_SIZE (1024 * 1024)\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_MONTSERRAT_24 1\n#define LV_FONT_MONTSERRAT_32 1\n")
add_subdirectory(../../managed_components/lvgl__lvgl "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
add_executable(test_ui test_ui.c test_support.c
  ../../main/wac/ui/settings.c ../../main/device_config.c ../../main/timezone_table.c
  ../../main/assets/lv_font_clock_time_144.c)
target_include_directories(test_ui PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../../main)
target_compile_features(test_ui PRIVATE c_std_11)
target_compile_options(test_ui PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_ui PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME settings_ui COMMAND test_ui)
set_tests_properties(settings_ui PROPERTIES TIMEOUT 15)
