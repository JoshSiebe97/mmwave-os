/*
 * Stub nuttx/kthread.h for host-side testing.
 */

#ifndef __NUTTX_KTHREAD_H
#define __NUTTX_KTHREAD_H

#include <stdint.h>

typedef int pid_t_stub;

static inline int kthread_create(const char *name, int priority,
                                 int stack_size,
                                 int (*entry)(int, char **),
                                 char **argv)
{
  (void)name; (void)priority; (void)stack_size;
  (void)entry; (void)argv;
  /* Don't actually start a thread in host tests */
  return 42;  /* fake pid */
}

#endif /* __NUTTX_KTHREAD_H */
