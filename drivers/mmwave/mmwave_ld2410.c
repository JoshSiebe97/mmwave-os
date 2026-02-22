/****************************************************************************
 * drivers/mmwave/mmwave_ld2410.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NuttX character device driver for the HLK-LD2410 24GHz mmWave radar.
 *
 * Registers /dev/mmwave0. Reads binary frames from UART at 10Hz,
 * parses presence/motion/static data, and exposes it via read() and
 * ioctl(). A background polling task handles continuous UART reads.
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
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <nuttx/clock.h>
#include <nuttx/semaphore.h>
#include <nuttx/kthread.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "mmwave_ld2410.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MMWAVE_POLL_STACK_SIZE   2048
#define MMWAVE_POLL_PRIORITY     100
#define MMWAVE_CMD_TIMEOUT_MS    1000
#define MMWAVE_READ_TIMEOUT_MS   200

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int     mmwave_open(FAR struct file *filep);
static int     mmwave_close(FAR struct file *filep);
static ssize_t mmwave_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen);
static int     mmwave_ioctl(FAR struct file *filep, int cmd,
                            unsigned long arg);

static int     mmwave_poll_task(int argc, FAR char *argv[]);
static int     mmwave_parse_byte(FAR struct mmwave_dev_s *priv, uint8_t byte);
static int     mmwave_process_data_frame(FAR struct mmwave_dev_s *priv);
static int     mmwave_send_command(FAR struct mmwave_dev_s *priv,
                                   uint16_t cmd,
                                   FAR const uint8_t *data,
                                   uint16_t datalen);
static int     mmwave_enter_config(FAR struct mmwave_dev_s *priv);
static int     mmwave_exit_config(FAR struct mmwave_dev_s *priv);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_mmwave_fops =
{
  mmwave_open,    /* open */
  mmwave_close,   /* close */
  mmwave_read,    /* read */
  NULL,           /* write */
  NULL,           /* seek */
  mmwave_ioctl,   /* ioctl */
  NULL,           /* mmap */
  NULL,           /* truncate */
  NULL            /* poll */
};

/* Single device instance (we only support one mmWave sensor) */

static FAR struct mmwave_dev_s *g_mmwave_dev = NULL;
static pid_t g_poll_pid = -1;
static volatile bool g_poll_running = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mmwave_uart_configure
 *
 * Description:
 *   Open and configure the UART port for LD2410 communication.
 *
 ****************************************************************************/

static int mmwave_uart_configure(FAR struct mmwave_dev_s *priv)
{
  struct termios tio;
  int fd;
  int ret;

  fd = open(priv->uart_path, O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      snerr("ERROR: Failed to open UART %s: %d\n", priv->uart_path, errno);
      return -errno;
    }

  /* Get current terminal settings */

  ret = tcgetattr(fd, &tio);
  if (ret < 0)
    {
      close(fd);
      return -errno;
    }

  /* Configure for raw binary communication */

  tio.c_iflag = 0;                  /* No input processing */
  tio.c_oflag = 0;                  /* No output processing */
  tio.c_lflag = 0;                  /* No line processing */
  tio.c_cflag = CS8 | CREAD | CLOCAL; /* 8N1, enable receiver */

  /* Set baud rate */

  speed_t speed;
  switch (priv->baud)
    {
      case 9600:    speed = B9600;   break;
      case 19200:   speed = B19200;  break;
      case 38400:   speed = B38400;  break;
      case 57600:   speed = B57600;  break;
      case 115200:  speed = B115200; break;
      case 230400:  speed = B230400; break;
      case 256000:  speed = B256000; break;
      case 460800:  speed = B460800; break;
      default:      speed = B256000; break;
    }

  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);

  /* Read timeout: return after 1 byte or 200ms */

  tio.c_cc[VMIN]  = 0;
  tio.c_cc[VTIME] = 2;              /* 200ms timeout in deciseconds */

  ret = tcsetattr(fd, TCSANOW, &tio);
  if (ret < 0)
    {
      close(fd);
      return -errno;
    }

  priv->uart_fd = fd;
  return OK;
}

/****************************************************************************
 * Name: mmwave_parse_byte
 *
 * Description:
 *   Feed one byte into the frame parser state machine.
 *   Returns 1 when a complete frame is available, 0 otherwise.
 *
 ****************************************************************************/

