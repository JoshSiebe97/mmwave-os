/****************************************************************************
 * apps/config/config_cmd.c
 *
 * SPDX-License-Identifier: MIT
 *
 * NSH command: config — Persistent configuration manager
 *
 * Usage:
 *   config list                — List all config keys
 *   config get <key>           — Get a config value
 *   config set <key> <value>   — Set a config value
 *   config delete <key>        — Delete a config key
 *   config reset               — Reset all configuration to defaults
 *
 * Config is stored in LittleFS at /config/ as individual files per key.
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CONFIG_BASE_PATH    "/config"
#define CONFIG_MAX_KEY_LEN  64
#define CONFIG_MAX_VAL_LEN  256

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void make_path(char *buf, size_t buflen, const char *key)
{
  snprintf(buf, buflen, "%s/%s", CONFIG_BASE_PATH, key);
}

static int config_list(void)
{
  DIR *dirp = opendir(CONFIG_BASE_PATH);
  if (dirp == NULL)
    {
      fprintf(stderr, "config: cannot open %s: %s\n",
              CONFIG_BASE_PATH, strerror(errno));
      return EXIT_FAILURE;
    }

  struct dirent *entry;
  int count = 0;

  printf("Configuration keys (%s):\n", CONFIG_BASE_PATH);
  printf("────────────────────────────\n");

  while ((entry = readdir(dirp)) != NULL)
    {
      /* Skip . and .. */

      if (entry->d_name[0] == '.') continue;

      /* Read value for display */

      char path[128];
      make_path(path, sizeof(path), entry->d_name);

      char val[CONFIG_MAX_VAL_LEN];
      int fd = open(path, O_RDONLY);
      if (fd >= 0)
        {
          ssize_t n = read(fd, val, sizeof(val) - 1);
          close(fd);
          if (n > 0)
            {
              val[n] = '\0';
              printf("  %-24s = %s\n", entry->d_name, val);
            }
          else
            {
              printf("  %-24s = (empty)\n", entry->d_name);
            }
        }
      else
        {
          printf("  %-24s = (unreadable)\n", entry->d_name);
        }

      count++;
    }

  closedir(dirp);

  if (count == 0)
    {
      printf("  (no configuration set)\n");
    }

  return OK;
}

static int config_get(const char *key)
{
  char path[128];
  make_path(path, sizeof(path), key);

  int fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "config: key '%s' not found\n", key);
      return EXIT_FAILURE;
    }

  char val[CONFIG_MAX_VAL_LEN];
  ssize_t n = read(fd, val, sizeof(val) - 1);
  close(fd);

  if (n >= 0)
    {
      val[n] = '\0';
      printf("%s\n", val);
    }

  return OK;
}

static int config_set(const char *key, const char *value)
{
  char path[128];
  make_path(path, sizeof(path), key);

  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      fprintf(stderr, "config: cannot write '%s': %s\n",
              key, strerror(errno));
      return EXIT_FAILURE;
    }

  size_t vlen = strlen(value);
  ssize_t written = write(fd, value, vlen);
  close(fd);

  if (written != (ssize_t)vlen)
    {
      fprintf(stderr, "config: write error\n");
      return EXIT_FAILURE;
    }

  printf("config: %s = %s\n", key, value);
  return OK;
}

static int config_delete(const char *key)
{
  char path[128];
  make_path(path, sizeof(path), key);

  if (unlink(path) < 0)
    {
      fprintf(stderr, "config: cannot delete '%s': %s\n",
              key, strerror(errno));
      return EXIT_FAILURE;
    }

  printf("config: '%s' deleted\n", key);
  return OK;
}

static int config_reset(void)
{
  DIR *dirp = opendir(CONFIG_BASE_PATH);
  if (dirp == NULL)
    {
      return OK;  /* Nothing to reset */
    }

  struct dirent *entry;
  char path[128];

  while ((entry = readdir(dirp)) != NULL)
    {
      if (entry->d_name[0] == '.') continue;
      make_path(path, sizeof(path), entry->d_name);
      unlink(path);
    }

  closedir(dirp);

  /* Write default config values */

  config_set("wifi.ssid", "");
  config_set("wifi.psk", "");
  config_set("ha.url", "");
  config_set("ha.port", "8123");
  config_set("ha.token", "");
  config_set("mmwave.uart", "/dev/ttyS1");
  config_set("mmwave.baud", "256000");
  config_set("boot.autostart_ha", "0");
  config_set("boot.autostart_wifi", "1");

  printf("config: reset to defaults\n");
  return OK;
}

static void print_usage(void)
{
  printf("Usage: config <command> [args]\n\n");
  printf("Commands:\n");
  printf("  list               List all config keys\n");
  printf("  get <key>          Get a value\n");
  printf("  set <key> <value>  Set a value\n");
  printf("  delete <key>       Delete a key\n");
  printf("  reset              Reset all to defaults\n");
  printf("\nStandard keys:\n");
  printf("  wifi.ssid          Wi-Fi network name\n");
  printf("  wifi.psk           Wi-Fi password\n");
  printf("  ha.url             Home Assistant URL/IP\n");
  printf("  ha.port            Home Assistant port (8123)\n");
  printf("  ha.token           HA long-lived access token\n");
  printf("  mmwave.uart        Sensor UART path (/dev/ttyS1)\n");
  printf("  mmwave.baud        Sensor baud rate (256000)\n");
  printf("  boot.autostart_ha  Auto-start HA reporting (0/1)\n");
  printf("  boot.autostart_wifi Auto-start Wi-Fi (0/1)\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      return config_list();
    }

  const char *cmd = argv[1];

  if (strcmp(cmd, "list") == 0)
    {
      return config_list();
    }
  else if (strcmp(cmd, "get") == 0)
    {
      if (argc < 3)
        {
          fprintf(stderr, "config: usage: config get <key>\n");
          return EXIT_FAILURE;
        }

      return config_get(argv[2]);
    }
  else if (strcmp(cmd, "set") == 0)
    {
      if (argc < 4)
        {
          fprintf(stderr, "config: usage: config set <key> <value>\n");
          return EXIT_FAILURE;
        }

      return config_set(argv[2], argv[3]);
    }
  else if (strcmp(cmd, "delete") == 0)
    {
      if (argc < 3)
        {
          fprintf(stderr, "config: usage: config delete <key>\n");
          return EXIT_FAILURE;
        }

      return config_delete(argv[2]);
    }
  else if (strcmp(cmd, "reset") == 0)
    {
      return config_reset();
    }
  else
    {
      print_usage();
    }

  return OK;
}
