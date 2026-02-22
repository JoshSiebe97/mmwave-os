/****************************************************************************
 * boards/esp32c6/src/mmwave_bringup.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board-specific initialization for mmWave OS on ESP32-C6.
 * Called from NuttX board_late_initialize() or nsh_archinitialize().
 *
 * Boot sequence:
 *   1. Mount LittleFS at /config
 *   2. Register mmWave LD2410 driver at /dev/mmwave0
 *   3. (Wi-Fi and HA started later from init script)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <debug.h>
#include <sys/mount.h>

#include <nuttx/board.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/fs.h>

#ifdef CONFIG_MMWAVE_LD2410
#include "drivers/mmwave/mmwave_ld2410.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CONFIG_MOUNT_POINT    "/config"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mmwave_bringup
 *
 * Description:
 *   Perform board-specific initialization for mmWave OS.
 *   Called after basic NuttX kernel init is complete.
 *
 ****************************************************************************/

int mmwave_bringup(void)
{
  int ret;

  syslog(LOG_INFO, "mmWave OS: starting board bringup\n");

  /* ─── Step 1: Mount LittleFS for persistent configuration ─── */

#ifdef CONFIG_FS_LITTLEFS
  {
    syslog(LOG_INFO, "mmWave OS: mounting LittleFS at %s\n",
           CONFIG_MOUNT_POINT);

    /* Get the MTD partition for our storage area */

    FAR struct mtd_dev_s *mtd = NULL;

#ifdef CONFIG_ESP32C6_SPIFLASH
    /* The ESP32-C6 flash MTD is initialized by the chip-level code.
     * We access our storage partition via the registered MTD device.
     * Typically: /dev/esp-storage or an MTD registered by the partition
     * table. Here we use the platform-provided API.
     */

    extern FAR struct mtd_dev_s *esp32c6_get_storage_mtd(void);
    mtd = esp32c6_get_storage_mtd();
#endif

    if (mtd != NULL)
      {
        ret = mount(NULL, CONFIG_MOUNT_POINT, "littlefs", 0,
                    (FAR void *)mtd);
        if (ret < 0)
          {
            syslog(LOG_WARNING,
                   "mmWave OS: LittleFS mount failed (%d), formatting...\n",
                   ret);

            /* Format and retry */

            ret = mount(NULL, CONFIG_MOUNT_POINT, "littlefs", 0,
                        "forceformat");
            if (ret < 0)
              {
                syslog(LOG_ERR,
                       "mmWave OS: LittleFS format+mount failed: %d\n",
                       ret);
              }
          }

        if (ret == OK)
          {
            syslog(LOG_INFO, "mmWave OS: /config mounted OK\n");
          }
      }
    else
      {
        syslog(LOG_WARNING,
               "mmWave OS: no storage MTD available, /config disabled\n");
      }
  }
#endif /* CONFIG_FS_LITTLEFS */

  /* ─── Step 2: Register mmWave LD2410 driver ─── */

#ifdef CONFIG_MMWAVE_LD2410
  {
    syslog(LOG_INFO, "mmWave OS: registering LD2410 driver\n");

    ret = mmwave_ld2410_register(
            CONFIG_MMWAVE_LD2410_DEVPATH,
            CONFIG_MMWAVE_LD2410_UART_PATH,
            CONFIG_MMWAVE_LD2410_BAUD);

    if (ret < 0)
      {
        syslog(LOG_ERR,
               "mmWave OS: LD2410 registration failed: %d\n", ret);
      }
    else
      {
        syslog(LOG_INFO,
               "mmWave OS: LD2410 ready at %s (UART: %s @ %d baud)\n",
               CONFIG_MMWAVE_LD2410_DEVPATH,
               CONFIG_MMWAVE_LD2410_UART_PATH,
               CONFIG_MMWAVE_LD2410_BAUD);
      }
  }
#endif /* CONFIG_MMWAVE_LD2410 */

  /* ─── Step 3: Mount procfs ─── */

#ifdef CONFIG_FS_PROCFS
  {
    ret = mount(NULL, "/proc", "procfs", 0, NULL);
    if (ret < 0)
      {
        syslog(LOG_WARNING, "mmWave OS: procfs mount failed: %d\n", ret);
      }
  }
#endif

  syslog(LOG_INFO, "mmWave OS: bringup complete\n");

  return OK;
}

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   Called by NuttX after basic initialization is complete.
 *   This is where we hook our custom bringup.
 *
 ****************************************************************************/

#ifndef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  mmwave_bringup();
}
#endif

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Called before NSH starts. Alternative hook point.
 *
 ****************************************************************************/

#ifdef CONFIG_NSH_ARCHINIT
int board_app_initialize(uintptr_t arg)
{
  return mmwave_bringup();
}
#endif
