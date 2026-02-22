/****************************************************************************
 * apps/hactl/hactl_cmd.c
 *
 * SPDX-License-Identifier: MIT
 *
 * NSH command: hactl — Home Assistant control & reporting
 *
 * Usage:
 *   hactl status              — Show connection status
 *   hactl push                — Manually push current sensor state
 *   hactl config <url> <token> — Set HA URL and long-lived access token
 *   hactl start               — Start auto-reporting background task
 *   hactl stop                — Stop auto-reporting
 *   hactl test                — Test connectivity to HA
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "drivers/mmwave/mmwave_ld2410.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HA_CONFIG_FILE          "/config/ha.conf"
#define HA_ENTITY_ID            "binary_sensor.mmwave_presence"
#define HA_DEFAULT_PORT         8123
#define HA_MAX_URL_LEN          128
#define HA_MAX_TOKEN_LEN        256
#define HA_HTTP_BUF_SIZE        512
#define MMWAVE_DEV_PATH         "/dev/mmwave0"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct ha_config_s
{
  char     url[HA_MAX_URL_LEN];      /* e.g., "192.168.1.100" */
  uint16_t port;
  char     token[HA_MAX_TOKEN_LEN];  /* Long-lived access token */
  bool     auto_report;              /* Auto-reporting enabled */
  uint16_t report_interval_ms;       /* Min interval between reports */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct ha_config_s g_ha_config;
static volatile bool g_reporting = false;
static pid_t g_report_pid = -1;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * Load HA config from persistent storage.
 */

static int ha_load_config(void)
{
  FILE *f = fopen(HA_CONFIG_FILE, "r");
  if (f == NULL)
    {
      /* Defaults */

      memset(&g_ha_config, 0, sizeof(g_ha_config));
      g_ha_config.port = HA_DEFAULT_PORT;
      g_ha_config.report_interval_ms = 500;
      return -ENOENT;
    }

  char line[384];
  while (fgets(line, sizeof(line), f) != NULL)
    {
      char *eq = strchr(line, '=');
      if (eq == NULL) continue;
      *eq = '\0';
      char *val = eq + 1;

      /* Trim trailing newline */

      size_t vlen = strlen(val);
      if (vlen > 0 && val[vlen - 1] == '\n') val[vlen - 1] = '\0';

      if (strcmp(line, "url") == 0)
        {
          strncpy(g_ha_config.url, val, HA_MAX_URL_LEN - 1);
        }
      else if (strcmp(line, "port") == 0)
        {
          g_ha_config.port = (uint16_t)atoi(val);
        }
      else if (strcmp(line, "token") == 0)
        {
          strncpy(g_ha_config.token, val, HA_MAX_TOKEN_LEN - 1);
        }
      else if (strcmp(line, "interval") == 0)
        {
          g_ha_config.report_interval_ms = (uint16_t)atoi(val);
        }
    }

  fclose(f);
  return OK;
}

/**
 * Save HA config to persistent storage.
 */

static int ha_save_config(void)
{
  FILE *f = fopen(HA_CONFIG_FILE, "w");
  if (f == NULL)
    {
      return -errno;
    }

  fprintf(f, "url=%s\n", g_ha_config.url);
  fprintf(f, "port=%u\n", g_ha_config.port);
  fprintf(f, "token=%s\n", g_ha_config.token);
  fprintf(f, "interval=%u\n", g_ha_config.report_interval_ms);
  fclose(f);
  return OK;
}

/**
 * Send an HTTP POST to Home Assistant REST API to update entity state.
 *
 * Endpoint: POST /api/states/<entity_id>
 * Headers:  Authorization: Bearer <token>
 *           Content-Type: application/json
 * Body:     {"state": "on|off", "attributes": {...}}
 */

