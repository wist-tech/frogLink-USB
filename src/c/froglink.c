/*
 * froglink.c — C implementation of the frogLink USB interface.
 */

#include "froglink.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

/* How long to wait for a response (milliseconds) */
#define READ_DELAY_MS 1500
#define READ_POLL_MS  100

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Simple JSON string value extractor. Finds "key":"value" and copies
   value into dst. Returns true if found. */
static bool json_get_string(const char *json, const char *key,
                            char *dst, size_t dst_len)
{
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return false;
    p += strlen(pattern);
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    if (len >= dst_len) len = dst_len - 1;
    memcpy(dst, p, len);
    dst[len] = '\0';
    return true;
}

/* Extract a JSON array of strings: "key":["a","b","c"]
   Fills arr and sets *count. */
static bool json_get_string_array(const char *json, const char *key,
                                  char arr[][FL_MAX_NAME], int max,
                                  int *count)
{
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":[", key);
    const char *p = strstr(json, pattern);
    if (!p) { *count = 0; return false; }
    p += strlen(pattern);

    *count = 0;
    while (*p && *p != ']' && *count < max) {
        if (*p == '"') {
            p++;
            const char *end = strchr(p, '"');
            if (!end) break;
            size_t len = (size_t)(end - p);
            if (len >= FL_MAX_NAME) len = FL_MAX_NAME - 1;
            memcpy(arr[*count], p, len);
            arr[*count][len] = '\0';
            (*count)++;
            p = end + 1;
        } else {
            p++;
        }
    }
    return true;
}

/* Check if response is "no config" error */
static bool is_no_config(const char *response)
{
    return strstr(response, "\"err\":\"no config\"") != NULL;
}

/* Sleep for milliseconds */
static void msleep(int ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* Read all available data from fd with timeout */
static int read_response(int fd, char *buf, size_t buf_len, int timeout_ms)
{
    size_t total = 0;
    int elapsed = 0;

    /* Initial wait for device to process command */
    msleep(READ_DELAY_MS);

    while (elapsed < timeout_ms && total < buf_len - 1) {
        fd_set rfds;
        struct timeval tv = { 0, READ_POLL_MS * 1000 };

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(fd, &rfds)) {
            ssize_t n = read(fd, buf + total, buf_len - 1 - total);
            if (n > 0) {
                total += (size_t)n;
                elapsed = 0; /* reset timeout on data */
                continue;
            }
        }
        elapsed += READ_POLL_MS;

        /* If we have some data and no more is coming, we're done */
        if (total > 0 && elapsed >= READ_POLL_MS * 2)
            break;
    }

    buf[total] = '\0';
    return (int)total;
}

/* ------------------------------------------------------------------ */
/* Detection                                                           */
/* ------------------------------------------------------------------ */

