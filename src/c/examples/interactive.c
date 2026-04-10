/*
 * interactive.c — Interactive frogLink console.
 *
 * Build: gcc -o interactive interactive.c ../froglink.c -lpthread
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../froglink.h"

static volatile int running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
    printf("\nDisconnecting...\n");
}

static void on_message(const char *json_line, void *user_data)
{
    (void)user_data;
    printf("\n  << %s\n", json_line);
    printf("frogLink> ");
    fflush(stdout);
}

static void print_help(void)
{
    printf("\n");
    printf("Commands:\n");
    printf("  <message>              Toggle a message (e.g., \"Desk Lamp\")\n");
    printf("  <message> on           Switch on\n");
    printf("  <message> off          Switch off\n");
    printf("  <message> bright <N>   Set brightness (0-100)\n");
    printf("  <message> time <Xm>    Timed operation (e.g., 5m, 30s)\n");
    printf("  info                   Show project info\n");
    printf("  messages               List configured messages\n");
    printf("  rooms                  List configured rooms\n");
    printf("  types                  List configured types\n");
    printf("  status <addr> <output> Query device output (e.g., status 0822 A)\n");
    printf("  raw <json>             Send raw JSON command\n");
    printf("  help                   Show this help\n");
    printf("  quit                   Exit\n");
    printf("\n");
}

/* Trim leading/trailing whitespace in place */
static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}


