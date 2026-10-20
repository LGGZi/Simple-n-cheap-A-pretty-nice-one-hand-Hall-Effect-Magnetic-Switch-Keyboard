# Hall-effect keyboard firmware (rtkb-layout-edited)

[中文](README.md) | **English**

This document describes the firmware's **structure, data flow and conventions** for
debugging and for anyone picking the code up later. The Chinese comments in the code
explain *why* something is written that way; this file explains *how it works overall*.

- MCU: ATmega32u4 (Arduino Leonardo core, `arduino:avr:leonardo`)
- Keys: `BUTTONCOUNT = 14`, hall (magnetic) switches, one analog channel per key,
  scanned through three selector lines
- One rotary encoder (knob + push button)
- Firmware version: `FIRMWARE_VERSION` in `Universal.h`, currently `0x44` = 1.04
- Required host version: `HOST_VERSION_REQUIRED` in `Universal.h`, currently `0x47` = 1.07
- Host protocol: Report ID **4**, 8-byte commands (used by the KeyboardOG host tool)

> Relation to the six-key version (`6kb`): the protocol, mapping format and EEPROM
> mapping layout are the same. Differences: analog scan / rapid trigger / calibration /
> encoder here, and no RGB LEDs.

---

## 1. Files and responsibilities

| File | Responsibility | Usually changed when |
|---|---|---|
| `rtkb-layout-edited.ino` | `setup()` / `loop()`: init order, main-loop scheduling | adding work to the loop |
| `Universal.h` | global config: key count, mapping slots, EEPROM capacity, pins, parameter/mapping/report arrays, firmware version | key count, pins |
| `HallUnitManager.h` | analog scan `_kbScan()`, key state machine `getKeyState()`, `calibrateKey()` | RT/dead-zone feel, scan order |
| `KeyManager.h` | translates the key bitmap into HID reports | mapping semantics |
| `EEPROMManager.h` | EEPROM layout, parameter/mapping read-write, load at boot | storage layout |
| `HIDcmdHandler.h` | host command parsing, ACK/FIN, query replies, device-info frame | new commands, reply format |
| `LayoutManager.h` | layout table (x/y/w/h per key) + per-key upload (`0xFE`) | new layout (`kle2c.py`) |
| `EncoderManager.h` | quadrature decoding + knob detent as a media-key pulse | knob direction/steps |
| `HID.cpp` / `HID.h` / `SubHID.cpp` / `SubHID.h` | composite HID descriptors and reports | report structure |

---

## 2. Startup (`setup()`)

```c
delay(1000);                 // let USB settle
pinMode(encoder pins / SEL_0..2 / RD_0..1);
Serial.begin(9600);          // debug serial (getKeyState currently prints, see §9)
flagInit();                  // scan once, seed the high/low thresholds
keyParameterInit();          // load the six parameters of all 14 keys from EEPROM
keyCodeInit();               // load all 14 mappings into key_map[][][]
```

Order matters: `flagInit()` must run before the parameters are loaded so the first scan
cannot fire a spurious press.

---

## 3. Main loop (`loop()`)

```c
reportBufferClear();              // clear the HID report
keyOperation(getKeyState());      // scan + state machine -> bitmap -> report
encoderRotateStateRefresh();      // decode the knob, add a press bit when needed
ENCButtonOnPress(calibrateAll);   // knob button: held >= 10 loops, released -> calibrate all
HIDcmdResponseIfChanged();        // handle host commands (deduplicated)
HIDStateUpdate(2);                // send a real HID report every 2 iterations
delayMicroseconds(500);           // main-loop tick
```

- `HIDStateUpdate(2)` halves the effective USB report rate.
- `report` is rebuilt every iteration, so mappings are level-triggered.
- **Scan rate is directly affected by the debug prints in `getKeyState()`** (see §9).

---

## 4. Analog scan (`HallUnitManager.h`)

### 4.1 Topology

