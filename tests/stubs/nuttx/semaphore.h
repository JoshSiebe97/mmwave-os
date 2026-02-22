/*
 * Stub nuttx/semaphore.h for host-side testing.
 * Semaphores are no-ops in single-threaded host tests.
 */

#ifndef __NUTTX_SEMAPHORE_H
#define __NUTTX_SEMAPHORE_H

typedef struct { int count; } sem_t;

static inline int nxsem_init(sem_t *sem, int pshared, unsigned int value)
{
  (void)pshared;
  sem->count = (int)value;
  return 0;
}

static inline int nxsem_destroy(sem_t *sem)
{
  (void)sem;
  return 0;
}

static inline int nxsem_wait(sem_t *sem)
{
  sem->count--;
  return 0;
}

static inline int nxsem_post(sem_t *sem)
{
  sem->count++;
  return 0;
}

#endif /* __NUTTX_SEMAPHORE_H */
