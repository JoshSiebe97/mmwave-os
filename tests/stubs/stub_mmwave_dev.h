/*
 * tests/stubs/stub_mmwave_dev.h
 *
 * Injectable fake mmwave device for app-layer tests.
 *
 * Provides a pre-populated mmwave_data_s that tests can configure,
 * plus helpers to simulate device open/read/ioctl without real hardware.
 */

#ifndef __STUBS_STUB_MMWAVE_DEV_H
#define __STUBS_STUB_MMWAVE_DEV_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "drivers/mmwave/mmwave_ld2410.h"

/* ---- Configurable fake sensor data ---- */

static struct mmwave_data_s g_stub_data;
static bool g_stub_data_valid = true;

/* ---- Ioctl recording ---- */

#define STUB_IOCTL_LOG_SIZE 16

struct stub_ioctl_entry
{
  int cmd;
  unsigned long arg;
};

static struct stub_ioctl_entry g_stub_ioctl_log[STUB_IOCTL_LOG_SIZE];
static int g_stub_ioctl_count = 0;

/* ---- Setup helpers ---- */

static inline void stub_mmwave_reset(void)
{
  memset(&g_stub_data, 0, sizeof(g_stub_data));
  g_stub_data_valid = true;
  g_stub_ioctl_count = 0;
  memset(g_stub_ioctl_log, 0, sizeof(g_stub_ioctl_log));
}

static inline void stub_mmwave_set_presence(uint8_t state,
                                            uint16_t motion_dist,
                                            uint8_t motion_energy,
                                            uint16_t static_dist,
                                            uint8_t static_energy,
                                            uint16_t detect_dist)
{
  g_stub_data.target_state      = state;
  g_stub_data.motion_distance   = motion_dist;
  g_stub_data.motion_energy     = motion_energy;
  g_stub_data.static_distance   = static_dist;
  g_stub_data.static_energy     = static_energy;
  g_stub_data.detection_distance = detect_dist;
  g_stub_data.timestamp_ms      = 12345;
}

/* Simulated read(): copies g_stub_data into caller's buffer */
static inline int stub_mmwave_read(void *buf, size_t buflen)
{
  if (!g_stub_data_valid)
    {
      return -1;  /* EAGAIN */
    }

  if (buflen < sizeof(struct mmwave_data_s))
    {
      return -1;
    }

  memcpy(buf, &g_stub_data, sizeof(struct mmwave_data_s));
  return (int)sizeof(struct mmwave_data_s);
}

/* Simulated ioctl(): records the call */
static inline int stub_mmwave_ioctl(int cmd, unsigned long arg)
{
  if (g_stub_ioctl_count < STUB_IOCTL_LOG_SIZE)
    {
      g_stub_ioctl_log[g_stub_ioctl_count].cmd = cmd;
      g_stub_ioctl_log[g_stub_ioctl_count].arg = arg;
      g_stub_ioctl_count++;
    }

  return 0;
}

#endif /* __STUBS_STUB_MMWAVE_DEV_H */
