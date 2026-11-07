# Release notes (bell-aid firmware)

## v1.2

- **Sender shows the doorbell too.** When the sender recognises the bell it now plays the same full-ring alert as the receivers (all 12 LEDs: white flash, then amber chase; 15 s), so someone standing near the door sees it as well. Idle status blips (pixel 0) are unchanged. Short press on the sender button during the alert stops it on that unit.
- Sender test button (short press): sends the test ring and plays the 3 s alert on the sender as well.
- New console command `alert 0|1` (sender, default 1, kept across reboots). `status` shows `alert=on|off`.
- Alert drawing moved into one shared function used by sender and receiver (receiver look is unchanged, checked by a host test).
- **Update:** app-only image keeps role, pairing, learned sound and threshold: `write_flash 0x20000 dist/doorbell-app.bin`. The merged image is not needed (bootloader and partition table are unchanged).
- No radio protocol change: v1.1 receivers and v1.2 senders work together.
