# 磁轴键盘固件结构说明（rtkb-layout-edited）

**中文** | [English](README.en.md)

本文档说明这套固件的**代码结构、数据流和关键约定**，用于排查问题和给后来人做参考。
代码里的中文注释是“为什么这么写”，本文档回答“整体是怎么运转的”。

- 目标芯片：ATmega32u4（Arduino Leonardo 内核，`arduino:avr:leonardo`）
- 按键数量：`BUTTONCOUNT = 14`，按键类型：霍尔（磁轴），每键一个模拟通道，靠 3 根选择线分时扫描
- 编码器：1 个（旋钮 + 按键）
- 固件版本：`FIRMWARE_VERSION`（`Universal.h`，当前 `0x4A` = 1.12）
- 要求的上位机版本：`HOST_VERSION_REQUIRED`（`Universal.h`，当前 `0x47` = 1.07）
- 按键行为：和 6kb 同一套（粘滞键 / 连发 / 粘滞+连发），协议与 EEPROM 布局一致，
  详见 6kb 的 `README.md` 第 5.4 节和本文第 5 章
- 灯效：**14 颗 WS2812B（每键一颗），数据脚 `LEDPIN = D13`**，23 种效果，参数存 EEPROM；
  灯效实现是从 6kb 的 `WS2812BManager.h` 整体搬过来的，只改了引脚、灯数和 14 键的物理坐标表
- 上位机协议：Report ID **4**，8 字节指令（与 KeyboardOG 上位机配套）

> 和六键版（`6kb`）的关系：协议、映射格式、按键行为、灯效、EEPROM 布局（参数区 → 映射区 →
> 灯效参数区 → 连发参数区）都一致；区别是**多了模拟扫描 / RT / 校准 / 编码器**。

---

## 1. 文件与职责

| 文件 | 职责 | 改它通常因为 |
|---|---|---|
| `rtkb-layout-edited.ino` | `setup()` / `loop()`：初始化顺序、主循环调度 | 加新的每轮任务 |
| `Universal.h` | 全局配置：按键数、映射槽数、EEPROM 容量约定、引脚、全局数组（参数/映射/report）、固件版本 | 改按键数、改引脚、加全局状态 |
| `HallUnitManager.h` | 模拟扫描 `_kbScan()`、按键状态机 `updateKeyState()`、校准 `calibrateKey()` | 改手感（RT/死区）、改扫描顺序 |
| `KeyManager.h` | 把 `keys[]` 里的按下状态翻译成 HID 报文（键盘/手柄/鼠标/多媒体） | 改映射语义、加设备类型 |
| `EEPROMManager.h` | EEPROM 布局（参数区 / 映射区 / 灯效参数区）、参数与映射的读写、灯效参数存取、启动时载入内存 | 改存储格式、改参数默认值 |
| `WS2812BManager.h` | 灯效：14 颗 WS2812B、23 种效果、参数结构体与帧调度（和 6kb 同一份代码） | 加灯效、调参数、改灯珠顺序 |
| `KeyBehaviorManager.h` | 按键行为：粘滞键 / 连发 / 粘滞+连发（和 6kb 同一份代码） | 加新的 behavior、改功能时序 |
| `KeyBehaviorEEPROM.h` | 连发参数的 EEPROM 存读（`RAPID_PARAMS_START_POSITION`，紧跟灯效参数区） | 改参数存储格式 |
| `HIDcmdHandler.h` | 上位机指令解析、ACK/FIN、查询回传、设备信息帧、连发参数协议 | 加指令、改回传格式 |
| `LayoutManager.h` | 配列（每键 x/y/w/h）表 + 逐键上传（`0xFE` 帧） | 换配列（用 `kle2c.py` 生成） |
| `EncoderManager.h` | 编码器正交解码 + 把“旋钮一格”模拟成按键脉冲 | 改旋钮方向/步数、改旋钮映射 |
| `HID.cpp` / `HID.h` / `SubHID.cpp` / `SubHID.h` | 复合 HID 描述符与报文发送 | 改报文结构、加 HID 接口 |

---

## 2. 启动顺序（`setup()`）

