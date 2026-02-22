/*
 * tests/stubs/termios.h
 *
 * Minimal termios stub for host-side compilation.
 * The test suite never calls UART functions, but the driver source
 * includes <termios.h>, so we provide just enough to compile.
 */

#ifndef __TEST_STUBS_TERMIOS_H
#define __TEST_STUBS_TERMIOS_H

#ifdef __APPLE__
/* macOS has a real termios.h — use it */
#include_next <termios.h>
#else
/* Minimal fallback for Linux / other hosts */
#include_next <termios.h>
#endif

/* LD2410 uses B256000 which may not exist on the host.
 * Define it as a harmless value if missing. */
#ifndef B256000
#define B256000 256000
#endif

#ifndef B230400
#define B230400 230400
#endif

#ifndef B460800
#define B460800 460800
#endif

#endif /* __TEST_STUBS_TERMIOS_H */