- Three selector lines `SEL_0..2` choose one of eight columns.
- Two analog inputs `RD_0 (A0)` and `RD_1 (A1)`.
- Each (column, input) pair is one key: 8 × 2 = 16 channels, 14 used.

### 4.2 Channel ↔ key number

This table lives in `_kbScan()` (**hardware changes must update it**):

| SEL_2 SEL_1 SEL_0 | RD_0 | RD_1 |
|---|---|---|
| 0 0 0 | 3 | 10 |
| 0 0 1 | 4 | 5 |
| 0 1 0 | 9 | 0 |
| 0 1 1 | 8 | 12 |
| 1 0 0 | 13 | 1 |
| 1 0 1 | 2 | 7 |
| 1 1 0 | 11 | 6 |

### 4.3 Readings

`buttonstate[i]` is the raw 10-bit ADC value: **smaller = pressed deeper** (the magnet
approaches and the hall output drops). `flagInit()` uses one scan to seed the thresholds.

---

## 5. Key state machine (`getKeyState()`)

### 5.1 State

```c
static uint16_t buttonstate[BUTTONCOUNT];     // raw reading of this scan
static uint16_t high_buttonflag[BUTTONCOUNT]; // release threshold
static uint16_t low_buttonflag[BUTTONCOUNT];  // press threshold
```

### 5.2 The bitmap must be latched (important)

```c
static uint16_t buttonbuf;   // ★ must be static
```

A bit is set/cleared **only on the scan where the transition happens**; while a key sits
between the thresholds nothing changes it. If `buttonbuf` is a local (`uint16_t buttonbuf
= 0;`), every call throws away the previous state — the symptom is "readings look fine but
keys only register on the transition". That bug has shipped once.

### 5.3 Decision logic (dead zones first)

Every scan, the dead zones are checked first, then rapid trigger tracking runs:

```c
U = bt_maximum - high_deadzone;      // upper boundary
L = bt_minimum + low_deadzone;       // lower boundary

if (s > U)        { clear bit; low  = U - rt_active; high = U + 1; }   // upper dead zone = released
else if (s < L)   { set bit;   high = L + rt_deactive; low = L - 1; }  // lower dead zone = pressed
else {
    if (s <= low)  { set bit;   low = s - 1; high = s + rt_deactive; }
    if (s >= high) { clear bit; high = s + 1; low = s - rt_active; }
    // two extra "re-anchor" branches keep the thresholds following the reading
}
```

Why dead zones come first: clearing in the upper dead zone pins `low` to
"boundary − travel", so leaving the dead zone requires pressing `rt_active` deeper again —
a 1–2 count wobble can no longer produce a double click.

Parameters (stored in EEPROM as per-mille of full travel, converted to ADC counts at load):

| Parameter | Meaning | Range |
|---|---|---|
| `rt_active` | rapid-trigger press distance | 10..500 ‰ |
| `rt_deactive` | rapid-trigger release distance | 10..500 ‰ |
| `high_deadzone` | force release near the top | 0..200 ‰ (0 = off) |
| `low_deadzone` | force press at the bottom | 0..200 ‰ (0 = off) |
| `bt_maximum` / `bt_minimum` | calibrated top/bottom (raw ADC) | 0..1023 |

### 5.4 Calibration

`calibrateKey(key)` records the minimum and maximum while the user presses to the bottom
and releases, then writes both extremes to EEPROM (`MAX_VALUE` / `MIN_VALUE`) and reloads
via `parameterRefresh(key)`.

> The extremes are now actually persisted (`keyParameterSet` accepts 0..1023 for
> `MAX_VALUE`/`MIN_VALUE`). The previous version only allowed 11..979 with `MAX_VALUE = 0`,
> so calibration never reached EEPROM.

---

## 6. Mappings and HID reports (`KeyManager.h` / `SubHID.h`)

### 6.1 `key_map`

```c
uint8_t key_map[BUTTONCOUNT][MAXIMUM_BINDABLE_KEY][2];  // [key][slot][device, offset]
```