fl_error_t fl_detect(char *port_path, size_t path_len)
{
    glob_t g;

    /* Try /dev/serial/by-id/ first */
    if (glob("/dev/serial/by-id/*frogblue*FrogLink*", 0, NULL, &g) == 0
        && g.gl_pathc > 0) {
        strncpy(port_path, g.gl_pathv[0], path_len - 1);
        port_path[path_len - 1] = '\0';
        globfree(&g);
        return FL_OK;
    }
    globfree(&g);

    /* Try ttyUSB devices with udevadm check */
    if (glob("/dev/ttyUSB*", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                     "udevadm info --query=property --name=%s 2>/dev/null | "
                     "grep -q FrogLink", g.gl_pathv[i]);
            if (system(cmd) == 0) {
                strncpy(port_path, g.gl_pathv[i], path_len - 1);
                port_path[path_len - 1] = '\0';
                globfree(&g);
                return FL_OK;
            }
        }
    }
    globfree(&g);

    /* macOS fallback */
    if (glob("/dev/tty.usbserial-*", 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        strncpy(port_path, g.gl_pathv[0], path_len - 1);
        port_path[path_len - 1] = '\0';
        globfree(&g);
        return FL_OK;
    }
    globfree(&g);

    return FL_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/* Connection                                                          */
/* ------------------------------------------------------------------ */

void fl_init(froglink_t *fl, const char *port)
{
    memset(fl, 0, sizeof(*fl));
    fl->fd = -1;
    fl->connected = false;
    fl->listening = false;
    pthread_mutex_init(&fl->write_lock, NULL);

    if (port) {
        strncpy(fl->port_path, port, FL_MAX_PORT_PATH - 1);
    }
}

fl_error_t fl_connect(froglink_t *fl)
{
    if (fl->connected)
        return FL_OK;

    /* Auto-detect if no port specified */
    if (fl->port_path[0] == '\0') {
        fl_error_t err = fl_detect(fl->port_path, FL_MAX_PORT_PATH);
        if (err != FL_OK)
            return FL_ERR_NOT_FOUND;
    }

    fl->fd = open(fl->port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fl->fd < 0)
        return FL_ERR_OPEN;

    /* Clear non-blocking after open */
    int flags = fcntl(fl->fd, F_GETFL, 0);
    fcntl(fl->fd, F_SETFL, flags & ~O_NONBLOCK);

    /* Configure serial: 115200 8N1, no flow control */
    struct termios tty;
    if (tcgetattr(fl->fd, &tty) != 0) {
        close(fl->fd);
        fl->fd = -1;
        return FL_ERR_OPEN;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;        /* No parity */
    tty.c_cflag &= ~CSTOPB;        /* 1 stop bit */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            /* 8 data bits */
    tty.c_cflag &= ~CRTSCTS;       /* No flow control */
    tty.c_cflag |= CREAD | CLOCAL; /* Enable read, ignore modem */

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* Raw mode */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         /* No sw flow */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                      INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;         /* Raw output */

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;          /* 1 second timeout */

    if (tcsetattr(fl->fd, TCSANOW, &tty) != 0) {
        close(fl->fd);
        fl->fd = -1;
        return FL_ERR_OPEN;
    }

    tcflush(fl->fd, TCIOFLUSH);
    msleep(500);

    fl->connected = true;
    return FL_OK;
}

void fl_disconnect(froglink_t *fl)
{
    fl_stop_listener(fl);

    if (fl->fd >= 0) {
        close(fl->fd);
        fl->fd = -1;
    }
    fl->connected = false;
    pthread_mutex_destroy(&fl->write_lock);
}

/* ------------------------------------------------------------------ */
/* Low-level I/O                                                       */
/* ------------------------------------------------------------------ */

fl_error_t fl_send_raw(froglink_t *fl, const char *cmd,
                       char *resp_buf, size_t resp_len)
{
    if (!fl->connected)
        return FL_ERR_NOT_CONNECTED;

    pthread_mutex_lock(&fl->write_lock);

    /* Flush input */
    tcflush(fl->fd, TCIFLUSH);

    /* Send command + newline */
    size_t cmd_len = strlen(cmd);
    if (write(fl->fd, cmd, cmd_len) < 0 ||
        write(fl->fd, "\n", 1) < 0) {
        pthread_mutex_unlock(&fl->write_lock);
        return FL_ERR_SEND;
    }

    /* Read response */
    int n = read_response(fl->fd, resp_buf, resp_len, 1000);

    pthread_mutex_unlock(&fl->write_lock);

    if (n <= 0)
        return FL_ERR_TIMEOUT;

    if (is_no_config(resp_buf))
        return FL_ERR_NOT_PROVISIONED;

    return FL_OK;
}

/* ------------------------------------------------------------------ */
/* Query commands                                                      */
/* ------------------------------------------------------------------ */

fl_error_t fl_project(froglink_t *fl, fl_project_info_t *info)
{
    char resp[FL_MAX_RESPONSE];
    fl_error_t err = fl_send_raw(fl, "{\"cmd\":\"project\"}", resp, sizeof(resp));
    if (err != FL_OK)
        return err;

    memset(info, 0, sizeof(*info));
    json_get_string(resp, "project", info->project, FL_MAX_NAME);
    json_get_string(resp, "frogLinkName", info->froglink_name, FL_MAX_NAME);
    json_get_string(resp, "frogLinkRoom", info->froglink_room, FL_MAX_NAME);
    json_get_string(resp, "frogLinkBuilding", info->froglink_building, FL_MAX_NAME);
    json_get_string(resp, "swVersion", info->sw_version, FL_MAX_NAME);
    json_get_string(resp, "config", info->config, FL_MAX_NAME);
    json_get_string(resp, "address", info->address, FL_MAX_NAME);
    json_get_string(resp, "nadd", info->nadd, FL_MAX_NAME);
    json_get_string(resp, "netId", info->net_id, FL_MAX_NAME);
    json_get_string(resp, "api", info->api, FL_MAX_NAME);

    return FL_OK;
}

fl_error_t fl_messages(froglink_t *fl, char messages[][FL_MAX_NAME],
                       int *count)
{
    char resp[FL_MAX_RESPONSE];
    fl_error_t err = fl_send_raw(fl, "{\"cmd\":\"messages\"}", resp, sizeof(resp));
    if (err != FL_OK)
        return err;

    json_get_string_array(resp, "message", messages, FL_MAX_MESSAGES, count);
    return FL_OK;
}

fl_error_t fl_rooms(froglink_t *fl, char rooms[][FL_MAX_NAME], int *count)
{
    char resp[FL_MAX_RESPONSE];
    fl_error_t err = fl_send_raw(fl, "{\"cmd\":\"rooms\"}", resp, sizeof(resp));
    if (err != FL_OK)
        return err;

    json_get_string_array(resp, "rooms", rooms, FL_MAX_ROOMS, count);
    return FL_OK;
}

fl_error_t fl_types(froglink_t *fl, const char *room,
                    char types[][FL_MAX_NAME], int *count)
{
    char cmd[512];
    if (room)
        snprintf(cmd, sizeof(cmd), "{\"cmd\":\"types\",\"room\":\"%s\"}", room);
    else
        snprintf(cmd, sizeof(cmd), "{\"cmd\":\"types\"}");

    char resp[FL_MAX_RESPONSE];
    fl_error_t err = fl_send_raw(fl, cmd, resp, sizeof(resp));
    if (err != FL_OK)
        return err;

    json_get_string_array(resp, "types", types, FL_MAX_TYPES, count);
    return FL_OK;
}

bool fl_is_provisioned(froglink_t *fl)
{
    fl_project_info_t info;
    return fl_project(fl, &info) == FL_OK;
}

/* ------------------------------------------------------------------ */
/* Enable/disable                                                      */
/* ------------------------------------------------------------------ */

fl_error_t fl_enable_messages(froglink_t *fl, bool enabled)
{
    char cmd[128], resp[FL_MAX_RESPONSE];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"msgenable\",\"enabled\":%s}",
             enabled ? "true" : "false");
    return fl_send_raw(fl, cmd, resp, sizeof(resp));
}

fl_error_t fl_enable_status(froglink_t *fl, bool enabled)
{
    char cmd[128], resp[FL_MAX_RESPONSE];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"statusenable\",\"enabled\":%s}",
             enabled ? "true" : "false");
    return fl_send_raw(fl, cmd, resp, sizeof(resp));
}

