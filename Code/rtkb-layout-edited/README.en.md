# Hall-effect keyboard firmware (rtkb-layout-edited)

[中文](README.md) | **English**

This document describes the firmware's **structure, data flow and conventions** for
debugging and for anyone picking the code up later. The Chinese comments in the code
explain *why* something is written that way; this file explains *how it works overall*.

- MCU: ATmega32u4 (Arduino Leonardo core, `arduino:avr:leonardo`)
- Keys: `BUTTONCOUNT = 14`, hall (magnetic) switches, one analog channel per key,
  scanned through three selector lines
- One rotary encoder (knob + push button)
- Firmware version: `FIRMWARE_VERSION` in `Universal.h`, currently `0x4A` = 1.12
- Required host version: `HOST_VERSION_REQUIRED` in `Universal.h`, currently `0x47` = 1.07
- Key behaviours: the same set as 6kb (sticky / auto repeat / sticky+repeat), with the same
  protocol and EEPROM layout — see §5.4 of the 6kb README and §5 here
- LEDs: **14 × WS2812B (one per key), data pin `LEDPIN` = D13**, 23 effects, parameters in EEPROM;
  the effect code is the 6kb `WS2812BManager.h` with only the pin, LED count and the 14-key
  physical position table changed
- Host protocol: Report ID **4**, 8-byte commands (used by the KeyboardOG host tool)

> Relation to the six-key version (`6kb`): the protocol, mapping format, key behaviours, LED
> effects and EEPROM layout (parameters -> mappings -> LED parameters -> repeat parameters) are
> the same. Differences: analog scan / rapid trigger / calibration / encoder here.

---

## 1. Files and responsibilities

