/****************************************************************************
 * drivers/mmwave/mmwave_ld2410.h
 *
 * SPDX-License-Identifier: MIT
 *
 * mmWave LD2410 radar sensor driver header for NuttX
 *
 ****************************************************************************/

#ifndef __DRIVERS_MMWAVE_LD2410_H
#define __DRIVERS_MMWAVE_LD2410_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/fs/fs.h>
#include <nuttx/serial/serial.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* LD2410 Protocol Constants */

#define LD2410_DATA_HEADER         0xF4F3F2F1
#define LD2410_DATA_TAIL           0xF8F7F6F5
#define LD2410_CMD_HEADER          0xFDFCFBFA
#define LD2410_CMD_TAIL            0x04030201

#define LD2410_MAX_FRAME_LEN       64
#define LD2410_DEFAULT_BAUD        256000

/* LD2410 Target States */

#define LD2410_TARGET_NONE         0x00
#define LD2410_TARGET_MOTION       0x01
#define LD2410_TARGET_STATIC       0x02
#define LD2410_TARGET_BOTH         0x03

/* LD2410 Commands */

#define LD2410_CMD_ENABLE_CONFIG   0x00FF
#define LD2410_CMD_DISABLE_CONFIG  0x00FE
#define LD2410_CMD_SET_MAXGATE     0x0060
#define LD2410_CMD_SET_SENSITIVITY 0x0064
#define LD2410_CMD_READ_FIRMWARE   0x00A0
#define LD2410_CMD_SET_BAUDRATE    0x00A1
#define LD2410_CMD_FACTORY_RESET   0x00A2
#define LD2410_CMD_RESTART         0x00A3
#define LD2410_CMD_ENG_MODE_ON     0x0062
#define LD2410_CMD_ENG_MODE_OFF    0x0063
#define LD2410_CMD_READ_CONFIG     0x0061

/* LD2410 Gate configuration (0-8, each ~0.75m) */

#define LD2410_MAX_GATES           9
#define LD2410_GATE_DISTANCE_CM    75   /* Each gate ≈ 75cm */

/* IOCTL Commands */

#define MMWAVE_IOC_MAGIC           'M'
#define MMWAVE_IOC_SET_SENSITIVITY _IOW(MMWAVE_IOC_MAGIC, 1, struct mmwave_sensitivity_s)
#define MMWAVE_IOC_GET_CONFIG      _IOR(MMWAVE_IOC_MAGIC, 2, struct mmwave_config_s)
#define MMWAVE_IOC_SET_MAXGATE     _IOW(MMWAVE_IOC_MAGIC, 3, struct mmwave_maxgate_s)
#define MMWAVE_IOC_ENG_MODE        _IOW(MMWAVE_IOC_MAGIC, 4, int)
#define MMWAVE_IOC_RESTART         _IO(MMWAVE_IOC_MAGIC, 5)
#define MMWAVE_IOC_FACTORY_RESET   _IO(MMWAVE_IOC_MAGIC, 6)
#define MMWAVE_IOC_GET_FIRMWARE    _IOR(MMWAVE_IOC_MAGIC, 7, struct mmwave_firmware_s)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Data returned by read() on /dev/mmwave0 */

struct mmwave_data_s
{
  uint8_t  target_state;       /* LD2410_TARGET_xxx */
  uint16_t motion_distance;    /* Distance in cm */
  uint8_t  motion_energy;      /* 0-100 */
  uint16_t static_distance;    /* Distance in cm */
  uint8_t  static_energy;      /* 0-100 */
  uint16_t detection_distance; /* Nearest detection in cm */
  uint32_t timestamp_ms;       /* Tick count at data capture */
};

/* Engineering mode data — per-gate energy levels */

struct mmwave_eng_data_s
{
  struct mmwave_data_s basic;
  uint8_t  motion_gate_energy[LD2410_MAX_GATES];
  uint8_t  static_gate_energy[LD2410_MAX_GATES];
};

/* Sensitivity configuration for a single gate */

struct mmwave_sensitivity_s
{
  uint8_t  gate;               /* Gate index 0-8 */
  uint8_t  motion_threshold;   /* Motion sensitivity 0-100 */
  uint8_t  static_threshold;   /* Static sensitivity 0-100 */
};

/* Max gate / timeout configuration */

struct mmwave_maxgate_s
{
  uint8_t  max_motion_gate;    /* Max motion detection gate 0-8 */
  uint8_t  max_static_gate;    /* Max static detection gate 0-8 */
  uint16_t timeout_s;          /* No-presence timeout in seconds */
};

/* Current device configuration readback */

struct mmwave_config_s
{
  uint8_t  max_motion_gate;
  uint8_t  max_static_gate;
  uint16_t timeout_s;
  uint8_t  motion_sensitivity[LD2410_MAX_GATES];
  uint8_t  static_sensitivity[LD2410_MAX_GATES];
};

/* Firmware version info */

struct mmwave_firmware_s
{
  uint8_t  major;
  uint8_t  minor;
  uint32_t build;
};

/****************************************************************************
 * Driver State (Internal)
 ****************************************************************************/

struct mmwave_dev_s
{
  /* UART interface */

  int                    uart_fd;         /* File descriptor for UART */
  FAR const char        *uart_path;       /* e.g. "/dev/ttyS1" */
  uint32_t               baud;            /* UART baud rate */

  /* Latest sensor data */

  struct mmwave_data_s   data;            /* Latest sensor reading */
  struct mmwave_eng_data_s eng_data;      /* Engineering mode data */
  bool                   eng_mode;        /* Engineering mode active */
  bool                   data_valid;      /* At least one frame received */

  /* Frame parser state */

  uint8_t                rxbuf[LD2410_MAX_FRAME_LEN];
  uint8_t                rxpos;           /* Current position in rxbuf */
  enum
  {
    PARSE_HEADER = 0,
    PARSE_LENGTH,
    PARSE_PAYLOAD,
    PARSE_TAIL
  }                      parse_state;
  uint16_t               frame_len;       /* Expected payload length */

  /* Synchronization */

  sem_t                  data_sem;        /* Protects data fields */
  sem_t                  cmd_sem;         /* Serializes command access */
  sem_t                  wait_sem;        /* Wait for command response */

  /* Statistics */

  uint32_t               frames_ok;       /* Successfully parsed frames */
  uint32_t               frames_err;      /* Parse errors / CRC failures */
  uint32_t               cmd_timeouts;    /* Command response timeouts */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * Register the mmWave LD2410 driver.
 *
 * @param devpath  Device node path, e.g. "/dev/mmwave0"
 * @param uartpath UART device path, e.g. "/dev/ttyS1"
 * @param baud     UART baud rate (default 256000)
 * @return 0 on success, negative errno on failure
 */

int mmwave_ld2410_register(FAR const char *devpath,
                           FAR const char *uartpath,
                           uint32_t baud);

/**
 * Unregister and free the mmWave driver.
 */

int mmwave_ld2410_unregister(FAR const char *devpath);

#endif /* __DRIVERS_MMWAVE_LD2410_H */
