/*
 * froglink.h — C interface for the frogLink USB gateway.
 *
 * Communicates with frogblue devices via serial JSON commands.
 * Uses POSIX termios for serial I/O and a simple JSON parser
 * (included, no external dependencies).
 */

#ifndef FROGLINK_H
#define FROGLINK_H

#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Limits */
#define FL_MAX_PORT_PATH    256
#define FL_MAX_RESPONSE     4096
#define FL_MAX_MESSAGES     64
#define FL_MAX_ROOMS        32
#define FL_MAX_TYPES        32
#define FL_MAX_NAME         128

/* Error codes */
typedef enum {
    FL_OK = 0,
    FL_ERR_NOT_FOUND,       /* No frogLink device found */
    FL_ERR_OPEN,            /* Could not open serial port */
    FL_ERR_NOT_CONNECTED,   /* Not connected */
    FL_ERR_NOT_PROVISIONED, /* Device has no configuration */
    FL_ERR_SEND,            /* Failed to send command */
    FL_ERR_TIMEOUT,         /* No response received */
    FL_ERR_PARSE,           /* Failed to parse response */
} fl_error_t;

/* Project information */
typedef struct {
    char project[FL_MAX_NAME];
    char froglink_name[FL_MAX_NAME];
    char froglink_room[FL_MAX_NAME];
    char froglink_building[FL_MAX_NAME];
    char sw_version[FL_MAX_NAME];
    char config[FL_MAX_NAME];
    char address[FL_MAX_NAME];
    char nadd[FL_MAX_NAME];
    char net_id[FL_MAX_NAME];
    char api[FL_MAX_NAME];
} fl_project_info_t;

/* Callback for incoming messages — receives the raw JSON line */
typedef void (*fl_message_callback_t)(const char *json_line, void *user_data);

/* frogLink handle */
typedef struct {
    char port_path[FL_MAX_PORT_PATH];
    int fd;                     /* serial file descriptor */
    bool connected;

    /* Listener thread */
    pthread_t listener_thread;
    bool listening;
    fl_message_callback_t on_message;
    void *user_data;
    pthread_mutex_t write_lock;
} froglink_t;

/* --- Detection --- */

/**
 * Auto-detect a frogLink USB device.
 * Returns FL_OK and fills port_path, or FL_ERR_NOT_FOUND.
 */
fl_error_t fl_detect(char *port_path, size_t path_len);

/* --- Connection --- */

/**
 * Initialize a froglink handle. Call before any other function.
 * If port is NULL, fl_connect() will auto-detect.
 */
void fl_init(froglink_t *fl, const char *port);

/**
 * Open the serial connection.
 */
fl_error_t fl_connect(froglink_t *fl);

/**
 * Close the serial connection and stop the listener.
 */
void fl_disconnect(froglink_t *fl);

/* --- Low-level I/O --- */

/**
 * Send a raw string and read the response.
 * Response is written to resp_buf (null-terminated).
 */
fl_error_t fl_send_raw(froglink_t *fl, const char *cmd,
                       char *resp_buf, size_t resp_len);

/* --- Query commands --- */

fl_error_t fl_project(froglink_t *fl, fl_project_info_t *info);

fl_error_t fl_messages(froglink_t *fl, char messages[][FL_MAX_NAME],
                       int *count);

fl_error_t fl_rooms(froglink_t *fl, char rooms[][FL_MAX_NAME],
                    int *count);

fl_error_t fl_types(froglink_t *fl, const char *room,
                    char types[][FL_MAX_NAME], int *count);

bool fl_is_provisioned(froglink_t *fl);

/* --- Enable/disable receiving --- */

fl_error_t fl_enable_messages(froglink_t *fl, bool enabled);
fl_error_t fl_enable_status(froglink_t *fl, bool enabled);

/* --- Send control messages --- */

/**
 * Send a control message (toggle).
 */
fl_error_t fl_send(froglink_t *fl, const char *message,
                   char *resp_buf, size_t resp_len);

/**
 * Send a control message with on/off.
 */
fl_error_t fl_send_onoff(froglink_t *fl, const char *message, bool on,
                         char *resp_buf, size_t resp_len);

/**
 * Send a control message with brightness (0-100).
 */
fl_error_t fl_send_bright(froglink_t *fl, const char *message, int bright,
                          char *resp_buf, size_t resp_len);

/**
 * Send a control message with full parameters.
 * Set bright to -1 to omit. Set time_str to NULL to omit.
 * Set on to -1 for toggle, 0 for off, 1 for on.
 */
fl_error_t fl_send_full(froglink_t *fl, const char *message,
                        int on, int bright, const char *time_str,
                        char *resp_buf, size_t resp_len);

/**
 * Send a control status message (logical gate).
 */
fl_error_t fl_send_status(froglink_t *fl, const char *name, bool status,
                          char *resp_buf, size_t resp_len);

/**
 * Query the output state of a device.
 */
fl_error_t fl_query_output(froglink_t *fl, const char *target,
                           const char *output,
                           char *resp_buf, size_t resp_len);

/* --- Background listener --- */

/**
 * Start a background thread that calls callback for each incoming message.
 */
fl_error_t fl_start_listener(froglink_t *fl, fl_message_callback_t callback,
                             void *user_data);

/**
 * Stop the background listener.
 */
void fl_stop_listener(froglink_t *fl);

#ifdef __cplusplus
}
#endif

#endif /* FROGLINK_H */
