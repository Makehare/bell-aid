# Doorbell notifier firmware (bell-aid)

One ESP-IDF 5.4 image for the ESP32-S3-MINI-1-N8 on the shared DBN-MAIN board. The role is chosen at boot.

- **Sender**: I2S MEMS mic (16 kHz) -> on-device trainable detector -> ESP-NOW broadcast `RING`; shows the same full-ring alert itself (v1.2, `alert 0` turns it off).
- **Receiver**: ESP-NOW rx -> 12-px WS2812 ring flash, mute button, optional one-hop re-broadcast.

Pin map (ARCHITECTURE.md sec. 4.4, `main/board.h`): MIC SCK/WS/SD = GPIO4/5/6, LED_DIN = GPIO7, BTN_USER = GPIO9 (active low), ROLE = GPIO10, console/flash = native USB (GPIO19/20).

## Build

Needs only Docker (uses `espressif/idf:v5.4.2`):

    sh tools/build.sh          # builds, writes dist/doorbell-esp32s3-merged.bin, doorbell-app.bin, SHA256SUMS

First flash (blank board, any role): the merged image at offset 0:

    esptool.py --chip esp32s3 write_flash 0x0 dist/doorbell-esp32s3-merged.bin

Update (unit already set up): only the app at offset 0x20000. This keeps role, pairing, learned sound and threshold (exception: a sound learned with a firmware before detector v2 is erased once and must be trained again, see "Detector"):

    esptool.py --chip esp32s3 write_flash 0x20000 dist/doorbell-app.bin

**Warning:** the merged image erases all settings (role, pairing key, learned sound, threshold); every receiver must be paired again afterwards.

Console (115200 not needed, USB CDC): any serial terminal on the board's USB port; type `help`.

## Roles

1. NVS override (`role` command, or boot gesture below), if set.
2. Otherwise GPIO10 with internal pull-up: **LOW = sender** (R8 fitted), **HIGH = receiver**.

Prototype boards use universal population (every board has the ICS-43434 mic and all 12 pixels). R8 (ROLE) is DNP, so GPIO10 reads HIGH on every board: an unconfigured board always boots as **receiver**, and each sender must get the NVS role override once. Production: fit R8 on senders, no override needed. MIC_SD has a 100k pull-down (no firmware action). Set the role once. Either hold the user button while plugging in USB for 5 s (yellow D1, then 3 green blinks, device reboots in the other role), or use the console: `role sender` / `role receiver` / `role auto`.

Provisioning (prototype run, R8 DNP):

1. Flash the same merged image to every board. All boot as receiver (log: `no NVS role override ... -> RECEIVER`).
2. For each board that should be a sender: connect USB, open the console, type `role sender` (board reboots, log shows `role=SENDER`). Or hold the user button while plugging in USB for 5 s.
3. The override is stored in NVS and survives reboots and app-only updates (flashing the merged image or a full flash erase clears it). `role auto` removes the override; the GPIO10 strap then decides again.
4. Test pads on the back (TP1 U0TXD, TP2 U0RXD, TP8 EN, TP9 GPIO0) are available for serial recovery if USB fails.

Both roles use all 12 pixels (universal population). When idle, the sender uses only pixel 0 (D1) for status blips; on a detection (and on its test button) it plays the full-ring alert like the receivers (v1.2).

## Button

| Press | Sender | Receiver |
|-------|--------|----------|
| Short (<1.5 s) | Send a TEST ring (also: finish/abort training) | Toggle mute. While flashing: dismiss the flash only |
| Long (1.5-5 s) | Start training | Local test flash (3 s) |
| Very long (>5 s) | Pairing mode (30 s) | Pairing mode (60 s) |

D1 turns yellow while the long threshold is reached and magenta at the very-long threshold, so you know when to release. Events fire on release.

## Training the doorbell (sender)