- Up to `MAXIMUM_BINDABLE_KEY = 12` bindings per key, applied **in order**.
- A slot with `device == NO_DEVICE(0)` terminates the list.

### 6.2 device / offset semantics

| device | Meaning | offset |
|---|---|---|
| 1 `KEYBOARD` | keyboard | high nibble = index into `keyboardKeys[]`, low nibble = bit |
| 2 `CONTROLLER` | gamepad | button bit (`xboxButtons`) |
| 3 `MOUSE` | mouse | button bit (`mouseButtons`) |
| 4 `MULTIMEDIA` | media keys | bit0 mute / bit1 volume- / bit2 volume+ |

`keyboardKeys[]` is 14 bytes (13 normal key bytes + one modifier byte).
`offset = 0x04` → byte 0, bit 4; modifiers use `13*16 + bit` (`208 + bit`).

### 6.3 Reports

`HIDConReport_t` (`SubHID.h`) is the composite report. `keyOperation()` ORs bits into it;
sending happens in `SubHID.cpp` (Report ID 1/2/3 = gamepad/keyboard/mouse, media keys are
a separate report).

### 6.4 Encoder (`EncoderManager.h`)

- Quadrature decoding uses a 16-entry table; A/B are `ENC_L_A/ENC_L_B` with internal
  pull-ups. An external interrupt is used when available, otherwise polling.
- Four quarter-steps make one detent (`ENCODER_STEPS_PER_DETENT`).
- The knob has no dedicated HID usage; it is emulated as a media-key pulse:
  counter-clockwise → volume up (bit2), clockwise → volume down (bit1), held for
  `ENCODER_PULSE_LOOPS` loops.
- The knob button (`ENC_BT`): held for more than 10 loops and then released calls the
  supplied function (`calibrateAll` in `loop()`); a quick tap does nothing.

---

## 7. EEPROM layout (`EEPROMManager.h`)

```
offset 0        parameters: BUTTONCOUNT * PSOOSK = 14 * 12 = 168 bytes
                per key 12 bytes: [0]=MAX [2]=MIN [4]=RT_ACTIVE [6]=RT_DEACTIVE
                                  [8]=UPPER [10]=LOWER (each uint16_t takes 2 bytes)
offset 168      mappings: BUTTONCOUNT * KEYMAP_SIZE_PER_KEY = 14 * 24 = 336 bytes
                12 slots per key × (device, offset) 2 bytes
total           504 bytes (ATmega32u4 has 1024, ~520 free)
```

Conventions:

- An empty slot is any `device` outside 1..4 (0, 0xFF, stale data).
- `keyCodeInit()` loads the mappings into RAM at boot; runtime reads RAM only.
- After changing mappings/parameters the host sends `CMD_REFRESH` to re-run
  `keyCodeInit()` / `keyParameterInit()`.

---

## 8. Host protocol (`HIDcmdHandler.h`)

### 8.1 Commands

| Command | Value | Purpose |
|---|---|---|
| `CMD_SET_MAP` | 0x01 | write mapping: `[1]=device [2]=key [3]=offset [4]=APPEND/OVERRIDE/CLEAR [5]=slot` |
| `CMD_SET_PARAM` | 0x02 | write hall parameter: `[1]=parameter [2]=key [3..4]=value` |
| `CMD_CALIBRATE` | 0x03 | calibrate: `[2]=ALL/SINGLE [3]=key` |
| `CMD_QUERY` | 0x04 | query: `[1]=0 mapping / 1 parameter / 0xFE layout / 0xFF device info` |
| `CMD_ACK` / `CMD_FIN` | 0x05 / 0x06 | replies |
| `CMD_REFRESH` | 0x07 | reload from EEPROM |

Parameter limits enforced by `keyParameterSet`:

| Parameter | Range |
|---|---|
| RT press / RT release | 10..500 (‰, 1%..50%) |
| upper / lower dead zone | 0..200 (‰, 0 = off) |
| calibration extremes `MAX_VALUE` / `MIN_VALUE` | 0..1023 (raw ADC) |

