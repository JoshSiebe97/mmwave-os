/*
 * tests/test_data_extract.c
 *
 * Unit tests for mmwave_process_data_frame().
 * Verifies that parsed frames correctly populate mmwave_data_s fields,
 * and that engineering mode data fills per-gate arrays.
 *
 * Same technique as test_parser.c: include the driver .c for static access.
 */

#include "unity/unity.h"
#include "helpers/frame_builder.h"

/* Pull in driver source (static functions become available) */
#include "drivers/mmwave/mmwave_ld2410.c"

/* ---- Helpers ---- */

static struct mmwave_dev_s test_dev;

/* Reset device and feed a complete frame so rxbuf is populated */
static void reset_dev(void)
{
  memset(&test_dev, 0, sizeof(test_dev));
  test_dev.parse_state = PARSE_HEADER;
  test_dev.uart_fd = -1;
  nxsem_init(&test_dev.data_sem, 0, 1);
  nxsem_init(&test_dev.cmd_sem, 0, 1);
  nxsem_init(&test_dev.wait_sem, 0, 0);
}

/* Feed a frame and parse it, then call process_data_frame.
 * Returns the result of mmwave_process_data_frame. */
static int parse_and_process(struct mmwave_dev_s *dev,
                             const uint8_t *frame, int len)
{
  /* Feed bytes through parser */
  int complete = 0;
  for (int i = 0; i < len; i++)
    {
      complete = mmwave_parse_byte(dev, frame[i]);
    }

  if (!complete)
    {
      return -1;  /* frame wasn't valid */
    }

  /*
   * After parse_byte returns 1, it resets rxpos to 0 and parse_state
   * to PARSE_HEADER, but the rxbuf still contains the frame data.
   * We need to re-populate rxbuf for process_data_frame to read.
   * The simplest approach: copy the raw frame into rxbuf directly.
   */
  memcpy(dev->rxbuf, frame, len);
  dev->frame_len = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8);

  return mmwave_process_data_frame(dev);
}

void setUp(void)
{
  reset_dev();
}

void tearDown(void) {}

/* ================================================================
 * Tests: standard data extraction
 * ================================================================ */

void test_extract_target_state_none(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, LD2410_TARGET_NONE, 0, 0, 0, 0, 0);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_TRUE(test_dev.data_valid);
  TEST_ASSERT_EQUAL_UINT8(LD2410_TARGET_NONE, test_dev.data.target_state);
}

void test_extract_target_state_motion(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, LD2410_TARGET_MOTION, 150, 80, 0, 0, 150);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(LD2410_TARGET_MOTION, test_dev.data.target_state);
}

void test_extract_target_state_static(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, LD2410_TARGET_STATIC, 0, 0, 200, 40, 200);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(LD2410_TARGET_STATIC, test_dev.data.target_state);
}

void test_extract_target_state_both(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, LD2410_TARGET_BOTH, 150, 80, 200, 40, 150);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(LD2410_TARGET_BOTH, test_dev.data.target_state);
}

void test_extract_motion_distance(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 1234, 80, 0, 0, 1234);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT16(1234, test_dev.data.motion_distance);
}

void test_extract_motion_energy(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 95, 0, 0, 150);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(95, test_dev.data.motion_energy);
}

void test_extract_static_distance(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x02, 0, 0, 4567, 60, 4567);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT16(4567, test_dev.data.static_distance);
}

void test_extract_static_energy(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x02, 0, 0, 200, 73, 200);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(73, test_dev.data.static_energy);
}

void test_extract_detection_distance(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x03, 200, 50, 300, 40, 175);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT16(175, test_dev.data.detection_distance);
}

void test_extract_timestamp_is_set(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  /* clock_systime_ticks() returns 12345, TICK_PER_SEC is 1000
   * so timestamp_ms = 12345 * (1000/1000) = 12345 */
  TEST_ASSERT_EQUAL_UINT32(12345, test_dev.data.timestamp_ms);
}

/* ================================================================
 * Tests: max/edge values
 * ================================================================ */

void test_extract_max_distance_values(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  /* Max 16-bit = 65535 cm = ~655m (unrealistic but tests parsing) */
  int len = build_data_frame(frame, 0x03, 0xFFFF, 100, 0xFFFF, 100, 0xFFFF);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, test_dev.data.motion_distance);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, test_dev.data.static_distance);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, test_dev.data.detection_distance);
}

void test_extract_zero_values(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x00, 0, 0, 0, 0, 0);

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT16(0, test_dev.data.motion_distance);
  TEST_ASSERT_EQUAL_UINT8(0, test_dev.data.motion_energy);
  TEST_ASSERT_EQUAL_UINT16(0, test_dev.data.static_distance);
  TEST_ASSERT_EQUAL_UINT8(0, test_dev.data.static_energy);
  TEST_ASSERT_EQUAL_UINT16(0, test_dev.data.detection_distance);
}

/* ================================================================
 * Tests: engineering mode
 * ================================================================ */