static void handle_input(froglink_t *fl, char *line,
                         char messages[][FL_MAX_NAME], int msg_count)
{
    char *input = trim(line);
    if (*input == '\0')
        return;

    char resp[FL_MAX_RESPONSE];

    if (strcmp(input, "help") == 0) {
        print_help();
        return;
    }
    if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0 ||
        strcmp(input, "q") == 0) {
        running = 0;
        return;
    }
    if (strcmp(input, "info") == 0) {
        fl_project_info_t info;
        if (fl_project(fl, &info) == FL_OK) {
            printf("  project:     %s\n", info.project);
            printf("  name:        %s\n", info.froglink_name);
            printf("  room:        %s\n", info.froglink_room);
            printf("  swVersion:   %s\n", info.sw_version);
            printf("  config:      %s\n", info.config);
            printf("  address:     %s\n", info.address);
            printf("  nadd:        %s\n", info.nadd);
            printf("  netId:       %s\n", info.net_id);
            printf("  api:         %s\n", info.api);
        }
        return;
    }
    if (strcmp(input, "messages") == 0) {
        char msgs[FL_MAX_MESSAGES][FL_MAX_NAME];
        int count = 0;
        fl_messages(fl, msgs, &count);
        for (int i = 0; i < count; i++)
            printf("  - %s\n", msgs[i]);
        return;
    }
    if (strcmp(input, "rooms") == 0) {
        char rooms[FL_MAX_ROOMS][FL_MAX_NAME];
        int count = 0;
        fl_rooms(fl, rooms, &count);
        for (int i = 0; i < count; i++)
            printf("  - %s\n", rooms[i]);
        return;
    }
    if (strcmp(input, "types") == 0) {
        char types[FL_MAX_TYPES][FL_MAX_NAME];
        int count = 0;
        fl_types(fl, NULL, types, &count);
        for (int i = 0; i < count; i++)
            printf("  - %s\n", types[i]);
        return;
    }
    if (strncmp(input, "status ", 7) == 0) {
        char target[64], output[8];
        if (sscanf(input + 7, "%63s %7s", target, output) == 2) {
            if (fl_query_output(fl, target, output, resp, sizeof(resp)) == FL_OK)
                printf("  %s\n", resp);
            else
                printf("  Error querying output.\n");
        } else {
            printf("  Usage: status <addr> <output>\n");
        }
        return;
    }
    if (strncmp(input, "raw ", 4) == 0) {
        if (fl_send_raw(fl, input + 4, resp, sizeof(resp)) == FL_OK)
            printf("  %s\n", resp);
        else
            printf("  Error sending raw command.\n");
        return;
    }

    /* Treat as message name — try to match against known messages */
    const char *msg_name = NULL;
    const char *remainder = NULL;
    int best_len = 0;

    for (int i = 0; i < msg_count; i++) {
        int mlen = (int)strlen(messages[i]);
        if (strncmp(input, messages[i], (size_t)mlen) == 0 &&
            (input[mlen] == '\0' || input[mlen] == ' ') &&
            mlen > best_len) {
            best_len = mlen;
            msg_name = messages[i];
            remainder = input[mlen] == ' ' ? input + mlen + 1 : NULL;
        }
    }

    if (!msg_name) {
        /* Use the first word as message name */
        static char first_word[FL_MAX_NAME];
        const char *sp = strchr(input, ' ');
        if (sp) {
            size_t len = (size_t)(sp - input);
            if (len >= FL_MAX_NAME) len = FL_MAX_NAME - 1;
            memcpy(first_word, input, len);
            first_word[len] = '\0';
            msg_name = first_word;
            remainder = sp + 1;
        } else {
            msg_name = input;
            remainder = NULL;
        }
    }

    /* Parse parameters from remainder */
    int on = -1;
    int bright = -1;
    const char *time_str = NULL;
    static char time_buf[32];

    if (remainder) {
        char params[256];
        strncpy(params, remainder, sizeof(params) - 1);
        params[sizeof(params) - 1] = '\0';

        char *tok = strtok(params, " ");
        while (tok) {
            if (strcasecmp(tok, "on") == 0) {
                on = 1;
            } else if (strcasecmp(tok, "off") == 0) {
                on = 0;
            } else if (strcasecmp(tok, "bright") == 0) {
                tok = strtok(NULL, " ");
                if (tok) bright = atoi(tok);
            } else if (strcasecmp(tok, "time") == 0) {
                tok = strtok(NULL, " ");
                if (tok) {
                    strncpy(time_buf, tok, sizeof(time_buf) - 1);
                    time_buf[sizeof(time_buf) - 1] = '\0';
                    time_str = time_buf;
                }
            }
            tok = strtok(NULL, " ");
        }
    }

    printf("  Sending: %s%s\n", msg_name,
           (on < 0 && bright < 0 && !time_str) ? " (toggle)" : "");

    fl_error_t err = fl_send_full(fl, msg_name, on, bright, time_str,
                                  resp, sizeof(resp));
    if (err == FL_OK)
        printf("  >> %s\n", resp);
    else
        printf("  Error sending message (code %d).\n", err);
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

    if (fl_project(&fl, &info) == FL_ERR_NOT_PROVISIONED) {
        printf("frogLink is not provisioned. Run detect first.\n");
        fl_disconnect(&fl);
        return 1;
    }

    printf("Connected to project: %s\n", info.project);
    printf("Device: %s in %s\n", info.froglink_name, info.froglink_room);
    printf("Frogware: %s | API: %s\n\n", info.sw_version, info.api);

    /* Load messages for tab-matching */
    char messages[FL_MAX_MESSAGES][FL_MAX_NAME];
    int msg_count = 0;
    fl_messages(&fl, messages, &msg_count);

    printf("Available messages: ");
    for (int i = 0; i < msg_count; i++)
        printf("%s%s", messages[i], i < msg_count - 1 ? ", " : "");
    printf("\n\n");

    fl_enable_messages(&fl, true);
    fl_enable_status(&fl, true);
    fl_start_listener(&fl, on_message, NULL);

    printf("Listener started — incoming messages will appear below.\n");
    printf("Type 'help' for commands.\n\n");

    signal(SIGINT, sigint_handler);

    char line[512];
    while (running) {
        printf("frogLink> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        /* Remove trailing newline */
        line[strcspn(line, "\n")] = '\0';

        handle_input(&fl, line, messages, msg_count);
    }

    fl_disconnect(&fl);
    printf("Bye.\n");
    return 0;
}
