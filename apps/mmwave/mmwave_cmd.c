/****************************************************************************
 * apps/mmwave/mmwave_cmd.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NSH command: mmwave
 *
 * Usage:
 *   mmwave              — Print current sensor data
 *   mmwave -w           — Watch mode (continuous output)
 *   mmwave -e [on|off]  — Enable/disable engineering mode
 *   mmwave -s <gate> <motion> <static>  — Set gate sensitivity
 *   mmwave -g <motion_max> <static_max> <timeout>  — Set max gates
 *   mmwave -r           — Restart sensor
 *   mmwave -f           — Factory reset sensor
 *   mmwave -j           — Output as JSON (for scripting)
 *   mmwave -h           — Help
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
#include <sys/ioctl.h>
#include <signal.h>

#include "drivers/mmwave/mmwave_ld2410.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MMWAVE_DEV_PATH   "/dev/mmwave0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_watch_running = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const char *target_state_str(uint8_t state)
{
  switch (state)
    {
      case LD2410_TARGET_NONE:   return "none";
      case LD2410_TARGET_MOTION: return "motion";
      case LD2410_TARGET_STATIC: return "static";
      case LD2410_TARGET_BOTH:   return "motion+static";
      default:                   return "unknown";
    }
}

static void print_data(FAR const struct mmwave_data_s *data, bool json)
{
  if (json)
    {
      printf("{\"state\":\"%s\","
             "\"motion_dist\":%u,"
             "\"motion_energy\":%u,"
             "\"static_dist\":%u,"
             "\"static_energy\":%u,"
             "\"detect_dist\":%u,"
             "\"timestamp\":%lu}\n",
             target_state_str(data->target_state),
             data->motion_distance,
             data->motion_energy,
             data->static_distance,
             data->static_energy,
             data->detection_distance,
             (unsigned long)data->timestamp_ms);
    }
  else
    {
      printf("┌─────────────────────────────────────┐\n");
      printf("│ mmWave LD2410 Sensor Status          │\n");
      printf("├─────────────────────────────────────┤\n");
      printf("│ Presence : %-10s                │\n",
             data->target_state != LD2410_TARGET_NONE ? "YES" : "no");
      printf("│ State    : %-25s │\n",
             target_state_str(data->target_state));
      printf("│ Motion   : %3u%% energy @ %4u cm     │\n",
             data->motion_energy, data->motion_distance);
      printf("│ Static   : %3u%% energy @ %4u cm     │\n",
             data->static_energy, data->static_distance);
      printf("│ Nearest  : %4u cm                    │\n",
             data->detection_distance);
      printf("│ Time     : %lu ms                    │\n",
             (unsigned long)data->timestamp_ms);
      printf("└─────────────────────────────────────┘\n");
    }
}

static void print_eng_data(FAR const struct mmwave_eng_data_s *eng)
{
  print_data(&eng->basic, false);

  printf("\n Gate │ Motion Energy │ Static Energy\n");
  printf("──────┼───────────────┼──────────────\n");
  for (int i = 0; i < LD2410_MAX_GATES; i++)
    {
      printf("  %d   │     %3u       │     %3u\n",
             i,
             eng->motion_gate_energy[i],
             eng->static_gate_energy[i]);
    }
}

static int read_sensor(int fd, FAR struct mmwave_data_s *data,
                       FAR struct mmwave_eng_data_s *eng,
                       bool eng_mode)
{
  ssize_t nread;

  if (eng_mode && eng != NULL)
    {
      nread = read(fd, eng, sizeof(struct mmwave_eng_data_s));
      if (nread == sizeof(struct mmwave_eng_data_s))
        {
          return 1;  /* Engineering data */
        }
    }

  nread = read(fd, data, sizeof(struct mmwave_data_s));
  if (nread == sizeof(struct mmwave_data_s))
    {
      return 0;  /* Basic data */
    }

  return -1;
}

static void print_usage(void)
{
  printf("Usage: mmwave [options]\n\n");
  printf("Options:\n");
  printf("  (none)      Print current sensor reading\n");
  printf("  -w          Watch mode (continuous, Ctrl+C to stop)\n");
  printf("  -e on|off   Enable/disable engineering mode\n");
  printf("  -s G M S    Set gate G sensitivity (motion M, static S)\n");
  printf("  -g M S T    Set max gates (motion M, static S, timeout T sec)\n");
  printf("  -r          Restart the sensor module\n");
  printf("  -f          Factory reset the sensor\n");
  printf("  -j          Output as JSON\n");
  printf("  -h          Show this help\n");
}

