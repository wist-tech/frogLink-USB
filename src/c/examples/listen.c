/*
 * listen.c — Listen for all incoming frogLink messages.
 *
 * Build: gcc -o listen listen.c ../froglink.c -lpthread
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "../froglink.h"

static volatile int running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void on_message(const char *json_line, void *user_data)
{
    (void)user_data;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("[%02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec,
           json_line);
    fflush(stdout);
}

int main(void)
{
    froglink_t fl;
    fl_project_info_t info;

    fl_init(&fl, NULL);

    if (fl_connect(&fl) != FL_OK) {
        printf("Failed to connect. Is the frogLink plugged in?\n");
        return 1;
    }

    fl_error_t err = fl_project(&fl, &info);
    if (err == FL_ERR_NOT_PROVISIONED) {
        printf("frogLink is not provisioned. Run detect first.\n");
        fl_disconnect(&fl);
        return 1;
    }
    if (err != FL_OK) {
        printf("Error querying device.\n");
        fl_disconnect(&fl);
        return 1;
    }

    printf("Connected: %s | %s\n", info.project, info.froglink_name);
    printf("Frogware %s | API %s\n\n", info.sw_version, info.api);

    fl_enable_messages(&fl, true);
    fl_enable_status(&fl, true);

    fl_start_listener(&fl, on_message, NULL);
    printf("Listening for messages (Ctrl+C to stop)...\n\n");

    signal(SIGINT, sigint_handler);

    while (running)
        usleep(100000);

    printf("\nStopping...\n");
    fl_disconnect(&fl);
    return 0;
}
