/*
 * Stub nuttx/board.h for host-side testing.
 */

#ifndef __NUTTX_BOARD_H
#define __NUTTX_BOARD_H

#include <stdint.h>

static inline int board_app_initialize(uintptr_t arg)
{
  (void)arg;
  return 0;
}

#endif /* __NUTTX_BOARD_H */
