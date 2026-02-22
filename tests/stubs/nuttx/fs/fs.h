/*
 * Stub nuttx/fs/fs.h for host-side testing.
 */

#ifndef __NUTTX_FS_FS_H
#define __NUTTX_FS_FS_H

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

/* Minimal file struct (never actually used in tests) */

struct file
{
  int f_oflags;
  off_t f_pos;
  void *f_priv;
};

/* NuttX file_operations — the driver initializes one of these.
 * We only need the shape; the function pointers are never called in tests. */

typedef ssize_t (*read_fn_t)(void *, char *, size_t);

struct file_operations
{
  int     (*open)(struct file *filep);
  int     (*close)(struct file *filep);
  ssize_t (*read)(struct file *filep, char *buffer, size_t buflen);
  ssize_t (*write)(struct file *filep, const char *buffer, size_t buflen);
  off_t   (*seek)(struct file *filep, off_t offset, int whence);
  int     (*ioctl)(struct file *filep, int cmd, unsigned long arg);
  int     (*mmap)(struct file *filep, void *map);
  int     (*truncate)(struct file *filep, off_t length);
  int     (*poll)(struct file *filep, void *setup, bool teardown);
};

/* Stubs for driver registration */

static inline int register_driver(const char *path, const void *fops,
                                  mode_t mode, void *priv)
{
  (void)path; (void)fops; (void)mode; (void)priv;
  return 0;
}

static inline int unregister_driver(const char *path)
{
  (void)path;
  return 0;
}

#endif /* __NUTTX_FS_FS_H */
