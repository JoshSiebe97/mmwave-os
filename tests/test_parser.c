/*
 * tests/test_parser.c
 *
 * Unit tests for the LD2410 frame parser (mmwave_parse_byte).
 *
 * We #include the driver .c directly to reach the static functions.
 * NuttX headers are stubbed by the files in tests/stubs/.
 */

#include "unity/unity.h"
#include "helpers/frame_builder.h"

/* ---- Pull in the driver source (gives us static functions) ---- */

#include "drivers/mmwave/mmwave_ld2410.c"

/* ---- Test helpers ---- */

/* Reset parser state to a clean slate */
static void reset_parser(struct mmwave_dev_s *dev)
{
  memset(dev, 0, sizeof(*dev));
  dev->parse_state = PARSE_HEADER;
  dev->uart_fd = -1;
  nxsem_init(&dev->data_sem, 0, 1);
  nxsem_init(&dev->cmd_sem, 0, 1);
  nxsem_init(&dev->wait_sem, 0, 0);
}

static struct mmwave_dev_s test_dev;

void setUp(void)
{
  reset_parser(&test_dev);
}

void tearDown(void) {}

/* ---- Feed a buffer byte-by-byte, return how many complete frames ---- */
static int feed_bytes(struct mmwave_dev_s *dev,
                      const uint8_t *buf, int len)
{
  int frames = 0;
  for (int i = 0; i < len; i++)
    {
      frames += mmwave_parse_byte(dev, buf[i]);
    }
  return frames;
}

/* ================================================================
 * Tests: valid frame parsing
 * ================================================================ */

void test_valid_data_frame_detected(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  int frames = feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(1, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_ok);
  TEST_ASSERT_EQUAL_UINT32(0, test_dev.frames_err);
}

void test_valid_command_frame_detected(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  uint8_t resp[] = { 0x01, 0x00 };  /* ack payload */
  int len = build_cmd_frame(frame, 0x00FF, resp, 2);

  int frames = feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(1, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_ok);
}

void test_engineering_frame_detected(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  uint8_t mg[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
  uint8_t sg[9] = { 5, 15, 25, 35, 45, 55, 65, 75, 85 };
  int len = build_eng_frame(frame, 0x03, 100, 55, 200, 30, 100, mg, sg);

  int frames = feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(1, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_ok);
}

/* ================================================================
 * Tests: back-to-back frames
 * ================================================================ */

void test_back_to_back_data_frames(void)
{
  uint8_t buf[FRAME_BUF_SIZE * 3];
  int total = 0;

  total += build_data_frame(&buf[total], 0x01, 100, 70, 200, 40, 100);
  total += build_data_frame(&buf[total], 0x02, 300, 50, 400, 20, 300);
  total += build_data_frame(&buf[total], 0x00, 0, 0, 0, 0, 0);

  int frames = feed_bytes(&test_dev, buf, total);

  TEST_ASSERT_EQUAL_INT(3, frames);
  TEST_ASSERT_EQUAL_UINT32(3, test_dev.frames_ok);
  TEST_ASSERT_EQUAL_UINT32(0, test_dev.frames_err);
}

void test_data_then_command_frame(void)
{
  uint8_t buf[FRAME_BUF_SIZE * 2];
  int total = 0;

  total += build_data_frame(&buf[total], 0x01, 100, 70, 200, 40, 100);
  uint8_t resp[] = { 0x00 };
  total += build_cmd_frame(&buf[total], 0x00FE, resp, 1);

  int frames = feed_bytes(&test_dev, buf, total);

  TEST_ASSERT_EQUAL_INT(2, frames);
  TEST_ASSERT_EQUAL_UINT32(2, test_dev.frames_ok);
}

/* ================================================================
 * Tests: garbage resilience
 * ================================================================ */

void test_garbage_before_valid_frame(void)
{
  uint8_t buf[FRAME_BUF_SIZE + 32];
  int total = 0;

  /* 16 bytes of random junk */
  uint8_t junk[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33,
                     0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB };
  memcpy(buf, junk, 16);
  total = 16;

  total += build_data_frame(&buf[total], 0x01, 150, 80, 200, 40, 150);

  int frames = feed_bytes(&test_dev, buf, total);

  TEST_ASSERT_EQUAL_INT(1, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_ok);
}

void test_single_garbage_bytes_between_frames(void)
{
  uint8_t buf[FRAME_BUF_SIZE * 3 + 8];
  int total = 0;

  total += build_data_frame(&buf[total], 0x01, 100, 70, 200, 40, 100);

  /* 4 garbage bytes */
  buf[total++] = 0xFF;
  buf[total++] = 0xFE;
  buf[total++] = 0xFD;
  buf[total++] = 0xFC;

  total += build_data_frame(&buf[total], 0x02, 300, 50, 400, 20, 300);

  int frames = feed_bytes(&test_dev, buf, total);

  TEST_ASSERT_EQUAL_INT(2, frames);
  TEST_ASSERT_EQUAL_UINT32(2, test_dev.frames_ok);
}

/* ================================================================
 * Tests: corrupted frames
 * ================================================================ */

void test_corrupted_tail_causes_error(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  /* Corrupt the last byte of the tail */
  frame[len - 1] ^= 0xFF;

  int frames = feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(0, frames);
  TEST_ASSERT_EQUAL_UINT32(0, test_dev.frames_ok);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_err);
}

