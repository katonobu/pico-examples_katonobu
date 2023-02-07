#include <arch/cc.h>
void tinyusb_net_lwip_transfer(void);

int tinyusb_arch_init(void);
void tinyusb_arch_deinit(void);
sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);
uint32_t sys_now(void);

bool tud_network_recv_cb(const uint8_t *src, uint16_t size);
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);
void tud_network_init_cb(void);

