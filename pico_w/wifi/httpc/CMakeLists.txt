add_executable(picow_httpc_background
        ${CMAKE_CURRENT_LIST_DIR}/../tls_client/picow_tls_client.c
        )
target_compile_definitions(picow_httpc_background PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        )
target_include_directories(picow_httpc_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        )
target_link_libraries(picow_httpc_background
        pico_bootrom
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        )
pico_add_extra_outputs(picow_httpc_background)
