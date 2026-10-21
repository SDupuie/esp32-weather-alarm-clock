# Native screen tests also read this artwork list. Generators update asset
# contents; the maintained filenames have one build list here.
idf_component_register(
  SRCS
    "assets/analog_center_cap.c"
    "assets/analog_edge_indicator.c"
    "assets/analog_hour_hand.c"
    "assets/analog_minute_hand.c"
    "assets/analog_second_hand.c"
    "assets/analog_stage_day.c"
    "assets/analog_stage_night.c"
    "assets/digital_stage_day.c"
    "assets/digital_stage_night.c"
    "assets/forecast_tomorrow_frame_day.c"
    "assets/forecast_tomorrow_frame_night.c"
    "assets/lv_font_clock_time_144.c"
    "assets/lv_font_temp_102.c"
    "assets/round_stage_day.c"
    "assets/round_stage_night.c"
    "assets/weather_icon_clear_day.c"
    "assets/weather_icon_clear_night.c"
    "assets/weather_icon_cloudy.c"
    "assets/weather_icon_fog.c"
    "assets/weather_icon_partlycloudy_day.c"
    "assets/weather_icon_partlycloudy_night.c"
    "assets/weather_icon_rain.c"
    "assets/weather_icon_showers_day.c"
    "assets/weather_icon_showers_night.c"
    "assets/weather_icon_sleet.c"
    "assets/weather_icon_snow.c"
    "assets/weather_icon_thundersnow.c"
    "assets/weather_icon_thunderstorm.c"
    "assets/weather_icons.c"
    "assets/wac_fonts.c"
    "assets/wac_icons.c"
    "main.c"
    "ui/ui.c"
    "ui/clock_views.c"
    "ui/weather_views.c"
    "ui/settings.c"
    "clock_time.c"
    "alarm.c"
    "alarm_schedule.c"
    "audio.c"
    "night.c"
    "solar_times.c"
    "ds3231.c"
    "connectivity.c"
    "device_config.c"
    "forecast_representative.c"
    "location_lookup.c"
    "location_scoring.c"
    "message_service.c"
    "timezone_table.c"
    "weather_client.c"
    "weather_icon_names.c"
  INCLUDE_DIRS
    "."
  EMBED_TXTFILES
    "web/messages.html"
  REQUIRES
    espressif__esp_audio_codec
    espressif__esp_codec_dev
    espressif__esp_lcd_touch_gt911
    nvs_flash
    esp_event
    esp_http_client
    esp_http_server
    esp_netif
    esp_timer
    esp_wifi
    json
    lwip
    mbedtls
    spiffs
)

# The pinned codec adds an unquoted search path that breaks project paths with
# spaces. Its imported archive targets already supply the exact library paths.
idf_component_get_property(audio_codec_lib espressif__esp_audio_codec COMPONENT_LIB)
idf_component_get_property(audio_codec_dir espressif__esp_audio_codec COMPONENT_DIR)
set(audio_codec_search_flag "-L ${audio_codec_dir}/lib/${IDF_TARGET}")
foreach(link_property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
  get_target_property(audio_codec_links ${audio_codec_lib} ${link_property})
  if(audio_codec_links)
    list(REMOVE_ITEM audio_codec_links "${audio_codec_search_flag}"
      "$<LINK_ONLY:${audio_codec_search_flag}>")
    set_property(TARGET ${audio_codec_lib} PROPERTY ${link_property} "${audio_codec_links}")
  endif()
endforeach()

# Only the approved prepared loops are packaged; source recordings stay off-board.
spiffs_create_partition_image(storage
  "${CMAKE_CURRENT_LIST_DIR}/../assets/sounds/clock"
  FLASH_IN_PROJECT)

# Use the bundled decoder with finite PSRAM allocations supplied by the client.
# No managed-component source is patched.
idf_component_get_property(radar_lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${radar_lvgl_lib} PRIVATE LODEPNG_NO_COMPILE_ALLOCATORS)
