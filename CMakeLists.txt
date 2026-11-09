add_subdirectory(zmk-driver-lpps)

if(CONFIG_RGBLED_WIDGET)
    zephyr_library_sources(boards/shields/nickey44a/behavior_trackpoint_layer_led.c)
    zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
                                 boards/shields/nickey44a/trackpoint_layer_led_central.c)
endif()
