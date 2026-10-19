cmake_minimum_required(VERSION 3.16)
project(wac_native_tests C)
enable_testing()
find_package(Threads REQUIRED)

# Platform declarations live in the approved support header; forwarding headers
# are disposable native-build outputs, never added to firmware sources.
set(platform_headers "${CMAKE_CURRENT_BINARY_DIR}/platform")
file(MAKE_DIRECTORY "${platform_headers}/freertos" "${platform_headers}/driver" "${platform_headers}/bsp")
foreach(header bsp/display.h bsp/esp-bsp.h esp_codec_dev.h esp_err.h esp_system.h esp_wifi.h esp_event.h esp_check.h esp_netif.h esp_netif_sntp.h esp_sntp.h esp_http_client.h esp_crt_bundle.h nvs.h freertos/FreeRTOS.h freertos/semphr.h freertos/task.h freertos/queue.h freertos/event_groups.h driver/i2c_master.h driver/gpio.h esp_lcd_panel_io.h bsp/esp32_p4_wifi6_touch_lcd_xc.h)
  file(WRITE "${platform_headers}/${header}" "#include \"test_support.h\"\n")
endforeach()

file(WRITE "${platform_headers}/sdkconfig.h" "#pragma once\n")
file(WRITE "${platform_headers}/esp_timer.h" "#include \"test_support.h\"\n")
file(WRITE "${platform_headers}/esp_log.h" "#include <stdio.h>\n#define ESP_LOGI(tag,...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n")

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

add_executable(test_alarm_schedule test_alarm_schedule.c ../main/alarm_schedule.c)
target_include_directories(test_alarm_schedule PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_alarm_schedule PRIVATE c_std_11)
target_compile_options(test_alarm_schedule PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_alarm_schedule PRIVATE Threads::Threads)
add_test(NAME alarm_schedule COMMAND test_alarm_schedule)
set_tests_properties(alarm_schedule PROPERTIES TIMEOUT 15)

add_executable(test_alarm test_alarm.c test_support.c ../main/alarm.c ../main/alarm_schedule.c)
target_include_directories(test_alarm PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_alarm PRIVATE c_std_11)
target_compile_options(test_alarm PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_alarm PRIVATE Threads::Threads)
add_test(NAME alarm_recovery COMMAND test_alarm)
set_tests_properties(alarm_recovery PROPERTIES TIMEOUT 15)

# Use the actual audio owner included by the approved test source; only board,
# queue/task and copied-owner inputs are substituted in each fresh process.
add_executable(test_audio test_audio.c)
target_include_directories(test_audio PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main
  ../managed_components/espressif__esp_audio_codec/include
  ../managed_components/espressif__esp_audio_codec/include/simple_dec
  ../managed_components/espressif__esp_audio_codec/include/decoder
  ../managed_components/espressif__esp_audio_codec/include/decoder/impl)
