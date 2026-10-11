cmake_minimum_required(VERSION 3.16)
project(wac_native_tests C)
enable_testing()
find_package(Threads REQUIRED)

# Platform declarations live in the approved support header; forwarding headers
# are disposable native-build outputs, never added to firmware sources.
set(platform_headers "${CMAKE_CURRENT_BINARY_DIR}/platform")
file(MAKE_DIRECTORY "${platform_headers}/freertos" "${platform_headers}/driver" "${platform_headers}/bsp")
foreach(header bsp/display.h esp_err.h esp_system.h esp_wifi.h esp_event.h esp_check.h esp_netif.h esp_netif_sntp.h esp_sntp.h esp_http_client.h esp_crt_bundle.h nvs.h freertos/FreeRTOS.h freertos/semphr.h freertos/task.h freertos/event_groups.h driver/i2c_master.h driver/gpio.h esp_lcd_panel_io.h bsp/esp32_p4_wifi6_touch_lcd_xc.h)
  file(WRITE "${platform_headers}/${header}" "#include \"test_support.h\"\n")
endforeach()

file(WRITE "${platform_headers}/sdkconfig.h" "#pragma once\n")
file(WRITE "${platform_headers}/esp_timer.h" "#include \"test_support.h\"\n")
file(WRITE "${platform_headers}/esp_log.h" "#include <stdio.h>\n#define ESP_LOGI(tag,...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)\n#define ESP_LOGW ESP_LOGI\n")

add_executable(test_device_config
  test_device_config.c test_support.c
  ../main/device_config.c ../main/timezone_table.c)
target_include_directories(test_device_config PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_device_config PRIVATE c_std_11)
target_compile_options(test_device_config PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_device_config PRIVATE Threads::Threads m)
add_test(NAME device_config COMMAND test_device_config)
set_tests_properties(device_config PROPERTIES TIMEOUT 15)

