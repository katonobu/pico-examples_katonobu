if (EXISTS ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c)
    add_executable(picow_freertos_mqtt_sys
            picow_freertos_mqtt.c
            )

    pico_enable_stdio_usb(picow_freertos_mqtt_sys 1)
    pico_enable_stdio_uart(picow_freertos_mqtt_sys 1)
        
    target_compile_definitions(picow_freertos_mqtt_sys PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
            LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
            PING_USE_SOCKETS=1
            )
    target_include_directories(picow_freertos_mqtt_sys PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/../.. # for our common lwipopts
            )
    target_link_libraries(picow_freertos_mqtt_sys
            pico_bootrom
            pico_cyw43_arch_lwip_sys_freertos
            pico_stdlib
            pico_lwip_mqtt
            FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
            )
    pico_add_extra_outputs(picow_freertos_mqtt_sys)
endif()