void test_corrupted_header_no_frame(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  /* Corrupt byte 2 of header */
  frame[2] ^= 0xFF;

  int frames = feed_bytes(&test_dev, frame, len);

  /* Parser should never find a valid header sequence */
  TEST_ASSERT_EQUAL_INT(0, frames);
  TEST_ASSERT_EQUAL_UINT32(0, test_dev.frames_ok);
}

void test_header_tail_mismatch(void)
{
  /* Build a data frame but swap in command tail bytes */
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  /* Replace data tail (F5 F6 F7 F8) with cmd tail (01 02 03 04) */
  frame[len - 4] = 0x01;
  frame[len - 3] = 0x02;
  frame[len - 2] = 0x03;
  frame[len - 1] = 0x04;

  int frames = feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(0, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_err);
}

/* ================================================================
 * Tests: oversized frame length field
 * ================================================================ */

void test_oversized_length_resets_parser(void)
{
  uint8_t frame[FRAME_BUF_SIZE];

  /* Data header */
  frame[0] = 0xF1; frame[1] = 0xF2; frame[2] = 0xF3; frame[3] = 0xF4;

  /* Payload length = 0xFFFF (way too big) */
  frame[4] = 0xFF;
  frame[5] = 0xFF;

  /* Feed just the header + length */
  int frames = feed_bytes(&test_dev, frame, 6);

  TEST_ASSERT_EQUAL_INT(0, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_err);
  /* Parser should have reset to PARSE_HEADER */
  TEST_ASSERT_EQUAL_INT(PARSE_HEADER, test_dev.parse_state);
}

/* ================================================================
 * Tests: parser state after valid parse
 * ================================================================ */

void test_parser_resets_after_valid_frame(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(PARSE_HEADER, test_dev.parse_state);
  TEST_ASSERT_EQUAL_UINT8(0, test_dev.rxpos);
}

void test_parser_resets_after_error(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);
  frame[len - 1] ^= 0xFF;  /* corrupt tail */

  feed_bytes(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(PARSE_HEADER, test_dev.parse_state);
  TEST_ASSERT_EQUAL_UINT8(0, test_dev.rxpos);
}

/* ================================================================
 * Tests: frame counter accuracy
 * ================================================================ */

void test_frame_counters_accumulate(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len;

  /* 3 good frames */
  for (int i = 0; i < 3; i++)
    {
      len = build_data_frame(frame, 0x01, 100 * i, 50, 200, 30, 100);
      feed_bytes(&test_dev, frame, len);
    }

  /* 2 bad frames (corrupted tail) */
  for (int i = 0; i < 2; i++)
    {
      len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);
      frame[len - 1] ^= 0xFF;
      feed_bytes(&test_dev, frame, len);
    }

  TEST_ASSERT_EQUAL_UINT32(3, test_dev.frames_ok);
  TEST_ASSERT_EQUAL_UINT32(2, test_dev.frames_err);
}

/* ================================================================
 * Tests: partial header sliding window
 * ================================================================ */

void test_partial_header_then_valid_frame(void)
{
  uint8_t buf[FRAME_BUF_SIZE + 8];
  int total = 0;

  /* Partial data header bytes (wrong sequence) then real frame */
  buf[total++] = 0xF1;
  buf[total++] = 0xF2;
  /* Break off — not followed by F3 F4, but by garbage */
  buf[total++] = 0x00;
  buf[total++] = 0x00;

  /* Now a real complete frame */
  total += build_data_frame(&buf[total], 0x02, 250, 60, 300, 35, 250);

  int frames = feed_bytes(&test_dev, buf, total);

  TEST_ASSERT_EQUAL_INT(1, frames);
  TEST_ASSERT_EQUAL_UINT32(1, test_dev.frames_ok);
}

/* ================================================================
 * Tests: empty / zero-length
 * ================================================================ */

void test_empty_input(void)
{
  int frames = feed_bytes(&test_dev, NULL, 0);
  TEST_ASSERT_EQUAL_INT(0, frames);
  TEST_ASSERT_EQUAL_INT(PARSE_HEADER, test_dev.parse_state);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
  UNITY_BEGIN();

  /* Valid frames */
  RUN_TEST(test_valid_data_frame_detected);
  RUN_TEST(test_valid_command_frame_detected);
  RUN_TEST(test_engineering_frame_detected);

  /* Back-to-back */
  RUN_TEST(test_back_to_back_data_frames);
  RUN_TEST(test_data_then_command_frame);

  /* Garbage resilience */
  RUN_TEST(test_garbage_before_valid_frame);
  RUN_TEST(test_single_garbage_bytes_between_frames);

  /* Corrupted frames */
  RUN_TEST(test_corrupted_tail_causes_error);
  RUN_TEST(test_corrupted_header_no_frame);
  RUN_TEST(test_header_tail_mismatch);

  /* Oversized */
  RUN_TEST(test_oversized_length_resets_parser);

  /* Parser state */
  RUN_TEST(test_parser_resets_after_valid_frame);
  RUN_TEST(test_parser_resets_after_error);

  /* Counters */
  RUN_TEST(test_frame_counters_accumulate);

  /* Edge cases */
  RUN_TEST(test_partial_header_then_valid_frame);
  RUN_TEST(test_empty_input);

  return UNITY_END();
}
