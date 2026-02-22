/*
 * Stub nuttx/kmalloc.h for host-side testing.
 */

#ifndef __NUTTX_KMALLOC_H
#define __NUTTX_KMALLOC_H

#include <stdlib.h>
#include <string.h>

#define FAR

static inline void *kmm_zalloc(size_t size)
{
  void *p = malloc(size);
  if (p) memset(p, 0, size);
  return p;
}

static inline void kmm_free(void *p)
{
  free(p);
}

#endif /* __NUTTX_KMALLOC_H */
