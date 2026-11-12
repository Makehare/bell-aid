# Release notes (bell-aid firmware)

## v1.3.1

- **Web updater page only; firmware unchanged.** `doorbell-app.bin` is byte-identical to v1.3 (same sha256); devices already on v1.3 need no update. The version label is not compiled into the firmware.
- Updater page footer now links the legal notice (Impressum, https://makehare.com/impressum.html) and a new own privacy notice `docs/update/datenschutz.html` (DE + EN). The page still sets no cookies and makes no third-party requests.
- DE manual wording tightened (erase warning, safety bullet, cable figure).
- Manual: DE erase warning aligned with EN, and the EN erase warning now names what is lost (role, pairing, trained doorbell sound). Minor DE wording tightened to keep 4 pages: section 7 intro, section 8 "Entsorgung" and "Vor der Abgabe". No safety content removed.

## v1.3

- **Triple press turns the green heartbeat on/off** (sender and receiver). Three short presses, each gap <= 0.4 s. Confirmation on D1: green x2 = on, one slow green fade-out = off. Stored in NVS (`hb`), default on, survives power loss. Console: `heartbeat 0|1|on|off` (anything else prints the usage and changes nothing); `status` shows `heartbeat=on|off`.
- Only the periodic green "all ok" blip is suppressed. Orange (no template / unpaired), blue (pairing), the receiver's steady dim red (muted), alerts and training feedback are unchanged.
- **Behaviour change:** a short press (TEST on the sender, mute on the receiver) now acts 0.4 s after release (multi-click window). Dismissing a running alert, and short presses in training/pairing, stay immediate. Double press (and 4+) does nothing. Long, very long, pairing, training and the power-up role hold are unchanged.
- New pure-C click classifier `dbn_click` with host tests (`test/host/test_click.c`).
- **Update:** app-only image keeps everything: `write_flash 0x20000 dist/doorbell-app.bin`. No radio protocol change; v1.2 devices work with v1.3.

## v1.2

- **Sender shows the doorbell too.** When the sender recognises the bell it now plays the same full-ring alert as the receivers (all 12 LEDs: white flash, then amber chase; 15 s), so someone standing near the door sees it as well. Idle status blips (pixel 0) are unchanged. Short press on the sender button during the alert stops it on that unit.
- Sender test button (short press): sends the test ring and plays the 3 s alert on the sender as well.
- New console command `alert 0|1` (sender, default 1, kept across reboots). `status` shows `alert=on|off`.
- Alert drawing moved into one shared function used by sender and receiver (receiver look is unchanged, checked by a host test).
- **Update:** app-only image keeps role, pairing, learned sound and threshold: `write_flash 0x20000 dist/doorbell-app.bin`. The merged image is not needed (bootloader and partition table are unchanged).
- No radio protocol change: v1.1 receivers and v1.2 senders work together.
