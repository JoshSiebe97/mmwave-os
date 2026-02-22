/*
 * Stub sys/ioctl.h additions for host-side testing.
 * macOS/Linux already have sys/ioctl.h, but we need the
 * NuttX-style _IOW/_IOR/_IO macros if they aren't defined.
 */

#ifndef __TEST_STUBS_SYS_IOCTL_H
#define __TEST_STUBS_SYS_IOCTL_H

/* Pull in the real system ioctl first */

#include_next <sys/ioctl.h>

/* NuttX ioctl encoding macros (if not already provided) */

#ifndef _IOC
#define _IOC(dir, type, nr, size) \
  (((dir) << 30) | ((size) << 16) | ((type) << 8) | (nr))
#endif

#ifndef _IO
#define _IO(type, nr)          _IOC(0, (type), (nr), 0)
#endif

#ifndef _IOR
#define _IOR(type, nr, sz)     _IOC(2, (type), (nr), sizeof(sz))
#endif

#ifndef _IOW
#define _IOW(type, nr, sz)     _IOC(1, (type), (nr), sizeof(sz))
#endif

#endif /* __TEST_STUBS_SYS_IOCTL_H */