static int mmwave_parse_byte(FAR struct mmwave_dev_s *priv, uint8_t byte)
{
  switch (priv->parse_state)
    {
      case PARSE_HEADER:
        {
          /* Shift byte into header detection window */

          if (priv->rxpos < 4)
            {
              priv->rxbuf[priv->rxpos++] = byte;
            }

          if (priv->rxpos == 4)
            {
              uint32_t header = (uint32_t)priv->rxbuf[0]       |
                                ((uint32_t)priv->rxbuf[1] << 8)  |
                                ((uint32_t)priv->rxbuf[2] << 16) |
                                ((uint32_t)priv->rxbuf[3] << 24);

              if (header == LD2410_DATA_HEADER ||
                  header == LD2410_CMD_HEADER)
                {
                  priv->parse_state = PARSE_LENGTH;
                  /* Keep header in rxbuf, continue to length */
                }
              else
                {
                  /* Slide window: discard first byte, try again */

                  memmove(priv->rxbuf, priv->rxbuf + 1, 3);
                  priv->rxpos = 3;
                }
            }
        }
        break;

      case PARSE_LENGTH:
        {
          priv->rxbuf[priv->rxpos++] = byte;

          if (priv->rxpos == 6)  /* 4 header + 2 length bytes */
            {
              priv->frame_len = (uint16_t)priv->rxbuf[4] |
                                ((uint16_t)priv->rxbuf[5] << 8);

              if (priv->frame_len > LD2410_MAX_FRAME_LEN - 10)
                {
                  /* Frame too large — reset parser */

                  snwarn("WARN: Frame length %u too large\n",
                         priv->frame_len);
                  priv->rxpos = 0;
                  priv->parse_state = PARSE_HEADER;
                  priv->frames_err++;
                }
              else
                {
                  priv->parse_state = PARSE_PAYLOAD;
                }
            }
        }
        break;

      case PARSE_PAYLOAD:
        {
          priv->rxbuf[priv->rxpos++] = byte;

          /* Total frame = 4 (header) + 2 (len) + payload + 4 (tail) */

          if (priv->rxpos >= (size_t)(6 + priv->frame_len + 4))
            {
              priv->parse_state = PARSE_TAIL;

              /* Verify tail bytes */

              uint16_t tail_off = 6 + priv->frame_len;
              uint32_t tail = (uint32_t)priv->rxbuf[tail_off]       |
                              ((uint32_t)priv->rxbuf[tail_off + 1] << 8)  |
                              ((uint32_t)priv->rxbuf[tail_off + 2] << 16) |
                              ((uint32_t)priv->rxbuf[tail_off + 3] << 24);

              uint32_t header = (uint32_t)priv->rxbuf[0]       |
                                ((uint32_t)priv->rxbuf[1] << 8)  |
                                ((uint32_t)priv->rxbuf[2] << 16) |
                                ((uint32_t)priv->rxbuf[3] << 24);

              bool tail_ok = false;
              if (header == LD2410_DATA_HEADER &&
                  tail == LD2410_DATA_TAIL)
                {
                  tail_ok = true;
                }
              else if (header == LD2410_CMD_HEADER &&
                       tail == LD2410_CMD_TAIL)
                {
                  tail_ok = true;
                }

              if (tail_ok)
                {
                  priv->frames_ok++;
                  priv->rxpos = 0;
                  priv->parse_state = PARSE_HEADER;
                  return 1;  /* Complete frame ready */
                }
              else
                {
                  priv->frames_err++;
                  priv->rxpos = 0;
                  priv->parse_state = PARSE_HEADER;
                }
            }
        }
        break;

      default:
        priv->rxpos = 0;
        priv->parse_state = PARSE_HEADER;
        break;
    }

  return 0;
}