add_executable(test_time test_time.c test_support.c ../main/clock_time.c ../main/ds3231.c ../main/timezone_table.c)
target_include_directories(test_time PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_time PRIVATE c_std_11)
target_compile_options(test_time PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_compile_definitions(test_time PRIVATE time=wac_test_time settimeofday=wac_test_settimeofday)
target_link_libraries(test_time PRIVATE Threads::Threads m)
add_test(NAME time_timezone COMMAND test_time)
set_tests_properties(time_timezone PROPERTIES TIMEOUT 15)

add_executable(test_night test_night.c test_support.c ../main/night.c)
target_include_directories(test_night PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_night PRIVATE c_std_11)
target_compile_options(test_night PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_night PRIVATE Threads::Threads)
add_test(NAME night_policy COMMAND test_night)
set_tests_properties(night_policy PROPERTIES TIMEOUT 15)

add_executable(test_connectivity test_connectivity.c test_support.c ../main/connectivity.c)
target_include_directories(test_connectivity PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_connectivity PRIVATE c_std_11)
target_compile_options(test_connectivity PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_connectivity PRIVATE Threads::Threads)
# Separate processes keep production lifetime state intact, without reset hooks.
foreach(scenario init_first scan_first scan_busy init_error scan_error cleanup_disconnect time_pending time_init_retry)
  add_test(NAME connectivity_${scenario} COMMAND test_connectivity ${scenario})
  set_tests_properties(connectivity_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

# Exercise production controls with the existing LVGL dependency, rather than
# a second implementation of its object/event behavior.
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_BINARY_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(native_lv_conf "#pragma once\n#define LV_CONF_H\n#define LV_MEM_SIZE (4 * 1024 * 1024)\n#define LV_FONT_MONTSERRAT_14 1\n#define LV_FONT_MONTSERRAT_28 1\n#define LV_FONT_MONTSERRAT_26 1\n#define LV_FONT_MONTSERRAT_48 1\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_MONTSERRAT_24 1\n#define LV_FONT_MONTSERRAT_22 1\n#define LV_FONT_MONTSERRAT_32 1\n#define LV_FONT_MONTSERRAT_34 1\n#define LV_FONT_MONTSERRAT_36 1\n")
if(EXISTS "${LV_BUILD_CONF_PATH}")
  file(READ "${LV_BUILD_CONF_PATH}" previous_lv_conf)
endif()
if(NOT native_lv_conf STREQUAL previous_lv_conf)
  file(WRITE "${LV_BUILD_CONF_PATH}" "${native_lv_conf}")
endif()
add_subdirectory(../managed_components/lvgl__lvgl "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
find_path(CJSON_INCLUDE_DIR cJSON.h
  HINTS "$ENV{IDF_PATH}/components/json/cJSON"
        "$ENV{HOME}/.espressif/v5.5.5/esp-idf/components/json/cJSON"
  REQUIRED)
add_executable(test_ui test_ui.c test_support.c
  ../main/ui/settings.c ../main/device_config.c ../main/timezone_table.c
  ../main/assets/lv_font_clock_time_144.c ../main/assets/wac_fonts.c ../main/assets/wac_icons.c)
target_include_directories(test_ui PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_ui PRIVATE c_std_11)
target_compile_options(test_ui PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_ui PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME settings_ui COMMAND test_ui)
# Successful restart phases need separate runs: esp_restart never returns on hardware.
add_test(NAME settings_wifi_restart COMMAND test_ui wifi_restart)
add_test(NAME settings_weather_saved COMMAND test_ui weather_saved)
set_tests_properties(settings_wifi_restart settings_weather_saved PROPERTIES TIMEOUT 15)
set_tests_properties(settings_ui PROPERTIES TIMEOUT 15)

# Exercise the existing private worker and its publication helpers without a
# firmware test interface or a second implementation in the maintained test.
set(weather_main "${CMAKE_CURRENT_SOURCE_DIR}/../main/main.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${weather_main}")
file(READ "${weather_main}" weather_main_source)
set(weather_worker "static const char *TAG = \"native_weather_worker\";\nstatic app_runtime_state_t s_runtime_state;\nstatic SemaphoreHandle_t s_runtime_mutex;\n")
foreach(name RUNTIME_LOCK_TIMEOUT_MS WIFI_CONNECT_TIMEOUT_MS WEATHER_REFRESH_INTERVAL_MS WEATHER_RETRY_INTERVAL_MS)
  string(REGEX MATCH "static const uint32_t ${name} = [^;]*;" declaration "${weather_main_source}")
  if(NOT declaration)
    message(FATAL_ERROR "Weather worker constant ${name} missing")
  endif()
  string(APPEND weather_worker "${declaration}\n")
endforeach()
foreach(signature "static void copy_text(" "static void runtime_state_update(" "static bool runtime_state_has_valid_weather(" "static void weather_service_wait(" "static void weather_service_task(")
  string(FIND "${weather_main_source}" "${signature}" begin)
  if(begin LESS 0)
    message(FATAL_ERROR "Weather worker function ${signature} missing")
  endif()
  string(SUBSTRING "${weather_main_source}" ${begin} -1 remainder)
  string(FIND "${remainder}" "\n}" end)
  if(end LESS 0)
    message(FATAL_ERROR "Weather worker function ${signature} incomplete")
  endif()
  math(EXPR length "${end} + 2")
  string(SUBSTRING "${remainder}" 0 ${length} function_source)
  string(APPEND weather_worker "${function_source}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/weather_service_task.inc" "${weather_worker}")
add_executable(test_weather_service test_weather_service.c test_support.c
  ../main/weather_client.c ../main/forecast_representative.c
  ../main/storm_conditions.c ../main/weather_icon_names.c
  "${CJSON_INCLUDE_DIR}/cJSON.c")
target_include_directories(test_weather_service PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}" ../main)
target_compile_definitions(test_weather_service PRIVATE time=wac_test_time)
target_compile_features(test_weather_service PRIVATE c_std_11)
target_compile_options(test_weather_service PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_weather_service PRIVATE Threads::Threads m)
add_test(NAME weather_service COMMAND test_weather_service)
set_tests_properties(weather_service PROPERTIES TIMEOUT 15)

add_executable(test_ui_shell test_ui.c test_support.c
  ../main/ui/ui.c ../main/ui/clock_views.c ../main/ui/settings.c
  ../main/device_config.c ../main/timezone_table.c ../main/weather_icon_names.c
  ../main/assets/lv_font_clock_time_144.c ../main/assets/lv_font_temp_102.c
  ../main/assets/lv_font_temp_105.c ../main/assets/lv_font_wind_speed_70.c ../main/assets/wac_fonts.c ../main/assets/wac_icons.c)
target_compile_definitions(test_ui_shell PRIVATE WAC_UI_SHELL_TEST=1)
target_include_directories(test_ui_shell PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_ui_shell PRIVATE c_std_11)
target_compile_options(test_ui_shell PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_ui_shell PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME ui_shell COMMAND test_ui_shell)
set_tests_properties(ui_shell PROPERTIES TIMEOUT 15)

# Execute the actual private touch callback with real LVGL input handling;
# hardware reads alone are substituted. Firmware and hardware checks cover BSP
# startup, adapter tasks and callback registration.
set(touch_startup "static const char *TAG = \"native_touch\";\n")
string(FIND "${weather_main_source}" "static esp_err_t read_touch(" begin)
if(begin LESS 0)
  message(FATAL_ERROR "Touch callback missing")
endif()
string(SUBSTRING "${weather_main_source}" ${begin} -1 remainder)
string(FIND "${remainder}" "\n}" end)
if(end LESS 0)
  message(FATAL_ERROR "Touch callback incomplete")
endif()
math(EXPR length "${end} + 2")
string(SUBSTRING "${remainder}" 0 ${length} function_source)
string(APPEND touch_startup "${function_source}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/touch_startup.inc" "${touch_startup}")
add_executable(test_touch test_ui.c test_support.c)
target_compile_definitions(test_touch PRIVATE WAC_TOUCH_TEST=1
  CONFIG_ESP_LCD_TOUCH_MAX_POINTS=5 CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS=0)
target_include_directories(test_touch PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
  ../managed_components/espressif__esp_lcd_touch/include)
target_compile_features(test_touch PRIVATE c_std_11)
target_compile_options(test_touch PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_touch PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME touch_recovery COMMAND test_touch)
set_tests_properties(touch_recovery PROPERTIES TIMEOUT 15)