target_compile_definitions(test_audio PRIVATE
  WAC_AUDIO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/assets/sounds/clock")
target_compile_features(test_audio PRIVATE c_std_11)
target_compile_options(test_audio PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_audio PRIVATE Threads::Threads m)
foreach(scenario generator playback queuefail taskfail bspfail openfail volumefail writefail closefail machine noise metadata recording machinefail cancelsetup weathergenerator weather weatherfail weatherbspfail)
  add_test(NAME audio_${scenario} COMMAND test_audio ${scenario})
  set_tests_properties(audio_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

add_executable(test_night test_night.c test_support.c ../main/night.c ../main/solar_times.c)
target_include_directories(test_night PRIVATE
  "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_night PRIVATE c_std_11)
target_compile_options(test_night PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_night PRIVATE Threads::Threads m)
add_test(NAME night_policy COMMAND test_night)
set_tests_properties(night_policy PROPERTIES TIMEOUT 15)

add_executable(test_solar_times test_solar_times.c ../main/solar_times.c)
target_include_directories(test_solar_times PRIVATE ../main)
target_compile_features(test_solar_times PRIVATE c_std_11)
target_compile_options(test_solar_times PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_solar_times PRIVATE m)
add_test(NAME solar_times COMMAND test_solar_times)
set_tests_properties(solar_times PROPERTIES TIMEOUT 15)

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
set(native_lv_conf "#pragma once\n#define LV_CONF_H\n#define LV_MEM_SIZE (16 * 1024 * 1024)\n#define LV_FONT_MONTSERRAT_14 1\n#define LV_FONT_MONTSERRAT_28 1\n#define LV_FONT_MONTSERRAT_26 1\n#define LV_FONT_MONTSERRAT_48 1\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_MONTSERRAT_20 1\n#define LV_FONT_MONTSERRAT_24 1\n#define LV_FONT_MONTSERRAT_22 1\n#define LV_FONT_MONTSERRAT_32 1\n#define LV_FONT_MONTSERRAT_34 1\n#define LV_FONT_MONTSERRAT_36 1\n")
if(EXISTS "${LV_BUILD_CONF_PATH}")
  file(READ "${LV_BUILD_CONF_PATH}" previous_lv_conf)
endif()
if(NOT native_lv_conf STREQUAL previous_lv_conf)
  file(WRITE "${LV_BUILD_CONF_PATH}" "${native_lv_conf}")
endif()
add_subdirectory(../managed_components/lvgl__lvgl "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
# Use the bundled decoder for real PNG validation in the weather-client checks.
# Native allocators enforce the same per-allocation bound as firmware's PSRAM hooks.
target_compile_definitions(lvgl PUBLIC LV_USE_LODEPNG=1 LODEPNG_MAX_ALLOC=4194304)
find_path(CJSON_INCLUDE_DIR cJSON.h
  HINTS "$ENV{IDF_PATH}/components/json/cJSON"
        "$ENV{HOME}/.espressif/v5.5.5/esp-idf/components/json/cJSON"
  REQUIRED)
add_executable(test_ui test_ui.c test_support.c
  ../main/ui/settings.c ../main/device_config.c ../main/timezone_table.c ../main/alarm_schedule.c
  ../main/assets/lv_font_clock_time_144.c ../main/assets/wac_fonts.c ../main/assets/wac_icons.c)
target_include_directories(test_ui PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_ui PRIVATE c_std_11)
target_compile_options(test_ui PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_ui PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME settings_ui COMMAND test_ui)
add_test(NAME settings_alerts COMMAND test_ui alerts)
# Successful restart phases need separate runs: esp_restart never returns on hardware.
add_test(NAME settings_wifi_restart COMMAND test_ui wifi_restart)
add_test(NAME settings_weather_saved COMMAND test_ui weather_saved)
add_test(NAME settings_display_save COMMAND test_ui display_save)
add_test(NAME settings_closed_restart COMMAND test_ui closed_restart)
set_tests_properties(settings_wifi_restart settings_weather_saved settings_display_save settings_closed_restart PROPERTIES TIMEOUT 15)
set_tests_properties(settings_ui PROPERTIES TIMEOUT 15)
set_tests_properties(settings_alerts PROPERTIES TIMEOUT 15)
foreach(scenario day night location clear key)
  add_test(NAME settings_visual_crossing_${scenario} COMMAND test_ui vc_${scenario})
  set_tests_properties(settings_visual_crossing_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

# Include the actual weather owner and worker in the existing native service
# test; no generated extraction from main or production test interface is needed.
set(nws_mbedtls "${CJSON_INCLUDE_DIR}/../../mbedtls/mbedtls")
file(WRITE "${platform_headers}/nws_mbedtls_config.h" "#pragma once\n#define MBEDTLS_SHA256_C\n")
add_executable(test_weather_service test_weather_service.c test_support.c
  "${nws_mbedtls}/library/sha256.c" "${nws_mbedtls}/library/platform_util.c"
  ../main/forecast_representative.c
  ../main/location_scoring.c
  ../main/weather_icon_names.c
  "${CJSON_INCLUDE_DIR}/cJSON.c")
target_include_directories(test_weather_service PRIVATE
  "${nws_mbedtls}/include" "${nws_mbedtls}/library"
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}" ../main)
target_compile_definitions(test_weather_service PRIVATE MBEDTLS_CONFIG_FILE="nws_mbedtls_config.h" time=wac_test_time
  WAC_WEATHER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/test-data")
target_compile_features(test_weather_service PRIVATE c_std_11)
target_compile_options(test_weather_service PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_weather_service PRIVATE lvgl::lvgl Threads::Threads m)
add_test(NAME weather_service COMMAND test_weather_service)
set_tests_properties(weather_service PROPERTIES TIMEOUT 15)

add_executable(test_ui_shell test_ui.c test_support.c
  ../main/ui/ui.c ../main/ui/clock_views.c ../main/ui/weather_views.c ../main/ui/settings.c
  ../main/device_config.c ../main/timezone_table.c ../main/weather_icon_names.c ../main/alarm_schedule.c ../main/location_scoring.c
  "${CJSON_INCLUDE_DIR}/cJSON.c"
  ../main/assets/lv_font_clock_time_144.c ../main/assets/lv_font_temp_102.c
  ../main/assets/wac_fonts.c ../main/assets/wac_icons.c)
target_compile_definitions(test_ui_shell PRIVATE WAC_UI_SHELL_TEST=1)
target_include_directories(test_ui_shell PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
target_compile_features(test_ui_shell PRIVATE c_std_11)
target_compile_options(test_ui_shell PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_ui_shell PRIVATE lvgl::lvgl Threads::Threads m)
# Opt-in rendered review uses the firmware's own asset source list, never the
# small substitutes below. It is deliberately absent from CTest.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../main/CMakeLists.txt" firmware_source_list)
string(REGEX MATCHALL "\"assets/[^\"]+\\.c\"" firmware_asset_entries "${firmware_source_list}")
set(capture_assets)
foreach(entry IN LISTS firmware_asset_entries)
  string(REPLACE "\"" "" asset "${entry}")
  list(APPEND capture_assets "${CMAKE_CURRENT_SOURCE_DIR}/../main/${asset}")
endforeach()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../main/weather_client.c" capture_weather_source)
string(REGEX MATCH "static const weather_alert_type_t nws_catalog\\[\\] = \\{[^;]+;" capture_catalog "${capture_weather_source}")
if(NOT capture_catalog)
  message(FATAL_ERROR "Production NWS catalog unavailable for rendered review")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/capture_catalog.inc" "${capture_catalog}\n")
foreach(capture_target test_ui_capture test_settings_capture)
  if(capture_target STREQUAL "test_ui_capture")
    set(capture_sources ../main/ui/ui.c ../main/ui/clock_views.c ../main/ui/weather_views.c
      ../main/location_scoring.c)
  else()
    set(capture_sources)
  endif()
  add_executable(${capture_target} EXCLUDE_FROM_ALL test_ui.c test_support.c
    ../main/ui/settings.c ../main/device_config.c ../main/timezone_table.c ../main/alarm_schedule.c ../main/weather_icon_names.c
    "${CJSON_INCLUDE_DIR}/cJSON.c" ${capture_sources} ${capture_assets})
  target_compile_definitions(${capture_target} PRIVATE WAC_RENDER_CAPTURE=1)
  if(capture_target STREQUAL "test_ui_capture")
    target_compile_definitions(${capture_target} PRIVATE WAC_UI_SHELL_TEST=1)
  endif()
  target_include_directories(${capture_target} PRIVATE "${CJSON_INCLUDE_DIR}" "${platform_headers}"
    "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" ../main)
  target_compile_features(${capture_target} PRIVATE c_std_11)
  target_compile_options(${capture_target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(${capture_target} PRIVATE lvgl::lvgl Threads::Threads m)
endforeach()
add_test(NAME ui_shell COMMAND test_ui_shell)
add_test(NAME settings_weather_hidden_restart COMMAND test_ui_shell weather_hidden_restart)
set_tests_properties(ui_shell settings_weather_hidden_restart PROPERTIES TIMEOUT 15)

# Execute the actual private touch callback with real LVGL input handling;
# hardware reads alone are substituted. Firmware and hardware checks cover BSP
# startup, adapter tasks and callback registration.
set(application_main "${CMAKE_CURRENT_SOURCE_DIR}/../main/main.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${application_main}")
file(READ "${application_main}" application_main_source)
set(touch_startup "static const char *TAG = \"native_touch\";\n")
string(FIND "${application_main_source}" "static esp_err_t read_touch(" begin)
if(begin LESS 0)
  message(FATAL_ERROR "Touch callback missing")
endif()
string(SUBSTRING "${application_main_source}" ${begin} -1 remainder)
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

# Reuse the actual private clock-command and owner-copy integration helpers.
set(alarm_commands "static QueueHandle_t s_clock_commands;\n")
string(FIND "${application_main_source}" "typedef enum { CLOCK_COMMAND_WARNING" begin)
string(FIND "${application_main_source}" "static clock_alarm_result_t s_alarm_result;" end)
if(begin LESS 0 OR end LESS begin)
  message(FATAL_ERROR "Clock command declarations missing")
endif()
string(LENGTH "static clock_alarm_result_t s_alarm_result;" tail_length)
math(EXPR length "${end} - ${begin} + ${tail_length}")
string(SUBSTRING "${application_main_source}" ${begin} ${length} declarations)
string(APPEND alarm_commands "${declarations}\n")
foreach(signature "static esp_err_t request_clock_warning_dismiss(" "static bool alarm_occurrence_matches(" "static esp_err_t request_alarm_action(" "static esp_err_t execute_alarm_action(" "static void publish_alarm_result(" "static bool take_alarm_result(" "static void runtime_state_snapshot(")
  string(FIND "${application_main_source}" "${signature}" begin)
  if(begin LESS 0)
    message(FATAL_ERROR "Clock command function ${signature} missing")
  endif()
  string(SUBSTRING "${application_main_source}" ${begin} -1 remainder)
  string(FIND "${remainder}" "\n}" end)
  if(end LESS 0)
    message(FATAL_ERROR "Clock command function ${signature} incomplete")
  endif()
  math(EXPR length "${end} + 2")
  string(SUBSTRING "${remainder}" 0 ${length} function_source)
  string(APPEND alarm_commands "${function_source}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/alarm_commands.inc" "${alarm_commands}")
add_executable(test_alarm_commands test_alarm.c test_support.c ../main/alarm.c ../main/alarm_schedule.c)
target_compile_definitions(test_alarm_commands PRIVATE WAC_ALARM_COMMAND_TEST=1)
target_include_directories(test_alarm_commands PRIVATE
  "${CJSON_INCLUDE_DIR}" "${platform_headers}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" ../main)
target_compile_features(test_alarm_commands PRIVATE c_std_11)
target_compile_options(test_alarm_commands PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_alarm_commands PRIVATE Threads::Threads)
add_test(NAME alarm_commands COMMAND test_alarm_commands)
set_tests_properties(alarm_commands PROPERTIES TIMEOUT 15)