```c
delay(1000);                 // 等 USB 稳定，避免枚举失败
pinMode(编码器 3 根 / SEL_0..2 / RD_0..1);
Serial.begin(9600);          // 调试串口（updateKeyState 里目前有打印，见 §9）
wsInit();                    // 灯带初始化（FastLED.addLeds + 清屏 + 关抖动）
ledParamsGetFromEEPROM();    // 灯效参数（无效就写默认值并 wsApplyParams）
flagInit();                  // 先扫一次，把当前读数当成高/低阈值初值
keyParameterInit();          // 从 EEPROM 载入 14 键的 6 个参数
keyCodeInit();               // 从 EEPROM 载入 14 键的映射到 key_map[][][]（末尾调 stickyKeyInit）
rapidParamsGetFromEEPROM();  // 连发参数（紧跟灯效参数区；无效就写默认值）
```

顺序有依赖：`flagInit()` 必须在参数载入之前跑（它用初始读数压住阈值，避免上电第一轮误触发）。

---

## 3. 主循环（`loop()`）

```c
frameTimeUpdate();                // 每帧只读一次 millis()（见 §3.1）
updateKeyState();                 // 扫描 + 状态机 → keys[]（每键一个结构体）
reportBufferClear();              // 每轮清空 HID 报文
keyOperation();                   // 按 keys[] 填 report（含 behavior 分派）
keyOperation_stickyKey();         // 粘滞键 / 粘滞+连发：把“保持输出”的键写进 report
wsTick();                         // 灯效：内部按 fps 限速，每帧只 FastLED.show() 一次
encoderRotateStateRefresh();      // 旋钮解码，必要时往 report 里打“按下”位
ENCButtonOnPress(calibrateAll);   // 旋钮按键：按住 ≥10 轮后松开 → 触发全部校准
HIDcmdResponseIfChanged();        // 处理上位机指令（去重后回应）
HIDStateUpdate(2);                // 每 2 轮真正发一次 HID 报文
delayMicroseconds(500);           // 主循环节拍
```

- `HIDStateUpdate(2)` 是“隔轮发送”，把实际 USB 报文频率压到扫描频率的一半；
- `report` 每轮清零再重建，所以映射是“电平触发”，松手即消失；
- **扫描频率直接受 `updateKeyState()` 里调试打印影响**（见 §9）。

### 3.1 时间基准（`frame_time`）

`millis()` 每次调用都要关中断读计时器，同一帧里多次调用还可能跨过节拍。所以时间统一走
`Universal.h`：

```c
static uint32_t frame_time = 0;
static inline void frameTimeUpdate() { frame_time = millis(); }   // 每帧只在这里读一次
static inline uint32_t getFrameTime() { return frame_time; }      // 其它地方只读缓存
```

`loop()` 第一条语句是 `frameTimeUpdate();`。以后加任何需要计时的逻辑（连击、长按、灯效），
一律用 `getFrameTime()`，不要再直接调 `millis()`。

---

## 4. 模拟扫描（`HallUnitManager.h`）

### 4.1 硬件拓扑

- 3 根选择线 `SEL_0..2` 选 8 个列；
- 2 根模拟读线 `RD_0(A0)`、`RD_1(A1)`；
- 每个 (列, 读线) 对应一个按键 → 8×2 = 16 个通道，实际用 14 个；
- 灯带：14 颗 WS2812B（每键一颗），数据脚 `LEDPIN` = **D13**；灯珠顺序默认按按键 0~13，
  物理位置表在 `WS2812BManager.h` 的 `wsKeyPos[]`（2D 效果用），灯带实际顺序不同就改这张表。

### 4.2 物理通道 ↔ 按键编号

`_kbScan()` 里的对应关系（**改硬件必须同步改这里**）：

| SEL_2 SEL_1 SEL_0 | RD_0 | RD_1 |
|---|---|---|
| 0 0 0 | 3 | 10 |
| 0 0 1 | 4 | 5 |
| 0 1 0 | 9 | 0 |
| 0 1 1 | 8 | 12 |
| 1 0 0 | 13 | 1 |
| 1 0 1 | 2 | 7 |
| 1 1 0 | 11 | 6 |

### 4.3 读数

`buttonstate[i]` 是 0~1023 的原始 ADC 值：**数值越小 = 按得越深**（磁铁靠近，霍尔输出下降）。
`_kbScan()` 每次把所有键刷一遍，`flagInit()` 用它给阈值一个初值。