static int ha_post_state(FAR const struct mmwave_data_s *data)
{
  struct sockaddr_in server;
  char http_buf[HA_HTTP_BUF_SIZE];
  int sockfd;
  int ret;

  if (g_ha_config.url[0] == '\0' || g_ha_config.token[0] == '\0')
    {
      return -EINVAL;
    }

  /* Create socket */

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      return -errno;
    }

  /* Connect to HA */

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(g_ha_config.port);

  ret = inet_pton(AF_INET, g_ha_config.url, &server.sin_addr);
  if (ret <= 0)
    {
      /* Try DNS resolution */

      FAR struct hostent *he = gethostbyname(g_ha_config.url);
      if (he == NULL)
        {
          close(sockfd);
          return -ENOENT;
        }

      memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
    }

  ret = connect(sockfd, (FAR struct sockaddr *)&server, sizeof(server));
  if (ret < 0)
    {
      close(sockfd);
      return -errno;
    }

  /* Build JSON body */

  const char *state = (data->target_state != LD2410_TARGET_NONE)
                      ? "on" : "off";

  char body[256];
  int bodylen = snprintf(body, sizeof(body),
    "{\"state\":\"%s\","
    "\"attributes\":{"
    "\"friendly_name\":\"mmWave Presence\","
    "\"device_class\":\"occupancy\","
    "\"motion_energy\":%u,"
    "\"static_energy\":%u,"
    "\"motion_distance\":%u,"
    "\"static_distance\":%u,"
    "\"detection_distance\":%u"
    "}}",
    state,
    data->motion_energy,
    data->static_energy,
    data->motion_distance,
    data->static_distance,
    data->detection_distance);

  /* Build HTTP request */

  int httplen = snprintf(http_buf, sizeof(http_buf),
    "POST /api/states/%s HTTP/1.1\r\n"
    "Host: %s:%u\r\n"
    "Authorization: Bearer %s\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n"
    "\r\n"
    "%s",
    HA_ENTITY_ID,
    g_ha_config.url, g_ha_config.port,
    g_ha_config.token,
    bodylen,
    body);

  /* Send */

  ssize_t sent = send(sockfd, http_buf, httplen, 0);
  if (sent != httplen)
    {
      close(sockfd);
      return -EIO;
    }

  /* Read response (just check status line) */

  ssize_t nread = recv(sockfd, http_buf, sizeof(http_buf) - 1, 0);
  close(sockfd);

  if (nread > 0)
    {
      http_buf[nread] = '\0';

      /* Check for 200 or 201 status */

      if (strstr(http_buf, "200") != NULL ||
          strstr(http_buf, "201") != NULL)
        {
          return OK;
        }
    }

  return -EIO;
}

/**
 * Background auto-reporting task.
 * Reads mmWave data and pushes to HA whenever state changes.
 */

static int ha_report_task(int argc, FAR char *argv[])
{
  int fd;
  struct mmwave_data_s data;
  uint8_t prev_state = 0xFF;  /* Force initial report */

  fd = open(MMWAVE_DEV_PATH, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "hactl: cannot open sensor\n");
      return EXIT_FAILURE;
    }

  printf("hactl: auto-reporting started → %s:%u\n",
         g_ha_config.url, g_ha_config.port);

  g_reporting = true;

  while (g_reporting)
    {
      ssize_t nread = read(fd, &data, sizeof(data));
      if (nread == sizeof(data))
        {
          /* Report on state change OR every 30 seconds */

          if (data.target_state != prev_state)
            {
              int ret = ha_post_state(&data);
              if (ret == OK)
                {
                  prev_state = data.target_state;
                }
              else
                {
                  fprintf(stderr, "hactl: push failed (%d), retrying...\n",
                          ret);
                }
            }
        }

      usleep(g_ha_config.report_interval_ms * 1000);
    }

  close(fd);
  printf("hactl: auto-reporting stopped\n");
  return OK;
}

static void print_status(void)
{
  printf("Home Assistant Connection\n");
  printf("─────────────────────────\n");
  printf("  URL      : %s\n",
         g_ha_config.url[0] ? g_ha_config.url : "(not set)");
  printf("  Port     : %u\n", g_ha_config.port);
  printf("  Token    : %s\n",
         g_ha_config.token[0] ? "***configured***" : "(not set)");
  printf("  Entity   : %s\n", HA_ENTITY_ID);
  printf("  Reporting: %s\n", g_reporting ? "ACTIVE" : "stopped");
  printf("  Interval : %u ms\n", g_ha_config.report_interval_ms);
}

