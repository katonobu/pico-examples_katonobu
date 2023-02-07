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
#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "tinyusb_net_lwip.h"

static bool terminate_req = false;

int main(void)
{
  volatile uint32_t loop_count = 0;

  stdio_init_all();

  printf("tinyusb_net-mqtt start.\n");
  printf("this is build at %s %s\n",__DATE__, __TIME__);

  tinyusb_arch_init();
  loop_count = 0;
  const uint32_t dot_count = 100 * 1000;
  while (terminate_req == false)
  {
    tinyusb_net_lwip_transfer();
    if ((loop_count % dot_count) == 0){
      printf(".");
    }
    if (40 * dot_count < ++loop_count) {
      loop_count = 0;
      printf(".\n");
      break;
    }
  }
  tinyusb_arch_deinit();
  printf("------------------------------------\n");
  printf("reboot...\n");
  reset_usb_boot(0,0);
  while(1);
  return 0;
}