/****************************************************************************
 * Name: mmwave_process_data_frame
 *
 * Description:
 *   Process a complete data frame and update priv->data.
 *   Called when parser returns 1 (frame complete).
 *
 *   Standard data frame payload layout (after header + length):
 *     Byte 0:    Data type (0x02 = target data, 0x01 = engineering)
 *     Byte 1:    Head (0xAA)
 *     Byte 2:    Target state (0x00-0x03)
 *     Byte 3-4:  Motion target distance (cm, little-endian)
 *     Byte 5:    Motion target energy (0-100)
 *     Byte 6-7:  Static target distance (cm, little-endian)
 *     Byte 8:    Static target energy (0-100)
 *     Byte 9-10: Detection distance (cm, little-endian)
 *     ... (engineering data beyond this if data_type == 0x01)
 *
 ****************************************************************************/

static int mmwave_process_data_frame(FAR struct mmwave_dev_s *priv)
{
  FAR uint8_t *payload = &priv->rxbuf[6];  /* Skip header(4) + length(2) */

  uint8_t data_type = payload[0];

  if (data_type != 0x02 && data_type != 0x01)
    {
      /* Not a target data frame (could be command response) */

      return -EINVAL;
    }

  if (payload[1] != 0xAA)
    {
      snwarn("WARN: Missing 0xAA head marker\n");
      return -EINVAL;
    }

  int ret = nxsem_wait(&priv->data_sem);
  if (ret < 0)
    {
      return ret;
    }

  priv->data.target_state      = payload[2];
  priv->data.motion_distance   = (uint16_t)payload[3] |
                                 ((uint16_t)payload[4] << 8);
  priv->data.motion_energy     = payload[5];
  priv->data.static_distance   = (uint16_t)payload[6] |
                                 ((uint16_t)payload[7] << 8);
  priv->data.static_energy     = payload[8];
  priv->data.detection_distance = (uint16_t)payload[9] |
                                  ((uint16_t)payload[10] << 8);
  priv->data.timestamp_ms      = clock_systime_ticks() *
                                 (1000 / TICK_PER_SEC);
  priv->data_valid = true;

  /* Parse engineering mode per-gate data if present */

  if (data_type == 0x01 && priv->eng_mode)
    {
      memcpy(&priv->eng_data.basic, &priv->data,
             sizeof(struct mmwave_data_s));

      /* Engineering data starts at payload offset 11 */

      FAR uint8_t *eng = &payload[11];

      /* Motion gate energies (gates 0-8) */

      for (int i = 0; i < LD2410_MAX_GATES && i < (int)priv->frame_len - 15;
           i++)
        {
          priv->eng_data.motion_gate_energy[i] = eng[i];
        }

      /* Static gate energies follow motion gates */

      eng += LD2410_MAX_GATES;
      for (int i = 0; i < LD2410_MAX_GATES && i < (int)priv->frame_len - 24;
           i++)
        {
          priv->eng_data.static_gate_energy[i] = eng[i];
        }
    }

  nxsem_post(&priv->data_sem);
  return OK;
}

/****************************************************************************
 * Name: mmwave_send_command
 *
 * Description:
 *   Send a command frame to the LD2410.
 *   Format: CMD_HEADER(4) + LEN(2) + CMD(2) + DATA(n) + CMD_TAIL(4)
 *
 ****************************************************************************/

static int mmwave_send_command(FAR struct mmwave_dev_s *priv,
                               uint16_t cmd,
                               FAR const uint8_t *data,
                               uint16_t datalen)
{
  uint8_t frame[LD2410_MAX_FRAME_LEN];
  uint16_t payload_len = 2 + datalen;  /* CMD(2) + data */
  uint16_t frame_len = 4 + 2 + payload_len + 4;
  int ret;

  if (frame_len > LD2410_MAX_FRAME_LEN)
    {
      return -EINVAL;
    }

  ret = nxsem_wait(&priv->cmd_sem);
  if (ret < 0)
    {
      return ret;
    }

  /* Build frame */

  int pos = 0;

  /* Header */

  frame[pos++] = 0xFA;
  frame[pos++] = 0xFB;
  frame[pos++] = 0xFC;
  frame[pos++] = 0xFD;

  /* Length (little-endian) */

  frame[pos++] = (uint8_t)(payload_len & 0xFF);
  frame[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);

  /* Command word (little-endian) */

  frame[pos++] = (uint8_t)(cmd & 0xFF);
  frame[pos++] = (uint8_t)((cmd >> 8) & 0xFF);

  /* Command data */

  if (data != NULL && datalen > 0)
    {
      memcpy(&frame[pos], data, datalen);
      pos += datalen;
    }

  /* Tail */

  frame[pos++] = 0x01;
  frame[pos++] = 0x02;
  frame[pos++] = 0x03;
  frame[pos++] = 0x04;

  /* Write to UART */

  ssize_t written = write(priv->uart_fd, frame, pos);

  nxsem_post(&priv->cmd_sem);

  if (written != pos)
    {
      snerr("ERROR: UART write failed: wrote %zd of %d\n", written, pos);
      return -EIO;
    }

  /* Brief delay for LD2410 to process */

  usleep(50000);  /* 50ms */

  return OK;
}

