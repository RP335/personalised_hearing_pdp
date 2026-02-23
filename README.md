# personalised_hearing_pdp

Personalised hearing protection firmware for Tympan Rev F (Teensy 4.1).  
PDP course, Aalto University — collaboration with Savox-OTOS.

The system reads an audiogram (hearing test data), computes frequency-dependent gain using the NAL-R prescription algorithm, and applies it in real time through an 8-band IIR filterbank with optional WDRC compression and a broadband safety limiter.

## Signal flow

```
Line In → Pre-Gain → 8-Band Biquad Filterbank
                        ├── Band 0 (<200 Hz)    → Gain → WDRC → ┐
                        ├── Band 1 (200–500 Hz)  → Gain → WDRC → ┤
                        ├── Band 2 (500–1k Hz)   → Gain → WDRC → ┤
                        ├── Band 3 (1k–1.5k Hz)  → Gain → WDRC → ├── Mixer → BB Limiter → L Out
                        ├── Band 4 (1.5k–2.5k Hz)→ Gain → WDRC → ┤                     → R Out
                        ├── Band 5 (2.5k–4k Hz)  → Gain → WDRC → ┤
                        ├── Band 6 (4k–6.5k Hz)  → Gain → WDRC → ┤
                        └── Band 7 (>6.5k Hz)    → Gain → WDRC → ┘

Line In → A-Weighting → Level Meter (fast/slow) → Metrics → SD / BLE
```

The filterbank uses `AudioFilterbankBiquad_F32` (6th-order IIR biquad cascade). Each band has a separate gain block for NAL-R insertion gain and a WDRC compressor for dynamics processing. The broadband limiter at the output enforces a 91 dB SPL ceiling.

Left and right ears are processed independently with separate filterbank instances, gain blocks, and compressors. Each ear receives its own NAL-R gain curve based on its audiogram thresholds.

## Architecture

```
PersonalizedHearingProtection.ino   — main loop, state machine, serial dispatch
Config.h                            — all constants, DSP parameters, pin assignments
AudioGraph.h / .cpp                 — audio objects, patch cords, codec init
DSPManager.h / .cpp                 — filterbank design, gain/WDRC control, toggles
NALR.h / .cpp                       — NAL-R prescription algorithm
NFCReader.h / .cpp                  — PN532 audiogram card reader (optional)
Metrics.h / .cpp                    — SPL, LAeq, noise dose computation
SDLogger.h / .cpp                   — SD card CSV logging
BLESync.h / .cpp                    — Bluetooth metrics streaming
OLEDDisplay.h / .cpp                — SSD1306 status display
SerialControl.h / .cpp              — USB serial command interface
```

Only `Config.h`, `AudioGraph`, `DSPManager`, and `NALR` are needed for algorithm testing. The peripherals (NFC, OLED, SD, BLE) are optional and gracefully skip initialisation if hardware is not connected.

## NAL-R algorithm

The NAL-R (National Acoustic Laboratories — Revised) prescription converts audiometric thresholds to insertion gains.

```
G(f) = 0.31 × H(f) + K(f)
```

Where `H(f)` is the hearing threshold in dB HL at frequency `f`, and `K(f)` is a frequency-dependent correction that accounts for the speech spectrum:

| Frequency (Hz) | K(f) (dB) |
|-----------------|-----------|
| ≤250            | −17       |
| 500             | −8        |
| 750             | −3        |
| 1000            | 0         |
| 1500            | −1        |
| 2000            | −1        |
| 3000–8000       | −2        |

The 1–2 kHz region receives the most gain relative to other frequencies because it carries the most speech intelligibility information.

Thresholds are provided at 11 standard audiometric frequencies (125–8000 Hz). The algorithm interpolates linearly between these points to compute gain at each filterbank band centre. Output gains are clamped to 0–40 dB.

## DSP parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample rate | 44100 Hz | Higher Fs gives better filter resolution |
| Block size | 128 samples | ~2.9 ms latency per block |
| Filterbank | 8-band IIR biquad, order 6 | `AudioFilterbankBiquad_F32` |
| WDRC attack | 5 ms | |
| WDRC release | 100 ms | |
| WDRC ratio | 2:1 | Compression above knee |
| WDRC knee | 45 dB SPL | |
| Expansion ratio | 2:1 | Noise gate below 35 dB SPL |
| BB limiter ceiling | 91 dB SPL | Safety output limit |

## Algorithm testing via serial

