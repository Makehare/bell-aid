#include "app.h"
#include "board.h"
#include "driver/usb_serial_jtag.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static role_t s_role;

static void help(void)
{
    printf("commands:\n"
           "  status               show state\n"
           "  role auto|sender|receiver   NVS role override (reboots)\n"
           "  thr <30..99>         detection threshold in percent (sender)\n"
           "  copies <3..5>        RING repeats (sender)\n"
           "  alert 0|1            sender lights the full ring on detection/test, default 1 (sender)\n"
           "  heartbeat 0|1|on|off periodic green \"all ok\" blip on D1, default 1; same as 3 quick button presses (sender+receiver)\n"
           "  record <1..120>      stream <s> seconds of mic audio to the host (tools/record.py) (sender)\n"
           "  ring                 send a TEST ring (sender)\n"
           "  monitor 0|1          live level/score printout (sender)\n"
           "  cleartemplate        forget trained sound (sender)\n"
           "  rebroadcast 0|1      receiver re-broadcast once (receiver)\n"
           "  lr 0|1               Wi-Fi LR mode for ESP-NOW, must match on all devices (reboots)\n"
           "  unpair               forget paired sender (receiver)\n"
           "  newkey               new network key (sender; re-pair receivers)\n"
           "  reboot\n");
}

static void handle(char *line)
{
    char *cmd = strtok(line, " \t"), *arg = strtok(NULL, " \t");
    if (!cmd) return;
    int v = arg ? atoi(arg) : 0;
    if (!strcmp(cmd, "help")) help();
    else if (!strcmp(cmd, "status")) { if (s_role == ROLE_SENDER) sender_status_print(); else receiver_status_print(); }
    else if (!strcmp(cmd, "role") && arg) {
        uint8_t r = !strcmp(arg, "sender") ? 1 : !strcmp(arg, "receiver") ? 2 : 0;
        cfg_set_u8("role", r); printf("role override=%u, rebooting\n", r); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart();
    }
    else if (!strcmp(cmd, "thr") && arg && v >= 30 && v <= 99) { sender_cmd_threshold(v); printf("thr=%d%%\n", v); }
    else if (!strcmp(cmd, "copies") && arg && v >= 3 && v <= 5) { cfg_set_u8("copies", (uint8_t)v); printf("copies=%d\n", v); }
    else if (!strcmp(cmd, "alert") && arg && s_role == ROLE_SENDER) { sender_cmd_alert(v); printf("alert=%d\n", v ? 1 : 0); }
    else if (!strcmp(cmd, "heartbeat")) {
        int hv;
        if (dbn_parse_onoff(arg, &hv)) { hb_set(hv != 0); printf("heartbeat=%s\n", hv ? "on" : "off"); }
        else printf("usage: heartbeat 0|1 (or on|off); now %s\n", hb_enabled() ? "on" : "off");
    }
    else if (!strcmp(cmd, "record") && arg && v >= 1 && v <= 120 && s_role == ROLE_SENDER) sender_cmd_record(v);
    else if (!strcmp(cmd, "ring")) sender_cmd_ring();
    else if (!strcmp(cmd, "monitor") && arg) sender_cmd_monitor(v);
    else if (!strcmp(cmd, "cleartemplate")) { sender_cmd_cleartemplate(); printf("template cleared\n"); }
    else if (!strcmp(cmd, "rebroadcast") && arg) { cfg_set_u8("rebc", v ? 1 : 0); printf("rebroadcast=%d\n", v ? 1 : 0); }
    else if (!strcmp(cmd, "lr") && arg) { cfg_set_u8("lr", v ? 1 : 0); printf("lr=%d, rebooting\n", v ? 1 : 0); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else if (!strcmp(cmd, "unpair")) { cfg_erase("peer"); printf("unpaired, rebooting\n"); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else if (!strcmp(cmd, "newkey")) { cfg_erase("key"); printf("key erased, rebooting (new key generated on boot)\n"); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
    else if (!strcmp(cmd, "reboot")) esp_restart();
    else printf("unknown/invalid, try 'help'\n");
}

static void console_task(void *arg)
{
    char line[64];
    int n = 0;
    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (c == '\r' || c == '\n') {
            if (n) { line[n] = 0; printf("\n"); handle(line); n = 0; }
        } else if ((c == 0x7f || c == 8) && n) { n--; }
        else if (n < (int)sizeof(line) - 1 && c >= 32) { line[n++] = (char)c; putchar(c); fflush(stdout); }
    }
}

void console_start(role_t role)
{
    s_role = role;
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) return;
    xTaskCreate(console_task, "console", 4096, NULL, 2, NULL);
}