/****************************************************************************
 * Name: mmwave_enter_config / mmwave_exit_config
 *
 * Description:
 *   Enter/exit configuration mode. Required before sending config commands.
 *
 ****************************************************************************/

static int mmwave_enter_config(FAR struct mmwave_dev_s *priv)
{
  uint8_t data[] = { 0x01, 0x00 };  /* Enable config mode value */
  return mmwave_send_command(priv, LD2410_CMD_ENABLE_CONFIG, data, 2);
}

static int mmwave_exit_config(FAR struct mmwave_dev_s *priv)
{
  return mmwave_send_command(priv, LD2410_CMD_DISABLE_CONFIG, NULL, 0);
}

/****************************************************************************
 * Name: mmwave_poll_task
 *
 * Description:
 *   Background kernel thread that continuously reads from UART,
 *   feeds bytes into the frame parser, and updates sensor data.
 *
 ****************************************************************************/

static int mmwave_poll_task(int argc, FAR char *argv[])
{
  FAR struct mmwave_dev_s *priv = g_mmwave_dev;
  uint8_t byte;
  ssize_t nread;

  if (priv == NULL || priv->uart_fd < 0)
    {
      snerr("ERROR: mmwave poll task: no device\n");
      return -ENODEV;
    }

  sninfo("mmWave poll task started (UART: %s, baud: %lu)\n",
         priv->uart_path, (unsigned long)priv->baud);

  g_poll_running = true;

  while (g_poll_running)
    {
      nread = read(priv->uart_fd, &byte, 1);

      if (nread == 1)
        {
          int complete = mmwave_parse_byte(priv, byte);
          if (complete)
            {
              mmwave_process_data_frame(priv);
            }
        }
      else if (nread < 0 && errno != EAGAIN && errno != EINTR)
        {
          snerr("ERROR: UART read error: %d\n", errno);
          usleep(100000);  /* Back off on persistent errors */
        }
      else
        {
          /* No data available — yield briefly */

          usleep(10000);  /* 10ms — allows ~100Hz parse rate */
        }
    }

  sninfo("mmWave poll task stopped\n");
  return OK;
}

/****************************************************************************
 * Character Device Operations
 ****************************************************************************/

static int mmwave_open(FAR struct file *filep)
{
  /* Nothing special to do — polling task is always running */

  return OK;
}

static int mmwave_close(FAR struct file *filep)
{
  return OK;
}

/****************************************************************************
 * Name: mmwave_read
 *
 * Description:
 *   Read the latest sensor data. Returns sizeof(struct mmwave_data_s).
 *   If engineering mode is active and buffer is large enough, returns
 *   sizeof(struct mmwave_eng_data_s) instead.
 *
 ****************************************************************************/

static ssize_t mmwave_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen)
{
  FAR struct mmwave_dev_s *priv = g_mmwave_dev;
  int ret;

  if (priv == NULL)
    {
      return -ENODEV;
    }

  if (!priv->data_valid)
    {
      return -EAGAIN;  /* No data yet */
    }

  ret = nxsem_wait(&priv->data_sem);
  if (ret < 0)
    {
      return ret;
    }

  ssize_t copylen;

  if (priv->eng_mode &&
      buflen >= sizeof(struct mmwave_eng_data_s))
    {
      copylen = sizeof(struct mmwave_eng_data_s);
      memcpy(buffer, &priv->eng_data, copylen);
    }
  else if (buflen >= sizeof(struct mmwave_data_s))
    {
      copylen = sizeof(struct mmwave_data_s);
      memcpy(buffer, &priv->data, copylen);
    }
  else
    {
      nxsem_post(&priv->data_sem);
      return -EINVAL;
    }

  nxsem_post(&priv->data_sem);
  return copylen;
}