### 8.2 Reply conventions

- **ACK**: `{0x05, command, sequence, 0...}`.
- **FIN**: the received 8 bytes echoed back with `[0]` replaced by `0x06`.
- Query data: `[0]=0x01` (binding) / `[0]=0x02` (parameter) / `[0]=0xFE` (layout).
- **The device-info frame has no FIN** — receiving it ends the query:

  ```
  { 0xFF, BUTTONCOUNT, hall flag, encoder count, device type, layoutKind,
    HOST_VERSION_REQUIRED, FIRMWARE_VERSION }
  ```

  - `[2]=0xF0` = hall keyboard;
  - `[5]=0xFF` = the firmware uploads a layout;
  - **`[6]` = host version this firmware expects** (2|3|3 bits, currently `0x47` = 1.07);
  - `[7]` = firmware version.
- A command identical to the previous one is dropped, so `[7]` must change every time.

### 8.3 Layout upload

`layout_custom[]` in `LayoutManager.h` is a PROGMEM table of `{x, y, w, h, flags}`
(0.25 u units). `QUERY_LAYOUT (0xFE)` sends `{0xFE, index, x, y, w, h, flags, seq}` per
key, then FIN. The table is generated by `tools/kle2c.py` and its length is checked with
`static_assert` against `BUTTONCOUNT`.

---

## 9. Debugging

### 9.1 Serial output

`getKeyState()` currently prints one line per scan for key 0:

```
buttonstate  high_buttonflag  low_buttonflag  ((buttonbuf >> i) & 1)
```

Two things to watch:

1. Keep the print behind `#if DEBUG` or a temporary switch: 14 keys × 5 `Serial.print`
   calls at 9600 baud is ~200 bytes per scan, which drags the scan rate from hundreds of
   Hz down to a few Hz.
2. Print the state bit as `((buttonbuf >> i) & 1)`; `(buttonbuf >> 0) & i` is meaningless.

### 9.2 Symptoms → where to look

| Symptom | Check first |
|---|---|
| keys work only now and then, readings fine | is `buttonbuf` `static`? (§5.2) |
| a key is always pressed / always released | `low`/`high` pinned at an edge, calibration |
| needs a very deep press | `rt_active` too large, or `bt_minimum` not calibrated |
| sticky release | `rt_deactive` too large, or `low_deadzone` forces pressed |
| double click on light presses | dead zone too large; leaving it needs `rt_active` deeper |
| readings offset / one channel odd | analog channel, magnet position, `_kbScan()` table |
| host reports a 3000 ms timeout after connecting | device info has no FIN; end on the 0xFF frame |
| layout does not appear | device info `[5]` must be `0xFF`, table length must be 14 |

### 9.3 When changing code

- Changing `BUTTONCOUNT` touches: `key_map` dimensions, EEPROM maths (§7), the `_kbScan`
  channel table, the layout table length.
- `keyboardKeys[]` is 14 bytes; do not overrun into `mouseButtons`.
- Wrap variables declared in a `case` in `{}` (on AVR the later cases may stop executing).
- The firmware drops duplicate commands, so any "send once" command needs a changing `[7]`.

---

## 10. One-page cheat sheet

```
setup: pins -> Serial -> flagInit -> keyParameterInit -> keyCodeInit
loop : clear report -> getKeyState (scan + state machine) -> encoder -> knob button -> commands -> send -> 500us

key data: ADC (0..1023) -> buttonstate[i]
          -> dead zones -> compare with low/high -> buttonbuf bit (static latch)
          -> keyOperation() looks up key_map -> report fields
          -> HIDStateUpdate(2) sends every 2 loops

EEPROM: [0..167] parameters   [168..503] mappings (12 slots × 2 bytes per key)
cmds  : Report ID 4, 8 bytes; ACK=0x05, FIN=0x06, query data 0x01/0x02/0xFE, device info 0xFF
```