static void watch_signal_handler(int signo)
{
  g_watch_running = false;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int fd;
  int ret = 0;

  fd = open(MMWAVE_DEV_PATH, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "mmwave: cannot open %s: %s\n",
              MMWAVE_DEV_PATH, strerror(errno));
      return EXIT_FAILURE;
    }

  if (argc < 2)
    {
      /* Default: print one reading */

      struct mmwave_data_s data;
      if (read_sensor(fd, &data, NULL, false) >= 0)
        {
          print_data(&data, false);
        }
      else
        {
          fprintf(stderr, "mmwave: no data available (sensor warming up?)\n");
          ret = EXIT_FAILURE;
        }

      close(fd);
      return ret;
    }

  /* Parse options */

  int opt;
  bool json_mode = false;

  while ((opt = getopt(argc, argv, "we:s:g:rfjh")) != -1)
    {
      switch (opt)
        {
          case 'w':
            {
              /* Watch mode */

              struct sigaction sa;
              sa.sa_handler = watch_signal_handler;
              sa.sa_flags = 0;
              sigemptyset(&sa.sa_mask);
              sigaction(SIGINT, &sa, NULL);

              g_watch_running = true;
              struct mmwave_data_s data;
              struct mmwave_eng_data_s eng;

              printf("mmwave: watch mode (Ctrl+C to stop)\n\n");

              while (g_watch_running)
                {
                  int r = read_sensor(fd, &data, &eng, false);
                  if (r == 1)
                    {
                      printf("\033[2J\033[H");  /* Clear screen */
                      print_eng_data(&eng);
                    }
                  else if (r == 0)
                    {
                      printf("\033[2J\033[H");
                      print_data(&data, json_mode);
                    }

                  usleep(100000);  /* 100ms refresh */
                }

              printf("\nmmwave: watch stopped\n");
            }
            break;

          case 'e':
            {
              /* Engineering mode toggle */

              int enable = (strcmp(optarg, "on") == 0) ? 1 : 0;
              ret = ioctl(fd, MMWAVE_IOC_ENG_MODE, (unsigned long)enable);
              if (ret < 0)
                {
                  fprintf(stderr, "mmwave: engineering mode failed: %s\n",
                          strerror(errno));
                }
              else
                {
                  printf("mmwave: engineering mode %s\n",
                         enable ? "enabled" : "disabled");
                }
            }
            break;

          case 's':
            {
              /* Set sensitivity: -s gate motion_thresh static_thresh */

              if (optind + 1 >= argc)
                {
                  fprintf(stderr, "mmwave: -s requires gate, motion, "
                          "static args\n");
                  ret = EXIT_FAILURE;
                  break;
                }

              struct mmwave_sensitivity_s sens;
              sens.gate             = (uint8_t)atoi(optarg);
              sens.motion_threshold = (uint8_t)atoi(argv[optind++]);
              sens.static_threshold = (uint8_t)atoi(argv[optind++]);

              ret = ioctl(fd, MMWAVE_IOC_SET_SENSITIVITY,
                          (unsigned long)&sens);
              if (ret < 0)
                {
                  fprintf(stderr, "mmwave: set sensitivity failed: %s\n",
                          strerror(errno));
                }
              else
                {
                  printf("mmwave: gate %u sensitivity set "
                         "(motion=%u, static=%u)\n",
                         sens.gate, sens.motion_threshold,
                         sens.static_threshold);
                }
            }
            break;

          case 'g':
            {
              /* Set max gates: -g motion_max static_max timeout_s */

              if (optind + 1 >= argc)
                {
                  fprintf(stderr, "mmwave: -g requires motion_max, "
                          "static_max, timeout args\n");
                  ret = EXIT_FAILURE;
                  break;
                }

              struct mmwave_maxgate_s mg;
              mg.max_motion_gate = (uint8_t)atoi(optarg);
              mg.max_static_gate = (uint8_t)atoi(argv[optind++]);
              mg.timeout_s       = (uint16_t)atoi(argv[optind++]);

              ret = ioctl(fd, MMWAVE_IOC_SET_MAXGATE,
                          (unsigned long)&mg);
              if (ret < 0)
                {
                  fprintf(stderr, "mmwave: set max gates failed: %s\n",
                          strerror(errno));
                }
              else
                {
                  printf("mmwave: max gates set (motion=%u, static=%u, "
                         "timeout=%us)\n",
                         mg.max_motion_gate, mg.max_static_gate,
                         mg.timeout_s);
                }
            }
            break;

          case 'r':
            {
              ret = ioctl(fd, MMWAVE_IOC_RESTART, 0);
              printf("mmwave: %s\n",
                     ret == 0 ? "sensor restarted" : "restart failed");
            }
            break;

          case 'f':
            {
              printf("mmwave: factory reset... ");
              ret = ioctl(fd, MMWAVE_IOC_FACTORY_RESET, 0);
              printf("%s\n", ret == 0 ? "done" : "failed");
            }
            break;

          case 'j':
            {
              struct mmwave_data_s data;
              if (read_sensor(fd, &data, NULL, false) >= 0)
                {
                  print_data(&data, true);
                }
              else
                {
                  fprintf(stderr,
                          "{\"error\":\"no data available\"}\n");
                  ret = EXIT_FAILURE;
                }
            }
            break;

          case 'h':
          default:
            print_usage();
            break;
        }
    }

  close(fd);
  return ret;
}