---

## 5. 按键状态机（`updateKeyState()`）

### 5.1 数据结构

```c
static uint16_t buttonstate[BUTTONCOUNT];     // 本轮原始读数
static uint16_t high_buttonflag[BUTTONCOUNT]; // 释放阈值（抬到这儿算松开）
static uint16_t low_buttonflag[BUTTONCOUNT];  // 按下阈值（压到这儿算按下）
```

按键状态本身放在 `Universal.h` 的 `keys[]` 里，和 6kb 固件是同一套结构体：

```c
struct KeyRuntime {
  uint8_t  down;       // 状态机判定后的按下状态（对外唯一真相）
  uint8_t  down_edge;  // 只在“刚按下”那一帧为 1（粘滞键 / 连发靠它触发）
  uint32_t pressedAt;  // 最近一次检测到按下的 frame_time（6kb 用它做抬起去抖）
};
static KeyRuntime keys[BUTTONCOUNT];
```

对外只用 `keyDown(i)` / `keyDownEdge(i)` 读，不要直接改 `keys[i].down`。

> 2026-11 之前这里是 `static uint16_t buttonbuf` 位图：最多 16 键，而且以后每加一个功能
> （粘滞键 / 连发）都得再加一条同长度的并行数组。改成结构体数组后位宽不再受限，
> 也给后面移植 6kb 的按键行为留好了位置。

### 5.2 锁存约定（重点）

`down` **只在发生跳变的那一轮**被置位/清除；按键停在中间位置时两个条件都不成立，保持原样。
一旦写成每轮重算的临时变量，中间位置的状态就会丢，表现是“读数正常但按键时有时无、
只有跳变那一瞬间能读到”——这是实际踩过的坑（当年的 `buttonbuf` 必须是 `static` 也是这个原因）。

边沿检测用和 6kb **完全一样**的写法：`updateKeyState()` 里有一个跨扫描保存的

```c
static uint8_t pre_button_state[BUTTONCOUNT] = {0};
```

循环末尾 `pre_button_state[i] = keys[i].down;`，下一帧用它判断“这一帧是不是刚从松开变按下”。
（它必须 `static`：写成循环里的局部变量的话每帧都从 0 开始，边沿就永远成立。）

`down_edge` / `pressedAt` 的维护也和 6kb 一致：按键按着时每帧刷新 `pressedAt`；
`down` 由 0 变 1 的那一帧置 `down_edge = 1`，其余帧清 0。

### 5.3 判定逻辑（死区优先）

每轮先判死区，再在正常区做 RT 跟踪：

```c
U = bt_maximum - high_deadzone;      // 上死区边界
L = bt_minimum + low_deadzone;       // 下死区边界

if (s > U)        { down = 0; low  = U - rt_active; high = U + 1; }       // 进入上死区 = 已释放
else if (s < L)   { down = 1; high = L + rt_deactive; low = L - 1; }      // 进入下死区 = 已按下
else {
    if (s <= low)  { down = 1; low = s - 1; high = s + rt_deactive; }
    if (s >= high) { down = 0; high = s + 1; low = s - rt_active; }
    // 另外两条“回锚”分支让阈值跟着当前读数走
}
```

死区优先的意义：**死区清位时会把 `low` 钉在“边界 − 行程”**，所以离开死区后必须再压深
`rt_active` 才会重新触发，1~2 个计数的抖动不会造成双击。

参数含义（都存在 EEPROM，单位是千分比，1000 = 满行程；运行时换算成 ADC 计数）：

| 参数 | 含义 | 允许范围 |
|---|---|---|
| `rt_active` | RT 触发行程：从静止要往下压多少才算按下 | 10~500 ‰ |
| `rt_deactive` | RT 释放行程：按下后要回抬多少才算松开 | 10~500 ‰ |
| `high_deadzone` | 上死区：接近松开端时强制判释放 | 0~200 ‰（0 = 关闭） |
| `low_deadzone` | 下死区：接近压到底时强制判按下 | 0~200 ‰（0 = 关闭） |
| `bt_maximum` / `bt_minimum` | 校准得到的满行程/零点（原始 ADC 值） | 0~1023 |

### 5.4 校准