| File | Responsibility | Usually changed when |
|---|---|---|
| `rtkb-layout-edited.ino` | `setup()` / `loop()`: init order, main-loop scheduling | adding work to the loop |
| `Universal.h` | global config: key count, mapping slots, EEPROM capacity, pins, parameter/mapping/report arrays, firmware version | key count, pins |
| `HallUnitManager.h` | analog scan `_kbScan()`, key state machine `updateKeyState()`, `calibrateKey()` | RT/dead-zone feel, scan order |
| `KeyManager.h` | translates the `keys[]` pressed state into HID reports | mapping semantics |
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
Serial.begin(9600);          // debug serial (updateKeyState currently prints, see §9)
wsInit();                    // LED strip init (FastLED.addLeds + clear + no dithering)
ledParamsGetFromEEPROM();    // LED parameters (defaults are written when invalid)
flagInit();                  // scan once, seed the high/low thresholds
keyParameterInit();          // load the six parameters of all 14 keys from EEPROM
keyCodeInit();               // load all 14 mappings into key_map[][][] (ends with stickyKeyInit)
rapidParamsGetFromEEPROM();  // repeat parameters (right after the LED area; defaults if invalid)
```

Order matters: `flagInit()` must run before the parameters are loaded so the first scan
cannot fire a spurious press.

---

## 3. Main loop (`loop()`)

```c
frameTimeUpdate();                // read millis() once per frame (see §3.1)
updateKeyState();                 // scan + state machine -> keys[] (one struct per key)
reportBufferClear();              // clear the HID report
keyOperation();                   // fill the report from keys[] (behaviour dispatch included)
keyOperation_stickyKey();         // sticky / sticky+repeat: keep writing the latched keys
wsTick();                         // LED effects: paced by fps, one FastLED.show() per frame
encoderRotateStateRefresh();      // decode the knob, add a press bit when needed
ENCButtonOnPress(calibrateAll);   // knob button: held >= 10 loops, released -> calibrate all
HIDcmdResponseIfChanged();        // handle host commands (deduplicated)
HIDStateUpdate(2);                // send a real HID report every 2 iterations
delayMicroseconds(500);           // main-loop tick
```

- `HIDStateUpdate(2)` halves the effective USB report rate.
- `report` is rebuilt every iteration, so mappings are level-triggered.
- **Scan rate is directly affected by the debug prints in `updateKeyState()`** (see §9).

### 3.1 Time base (`frame_time`)

`millis()` disables interrupts to read the timer on every call, and several calls inside one
frame can straddle a tick. All timing goes through `Universal.h`:

```c
static uint32_t frame_time = 0;
static inline void frameTimeUpdate() { frame_time = millis(); }   // read once per frame
static inline uint32_t getFrameTime() { return frame_time; }      // everyone else reads this
```

The first statement of `loop()` is `frameTimeUpdate();`. Any new timing logic (long press,
combo, LED pacing) must use `getFrameTime()` instead of calling `millis()` directly.

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

## 5. Key state machine (`updateKeyState()`)

### 5.1 State

```c
static uint16_t buttonstate[BUTTONCOUNT];     // raw reading of this scan
static uint16_t high_buttonflag[BUTTONCOUNT]; // release threshold
static uint16_t low_buttonflag[BUTTONCOUNT];  // press threshold
```

The key state itself lives in `keys[]` in `Universal.h` — the same struct the 6kb firmware uses:

```c
struct KeyRuntime {
  uint8_t  down;       // debounced / RT state; the single source of truth
  uint8_t  down_edge;  // 1 only on the frame the key went down (sticky / repeat trigger)
  uint32_t pressedAt;  // frame_time of the last pressed scan (6kb uses it for release debounce)
};
static KeyRuntime keys[BUTTONCOUNT];
```

Read them through `keyDown(i)` / `keyDownEdge(i)`, not the raw fields.

> Before 2026-11 this was a `static uint16_t buttonbuf` bitmap: 16 keys max, and every new
> feature (sticky keys / repeat) would have needed another parallel array. The struct array
> removes the width limit and leaves room for porting 6kb behaviours.

### 5.2 The state must be latched (important)

`down` is set/cleared **only on the scan where the transition happens**; while a key sits
between the thresholds nothing changes it. If the state were recomputed from scratch every
scan, a key in the middle would drop out — the symptom is "readings look fine but keys only
register on the transition". That bug has shipped once (back when `buttonbuf` had to be `static`).

Edge detection uses exactly the 6kb pattern: `updateKeyState()` keeps a cross-scan array

```c
static uint8_t pre_button_state[BUTTONCOUNT] = {0};
```

and ends each iteration with `pre_button_state[i] = keys[i].down;`, so the next frame can tell
"did this key just go down?". It **must be `static`** — as a plain local inside the loop it
would start from 0 every scan and every frame would look like a fresh edge.

`down_edge` / `pressedAt` are maintained the same way as in 6kb: `pressedAt` is refreshed on
every pressed scan, and `down_edge` is 1 only on the 0 -> 1 transition.

### 5.3 Decision logic (dead zones first)

Every scan, the dead zones are checked first, then rapid trigger tracking runs:

```c
U = bt_maximum - high_deadzone;      // upper boundary
L = bt_minimum + low_deadzone;       // lower boundary