/* ------------------------------------------------------------------ */
/* Send control messages                                               */
/* ------------------------------------------------------------------ */

fl_error_t fl_send(froglink_t *fl, const char *message,
                   char *resp_buf, size_t resp_len)
{
    return fl_send_full(fl, message, -1, -1, NULL, resp_buf, resp_len);
}

fl_error_t fl_send_onoff(froglink_t *fl, const char *message, bool on,
                         char *resp_buf, size_t resp_len)
{
    return fl_send_full(fl, message, on ? 1 : 0, -1, NULL, resp_buf, resp_len);
}

fl_error_t fl_send_bright(froglink_t *fl, const char *message, int bright,
                          char *resp_buf, size_t resp_len)
{
    return fl_send_full(fl, message, -1, bright, NULL, resp_buf, resp_len);
}

fl_error_t fl_send_full(froglink_t *fl, const char *message,
                        int on, int bright, const char *time_str,
                        char *resp_buf, size_t resp_len)
{
    char cmd[512];
    int pos = snprintf(cmd, sizeof(cmd), "{\"msg\":\"%s\"", message);

    if (on >= 0)
        pos += snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                        ",\"on\":%s", on ? "true" : "false");
    if (bright >= 0)
        pos += snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                        ",\"bright\":%d", bright);
    if (time_str)
        pos += snprintf(cmd + pos, sizeof(cmd) - (size_t)pos,
                        ",\"time\":\"%s\"", time_str);

    snprintf(cmd + pos, sizeof(cmd) - (size_t)pos, "}");

    return fl_send_raw(fl, cmd, resp_buf, resp_len);
}

