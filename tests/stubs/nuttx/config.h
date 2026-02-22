/*
 * Stub nuttx/config.h for host-side testing.
 * Provides config defines the driver headers expect.
 */

#ifndef __NUTTX_CONFIG_H
#define __NUTTX_CONFIG_H

#define CONFIG_MMWAVE_LD2410          1
#define CONFIG_MMWAVE_LD2410_DEVPATH  "/dev/mmwave0"
#define CONFIG_MMWAVE_LD2410_UART_PATH "/dev/ttyS1"
#define CONFIG_MMWAVE_LD2410_BAUD     256000
#define CONFIG_VERSION_STRING         "test"

#define CONFIG_SERIAL                 1
#define CONFIG_FS_LITTLEFS            1
#define CONFIG_FS_PROCFS              1
#define CONFIG_NET_TCP                1

/* NuttX core macros */
#ifndef FAR
#define FAR
#endif

#ifndef OK
#define OK 0
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

/* Silence unused-variable warnings in stubs */
#define UNUSED(x) ((void)(x))

#endif /* __NUTTX_CONFIG_H */