/****************************************************************************
 * Name: mmwave_ioctl
 *
 * Description:
 *   Handle device control commands for configuration.
 *
 ****************************************************************************/

static int mmwave_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  FAR struct mmwave_dev_s *priv = g_mmwave_dev;
  int ret = OK;

  if (priv == NULL)
    {
      return -ENODEV;
    }

  switch (cmd)
    {
      case MMWAVE_IOC_SET_SENSITIVITY:
        {
          FAR struct mmwave_sensitivity_s *sens =
            (FAR struct mmwave_sensitivity_s *)arg;

          if (sens->gate >= LD2410_MAX_GATES)
            {
              return -EINVAL;
            }

          ret = mmwave_enter_config(priv);
          if (ret < 0) break;

          /* Command data: gate(2) + motion_val(4) + static_val(4) */

          uint8_t data[18];
          memset(data, 0, sizeof(data));

          /* Word 0: gate select */

          data[0] = 0x00;
          data[1] = 0x00;
          data[2] = sens->gate;
          data[3] = 0x00;
          data[4] = 0x00;
          data[5] = 0x00;

          /* Word 1: motion sensitivity */

          data[6]  = 0x01;
          data[7]  = 0x00;
          data[8]  = sens->motion_threshold;
          data[9]  = 0x00;
          data[10] = 0x00;
          data[11] = 0x00;

          /* Word 2: static sensitivity */

          data[12] = 0x02;
          data[13] = 0x00;
          data[14] = sens->static_threshold;
          data[15] = 0x00;
          data[16] = 0x00;
          data[17] = 0x00;

          ret = mmwave_send_command(priv, LD2410_CMD_SET_SENSITIVITY,
                                   data, sizeof(data));

          mmwave_exit_config(priv);
        }
        break;

      case MMWAVE_IOC_SET_MAXGATE:
        {
          FAR struct mmwave_maxgate_s *mg =
            (FAR struct mmwave_maxgate_s *)arg;

          ret = mmwave_enter_config(priv);
          if (ret < 0) break;

          /* Command data: motion_gate(4) + static_gate(4) + timeout(4) */

          uint8_t data[18];
          memset(data, 0, sizeof(data));

          data[0]  = 0x00;
          data[1]  = 0x00;
          data[2]  = mg->max_motion_gate;
          data[3]  = 0x00;
          data[4]  = 0x00;
          data[5]  = 0x00;

          data[6]  = 0x01;
          data[7]  = 0x00;
          data[8]  = mg->max_static_gate;
          data[9]  = 0x00;
          data[10] = 0x00;
          data[11] = 0x00;

          data[12] = 0x02;
          data[13] = 0x00;
          data[14] = (uint8_t)(mg->timeout_s & 0xFF);
          data[15] = (uint8_t)((mg->timeout_s >> 8) & 0xFF);
          data[16] = 0x00;
          data[17] = 0x00;

          ret = mmwave_send_command(priv, LD2410_CMD_SET_MAXGATE,
                                   data, sizeof(data));

          mmwave_exit_config(priv);
        }
        break;

      case MMWAVE_IOC_ENG_MODE:
        {
          int enable = (int)arg;

          ret = mmwave_enter_config(priv);
          if (ret < 0) break;

          if (enable)
            {
              ret = mmwave_send_command(priv, LD2410_CMD_ENG_MODE_ON,
                                       NULL, 0);
              if (ret == OK) priv->eng_mode = true;
            }
          else
            {
              ret = mmwave_send_command(priv, LD2410_CMD_ENG_MODE_OFF,
                                       NULL, 0);
              if (ret == OK) priv->eng_mode = false;
            }

          mmwave_exit_config(priv);
        }
        break;

      case MMWAVE_IOC_RESTART:
        {
          ret = mmwave_enter_config(priv);
          if (ret < 0) break;

          ret = mmwave_send_command(priv, LD2410_CMD_RESTART, NULL, 0);
          mmwave_exit_config(priv);
        }
        break;

      case MMWAVE_IOC_FACTORY_RESET:
        {
          ret = mmwave_enter_config(priv);
          if (ret < 0) break;

          ret = mmwave_send_command(priv, LD2410_CMD_FACTORY_RESET, NULL, 0);
          mmwave_exit_config(priv);
        }
        break;

      default:
        ret = -ENOTTY;
        break;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mmwave_ld2410_register
 *
 * Description:
 *   Register the LD2410 mmWave driver as a character device and start
 *   the background polling task.
 *
 ****************************************************************************/

int mmwave_ld2410_register(FAR const char *devpath,
                           FAR const char *uartpath,
                           uint32_t baud)
{
  FAR struct mmwave_dev_s *priv;
  int ret;

  /* Allocate device structure */

  priv = (FAR struct mmwave_dev_s *)kmm_zalloc(sizeof(struct mmwave_dev_s));
  if (priv == NULL)
    {
      snerr("ERROR: Failed to allocate mmwave_dev_s\n");
      return -ENOMEM;
    }

  priv->uart_path = uartpath;
  priv->baud      = baud > 0 ? baud : LD2410_DEFAULT_BAUD;
  priv->uart_fd   = -1;
  priv->eng_mode  = false;
  priv->data_valid = false;
  priv->rxpos      = 0;
  priv->parse_state = PARSE_HEADER;
  priv->frames_ok  = 0;
  priv->frames_err = 0;
  priv->cmd_timeouts = 0;

  /* Initialize semaphores */

  nxsem_init(&priv->data_sem, 0, 1);
  nxsem_init(&priv->cmd_sem, 0, 1);
  nxsem_init(&priv->wait_sem, 0, 0);

  /* Open and configure UART */

  ret = mmwave_uart_configure(priv);
  if (ret < 0)
    {
      snerr("ERROR: UART configure failed: %d\n", ret);
      goto errout_with_alloc;
    }

  /* Register character device */

  ret = register_driver(devpath, &g_mmwave_fops, 0666, priv);
  if (ret < 0)
    {
      snerr("ERROR: register_driver(%s) failed: %d\n", devpath, ret);
      goto errout_with_uart;
    }

  g_mmwave_dev = priv;

  /* Start background polling task */

  g_poll_pid = kthread_create("mmwave_poll",
                              MMWAVE_POLL_PRIORITY,
                              MMWAVE_POLL_STACK_SIZE,
                              mmwave_poll_task,
                              NULL);
  if (g_poll_pid < 0)
    {
      snerr("ERROR: Failed to start poll task: %d\n", g_poll_pid);
      ret = g_poll_pid;
      goto errout_with_driver;
    }

  sninfo("mmWave LD2410 registered at %s (UART: %s @ %lu baud)\n",
         devpath, uartpath, (unsigned long)baud);

  return OK;

errout_with_driver:
  unregister_driver(devpath);
  g_mmwave_dev = NULL;

errout_with_uart:
  close(priv->uart_fd);

errout_with_alloc:
  nxsem_destroy(&priv->data_sem);
  nxsem_destroy(&priv->cmd_sem);
  nxsem_destroy(&priv->wait_sem);
  kmm_free(priv);
  return ret;
}

/****************************************************************************
 * Name: mmwave_ld2410_unregister
 *
 * Description:
 *   Stop the polling task, close UART, unregister the device, free memory.
 *
 ****************************************************************************/

int mmwave_ld2410_unregister(FAR const char *devpath)
{
  FAR struct mmwave_dev_s *priv = g_mmwave_dev;

  if (priv == NULL)
    {
      return -ENODEV;
    }

  /* Stop polling task */

  g_poll_running = false;
  if (g_poll_pid > 0)
    {
      /* Wait for the task to exit — give it up to 1 second */

      for (int i = 0; i < 100 && g_poll_running; i++)
        {
          usleep(10000);
        }
    }

  /* Close UART */

  if (priv->uart_fd >= 0)
    {
      close(priv->uart_fd);
    }

  /* Unregister driver */

  unregister_driver(devpath);

  /* Clean up */

  nxsem_destroy(&priv->data_sem);
  nxsem_destroy(&priv->cmd_sem);
  nxsem_destroy(&priv->wait_sem);
  kmm_free(priv);

  g_mmwave_dev = NULL;
  g_poll_pid = -1;

  return OK;
}