`calibrateKey(key)`：记录该键的 min/max，等用户按到底再松到底，最后把两个极值写回 EEPROM
（`MAX_VALUE` / `MIN_VALUE`）并 `parameterRefresh(key)` 重新载入。

> 极值现在会真正写进 EEPROM（`keyParameterSet` 对 `MAX_VALUE`/`MIN_VALUE` 开放 0~1023），
> 所以校准结果能跨重启保留。之前那版只允许 11~979 且 `MAX_VALUE=0`，校准其实没存下来。

---

## 6. 映射与 HID 报文（`KeyManager.h` / `SubHID.h`）

### 6.1 `key_map`

```c
uint8_t key_map[BUTTONCOUNT][MAX_BINDINGS_PER_KEY][2];  // [按键][槽][device, offset]
```

- 每键最多 `MAX_BINDINGS_PER_KEY = 12` 条映射，**按顺序**生效；
- 槽的 `device == NO_DEVICE(0)` 表示到此为止。

### 6.2 device / offset 语义

| device | 含义 | offset 的解释 |
|---|---|---|
| 1 `KEYBOARD` | 键盘 | 高 4 位 = `keyboardKeys[]` 下标，低 4 位 = 该字节里的 bit |
| 2 `CONTROLLER` | 手柄 | 按钮 bit（`xboxButtons`） |
| 3 `MOUSE` | 鼠标 | 按钮 bit（`mouseButtons`） |
| 4 `MULTIMEDIA` | 多媒体 | bit0 静音 / bit1 音量- / bit2 音量+ |

键盘用“下标 + 位”的压缩方式：`keyboardKeys[]` 共 14 字节，前 13 字节普通键、最后一字节修饰键。
例：`offset = 0x04` → 第 0 字节 bit4；修饰键是 `13*16 + bit`（`208 + bit`）。

### 6.3 报文

`HIDConReport_t`（`SubHID.h`）是复合设备报文结构，`keyOperation()` 只往上“或”位；
发送在 `SubHID.cpp`（Report ID 1/2/3 = 手柄/键盘/鼠标，多媒体另发一条）。

### 6.4 编码器（`EncoderManager.h`）

- 正交解码用 16 项状态表（`enc_table`），A/B 接 `ENC_L_A/ENC_L_B`（内部上拉），优先挂外部中断，挂不上就退化成轮询；
- 4 个四分之一状态算一格（`ENCODER_STEPS_PER_DETENT`）；
- 旋钮没有独立的 HID 用法，而是**模拟成一个多媒体按键脉冲**：逆时针 → 音量增(bit2)、顺时针 → 音量减(bit1)，脉冲保持 `ENCODER_PULSE_LOOPS` 轮；
- 旋钮按键（`ENC_BT`）：按住超过 10 轮再松开 → 调用传入的函数（`loop` 里是 `calibrateAll`），快速点一下不会触发。

---

## 7. EEPROM 布局（`EEPROMManager.h`）

```
偏移 0                     参数区：BUTTONCOUNT * PSOOSK = 14 * 12 = 168 字节
                           每键 12 字节：[0]=MAX [2]=MIN [4]=RT_ACTIVE
                           [6]=RT_DEACTIVE [8]=UPPER [10]=LOWER（每个 uint16_t 占 2 字节）
偏移 168                   映射区：BUTTONCOUNT * KEYMAP_SIZE_PER_KEY = 14 * 24 = 336 字节
                           每键 12 槽 × (device, offset) 2 字节
偏移 504                   灯效参数区：sizeof(LEDParams_t) = 11 字节
                           magic(0x5B)/effect/brightness/speed/fps/color1[3]/color2[3]
偏移 515                   连发参数区：RapidKeyArgs_t = 7 字节
                           magic(0x6B)/toggle/pulse/interval/variation（见 KeyBehaviorEEPROM.h）
合计                       522 字节（ATmega32u4 有 1024 字节，剩余约 502）
```

> 顺序和 6kb 一致：参数区 → 映射区 → 灯效参数区 → 连发参数区。
> 6kb 的 README 把灯效参数写成“10 字节 / 偏移 155”，那是笔误：结构体实际 11 字节，
> 它的代码用 `sizeof(LEDParams_t)` 算位置（连发参数实际在 156），这里按代码口径对齐。

