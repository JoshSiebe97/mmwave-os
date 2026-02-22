/*
 * Stub nuttx/clock.h for host-side testing.
 */

#ifndef __NUTTX_CLOCK_H
#define __NUTTX_CLOCK_H

#include <stdint.h>

#ifndef TICK_PER_SEC
#define TICK_PER_SEC 1000
#endif

static inline uint32_t clock_systime_ticks(void)
{
  /* Return a fixed value for deterministic tests */
  return 12345;
}

#endif /* __NUTTX_CLOCK_H */
