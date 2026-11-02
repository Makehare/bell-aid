# Detector evaluation

Detector v2 (`components/detector`) on the evaluation set described in
[eval/README.md](eval/README.md). Every number below comes from `test/eval/run_eval.py` with the
firmware defaults (threshold 0.74). "Previous" is the v2 detector before the bell-push fix
(burst trimming in training, see "Real doorbell"); v1 is the original detector. Not yet verified
with the sender's own microphone: see "Limits".

## Method

- 15 doorbells: 11 public recordings, 3 generated melodies and `home-bell`, a real front-door
  chime that the detector failed on in a field test (phone recording, private, not distributed).
  5 are long melodies (4.6-11.2 s, with pauses between notes).
- Leave-one-ring-out: train on a recording with 3 other rings (the device's own training
  segmentation, as on the sender), then test the held-out ring 6 times (level +-6 dB, reverb,
  household or TV background at 10-30 dB SNR). 7 bells (the 3 generated melodies, the 2
  Westminster recordings and `home-bell`) have a single recording, so their "held-out" rings are
  copies of that recording with new room, level and background.
- Three conditions: `direct` (the bell itself), `speaker` (the bell played from a phone or laptop,
  as when training from a video) and `click` (the click of the bell push 0.2-1.2 s before the
  chime, -6..+12 dB against the chime's peak, in training and test).
- 177 trained templates. Each template is also run over all negatives: 173 min core (speech, TV,
  household), 21 min transients (keys, light switches, doors, push buttons, footsteps, knocks,
  claps, bell-push clicks), 97 min outdoor, 23 min hard (ring tones, other bells, alarm clocks).
- A ring counts as detected if the detector fires during it (in `click`: from the chime on). Any
  other firing is a false trigger, including a firing on the click before the chime.

## Results

| Doorbell | Kind | Template s | Detected direct | Detected speaker | Detected click (previous -> now) | False triggers core / transient / outdoor / hard (all conditions) |
|---|---|---|---|---|---|---|
| westminster-a | Westminster melody, 8 notes, 2 phrases with pause, 8.4 s | 8.0 | 24/24 (100 %) | 24/24 (100 %) | 24/24 (100 %) | 0 / 0 / 0 / 0 |
| westminster-b | Westminster melody, 8 sustained notes, 11.2 s | 8.0 | 24/24 (100 %) | 24/24 (100 %) | 23/24 (96 %) | 0 / 0 / 0 / 0 |
| synth-westminster | generated melody, 8 notes | 8.0 | 24/24 (100 %) | 24/24 (100 %) | 24/24 (100 %) | 0 / 0 / 0 / 0 |
| synth-tune8 | generated melody, 8 notes | 6.1 | 24/24 (100 %) | 24/24 (100 %) | 24/24 (100 %) | 0 / 0 / 0 / 0 |
| synth-slow3 | generated, 3 slow notes, 1.2 s gaps | 3.8-4.3 | 24/24 (100 %) | 24/24 (100 %) | 21/24 (88 %) -> 24/24 (100 %) | 0 / 0 / 0 / 0 |
| home-bell | real front door: electronic 3-tone chime (F6, C#6, G#5) played twice, 1.4 s | 1.6 | 24/24 (100 %) | 24/24 (100 %) | 17/24 (71 %) -> **24/24 (100 %)** | 0 / 0 / 0 / 0 |
| dingdong-a | ding-dong | 1.2-1.3 | 18/24 (75 %) -> 21/24 (88 %) | 22/24 (92 %) | 14/24 (58 %) -> 21/24 (88 %) | 0 / 0 / 0 / 0 |
| dingdong-c | ding-dong | 3.1 | 18/18 (100 %) | 18/18 (100 %) | 18/18 (100 %) | 0 / 0 / 0 / 0 |
| dingdong-d | ding-dong | 1.7-2.2 | 41/42 (98 %) | 42/42 (100 %) | 40/42 (95 %) -> 41/42 (98 %) | 0 / 0 / 0 / 0 |
| electric-dingdong | electric ding-dong | 3.3 | 18/18 (100 %) | 18/18 (100 %) | 18/18 (100 %) | 0 / 0 / 6 / 0 (previous 0 / 0 / 4 / 0) |
| strike-b | single strike, long decay | 2.0-2.2 | 18/18 (100 %) | 18/18 (100 %) | 18/18 (100 %) | 0 / 0 / 7 / 39 (previous 0 / 0 / 5 / 25) |
| chime-cluster | multi-tone chime | 2.7-3.1 | 22/24 (92 %) | 23/24 (96 %) | 8/24 (33 %) -> 20/24 (83 %) | 0 / 0 / 0 / 0 |
| mech-bell | mechanical bell, rapid strikes | 2.4-2.6 | 24/24 (100 %) | 24/24 (100 %) | 21/24 (88 %) -> 23/24 (96 %) | 0 / 0 / 0 / 0 |
| buzzer-held | harsh buzzer, held 2.6-9 s | 3.8 | 18/18 (100 %) | 18/18 (100 %) | 16/18 (89 %) -> 18/18 (100 %) | 0 / 0 / 0 / 0 |
| buzzer-tone | tonal buzzer, presses of 0.9-2.7 s | 1.1-1.3 | 18/24 (75 %) | 17/24 (71 %) | 18/24 (75 %) | 0 / 0 / 0 / 0 |

Template s = mean length of the trained template (max 8 s). Direct and speaker are unchanged
from the previous detector except dingdong-a direct. The extra false triggers on
electric-dingdong (+2) and strike-b (+2 / +14) all come from their `click` templates, which are
now the bell alone, like in `direct`; in `direct` and `speaker` the false triggers are the same as
before. Full per-condition
table with latency: `python3 test/eval/run_eval.py --md results.md`.

| | v1 | v2 previous | v2 now |
|---|---|---|---|
| Public set, direct + speaker (14 bells, 110 templates) | 269/660 (40.8 %) | 635/660 (96.2 %) | **638/660 (96.7 %)** |
| Long melodies (5 bells, direct + speaker) | 109/240 (45 %) | 240/240 (100 %) | **240/240 (100 %)** |
| Bell-push click before the chime (15 bells) | - | 304/354 (85.9 %) | **338/354 (95.5 %)** |
| home-bell (direct / speaker / click) | - | 24/24 / 24/24 / 17/24 | **24/24 / 24/24 / 24/24** |
| All 177 templates, all conditions | - | 987/1062 (92.9 %) | **1024/1062 (96.4 %)** |
| Templates that never fire on their held-out ring | - | 5/177 | **2/177** (both buzzer-tone) |
| False triggers, core (speech/TV/household, 511 template-hours) | 43 per hour | 0 | **0** |
| False triggers, transients (63 template-hours) | - | 0 | **0** |
| False triggers, ESC-50 transient classes inside core (knock, click, typing, clapping, footsteps, ...) | - | 0 | **0** |
| False triggers, outdoor (285 template-hours) | 8,918 (110 templates) | 9 | 13 (siren 10, frog 3) |
| False triggers, hard (69 template-hours) | 1,166 (110 templates) | 25 | 39 (other bells, all on strike-b templates) |

Mean detection latency after the chime starts: 1.1 s over all bells (0.5-1.4 s for the short
bells, 1.1-1.7 s for the melodies; a long template fires once ~2 s of it has matched).

Threshold sweep (all 1062 held-out rings / core-negative clips with a false trigger):

| Threshold | 0.66 | 0.68 | 0.70 | **0.72** | **0.74** | 0.76 |
|---|---|---|---|---|---|---|
| Detected, previous | 98.5 % | 97.8 % | 96.2 % | 94.5 % | 92.9 % | 90.8 % |
| Detected, now | 99.5 % | 99.2 % | 98.4 % | 97.8 % | **96.4 %** | 94.2 % |
| Core FP clips (both) | 206-340 | 83-113 | 24-25 | 0 | **0** | 0 |

The first core false trigger is still at 0.70, so the default 0.74 keeps its margin; do not set
`thr` below 72.

## Real doorbell (home-bell): root cause and fix

**Field result.** The previous detector trained "green x3" on the real doorbell, then did not
react to the chime. During training the LED already turned cyan (recording) on the press of the
bell push, before the chime.

**The bell.** An electronic chime: three square-wave tones, F6 1391 Hz, C#6 1109 Hz, G#5 828 Hz
(odd harmonics at 3x, e.g. 4172, 3328, 2500 Hz), each ~0.22 s, the phrase played twice, 1.4 s in
total, abrupt end (-35 dB within 60 ms). Phone recording: peak -16 dBFS, room noise ~40 dB below
the chime. All partials lie inside the detector's 300 Hz-6.4 kHz semitone bands, the level is far
above the 12 dB onset threshold, and the ring is shorter than the 2 s prefix, so the whole 1.5 s
ring is the template. The phone (AAC 48 kHz) and the sender mic (MEMS, 16 kHz, `raw >> 14` =
+12 dB, clips at about 108 dB SPL) both pass this band flat; the chime is not near clipping on
either. Trained and tested on the recording itself, the previous detector scores 0.99, and in the
`direct` and `speaker` conditions 24/24. So the bell itself is not the problem.

**Root cause: the bell-push click becomes part of the template.** With the push near the sender,
its click is the onset that starts the recording (LED cyan on the press). The chime follows
0.2-1.2 s later, inside the 2 s "still ringing" window, so the click, the gap and the chime are
stored as one ring. The previous rule that restarts a ring on a much louder sound needs +12 dB,
which a chime rarely has over a close click. In matching, the click hops weigh as much as the
chime, and the click and the press-to-chime delay are different on every press. Training still
looks perfect (the 3 rings score 0.83-0.90 against each other), but later rings score lower.
Host reproduction with the real recording plus a push click (`sim_leon` setup: 3 training rings
4-6 s apart, room reverb RT60 0.25-0.7 s, level -30..-16 dBFS, MEMS-mic band limit, household/TV
background at 10-30 dB SNR; click +6 dB against the chime peak), true peak score per test ring
(threshold 0.99 for the measurement):

| Trial | Previous: template (chime is 1.5 s) | Previous: peak score of 6 test rings | Now: template | Now: peak score |
|---|---|---|---|---|
| 0 | 2.30 s | 0.85-0.89 | 1.54 s | 0.93-0.97 |
| 1 | 2.66 s | 0.77-0.87 | 1.60 s | 0.92-0.97 |
| 2 | 2.66 s | **0.65-0.70: never fires** | 1.57 s | 0.87-0.90 |
| 3 | 2.37 s | 0.76-0.90 | 1.73 s | 0.91-0.95 |
| 4 | 2.50 s | **0.57-0.74: fires 1 of 6** | 1.50 s | 0.79-0.94 |
| 5 | 2.72 s | 0.75-0.86 | 1.60 s | 0.94-0.96 |

Without a click both detectors give the same 1.50-1.66 s templates and 0.83-0.97. Over 12
trainings per click level (detected test rings / templates that never fire):

| Click vs chime peak | -12 dB | -6 dB | 0 dB | +6 dB | +12 dB | +20 dB |
|---|---|---|---|---|---|---|
| Previous | 69/72, 0 | 68/72, 0 | 56/72, 2 | 45/72, 3 | 45/72, 3 | 45/72, 1 |
| Now | 72/72, 0 | 72/72, 0 | 72/72, 0 | 72/72, 0 | 72/72, 0 | 72/72, 0 |

The same mechanism explains part of the "dead templates" of the earlier QA check (training
green x3, template never fires): dingdong-a/direct/fold3 had stray noise before the ring and is
fixed now; the remaining 2 are buzzer-tone (presses of different length).

**Fix (training only, detection unchanged).** After a ring is recorded, short bursts are cut off
its start and end (`trim_bursts` in `detector.c`): the ring is split into parts at quiet gaps of
>= 0.1 s or at a new attack (level back up 10 dB after a 10 dB decay, for a click whose echo
bridges the gap); a first or last part that sounds <= 0.3 s, is loud (within 10 dB of its peak)
for <= 0.16 s and is at most 1/3 of the longest part is dropped, unless less than 0.64 s would
remain. Bell notes ring longer than a click, and notes of a melody are not that much shorter than
its longest note, so melodies keep all notes (synth-slow3 with 0.5 s notes and 1.2 s gaps: 100 %).
Nothing is special-cased for this bell; the thresholds were chosen on the whole set (a stricter
0.1 s "loud" limit lost click cases, a looser 0.22 s one cut the first note of synth-slow3).

**Not confirmed on the device.** Whether the sender really hears a loud click depends on where
the push and the sender are. In the phone recording two weak sounds precede the chime (1.4 s and
1.8 s, ~1.2 kHz, ~12 dB above the room), probably press and release; even raised by 24-30 dB and
with 0-0.8 s jitter they keep the score at 0.84-0.94, so a sharper, louder click at the sender
(not audible in the phone recording) is the likely trigger. A `record` capture from the sender
confirms it (README, "Recording what the sender hears"): press the push and let the chime play,
wait 5 s, press again, wait 5 s, knock on the door; then `wavtool train capture.wav -- test
capture.wav` shows the ring lengths (`rings_s=`, 1.5 s expected for this chime) and the scores.

## Acceptance

- At least 5 doorbells including at least 2 long melodies (>= 3 s), trained on 3 rings each, with
  >= 95 % of held-out rings detected: **met** (12 of 15 bells >= 95 % in `direct` and `speaker`,
  all 5 long melodies 100 %; public set 96.7 %).
- 0 false positives on >= 30 min of speech/TV/household at the default threshold: **met** (0 in
  173 min x 177 templates), and 0 on 21 min of transients plus 27 min of ESC-50 transient classes.
- home-bell: 100 % in all three conditions.
- Not met for 3 of the 15 bells: buzzer-tone (73 %), chime-cluster (92-96 %, 83 % with click),
  dingdong-a (88-92 %).
  - buzzer-tone: the presses vary in length (1-2.5 s). The template is one ring, so a short press
    does not cover a long template.
  - dingdong-a and chime-cluster: scores sit just around the threshold (0.70-0.74 in the missed
    cases).

## Root causes of the v1 failures

**False triggers on voice and noise.** v1 compared each frame by its spectral shape only: the log
energy of 32 wide bands, mean-removed and normalised to unit length. That throws away loudness
and blurs the tonal partials, so any broadband sound (voice, clatter, TV) produced vectors close
to a short bell onset. The template was only 0.77 s long, and a match needed only a mean cosine
of 0.70-0.90. On the evaluation set this gave 43 false triggers per template-hour of
speech/TV/household.

**Long melody never detected.** v1 recorded a fixed 24 frames (0.77 s) after an onset. It then
treated 0.5 s of quiet as "ring over" and re-armed. A melody with pauses between notes was
therefore cut into several "rings" during training. For example, one 8.4 s Westminster ring was
counted as 2 rings. The template and its automatic threshold then came from different notes. In
detection only the first 0.77 s was compared, and frames below floor + 6 dB scored 0, so the live
melody rarely reached the threshold. On the evaluation set v1 detected 45 % of
long-melody rings (4-75 % per bell and condition).

## Limits

- No recording from a real sender yet: mic gain (`raw >> 14`), the real room, the position of the
  bell push and the real speaker used for training are not covered. Use `record` (README) to
  capture what the sender hears and run `wavtool` on it.
- The detector parameters and the burst limits were chosen on this set; an independent set of 15
  other public bells gave 91.9 % with the previous detector (QA check).
- Margins are small (held-out rings mostly score 0.73-0.83, the highest core negative 0.71), so the
  threshold matters. `monitor 1` shows the live score for tuning.
- Training feedback (green x3) does not show whether the learned sound matches: ring once more
  after training (README, step 5).
- Hard negatives: a single-strike bell template also fires on other bells and chimes (all 39
  hard-negative triggers). A unit trained on a simple "ding" may react to a TV doorbell or a
  neighbour's chime.