约定：

- 空槽的 `device` 不是 1~4（0 / 0xFF / 旧数据）→ 一律当空槽；
- `keyCodeInit()` 启动时把映射读进 `key_map[][]`，运行期读内存；
- 上位机改完映射/参数后发 `CMD_REFRESH`，让固件重新 `keyCodeInit()` / `keyParameterInit()`。

---

## 8. 上位机协议（`HIDcmdHandler.h`）

### 8.1 指令

| 指令 | 值 | 用途 |
|---|---|---|
| `CMD_SET_MAP` | 0x01 | 写映射：`[1]=device [2]=按键 [3]=offset [4]=APPEND/OVERRIDE/CLEAR [5]=槽` |
| `CMD_SET_PARAM` | 0x02 | 写磁轴参数：`[1]=参数号 [2]=按键 [3..4]=值` |
| `CMD_CALIBRATE` | 0x03 | 校准：`[2]=ALL/SINGLE [3]=按键` |
| `CMD_QUERY` | 0x04 | 查询：`[1]=0 映射 / 1 参数 / 0xFE 布局 / 0xFF 设备信息` |
| `CMD_ACK` / `CMD_FIN` | 0x05 / 0x06 | 回传用 |
| `CMD_REFRESH` | 0x07 | 让固件从 EEPROM 重新载入 |
| `CMD_SET_LED` | 0x08 | 即时灯效：`[1]=effect [2]=亮度 [3]=速度 [4..6]=主色 RGB` |
| `CMD_SET_LED_ACCENT` | 0x09 | 即时副色：`[1..3]=副色 RGB` |
| `CMD_SAVE_LED` | 0x0A | 保存灯效参数：`[1]=0x5A` 确认（别和结构体 magic 0x5B 混用） |
| `CMD_SET_LED_FPS` | 0x0B | 单独设置灯效刷新率：`[1]=fps（10~200）` |
| `CMD_SET_RAPID` | 0x0C | 写连发参数：`[1]=保留 [2]=脉宽 [3..4]=间隔 [5..6]=步长` |
| `CMD_SAVE_RAPID` | 0x0D | 保存连发参数到 EEPROM：`[1]=0x6C`（固件先把开关强制置 1） |

连发参数查询：`CMD_QUERY` 的 `[1]=1`、`[2]=RAPID_TAB(0x02)`（Byte3 复用成参数组），
固件回 4 帧后 FIN：

| 帧 | 值 | 字节布局 |
|---|---|---|
| 参数 | `0x02` | `[1]=参数码(0x10 开关 / 0x11 脉宽 / 0x12 间隔 / 0x13 步长) [2]=0x02 [3..4]=值 [5..6]=0 [7]=序号` |

灯效查询（`[1]=0x03`）：固件回两帧后 FIN。8 字节装不下 11 个参数字节，所以拆成两帧：

| 帧 | 值 | 字节布局 |
|---|---|---|
| 基本参数 | `0x0C` | `[1]=effect [2]=亮度 [3]=速度 [4]=fps [5..6]=0 [7]=序号` |
| 颜色 | `0x0D` | `[1..3]=主色 RGB [4..6]=副色 RGB [7]=序号` |

灯效编号和 6kb 完全一致（0~22，见 6kb 的 `README.md` §8.4）。

参数写入的限制（`keyParameterSet`）：

| 参数 | 允许范围 |
|---|---|
| RT 触发 / RT 释放 | 10~500（‰，1%~50%） |
| 上死区 / 下死区 | 0~200（‰，0 = 关闭） |
| 校准极值 `MAX_VALUE` / `MIN_VALUE` | 0~1023（原始 ADC 值） |

### 8.2 回传约定

- **ACK**：`{0x05, 指令码, 序号, 0...}`；
- **FIN**：把收到的 8 字节**原样回传**、只把 `[0]` 换成 `0x06`；
- 查询结果：`[0]=0x01`（键值）/ `[0]=0x02`（参数）/ `[0]=0xFE`（布局）；
- **设备信息帧没有 FIN**，收到它就是结束标志：

  ```
  { 0xFF, BUTTONCOUNT, 霍尔标志, 编码器数, 设备类型, layoutKind,
    HOST_VERSION_REQUIRED, FIRMWARE_VERSION }
  ```

  - `[2]=0xF0` 表示磁轴；
  - `[5]=0xFF` 表示会上传布局；
  - **`[6]` = 这套固件要求的上位机版本**（2|3|3 位，当前 `0x47` = 1.07）；
  - `[7]` = 固件版本。