void test_extract_engineering_basic_fields(void)
{
  uint8_t mg[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
  uint8_t sg[9] = { 5, 15, 25, 35, 45, 55, 65, 75, 85 };

  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_eng_frame(frame, 0x03, 150, 80, 200, 40, 150, mg, sg);

  test_dev.eng_mode = true;  /* Must be enabled for eng data parse */

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(0x03, test_dev.eng_data.basic.target_state);
  TEST_ASSERT_EQUAL_UINT16(150, test_dev.eng_data.basic.motion_distance);
  TEST_ASSERT_EQUAL_UINT8(80, test_dev.eng_data.basic.motion_energy);
}

void test_extract_engineering_motion_gates(void)
{
  uint8_t mg[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
  uint8_t sg[9] = { 0 };

  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_eng_frame(frame, 0x01, 100, 70, 200, 30, 100, mg, sg);

  test_dev.eng_mode = true;

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(mg, test_dev.eng_data.motion_gate_energy, 9);
}

void test_extract_engineering_static_gates(void)
{
  uint8_t mg[9] = { 0 };
  uint8_t sg[9] = { 5, 15, 25, 35, 45, 55, 65, 75, 85 };

  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_eng_frame(frame, 0x02, 0, 0, 300, 50, 300, mg, sg);

  test_dev.eng_mode = true;

  int ret = parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(sg, test_dev.eng_data.static_gate_energy, 9);
}

void test_engineering_mode_off_skips_gates(void)
{
  uint8_t mg[9] = { 99, 99, 99, 99, 99, 99, 99, 99, 99 };
  uint8_t sg[9] = { 88, 88, 88, 88, 88, 88, 88, 88, 88 };

  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_eng_frame(frame, 0x01, 100, 70, 200, 30, 100, mg, sg);

  test_dev.eng_mode = false;  /* Engineering mode OFF */

  int ret = parse_and_process(&test_dev, frame, len);

  /* Basic data should still be extracted */
  TEST_ASSERT_EQUAL_INT(OK, ret);
  TEST_ASSERT_EQUAL_UINT8(0x01, test_dev.data.target_state);

  /* Gate arrays should remain zero (not populated) */
  for (int i = 0; i < 9; i++)
    {
      TEST_ASSERT_EQUAL_UINT8(0, test_dev.eng_data.motion_gate_energy[i]);
      TEST_ASSERT_EQUAL_UINT8(0, test_dev.eng_data.static_gate_energy[i]);
    }
}

/* ================================================================
 * Tests: rejection of invalid data type / markers
 * ================================================================ */

void test_reject_bad_data_type(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  /* Tamper with data type byte: change 0x02 to 0x05 */
  frame[6] = 0x05;

  /* Manually stuff rxbuf (parser would actually reject mismatched tail
   * if header/tail don't match, but let's test process_data_frame
   * directly with a spoofed buffer) */
  memcpy(test_dev.rxbuf, frame, len);
  test_dev.frame_len = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8);

  int ret = mmwave_process_data_frame(&test_dev);

  TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_reject_missing_head_marker(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  /* Tamper with head marker: change 0xAA to 0xBB */
  frame[7] = 0xBB;

  memcpy(test_dev.rxbuf, frame, len);
  test_dev.frame_len = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8);

  int ret = mmwave_process_data_frame(&test_dev);

  TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* ================================================================
 * Tests: data_valid flag
 * ================================================================ */

void test_data_valid_initially_false(void)
{
  TEST_ASSERT_FALSE(test_dev.data_valid);
}

void test_data_valid_set_after_good_frame(void)
{
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);

  parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_TRUE(test_dev.data_valid);
}

void test_data_valid_survives_bad_frame(void)
{
  /* First: valid frame sets data_valid */
  uint8_t frame[FRAME_BUF_SIZE];
  int len = build_data_frame(frame, 0x01, 150, 80, 200, 40, 150);
  parse_and_process(&test_dev, frame, len);

  TEST_ASSERT_TRUE(test_dev.data_valid);

  /* Second: invalid frame (bad type) should NOT clear data_valid */
  len = build_data_frame(frame, 0x01, 0, 0, 0, 0, 0);
  frame[6] = 0x05;  /* bad type */
  memcpy(test_dev.rxbuf, frame, len);
  test_dev.frame_len = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8);
  mmwave_process_data_frame(&test_dev);

  TEST_ASSERT_TRUE(test_dev.data_valid);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
  UNITY_BEGIN();

  /* Basic field extraction */
  RUN_TEST(test_extract_target_state_none);
  RUN_TEST(test_extract_target_state_motion);
  RUN_TEST(test_extract_target_state_static);
  RUN_TEST(test_extract_target_state_both);
  RUN_TEST(test_extract_motion_distance);
  RUN_TEST(test_extract_motion_energy);
  RUN_TEST(test_extract_static_distance);
  RUN_TEST(test_extract_static_energy);
  RUN_TEST(test_extract_detection_distance);
  RUN_TEST(test_extract_timestamp_is_set);

  /* Edge values */
  RUN_TEST(test_extract_max_distance_values);
  RUN_TEST(test_extract_zero_values);

  /* Engineering mode */
  RUN_TEST(test_extract_engineering_basic_fields);
  RUN_TEST(test_extract_engineering_motion_gates);
  RUN_TEST(test_extract_engineering_static_gates);
  RUN_TEST(test_engineering_mode_off_skips_gates);

  /* Invalid data */
  RUN_TEST(test_reject_bad_data_type);
  RUN_TEST(test_reject_missing_head_marker);

  /* data_valid flag */
  RUN_TEST(test_data_valid_initially_false);
  RUN_TEST(test_data_valid_set_after_good_frame);
  RUN_TEST(test_data_valid_survives_bad_frame);

  return UNITY_END();
}