if (s > U)        { down = 0; low  = U - rt_active; high = U + 1; }   // upper dead zone = released
else if (s < L)   { down = 1; high = L + rt_deactive; low = L - 1; }  // lower dead zone = pressed
else {
    if (s <= low)  { down = 1; low = s - 1; high = s + rt_deactive; }
    if (s >= high) { down = 0; high = s + 1; low = s - rt_active; }
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
uint8_t key_map[BUTTONCOUNT][MAX_BINDINGS_PER_KEY][2];  // [key][slot][device, offset]
```

- Up to `MAX_BINDINGS_PER_KEY = 12` bindings per key, applied **in order**.
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
offset 504      LED parameters: sizeof(LEDParams_t) = 11 bytes
                magic(0x5B)/effect/brightness/speed/fps/color1[3]/color2[3]
offset 515      repeat parameters: RapidKeyArgs_t = 7 bytes
                magic(0x6B)/toggle/pulse/interval/variation (see KeyBehaviorEEPROM.h)
total           522 bytes (ATmega32u4 has 1024, ~502 free)
```

> The order matches 6kb: parameters -> mappings -> LED parameters -> repeat parameters.
> The 6kb README says "10 bytes / offset 155" for the LED area, which is a typo: the struct is
> actually 11 bytes and its code uses `sizeof(LEDParams_t)` (repeat parameters sit at 156).
> This firmware follows the code, not the typo.

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
| `CMD_SET_LED` | 0x08 | live LED: `[1]=effect [2]=brightness [3]=speed [4..6]=color1 RGB` |
| `CMD_SET_LED_ACCENT` | 0x09 | live accent colour: `[1..3]=color2 RGB` |
| `CMD_SAVE_LED` | 0x0A | save LED parameters: `[1]=0x5A` confirm (not the struct magic 0x5B) |
| `CMD_SET_LED_FPS` | 0x0B | set the LED refresh rate: `[1]=fps (10..200)` |
| `CMD_SET_RAPID` | 0x0C | write repeat params: `[1]=reserved [2]=pulse [3..4]=interval [5..6]=step` |
| `CMD_SAVE_RAPID` | 0x0D | save repeat params to EEPROM: `[1]=0x6C` (firmware forces the switch on) |

Repeat-parameter query: `CMD_QUERY` with `[1]=1`, `[2]=RAPID_TAB(0x02)` (Byte3 reused as the
parameter group); the firmware answers with 4 frames, then FIN:

| Frame | Value | Layout |
|---|---|---|
| parameter | `0x02` | `[1]=code (0x10 switch / 0x11 pulse / 0x12 interval / 0x13 step) [2]=0x02 [3..4]=value [5..6]=0 [7]=seq` |

LED query (`[1]=0x03`): two frames, then FIN. Eleven parameter bytes do not fit in one 8-byte
report, so they are split:

| Frame | Value | Layout |
|---|---|---|
| basic | `0x0C` | `[1]=effect [2]=brightness [3]=speed [4]=fps [5..6]=0 [7]=seq` |
| colours | `0x0D` | `[1..3]=color1 RGB [4..6]=color2 RGB [7]=seq` |

Effect numbers are identical to 6kb (0..22, see §8.4 of the 6kb README).

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

`updateKeyState()` currently prints one line per scan for key 0:

```
buttonstate  high_buttonflag  low_buttonflag  keys[0].down
```

Two things to watch:

1. Keep the print behind `#if DEBUG` or a temporary switch: 14 keys × 5 `Serial.print`
   calls at 9600 baud is ~200 bytes per scan, which drags the scan rate from hundreds of
   Hz down to a few Hz.
2. Read a key's state with `keyDown(i)` (or `keys[i].down`); `keys[i].down_edge` is 1 only
   on the press frame.

> In the current working tree that print is **enabled** (the `if(i == 0){ ... }` block is not
> guarded by `#if DEBUG`), so comment it out again once debugging is done.

### 9.2 Symptoms → where to look

| Symptom | Check first |
|---|---|
| keys work only now and then, readings fine | is `down` recomputed each scan instead of latched? (§5.2) |
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
loop : frame time -> updateKeyState (scan + state machine -> keys[]) -> clear report -> keyOperation -> encoder -> knob button -> commands -> send -> 500us

key data: ADC (0..1023) -> buttonstate[i]
          -> dead zones -> compare with low/high -> keys[i].down (latched) + down_edge/pressedAt
          -> keyOperation() looks up key_map -> report fields
          -> HIDStateUpdate(2) sends every 2 loops

EEPROM: [0..167] parameters   [168..503] mappings (12 slots × 2 bytes per key)
cmds  : Report ID 4, 8 bytes; ACK=0x05, FIN=0x06, query data 0x01/0x02/0xFE, device info 0xFF
```