1. Hold the button 1.5-5 s, release. D1 pulses blue ("waiting").
2. Ring the real doorbell (or play it from a phone/laptop) and let it play to the end. D1 is cyan for the whole ring, then flashes green. If the bell push is near the sender, D1 may already turn cyan on its click; that is expected (see below).
3. Wait at least 3 s, ring it again. Up to 3 rings in total (3 is best).
4. Training ends after the 3rd ring, or 8 s after the last ring, or on a short press. Green x3 = saved, red x3 = failed or aborted (no ring heard within 25 s).
5. Check: wait a few seconds, then ring once more. The sender plays the full-ring alert (white/amber chase on all 12 pixels, 15 s; with `alert 0` D1 only flashes white for 0.6 s) and every paired receiver must light up (unless muted). If they do not, train again and look at the score with `monitor 1` (below). Green x3 only means that 3 rings were recorded, not that the learned sound matches.

A ring lasts from its onset until the sound has been quiet for 2 s, so a melody with pauses between its notes (Westminster, several seconds) is learned as one ring. Up to 8 s per ring are kept; longer melodies are recognised by their first 8 s. Keep the room quiet during training: talking or other sounds in the 2 s after the last note make the ring longer than it is. Short bursts at the start or end of a ring (the click of the bell push, a knock, a door) are cut off, so for chimes and melodies they are not part of the learned sound. This does not work for very short bells: nothing is cut when less than 0.64 s of sound would remain, so a single short "ding" (about 0.8 s or less) can still learn the click with it. In tests with a click or knock before every ring, such bells were detected only 32-47 % of the time, and training again repeats the problem. With a short bell, keep the click away from the sender during training: have someone press the push with the door closed while you stay at the sender, or record the bell with a phone next to the chime unit (where the push is not heard) and play it about 1 m from the sender. Step 5 shows whether it worked.

From the 3 rings, the most typical one is the template and the others are averaged into it; a "ring" that resembles none of the others (a door slam during training) is ignored. Template and threshold are stored in NVS and survive reboots and app-only updates. The default threshold is 0.74.

Train with the sender mounted where it will live. When training from a video, use a phone or laptop at a normal listening volume about 1 m away.

Tuning (console): `monitor 1` prints `db / floor / score / thr` ~4x per second. Ring the bell and watch the peak score (the score rises while the ring plays and fires at `thr`); then talk and make kitchen noise next to the sender and watch those scores. `thr 72` lowers the threshold (more sensitive; below 72 speech and TV start to trigger it on the evaluation set), `thr 78` raises it (fewer false alarms). The setting is stored; re-training or `cleartemplate` (forgets the sound) resets it to the default.

### Recording what the sender hears

`record <seconds>` (sender, 1-120 s) streams the microphone signal over the USB console, exactly as the detector receives it. On a PC with Python 3 (`pip install pyserial` recommended):

    python3 tools/record.py 40 -o mybell.wav            # port is auto-detected; or --port /dev/ttyACM0 / COM5

Close any other serial monitor first. Ring the bell 3 times during the recording, 4 s or more apart. The tool reports missing or corrupt hops; detection keeps running while recording. Then try the detector on the PC:

    make -C test/host wavtool
    test/host/wavtool train mybell.wav -- test mybell.wav other.wav

The `TRAIN` line shows how the rings were cut (`rings_s=`), each `FILE` line the number of matches and the peak score.

Sender LED: green blip every 3 s = armed, orange blip = no template yet. On a detection the sender plays the full-ring alert (see below).

## Pairing

Receivers only act on packets from the sender they are paired to, identified by sender MAC plus an 8-byte network key that the sender generates on first boot.

1. Receiver: unpaired receivers listen for 60 s after boot (orange blink = unpaired, blue fast blink = listening). To re-enter pairing on a paired receiver: hold the button >5 s.
2. Sender: hold the button >5 s (magenta, then release). D1 blinks magenta and the sender broadcasts the beacon for 30 s (short press cancels).
3. The receiver stores sender MAC + key in NVS, and blinks green x3. Repeat for each receiver (up to 5; there is no hard limit).

`unpair` (receiver) forgets the sender. `newkey` (sender) rotates the key; re-pair all receivers afterwards. Note: the key is a spoof/crosstalk guard against a neighbour's unit, not cryptography; ESP-NOW broadcast cannot be encrypted.

## Radio behaviour