For testing algorithms without NFC, OLED, or other peripherals, connect Tympan via USB and use the Arduino Serial Monitor (115200 baud, Newline line ending).

### Injecting audiogram profiles

Audiogram thresholds are entered as 11 values in dB HL at frequencies:  
`125  250  500  750  1000  1500  2000  3000  4000  6000  8000`

Set left and right ear thresholds separately, then apply:

```
L <v0> <v1> ... <v10>
R <v0> <v1> ... <v10>
p
```

### Example profiles

Profiles are based on audiogram simulations from [hearingtest.online](http://hearingtest.online).

**Mild sloping loss:**
```
L 0 0 10 15 20 25 30 30 40 30 20
R 0 0 10 15 20 25 30 30 40 30 20
p
```

**Moderate loss (symmetric):**
```
L 10 10 20 20 20 30 40 60 70 80 80
R 10 10 20 20 20 30 40 60 70 80 80
p
```

**Severe sloping loss:**
```
L 20 25 35 40 50 55 65 75 80 85 90
R 20 25 35 40 50 55 65 75 80 85 90
p
```

**Asymmetric loss (left worse):**
```
L 10 15 25 30 40 50 55 65 70 75 80
R 5 5 10 15 20 25 30 40 50 55 60
p
```

### Serial commands

| Command | Action |
|---------|--------|
| `h` or `?` | Print help |
| `L v0 v1 ... v10` | Set left ear thresholds (11 values, dB HL) |
| `R v0 v1 ... v10` | Set right ear thresholds (11 values, dB HL) |
| `p` | Apply custom profile (compute NAL-R gains, enable processing) |
| `b` | Toggle full bypass (flat response) |
| `n` | Toggle NAL-R gains on/off |
| `w` | Toggle WDRC compression on/off |
| `l` | Toggle broadband limiter on/off |
| `e` | Toggle expansion / noise gate on/off |
| `+` / `-` | Adjust pre-gain ±3 dB |
| `d` | Cycle OLED display page |
| `s` | Print full DSP status (gains, levels, CPU) |
| `r` | Reset to waiting state |
| `m` | Print current metrics snapshot |
| `0` | Reset metrics accumulators |
| `a` | Toggle raw audio SD recording |
| `f` | List SD log files |

### Recommended test workflow

1. Connect line-in audio (phone, laptop, or signal generator)
2. Type `s` to verify DSP status — should show bypass mode
3. Type `b` to toggle bypass, listen to passthrough audio quality
4. Inject a profile (e.g. moderate loss above), type `p`
5. Compare processed audio against bypass (`b` to toggle)
6. Toggle individual stages:
   - `n` — NAL-R off: hear filterbank + WDRC without frequency shaping
   - `w` — WDRC off: hear NAL-R linear gain only (closest to Python sim behaviour)
   - `e` — expansion off: hear if noise gate is affecting the signal
7. Type `s` to inspect per-band levels and gains

### Bypassing peripherals

The firmware handles missing hardware:

- **NFC**: If PN532 is not detected at boot, NFC polling is skipped. Profiles are injected via serial `L`/`R`/`p` commands.
- **OLED**: If SSD1306 is not on Wire2, display calls are skipped. All status output goes to serial.
- **SD card**: If no card is inserted, logging is disabled. A message is printed at boot.
- **BLE**: Can be bypassed. Serial monitor provides the same command interface.

For pure algorithm testing, no peripherals need to be connected. Tympan + USB + line-in + headphones is sufficient.

## How personalisation works

1. An audiogram is obtained (hearing test results at 11 frequencies per ear)
2. The audiogram is loaded onto an NFC card (or injected via serial)
3. The firmware reads the thresholds and computes NAL-R insertion gains per band per ear
4. Gains are applied through separate gain blocks in each frequency band
5. WDRC compressors shape the dynamics (compress loud signals, expand/gate noise)
6. The broadband limiter enforces a safe output ceiling
7. Left and right ears receive independent processing based on their respective audiograms

The result is that frequencies where the user has more hearing loss receive more amplification, while frequencies with normal hearing are left approximately unchanged. The compression and limiting ensure that the output remains comfortable and safe.

## Build

Requires:
- Arduino IDE or PlatformIO
- [Tympan Library](https://github.com/Tympan/Tympan_Library) installed
- Tympan Rev F hardware

Compile and upload `PersonalizedHearingProtection.ino`. All `.h` and `.cpp` files must be in the same sketch folder.