static void print_usage(void)
{
  printf("Usage: hactl <command>\n\n");
  printf("Commands:\n");
  printf("  status                Show connection status\n");
  printf("  config <url> <token>  Set HA URL/IP and access token\n");
  printf("  push                  Manually push current state to HA\n");
  printf("  start                 Start auto-reporting task\n");
  printf("  stop                  Stop auto-reporting task\n");
  printf("  test                  Test connectivity to HA\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  ha_load_config();

  if (argc < 2)
    {
      print_status();
      return OK;
    }

  const char *cmd = argv[1];

  if (strcmp(cmd, "status") == 0)
    {
      print_status();
    }
  else if (strcmp(cmd, "config") == 0)
    {
      if (argc < 4)
        {
          fprintf(stderr, "hactl: usage: hactl config <url|ip> <token>\n");
          return EXIT_FAILURE;
        }

      strncpy(g_ha_config.url, argv[2], HA_MAX_URL_LEN - 1);
      strncpy(g_ha_config.token, argv[3], HA_MAX_TOKEN_LEN - 1);

      int ret = ha_save_config();
      if (ret == OK)
        {
          printf("hactl: config saved to %s\n", HA_CONFIG_FILE);
        }
      else
        {
          fprintf(stderr, "hactl: save failed: %d\n", ret);
          return EXIT_FAILURE;
        }
    }
  else if (strcmp(cmd, "push") == 0)
    {
      int fd = open(MMWAVE_DEV_PATH, O_RDONLY);
      if (fd < 0)
        {
          fprintf(stderr, "hactl: cannot open sensor\n");
          return EXIT_FAILURE;
        }

      struct mmwave_data_s data;
      ssize_t nread = read(fd, &data, sizeof(data));
      close(fd);

      if (nread != sizeof(data))
        {
          fprintf(stderr, "hactl: no sensor data\n");
          return EXIT_FAILURE;
        }

      printf("hactl: pushing state '%s' to HA... ",
             data.target_state != LD2410_TARGET_NONE ? "on" : "off");

      int ret = ha_post_state(&data);
      printf("%s\n", ret == OK ? "ok" : "FAILED");
      return ret == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "start") == 0)
    {
      if (g_reporting)
        {
          printf("hactl: already reporting\n");
          return OK;
        }

      if (g_ha_config.url[0] == '\0' || g_ha_config.token[0] == '\0')
        {
          fprintf(stderr, "hactl: run 'hactl config <url> <token>' first\n");
          return EXIT_FAILURE;
        }

      /* Start background reporting task via task_create */

      g_report_pid = task_create("ha_report",
                                 100,    /* priority */
                                 2048,   /* stack */
                                 ha_report_task,
                                 NULL);
      if (g_report_pid < 0)
        {
          fprintf(stderr, "hactl: failed to start task\n");
          return EXIT_FAILURE;
        }
    }
  else if (strcmp(cmd, "stop") == 0)
    {
      g_reporting = false;
      printf("hactl: stopping...\n");
    }
  else if (strcmp(cmd, "test") == 0)
    {
      printf("hactl: testing connection to %s:%u... ",
             g_ha_config.url, g_ha_config.port);

      struct sockaddr_in server;
      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd < 0)
        {
          printf("FAILED (socket)\n");
          return EXIT_FAILURE;
        }

      memset(&server, 0, sizeof(server));
      server.sin_family = AF_INET;
      server.sin_port = htons(g_ha_config.port);
      inet_pton(AF_INET, g_ha_config.url, &server.sin_addr);

      int ret = connect(sockfd, (FAR struct sockaddr *)&server,
                        sizeof(server));
      close(sockfd);

      printf("%s\n", ret == 0 ? "OK" : "FAILED");
      return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else
    {
      print_usage();
    }

  return OK;
}
