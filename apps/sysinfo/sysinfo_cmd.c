/****************************************************************************
 * apps/sysinfo/sysinfo_cmd.c
 *
 * SPDX-License-Identifier: MIT
 *
 * NSH command: sysinfo — System information dashboard
 *
 * Usage:
 *   sysinfo          — Print full system status
 *   sysinfo -m       — Memory only
 *   sysinfo -j       — JSON output
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <nuttx/clock.h>

#include "drivers/mmwave/mmwave_ld2410.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void print_uptime(void)
{
  uint32_t ticks = clock_systime_ticks();
  uint32_t secs  = ticks / TICK_PER_SEC;
  uint32_t mins  = secs / 60;
  uint32_t hours = mins / 60;
  uint32_t days  = hours / 24;

  printf("  Uptime   : ");
  if (days > 0)
    {
      printf("%lud %luh %lum %lus\n",
             (unsigned long)days,
             (unsigned long)(hours % 24),
             (unsigned long)(mins % 60),
             (unsigned long)(secs % 60));
    }
  else if (hours > 0)
    {
      printf("%luh %lum %lus\n",
             (unsigned long)hours,
             (unsigned long)(mins % 60),
             (unsigned long)(secs % 60));
    }
  else
    {
      printf("%lum %lus\n",
             (unsigned long)mins,
             (unsigned long)(secs % 60));
    }
}

static void print_memory(void)
{
  struct mallinfo info = mallinfo();

  printf("  Heap total : %d bytes\n", info.arena);
  printf("  Heap used  : %d bytes\n", info.uordblks);
  printf("  Heap free  : %d bytes\n", info.fordblks);
  printf("  Heap frag  : %d blocks\n", info.ordblks);

  if (info.arena > 0)
    {
      int pct = (info.uordblks * 100) / info.arena;
      printf("  Usage      : %d%%\n", pct);

      /* Visual bar */

      printf("  [");
      for (int i = 0; i < 40; i++)
        {
          printf("%c", i < (pct * 40 / 100) ? '#' : '.');
        }

      printf("]\n");
    }
}

static void print_mmwave_stats(void)
{
  int fd = open("/dev/mmwave0", O_RDONLY);
  if (fd < 0)
    {
      printf("  Radar      : not available\n");
      return;
    }

  struct mmwave_data_s data;
  ssize_t nread = read(fd, &data, sizeof(data));
  close(fd);

  if (nread == sizeof(data))
    {
      printf("  Radar      : active\n");
      printf("  Presence   : %s\n",
             data.target_state != LD2410_TARGET_NONE ? "YES" : "no");
    }
  else
    {
      printf("  Radar      : warming up\n");
    }
}

static void print_json(void)
{
  struct mallinfo info = mallinfo();
  uint32_t secs = clock_systime_ticks() / TICK_PER_SEC;

  printf("{\"uptime_s\":%lu,", (unsigned long)secs);
  printf("\"heap_total\":%d,", info.arena);
  printf("\"heap_used\":%d,", info.uordblks);
  printf("\"heap_free\":%d", info.fordblks);

  int fd = open("/dev/mmwave0", O_RDONLY);
  if (fd >= 0)
    {
      struct mmwave_data_s data;
      if (read(fd, &data, sizeof(data)) == sizeof(data))
        {
          printf(",\"radar_active\":true");
          printf(",\"presence\":%s",
                 data.target_state != LD2410_TARGET_NONE ? "true" : "false");
        }
      close(fd);
    }

  printf("}\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc > 1 && strcmp(argv[1], "-j") == 0)
    {
      print_json();
      return OK;
    }

  if (argc > 1 && strcmp(argv[1], "-m") == 0)
    {
      printf("Memory\n");
      printf("──────\n");
      print_memory();
      return OK;
    }

  printf("╔═══════════════════════════════════╗\n");
  printf("║     mmWave OS — System Info       ║\n");
  printf("╠═══════════════════════════════════╣\n");
  printf("║ Platform                          ║\n");
  printf("╟───────────────────────────────────╢\n");

  printf("  Board    : ESP32-C6 DevKitC\n");
  printf("  OS       : NuttX " CONFIG_VERSION_STRING "\n");
  printf("  CPU      : RISC-V @ 160MHz\n");
  print_uptime();

  printf("╟───────────────────────────────────╢\n");
  printf("║ Memory                            ║\n");
  printf("╟───────────────────────────────────╢\n");
  print_memory();

  printf("╟───────────────────────────────────╢\n");
  printf("║ Sensors                           ║\n");
  printf("╟───────────────────────────────────╢\n");
  print_mmwave_stats();

  printf("╚═══════════════════════════════════╝\n");

  return OK;
}
