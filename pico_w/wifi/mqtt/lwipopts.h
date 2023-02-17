#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "lwipopts_examples_common.h"

// counter measure for lwip-mqtt: sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)

#endif
