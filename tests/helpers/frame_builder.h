/*
 * tests/helpers/frame_builder.h
 *
 * Utilities for constructing LD2410 binary frames in tests.
 */

#ifndef __TESTS_HELPERS_FRAME_BUILDER_H
#define __TESTS_HELPERS_FRAME_BUILDER_H

#include <stdint.h>
#include <string.h>

/*
 * Maximum frame size. Same as driver limit.
 */
#define FRAME_BUF_SIZE 64

/*
 * Build a standard LD2410 data frame (type 0x02).
 *
 * Returns total frame length in bytes.
 * The caller provides a buffer of at least FRAME_BUF_SIZE bytes.
 */
static inline int build_data_frame(uint8_t *buf,
                                   uint8_t target_state,
                                   uint16_t motion_dist,
                                   uint8_t motion_energy,
                                   uint16_t static_dist,
                                   uint8_t static_energy,
                                   uint16_t detect_dist)
{
  int pos = 0;

  /* Header: F1 F2 F3 F4 (little-endian 0xF4F3F2F1) */
  buf[pos++] = 0xF1;
  buf[pos++] = 0xF2;
  buf[pos++] = 0xF3;
  buf[pos++] = 0xF4;

  /* Payload: type(1) + head(1) + state(1) + motion_dist(2) +
   *          motion_energy(1) + static_dist(2) + static_energy(1) +
   *          detect_dist(2) = 11 bytes */
  uint16_t payload_len = 11;

  /* Length (little-endian) */
  buf[pos++] = (uint8_t)(payload_len & 0xFF);
  buf[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);

  /* Payload */
  buf[pos++] = 0x02;  /* data type: standard */
  buf[pos++] = 0xAA;  /* head marker */
  buf[pos++] = target_state;

  buf[pos++] = (uint8_t)(motion_dist & 0xFF);
  buf[pos++] = (uint8_t)((motion_dist >> 8) & 0xFF);
  buf[pos++] = motion_energy;

  buf[pos++] = (uint8_t)(static_dist & 0xFF);
  buf[pos++] = (uint8_t)((static_dist >> 8) & 0xFF);
  buf[pos++] = static_energy;

  buf[pos++] = (uint8_t)(detect_dist & 0xFF);
  buf[pos++] = (uint8_t)((detect_dist >> 8) & 0xFF);

  /* Tail: F5 F6 F7 F8 (little-endian 0xF8F7F6F5) */
  buf[pos++] = 0xF5;
  buf[pos++] = 0xF6;
  buf[pos++] = 0xF7;
  buf[pos++] = 0xF8;

  return pos;  /* total frame length */
}

/*
 * Build an engineering mode data frame (type 0x01).
 *
 * Includes per-gate energy arrays (9 motion + 9 static).
 * Returns total frame length in bytes.
 */
static inline int build_eng_frame(uint8_t *buf,
                                  uint8_t target_state,
                                  uint16_t motion_dist,
                                  uint8_t motion_energy,
                                  uint16_t static_dist,
                                  uint8_t static_energy,
                                  uint16_t detect_dist,
                                  const uint8_t motion_gates[9],
                                  const uint8_t static_gates[9])
{
  int pos = 0;

  /* Header */
  buf[pos++] = 0xF1;
  buf[pos++] = 0xF2;
  buf[pos++] = 0xF3;
  buf[pos++] = 0xF4;

  /* Payload = 11 (basic) + 9 (motion gates) + 9 (static gates) = 29 */
  uint16_t payload_len = 29;

  buf[pos++] = (uint8_t)(payload_len & 0xFF);
  buf[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);

  /* Basic payload (same layout as standard, but type 0x01) */
  buf[pos++] = 0x01;  /* engineering mode */
  buf[pos++] = 0xAA;
  buf[pos++] = target_state;
  buf[pos++] = (uint8_t)(motion_dist & 0xFF);
  buf[pos++] = (uint8_t)((motion_dist >> 8) & 0xFF);
  buf[pos++] = motion_energy;
  buf[pos++] = (uint8_t)(static_dist & 0xFF);
  buf[pos++] = (uint8_t)((static_dist >> 8) & 0xFF);
  buf[pos++] = static_energy;
  buf[pos++] = (uint8_t)(detect_dist & 0xFF);
  buf[pos++] = (uint8_t)((detect_dist >> 8) & 0xFF);

  /* Per-gate motion energy */
  memcpy(&buf[pos], motion_gates, 9);
  pos += 9;

  /* Per-gate static energy */
  memcpy(&buf[pos], static_gates, 9);
  pos += 9;

  /* Tail */
  buf[pos++] = 0xF5;
  buf[pos++] = 0xF6;
  buf[pos++] = 0xF7;
  buf[pos++] = 0xF8;

  return pos;
}

/*
 * Build a command response frame.
 * Returns total frame length.
 */
static inline int build_cmd_frame(uint8_t *buf,
                                  uint16_t cmd_code,
                                  const uint8_t *data,
                                  uint16_t datalen)
{
  int pos = 0;

  /* Command header: FA FB FC FD */
  buf[pos++] = 0xFA;
  buf[pos++] = 0xFB;
  buf[pos++] = 0xFC;
  buf[pos++] = 0xFD;

  uint16_t payload_len = 2 + datalen;  /* cmd(2) + data */
  buf[pos++] = (uint8_t)(payload_len & 0xFF);
  buf[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);

  buf[pos++] = (uint8_t)(cmd_code & 0xFF);
  buf[pos++] = (uint8_t)((cmd_code >> 8) & 0xFF);

  if (data && datalen > 0)
    {
      memcpy(&buf[pos], data, datalen);
      pos += datalen;
    }

  /* Command tail: 01 02 03 04 */
  buf[pos++] = 0x01;
  buf[pos++] = 0x02;
  buf[pos++] = 0x03;
  buf[pos++] = 0x04;

  return pos;
}

/*
 * Corrupt a single byte in a frame buffer.
 * Useful for negative testing.
 */
static inline void corrupt_byte(uint8_t *buf, int offset)
{
  buf[offset] ^= 0xFF;
}

#endif /* __TESTS_HELPERS_FRAME_BUILDER_H */