fl_error_t fl_send_status(froglink_t *fl, const char *name, bool status,
                          char *resp_buf, size_t resp_len)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "{\"msg\":\"%s\",\"status\":%s,\"changed\":true}",
             name, status ? "true" : "false");
    return fl_send_raw(fl, cmd, resp_buf, resp_len);
}

fl_error_t fl_query_output(froglink_t *fl, const char *target,
                           const char *output,
                           char *resp_buf, size_t resp_len)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"status\",\"type\":\"bright\","
             "\"target\":\"%s\",\"output\":\"%s\"}",
             target, output);
    return fl_send_raw(fl, cmd, resp_buf, resp_len);
}

/* ------------------------------------------------------------------ */
/* Background listener                                                 */
/* ------------------------------------------------------------------ */

static void *listener_thread_func(void *arg)
{
    froglink_t *fl = (froglink_t *)arg;
    char buffer[FL_MAX_RESPONSE];
    size_t buf_pos = 0;

    while (fl->listening && fl->connected) {
        fd_set rfds;
        struct timeval tv = { 0, 100000 }; /* 100ms */

        FD_ZERO(&rfds);
        FD_SET(fl->fd, &rfds);

        int ret = select(fl->fd + 1, &rfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(fl->fd, &rfds)) {
            char chunk[1024];
            ssize_t n = read(fl->fd, chunk, sizeof(chunk));
            if (n <= 0)
                break;

            for (ssize_t i = 0; i < n && buf_pos < sizeof(buffer) - 1; i++) {
                if (chunk[i] == '\n') {
                    buffer[buf_pos] = '\0';

                    /* Trim trailing whitespace */
                    while (buf_pos > 0 &&
                           (buffer[buf_pos - 1] == '\r' ||
                            buffer[buf_pos - 1] == ' '))
                        buffer[--buf_pos] = '\0';

                    if (buf_pos > 0 && fl->on_message)
                        fl->on_message(buffer, fl->user_data);

                    buf_pos = 0;
                } else {
                    buffer[buf_pos++] = chunk[i];
                }
            }
        }
    }

    return NULL;
}

fl_error_t fl_start_listener(froglink_t *fl, fl_message_callback_t callback,
                             void *user_data)
{
    if (fl->listening)
        return FL_OK;
    if (!fl->connected)
        return FL_ERR_NOT_CONNECTED;

    fl->on_message = callback;
    fl->user_data = user_data;
    fl->listening = true;

    if (pthread_create(&fl->listener_thread, NULL,
                       listener_thread_func, fl) != 0) {
        fl->listening = false;
        return FL_ERR_SEND;
    }

    return FL_OK;
}

void fl_stop_listener(froglink_t *fl)
{
    if (!fl->listening)
        return;

    fl->listening = false;
    pthread_join(fl->listener_thread, NULL);
}
