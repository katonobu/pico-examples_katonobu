cmake_minimum_required(VERSION 3.13)

if (DEFINED ENV{PICO_TINYUSB_PATH} AND (NOT PICO_TINYUSB_PATH))
    set(PICO_TINYUSB_PATH $ENV{PICO_TINYUSB_PATH})
    message("Using PICO_TINYUSB_PATH from environment ('${PICO_TINYUSB_PATH}')")
endif ()

set(TINYUSB_TEST_PATH "src/portable/raspberrypi/rp2040")
if (NOT PICO_TINYUSB_PATH)
    set(PICO_TINYUSB_PATH ${PROJECT_SOURCE_DIR}/lib/tinyusb)
    if (NOT EXISTS ${PICO_TINYUSB_PATH}/${TINYUSB_TEST_PATH})
        message(WARNING "TinyUSB submodule has not been initialized; USB support will be unavailable
hint: try 'git submodule update --init' from your SDK directory (${PICO_SDK_PATH}).")
    endif()
elseif (NOT EXISTS ${PICO_TINYUSB_PATH}/${TINYUSB_TEST_PATH})
    message(WARNING "PICO_TINYUSB_PATH specified but content not present.")
endif()

if (EXISTS ${PICO_TINYUSB_PATH}/${TINYUSB_TEST_PATH})
    message("Using PICO_TINYUSB_PATH as ('${PICO_TINYUSB_PATH}')")

    if (DEFINED ENV{PICO_LWIP_PATH} AND (NOT PICO_LWIP_PATH))
        set(PICO_LWIP_PATH $ENV{PICO_LWIP_PATH})
        message("Using PICO_LWIP_PATH from environment ('${PICO_LWIP_PATH}')")
    endif ()

    set(LWIP_TEST_PATH "src/Filelists.cmake")
    if (NOT PICO_LWIP_PATH)
        set(PICO_LWIP_PATH ${PROJECT_SOURCE_DIR}/lib/lwip)
#        if (NOT EXISTS ${PICO_LWIP_PATH}/${LWIP_TEST_PATH})
#            message(WARNING "LWIP submodule has not been initialized; Pico W wireless support will be unavailable
#hint: try 'git submodule update --init' from your SDK directory (${PICO_SDK_PATH}).")
#        endif()
    elseif (NOT EXISTS ${PICO_LWIP_PATH}/${LWIP_TEST_PATH})
        message(WARNING "PICO_LWIP_PATH specified but content not present.")
    endif()

    if (EXISTS ${PICO_LWIP_PATH}/${LWIP_TEST_PATH})
        message("Using PICO_LWIP_PATH as ('${PICO_LWIP_PATH}')")

        add_executable(tinyusb_net)

        target_sources(tinyusb_net PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/main.c
                ${CMAKE_CURRENT_LIST_DIR}/tinyusb_net_lwip.c
                ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
                )

        # Make sure TinyUSB can find tusb_config.h
        target_include_directories(tinyusb_net PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}
                ${PICO_LWIP_PATH}/src/include
                ${PICO_LWIP_PATH}/src/include/ipv4
                ${PICO_LWIP_PATH}/src/include/lwip/apps
                ${PICO_TINYUSB_PATH}/lib/networking
                )
        target_sources(tinyusb_net PUBLIC
                ${PICO_LWIP_PATH}/src/core/altcp.c
                ${PICO_LWIP_PATH}/src/core/altcp_alloc.c
                ${PICO_LWIP_PATH}/src/core/altcp_tcp.c
                ${PICO_LWIP_PATH}/src/core/def.c
                ${PICO_LWIP_PATH}/src/core/dns.c
                ${PICO_LWIP_PATH}/src/core/inet_chksum.c
                ${PICO_LWIP_PATH}/src/core/init.c
                ${PICO_LWIP_PATH}/src/core/ip.c
                ${PICO_LWIP_PATH}/src/core/mem.c
                ${PICO_LWIP_PATH}/src/core/memp.c
                ${PICO_LWIP_PATH}/src/core/netif.c
                ${PICO_LWIP_PATH}/src/core/pbuf.c
                ${PICO_LWIP_PATH}/src/core/raw.c
                ${PICO_LWIP_PATH}/src/core/stats.c
                ${PICO_LWIP_PATH}/src/core/sys.c
                ${PICO_LWIP_PATH}/src/core/tcp.c
                ${PICO_LWIP_PATH}/src/core/tcp_in.c
                ${PICO_LWIP_PATH}/src/core/tcp_out.c
                ${PICO_LWIP_PATH}/src/core/timeouts.c
                ${PICO_LWIP_PATH}/src/core/udp.c
                ${PICO_LWIP_PATH}/src/core/ipv4/autoip.c
                ${PICO_LWIP_PATH}/src/core/ipv4/dhcp.c
                ${PICO_LWIP_PATH}/src/core/ipv4/etharp.c
                ${PICO_LWIP_PATH}/src/core/ipv4/icmp.c
                ${PICO_LWIP_PATH}/src/core/ipv4/igmp.c
                ${PICO_LWIP_PATH}/src/core/ipv4/ip4.c
                ${PICO_LWIP_PATH}/src/core/ipv4/ip4_addr.c
                ${PICO_LWIP_PATH}/src/core/ipv4/ip4_frag.c
                ${PICO_LWIP_PATH}/src/netif/ethernet.c
                ${PICO_LWIP_PATH}/src/netif/slipif.c
                ${PICO_LWIP_PATH}/src/apps/http/httpd.c
                ${PICO_LWIP_PATH}/src/apps/http/fs.c
                ${PICO_TINYUSB_PATH}/lib/networking/dhserver.c
                ${PICO_TINYUSB_PATH}/lib/networking/dnserver.c
                ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
                )

        # In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
        # for TinyUSB device support and tinyusb_board for the additional board support library used by the example
        target_link_libraries(tinyusb_net PUBLIC pico_stdlib tinyusb_device tinyusb_board)

        # Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
        #target_compile_definitions(tinyusb_net PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

        pico_add_extra_outputs(tinyusb_net)

        # add url via pico_set_program_url
        example_auto_set_url(tinyusb_net)


    endif()
endif()
