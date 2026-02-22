/*
 * tests/test_ha_format.c
 *
 * Unit tests for ha_format_state_json() and ha_format_http_request().
 * Verifies that sensor data is correctly serialized to JSON for HA.
 */

#include "unity/unity.h"

#include <string.h>

/* We include the format header directly — it's a pure-function header,
 * no NuttX machinery needed beyond the data struct from the driver header. */
#include "apps/hactl/ha_format.h"

/* ---- Test helpers ---- */

static char json_buf[512];
static char http_buf[1024];

static struct mmwave_data_s make_data(uint8_t state,
                                      uint16_t motion_dist,
                                      uint8_t motion_energy,
                                      uint16_t static_dist,
                                      uint8_t static_energy,
                                      uint16_t detect_dist)
{
  struct mmwave_data_s d;
  memset(&d, 0, sizeof(d));
  d.target_state      = state;
  d.motion_distance   = motion_dist;
  d.motion_energy     = motion_energy;
  d.static_distance   = static_dist;
  d.static_energy     = static_energy;
  d.detection_distance = detect_dist;
  d.timestamp_ms      = 12345;
  return d;
}

void setUp(void)
{
  memset(json_buf, 0, sizeof(json_buf));
  memset(http_buf, 0, sizeof(http_buf));
}

void tearDown(void) {}

/* ================================================================
 * Tests: JSON state string
 * ================================================================ */

void test_state_on_when_motion(void)
{
  struct mmwave_data_s d = make_data(LD2410_TARGET_MOTION, 150, 80, 0, 0, 150);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"state\":\"on\""));
}

void test_state_on_when_static(void)
{
  struct mmwave_data_s d = make_data(LD2410_TARGET_STATIC, 0, 0, 200, 40, 200);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"state\":\"on\""));
}

void test_state_on_when_both(void)
{
  struct mmwave_data_s d = make_data(LD2410_TARGET_BOTH, 100, 70, 200, 30, 100);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"state\":\"on\""));
}

void test_state_off_when_none(void)
{
  struct mmwave_data_s d = make_data(LD2410_TARGET_NONE, 0, 0, 0, 0, 0);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"state\":\"off\""));
}

/* ================================================================
 * Tests: JSON attribute values
 * ================================================================ */

void test_motion_energy_in_json(void)
{
  struct mmwave_data_s d = make_data(0x01, 150, 83, 200, 40, 150);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"motion_energy\":83"));
}

void test_static_energy_in_json(void)
{
  struct mmwave_data_s d = make_data(0x02, 0, 0, 200, 47, 200);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"static_energy\":47"));
}

void test_motion_distance_in_json(void)
{
  struct mmwave_data_s d = make_data(0x01, 1234, 80, 0, 0, 1234);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"motion_distance\":1234"));
}

void test_static_distance_in_json(void)
{
  struct mmwave_data_s d = make_data(0x02, 0, 0, 5678, 60, 5678);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"static_distance\":5678"));
}

void test_detection_distance_in_json(void)
{
  struct mmwave_data_s d = make_data(0x03, 200, 50, 300, 40, 175);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"detection_distance\":175"));
}

void test_friendly_name_in_json(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"friendly_name\":\"mmWave Presence\""));
}

void test_device_class_in_json(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"device_class\":\"occupancy\""));
}

/* ================================================================
 * Tests: JSON is valid structure
 * ================================================================ */

void test_json_starts_with_brace(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_EQUAL_CHAR('{', json_buf[0]);
}

void test_json_ends_with_brace(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int n = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL_CHAR('}', json_buf[n - 1]);
}

void test_json_return_value_is_length(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int n = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_EQUAL_INT((int)strlen(json_buf), n);
}

/* ================================================================
 * Tests: buffer too small
 * ================================================================ */

void test_json_truncation_returns_negative(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  /* Tiny buffer — can't fit the JSON */
  int n = ha_format_state_json(json_buf, 10, &d);

  TEST_ASSERT_EQUAL_INT(-1, n);
}

/* ================================================================
 * Tests: zero / max boundaries
 * ================================================================ */

