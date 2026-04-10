/*
 * detect.c — Detect a frogLink USB device and report its state.
 *
 * Build: gcc -o detect detect.c ../froglink.c -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include "../froglink.h"

int main(void)
{
    froglink_t fl;
    fl_project_info_t info;

    printf("Searching for frogLink USB device...\n");

    char port[FL_MAX_PORT_PATH];
    if (fl_detect(port, sizeof(port)) != FL_OK) {
        printf("No frogLink USB device found.\n");
        printf("Check that it is plugged in and you have permission.\n");
        printf("  Linux: sudo usermod -aG dialout $USER  (then re-login)\n");
        return 1;
    }

    printf("Found frogLink at: %s\n\n", port);

    fl_init(&fl, port);

    if (fl_connect(&fl) != FL_OK) {
        printf("Failed to open serial port.\n");
        return 1;
    }

    fl_error_t err = fl_project(&fl, &info);

    if (err == FL_ERR_NOT_PROVISIONED) {
        printf("Status: UNPROVISIONED (factory state)\n\n");
        printf("The frogLink has no project configuration.\n");
        printf("To provision it:\n");
        printf("  1. Open the frogblue ProjectApp\n");
        printf("  2. Add the frogLink to your project\n");
        printf("  3. Assign messages, rooms, and types\n");
        printf("  4. Deploy the configuration\n\n");
        printf("Run this program again after provisioning.\n");
        fl_disconnect(&fl);
        return 0;
    }

    if (err != FL_OK) {
        printf("Error querying device (code %d).\n", err);
        fl_disconnect(&fl);
        return 1;
    }

    printf("Status: PROVISIONED\n\n");
    printf("Project Information:\n");
    printf("  Project:      %s\n", info.project);
    printf("  Device Name:  %s\n", info.froglink_name);
    printf("  Room:         %s\n", info.froglink_room);
    printf("  Frogware:     %s\n", info.sw_version);
    printf("  Config Date:  %s\n", info.config);
    printf("  MAC Address:  %s\n", info.address);
    printf("  Network Addr: %s\n", info.nadd);
    printf("  Net ID:       %s\n", info.net_id);
    printf("  API Version:  %s\n", info.api);
    printf("\n");

    /* Messages */
    char messages[FL_MAX_MESSAGES][FL_MAX_NAME];
    int msg_count = 0;
    fl_messages(&fl, messages, &msg_count);
    printf("Messages (%d):\n", msg_count);
    if (msg_count > 0) {
        for (int i = 0; i < msg_count; i++)
            printf("  - %s\n", messages[i]);
    } else {
        printf("  (none configured)\n");
    }
    printf("\n");

    /* Rooms */
    char rooms[FL_MAX_ROOMS][FL_MAX_NAME];
    int room_count = 0;
    fl_rooms(&fl, rooms, &room_count);
    printf("Rooms (%d):\n", room_count);
    if (room_count > 0) {
        for (int i = 0; i < room_count; i++)
            printf("  - %s\n", rooms[i]);
    } else {
        printf("  (none configured)\n");
    }
    printf("\n");

    /* Types */
    char types[FL_MAX_TYPES][FL_MAX_NAME];
    int type_count = 0;
    fl_types(&fl, NULL, types, &type_count);
    printf("Types (%d):\n", type_count);
    if (type_count > 0) {
        for (int i = 0; i < type_count; i++)
            printf("  - %s\n", types[i]);
    } else {
        printf("  (none configured)\n");
    }

    fl_disconnect(&fl);
    return 0;
}
