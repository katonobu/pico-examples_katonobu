add_executable(picow_mqtt_background
        mqtt_example.c
        )
target_compile_definitions(picow_mqtt_background PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        )
target_include_directories(picow_mqtt_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        )
target_link_libraries(picow_mqtt_background
        pico_bootrom
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        pico_lwip_mqtt
        )

pico_add_extra_outputs(picow_mqtt_background)

