/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Peter Lawrence
 *
 * influenced by lrndis https://github.com/fetisov/lrndis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

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

#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

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

int main(void)
{
  volatile uint32_t loop_count = 0;

  stdio_init_all();

  printf("tinyusb_net-httpd start.\n");
  printf("this is build at %s %s\n",__DATE__, __TIME__);

  tinyusb_arch_init();
  httpd_init();

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