- 固件会丢弃“与上一条完全相同”的指令（`HIDcmdResponseIfChanged`），所以每条指令的 `[7]` 序号必须自增。

### 8.3 布局上传

`LayoutManager.h` 里的 `layout_custom[]` 是 PROGMEM 常量表，每项 `{x, y, w, h, flags}`（0.25u）。
收到 `QUERY_LAYOUT(0xFE)` 时逐键发 `{0xFE, index, x, y, w, h, flags, 序号}`，最后 FIN。
表用 `tools/kle2c.py` 从 keyboard-layout-editor 生成，长度由 `static_assert` 对齐 `BUTTONCOUNT`。

---

## 9. 调试与排查

### 9.1 串口输出

`updateKeyState()` 末尾目前有逐键打印（只看 0 号键，**每个扫描一行**）：

```
buttonstate  high_buttonflag  low_buttonflag  keys[0].down
```

注意：

1. 打印必须在 `#if DEBUG` 或临时开关里，14 个键每个 5 次 `Serial.print` 在 9600 波特率下
   每轮约 200 字节，会明显拖慢主循环（扫描从几百 Hz 掉到几 Hz）；
2. 想看某键状态用 `keyDown(i)`（或 `keys[i].down`）；`keys[i].down_edge` 只在按下那一帧为 1。

> 当前工作区里这段打印是**打开**的（`if(i == 0){ ... }` 没有 `#if DEBUG` 包着），
> 调试完记得注释回去，否则会一直往串口写。

### 9.2 常见现象 → 可能原因

| 现象 | 先看哪里 |
|---|---|
| 按键“有时有、有时没有”，读数正常 | `down` 是否每轮被重算（§5.2 的锁存约定） |
| 某个键一直按下 / 一直松开 | 该键 `low/high` 是否贴边、`bt_maximum/minimum` 是否已校准 |
| 按下去要很深才触发 | `rt_active` 太大，或该键 `bt_minimum` 没校准 |
| 松手后还粘着 | `rt_deactive` 太大，或 `low_deadzone` 让低值被判按下 |
| 轻按出现双击 | 死区清位后要重压 `rt_active` 才触发；检查死区是否过大 |
| 读数整体偏移 / 某个键异常 | 该键的模拟通道、磁铁位置、`_kbScan()` 的通道表 |
| 上位机连上就报 3000ms 超时 | 设备信息帧没有 FIN，要按“收到 0xFF 帧”结束（§8.2） |
| 布局显示不出来 | 设备信息 `[5]` 是否为 `0xFF`、`layout_custom` 长度是否 14 |

### 9.3 改代码时的注意点

- 改 `BUTTONCOUNT` 要同步：`key_map` 维度、EEPROM 计算（§7）、`_kbScan` 通道表、布局表长度；
- 改 `keyboardKeys[]`（14 字节）注意别越界到 `mouseButtons`；
- `case` 里声明变量用 `{}` 包起来（AVR 上不加会影响后面几个 case 的执行）；
- 固件会丢弃重复指令：新增“只发一次就够”的指令时，上位机的序号要变化。

---

## 10. 一页速查

```
setup: 引脚 → Serial → flagInit → keyParameterInit → keyCodeInit
loop : 帧时间 → updateKeyState(扫描+状态机→keys[]) → 清报文 → keyOperation → 编码器 → 旋钮按键 → 指令 → 发送 → 500us

按键数据: ADC(0..1023) → buttonstate[i]
          → 死区判定 → 与 low/high 比较 → keys[i].down（锁存）+ down_edge/pressedAt
          → keyOperation() 查 key_map → report 各字段
          → HIDStateUpdate(2) 每两轮发一次

EEPROM : [0..167] 参数   [168..503] 映射（每键 12 槽 × 2 字节）
指令   : Report ID 4，8 字节；ACK=0x05、FIN=0x06、查询结果 0x01/0x02/0xFE、设备信息 0xFF
```
