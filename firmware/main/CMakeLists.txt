idf_component_register(
    SRCS "main.c" "cfg.c" "leds.c" "button.c" "radio.c" "sender.c" "receiver.c" "console.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_timer driver esp_driver_i2s esp_driver_gpio esp_driver_usb_serial_jtag esp_system detector dbn_common)
