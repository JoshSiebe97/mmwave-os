/*
 * Stub debug.h for host-side testing.
 * Replaces NuttX debug/syslog macros with printf or no-ops.
 */

#ifndef __NUTTX_DEBUG_H
#define __NUTTX_DEBUG_H

#include <stdio.h>

/* Log levels */
#define LOG_INFO     6
#define LOG_WARNING  4
#define LOG_ERR      3

/* NuttX sensor debug macros — silent in tests */
#define snerr(fmt, ...)    do { } while (0)
#define snwarn(fmt, ...)   do { } while (0)
#define sninfo(fmt, ...)   do { } while (0)
#define syslog(level, ...) do { } while (0)

#endif /* __NUTTX_DEBUG_H */
