/*
this appears as either a RNDIS or CDC-ECM USB virtual network adapter; the OS picks its preference

RNDIS should be valid on Linux and Windows hosts, and CDC-ECM should be valid on Linux and macOS hosts

The MCU appears to the host as IP address 192.168.7.1, and provides a DHCP server, DNS server, and web server.
*/
/*
Some smartphones *may* work with this implementation as well, but likely have limited (broken) drivers,
and likely their manufacturer has not tested such functionality.  Some code workarounds could be tried:

The smartphone may only have an ECM driver, but refuse to automatically pick ECM (unlike the OSes above);
try modifying ./examples/devices/net_lwip_webserver/usb_descriptors.c so that CONFIG_ID_ECM is default.

The smartphone may be artificially picky about which Ethernet MAC address to recognize; if this happens, 
try changing the first byte of tud_network_mac_address[] below from 0x02 to 0x00 (clearing bit 1).
*/

/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lwip/apps/httpd.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tinyusb_net_lwip.h"

static bool terminate_req = false;

err_t
httpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
  printf("\nhttpd_post_begin()\n");
  return ERR_OK;
}

err_t
httpd_post_receive_data(void *connection, struct pbuf *p)
{
  printf("httpd_post_receive_data()\n");
  return ERR_OK;
}

void
httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
  printf("httpd_post_finished()\n");
  terminate_req = true;
}

int app_main(void)
{
  printf("tinyusb_net-httpd start.\n");
  printf("this is build at %s %s\n",__DATE__, __TIME__);

  tinyusb_arch_init();
  httpd_init();

  while (terminate_req == false)
  {
    tinyusb_net_lwip_transfer();
    vTaskDelay(1);
  }
  tinyusb_arch_deinit();
  printf("------------------------------------\n");
  printf("reboot...\n");
  reset_usb_boot(0,0);
  while(1);
  return 0;
}
