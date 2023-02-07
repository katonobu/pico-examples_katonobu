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

        add_executable(mqtt)

        target_sources(mqtt PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/main.c
                ${CMAKE_CURRENT_LIST_DIR}/../tinyusb_net/tinyusb_net_lwip.c
                ${CMAKE_CURRENT_LIST_DIR}/../tinyusb_net/dhserver.c
                ${CMAKE_CURRENT_LIST_DIR}/../tinyusb_net/usb_descriptors.c
                ${CMAKE_CURRENT_LIST_DIR}/../tinyusb_net/rndis_reports.c
                ${PICO_TINYUSB_PATH}/lib/networking/dnserver.c
                )

        # Make sure TinyUSB can find tusb_config.h
        target_include_directories(mqtt PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/../tinyusb_net/
                ${PICO_TINYUSB_PATH}/lib/networking
                )

        # In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
        # for TinyUSB device support and tinyusb_board for the additional board support library used by the example
        target_link_libraries(mqtt PUBLIC
            pico_bootrom
            pico_stdlib
            pico_lwip
            tinyusb_device
            tinyusb_board
        )

        # Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
        #target_compile_definitions(mqtt PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

        pico_add_extra_outputs(mqtt)

        # add url via pico_set_program_url
        example_auto_set_url(mqtt)


    endif()
endif()
