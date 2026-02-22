/*
 * apps/hactl/ha_format.h
 *
 * Pure-function JSON body builder for Home Assistant state updates.
 * Extracted from hactl_cmd.c so it can be unit-tested without sockets.
 */

#ifndef __APPS_HACTL_HA_FORMAT_H
#define __APPS_HACTL_HA_FORMAT_H

#include <stdio.h>
#include <stdint.h>

/* Use the driver header only for the data struct and target constants.
 * The driver header references sem_t in its internal struct, so pull
 * in the semaphore header first (stubs provide a no-op version). */
#include <nuttx/semaphore.h>
#include "drivers/mmwave/mmwave_ld2410.h"

/*
 * Build the JSON body for a Home Assistant POST /api/states/<entity>
 *
 * Returns the number of bytes written (excluding NUL), or -1 on truncation.
 *
 * Example output:
 *   {"state":"on","attributes":{"friendly_name":"mmWave Presence",
 *    "device_class":"occupancy","motion_energy":80,...}}
 */
static inline int ha_format_state_json(char *buf, size_t bufsize,
                                       const struct mmwave_data_s *data)
{
  const char *state = (data->target_state != LD2410_TARGET_NONE)
                      ? "on" : "off";

  int n = snprintf(buf, bufsize,
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

  if (n < 0 || (size_t)n >= bufsize)
    {
      return -1;
    }

  return n;
}

/*
 * Build the HTTP request line + headers for HA POST.
 * Writes to buf, returns bytes written or -1 on truncation.
 */
static inline int ha_format_http_request(char *buf, size_t bufsize,
                                         const char *entity_id,
                                         const char *host,
                                         uint16_t port,
                                         const char *token,
                                         const char *json_body,
                                         int body_len)
{
  int n = snprintf(buf, bufsize,
    "POST /api/states/%s HTTP/1.1\r\n"
    "Host: %s:%u\r\n"
    "Authorization: Bearer %s\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n"
    "\r\n"
    "%s",
    entity_id,
    host, port,
    token,
    body_len,
    json_body);

  if (n < 0 || (size_t)n >= bufsize)
    {
      return -1;
    }

  return n;
}

#endif /* __APPS_HACTL_HA_FORMAT_H */