- Channel 1, ESP-NOW broadcast. Packet (23 bytes, `dbn_common.h`): magic, version, type (RING/TEST/PAIR_BEACON), event seq, copy index, hop, sender MAC, key, CRC-8.
- Sender sends each RING 3-5 times (`copies`, default 4) with 30-55 ms jittered spacing and one seq per event.
- Receiver dedupes by seq, flashes on the first copy, and (default on, `rebroadcast 0` to disable) re-broadcasts it once with `hop=1` after 15-60 ms to cover dead spots. Re-broadcast copies are never re-broadcast again.
- Sender alert (v1.2, `alert 1`, default on; `alert 0` = old behaviour, D1 flashes white for 0.6 s only): on a detection the sender plays the exact same full-ring alert as a receiver (`dbn_alert_pixel`, one shared function): 800 ms cycle of all 12 pixels white 300 ms, off 100 ms, six amber pixels chasing clockwise 400 ms, for 15 s. A short press of the sender button during the alert stops it on this unit. The test button (short press when idle) sends a TEST ring and plays the 3 s alert on the sender too. The alert only runs in the idle (armed) state, never during training or pairing. The setting is stored in NVS (`salert`) and survives reboots; the radio is not delayed by the alert. Idle status blips (pixel 0) are unchanged.
- Receiver flash: 15 s of white flash + amber chase, then stops. A muted receiver ignores RING but still shows TEST (so the link can be checked). Mute is RAM only (cleared on reboot).
- LED current: estimated at 20 mA per colour channel at 255 (WS2812B); the whole frame is scaled down to <= 600 mA (full white 12 px = 720 mA -> 83 %). Verify the real per-pixel current on the bench (`LED_CAP_MA`, `DBN_LED_MA_PER_CH_FULL`).
- Status pixel D1 on receiver: green heartbeat = ok, steady dim red = muted, orange blink = unpaired, blue blink = pairing.

### ESP-NOW Long Range (LR) on ESP32-S3

Supported: ESP-IDF 5.4 documents LR for all ESP32-series chips except C2, and the S3 build accepts `WIFI_PROTOCOL_LR` and `esp_now_set_peer_rate_config(... WIFI_PHY_MODE_LR, WIFI_PHY_RATE_LORA_250K)`. It is implemented as an option, **off by default**: `lr 1` on the console (reboots). With `lr 1` a device enables BGN+LR and the sender transmits at the 250 kbps LR rate. It must be set on ALL devices; a receiver with `lr 0` cannot hear an LR sender. Not verified on hardware yet: measure range with and without LR in the actual apartment before enabling it by default.

## Console commands

`status`, `role auto|sender|receiver`, `thr 30..99`, `copies 3..5`, `alert 0|1`, `record 1..120`, `ring`, `monitor 0|1`, `cleartemplate`, `rebroadcast 0|1`, `lr 0|1`, `unpair`, `newkey`, `reboot`, `help`.

## Detector (components/detector, pure C)

**Features.** 16 kHz, hop 512 samples (32 ms), 1024-sample Hann window, FFT. 54 semitone bands from 300 Hz to ~6.4 kHz; per band the strongest bin, in dB above an adaptive per-band noise floor (floor + 6 dB). This keeps the tonal structure of a bell (sharp, stable peaks at its notes) and its level relative to the room, which v1's per-frame normalised 32-band envelope threw away.

**Matching.** Each hop is reduced to the bands within 30 dB of its strongest band; bands that are not a local spectral peak count only 35 %. A bell becomes a few sharp peaks, while speech, clatter and TV become flat or moving patterns. Hops whose strongest band is less than 10 dB above floor + 6 dB are "pause". The live hops are aligned to the whole template with streaming subsequence DTW (diagonal steps, plus 2x faster or 2x slower), so tempo differences and pauses between notes are tolerated. The cost per hop is cosine distance. Template pauses weigh 0.2 and match live pauses; live sound during a template pause costs 0.7. Score = 1 - weighted mean cost. A long template may fire once 2 s of it and at least 25 sounding hops have matched, so a melody is reported during its first notes and cannot fire on a pause. Then the detector is locked for the rest of the template + 1 s.

**Why this design.** A template-matching detector trained on the device keeps the training flow (ring 3x), runs in pure C and needs no model training. DTW over the full ring handles multi-second melodies with pauses, which a fixed onset window cannot. Spectral peaks instead of envelopes separate tonal bells from speech. A neural classifier would need a "doorbell" dataset and still not know the user's bell.