void test_all_zeros_json(void)
{
  struct mmwave_data_s d = make_data(0x00, 0, 0, 0, 0, 0);
  int n = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"motion_energy\":0"));
  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"static_energy\":0"));
  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"motion_distance\":0"));
}

void test_max_values_json(void)
{
  struct mmwave_data_s d = make_data(0x03, 65535, 100, 65535, 100, 65535);
  int n = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"motion_distance\":65535"));
  TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"static_distance\":65535"));
}

/* ================================================================
 * Tests: HTTP request formatting
 * ================================================================ */

void test_http_request_has_post_method(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int jlen = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  ha_format_http_request(http_buf, sizeof(http_buf),
                         "binary_sensor.mmwave_presence",
                         "192.168.1.100", 8123,
                         "test_token_abc123",
                         json_buf, jlen);

  TEST_ASSERT_NOT_NULL(strstr(http_buf, "POST /api/states/binary_sensor.mmwave_presence"));
}

void test_http_request_has_auth_header(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int jlen = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  ha_format_http_request(http_buf, sizeof(http_buf),
                         "binary_sensor.mmwave_presence",
                         "192.168.1.100", 8123,
                         "my_secret_token",
                         json_buf, jlen);

  TEST_ASSERT_NOT_NULL(strstr(http_buf, "Authorization: Bearer my_secret_token"));
}

void test_http_request_has_content_type(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int jlen = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  ha_format_http_request(http_buf, sizeof(http_buf),
                         "binary_sensor.mmwave_presence",
                         "192.168.1.100", 8123,
                         "tok",
                         json_buf, jlen);

  TEST_ASSERT_NOT_NULL(strstr(http_buf, "Content-Type: application/json"));
}

void test_http_request_has_content_length(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int jlen = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  ha_format_http_request(http_buf, sizeof(http_buf),
                         "binary_sensor.mmwave_presence",
                         "192.168.1.100", 8123,
                         "tok",
                         json_buf, jlen);

  char expected[32];
  snprintf(expected, sizeof(expected), "Content-Length: %d", jlen);
  TEST_ASSERT_NOT_NULL(strstr(http_buf, expected));
}

void test_http_request_body_appended(void)
{
  struct mmwave_data_s d = make_data(0x01, 100, 50, 200, 30, 100);
  int jlen = ha_format_state_json(json_buf, sizeof(json_buf), &d);

  ha_format_http_request(http_buf, sizeof(http_buf),
                         "binary_sensor.mmwave_presence",
                         "192.168.1.100", 8123,
                         "tok",
                         json_buf, jlen);

  /* The JSON body should appear at the end */
  TEST_ASSERT_NOT_NULL(strstr(http_buf, json_buf));
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
  UNITY_BEGIN();

  /* State string */
  RUN_TEST(test_state_on_when_motion);
  RUN_TEST(test_state_on_when_static);
  RUN_TEST(test_state_on_when_both);
  RUN_TEST(test_state_off_when_none);

  /* Attribute values */
  RUN_TEST(test_motion_energy_in_json);
  RUN_TEST(test_static_energy_in_json);
  RUN_TEST(test_motion_distance_in_json);
  RUN_TEST(test_static_distance_in_json);
  RUN_TEST(test_detection_distance_in_json);
  RUN_TEST(test_friendly_name_in_json);
  RUN_TEST(test_device_class_in_json);

  /* Structural validity */
  RUN_TEST(test_json_starts_with_brace);
  RUN_TEST(test_json_ends_with_brace);
  RUN_TEST(test_json_return_value_is_length);

  /* Truncation */
  RUN_TEST(test_json_truncation_returns_negative);

  /* Boundaries */
  RUN_TEST(test_all_zeros_json);
  RUN_TEST(test_max_values_json);

  /* HTTP request format */
  RUN_TEST(test_http_request_has_post_method);
  RUN_TEST(test_http_request_has_auth_header);
  RUN_TEST(test_http_request_has_content_type);
  RUN_TEST(test_http_request_has_content_length);
  RUN_TEST(test_http_request_body_appended);

  return UNITY_END();
}