**Training.** A ring starts when the level rises 12 dB above the room floor. It ends after 2 s below max(floor + 6 dB, ring peak - 25 dB), max 8 s. The band floors are frozen while a ring is recorded. Then short bursts are cut off its start and end: the ring is split into parts at quiet gaps (>= 0.1 s) or at a new attack (level back up 10 dB after falling 10 dB), and a first or last part that sounds <= 0.3 s, is loud (within 10 dB of its peak) for <= 0.16 s and is at most 1/3 of the longest part is dropped, unless less than 0.64 s of sound would remain. This removes the bell-push click that otherwise starts the recording 0.2-1.2 s before the chime (see test/EVALUATION.md, "Real doorbell"). It does not help bells with less than ~0.8 s of sound: there the 0.64 s guard keeps the click (workaround under "Training the doorbell"). Then: medoid of the rings by DTW score, others aligned and averaged into it.

**Cost.** Template up to 250 hops x 54 bands (13.5 KB NVS blob `tmpl2`, NVS partition 64 KB), ~76 KB RAM for the detector state including the 3 training recordings, one 1024-point FFT and one DTW column (<= 250 x 54 MACs) per 32 ms.

**Upgrade.** The stored template has a magic and version field. v1 templates (`tmpl`, `tlen`, and the old `thr`) cannot be converted. They are erased on first boot, and the sender shows "untrained" (orange blip) until it is trained again.

**Evaluation.** On a public set of 14 doorbells (5 long melodies up to 11 s) plus one real front-door chime, trained on 3 rings and tested on held-out rings with household/TV background, also played via a phone speaker and with a bell-push click before every ring:
- public set, direct + speaker: 96.7 % of held-out rings detected (638/660), all 5 melodies 100 %;
- with a bell-push click before the chime: 95.5 % (previous firmware: 85.9 %); the real front-door chime 100 % in all three conditions (previous firmware with click: 71 %);
- bells with less than ~0.8 s of sound and a click or knock before every ring: only 32-47 % (independent QA set, 3 bells), see "Training the doorbell";
- 0 false triggers in 173 min of speech/TV/household and 21 min of clicks, knocks, keys, doors, switches, footsteps and claps per template (177 templates);
- v1 on the public set: 40.8 % detected and 43 false triggers per hour.

Details, per-bell numbers and limits: [test/EVALUATION.md](test/EVALUATION.md).

## Tests

Host tests (gcc, no hardware):

    docker run --rm -v $PWD:/p -w /p/test/host espressif/idf:v5.4.2 make test

(or `make -C test/host test` with a local gcc.)

- `test_detector`: synthetic audio. A ding-dong and a 5.5 s melody with a pause are each trained on 3 rings, then 30 positives each (random level, noise and onset) and negatives: noise, a different chime, noise bursts, speech-like audio, knocks, 1 kHz beeps, another melody. Also the threshold, a ring held longer, training with a loud bell-push click before every ring (the click must not end up in the template, and clicks alone must not fire), the template blob round trip, and rejection of old (v1) blobs.
- `test_proto`: packet build/parse/corruption rejection, dedupe, LED current cap, full-ring alert pattern (compared against the v1.1 receiver reference over 30 s, all 12 pixels, 15 s / 3 s durations).
- `test_record.py`: `tools/record.py` against a simulated sender (`fake_sender`), clean and with dropped/corrupt lines.
- `wavtool`: the detector on WAV files (16 kHz mono s16): `wavtool train rings.wav -- test a.wav b.wav`. `-o tmpl.bin` saves the template, `load tmpl.bin -- test ...` reuses it, `-t 0.70` sets the threshold.
- Real-audio evaluation (public doorbells, speech, TV, household noise; data downloaded separately): see `test/eval/README.md` and the results in `test/EVALUATION.md`.

## Status / not verified

- Builds clean for esp32s3 (IDF 5.4.2, no warnings). Not yet run on hardware: I2S mic sample format/gain, RMT drive through the 74AHCT1G125, real radio range, LR benefit. Mic bit-shift (`raw >> 14` in `sender.c`) assumes a 24-bit left-aligned ICS-43434 frame.
- Detection is evaluated on public recordings and simulated speaker playback, not yet with the sender's own microphone. Check with `record` + `wavtool` on the first unit.
- No OTA (partition table has only nvs + phy + 3 MB factory app).
