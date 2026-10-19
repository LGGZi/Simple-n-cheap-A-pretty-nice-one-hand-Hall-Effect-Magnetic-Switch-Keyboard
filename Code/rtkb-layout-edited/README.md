# 磁轴键盘固件结构说明（rtkb-layout-edited）

本文档说明这套固件的**代码结构、数据流和关键约定**，用于排查问题和给后来人做参考。
代码里的中文注释是“为什么这么写”，本文档回答“整体是怎么运转的”。

- 目标芯片：ATmega32u4（Arduino Leonardo 内核，`arduino:avr:leonardo`）
- 按键数量：`BUTTONCOUNT = 14`
- 按键类型：霍尔（磁轴），每键一个模拟通道，靠 3 根选择线分时扫描
- 固件版本：`FIRMWARE_VERSION`（`Universal.h`，当前 `0x44` = 1.04）
- 上位机协议：Report ID **4**，8 字节指令（与 KeyboardOG 上位机配套）

---

## 1. 文件与职责

| 文件 | 职责 | 改它通常因为 |
|---|---|---|
| `rtkb-layout-edited.ino` | `setup()` / `loop()`：初始化顺序、主循环调度 | 加新的每轮任务 |
| `Universal.h` | 全局配置：按键数、映射槽数、EEPROM 容量约定、引脚、全局数组（参数/映射/report）、固件版本 | 改按键数、改引脚、加全局状态 |
| `HallUnitManager.h` | 模拟扫描 `_kbScan()`、按键状态机 `getKeyState()`、校准 `calibrateKey()` | 改手感（RT/死区）、改扫描顺序 |
| `KeyManager.h` | 把按键位图翻译成 HID 报文（键盘/手柄/鼠标/多媒体） | 改映射语义、加新设备类型 |
| `EEPROMManager.h` | EEPROM 布局、参数与映射的读写、启动时载入内存 | 改存储格式、改参数默认值 |
| `HIDcmdHandler.h` | 上位机指令解析、ACK/FIN、查询回传、设备信息帧 | 加指令、改回传格式 |
| `LayoutManager.h` | 配列（每键 x/y/w/h）表 + 逐键上传（`0xFE` 帧） | 换配列（用 `kle2c.py` 生成） |
| `EncoderManager.h` | 编码器正交解码 + 把“旋钮一格”模拟成按键脉冲 | 改旋钮方向/步数、改旋钮映射 |
| `HID.cpp` / `HID.h` / `SubHID.cpp` / `SubHID.h` | 复合 HID 描述符与报文发送（键盘/鼠标/手柄/多媒体） | 改报文结构、加 HID 接口 |

---

## 2. 启动顺序（`setup()`）

```c
delay(1000);                 // 等 USB 稳定，避免枚举失败
pinMode(编码器 3 根 / SEL_0..2 / RD_0..1);
Serial.begin(9600);          // 调试串口（当前 getKeyState 里有打印）
flagInit();                  // 先扫一次，把当前读数当成高/低阈值初值
keyParameterInit();          // 从 EEPROM 载入 14 键的 6 个参数
keyCodeInit();               // 从 EEPROM 载入 14 键的映射到 key_map[][][]
```

顺序有依赖：`flagInit()` 必须在参数载入之前跑（它要用初始读数压住阈值，避免上电第一轮误触发）。

---

## 3. 主循环（`loop()`）

```c
reportBufferClear();              // 每轮清空 HID 报文
keyOperation(getKeyState());      // 扫描 + 状态机 → 位图 → 填 report
encoderRotateStateRefresh();      // 旋钮解码，必要时往 report 里打“按下”位
ENCButtonOnPress(calibrateAll);   // 旋钮按键：按住 ≥10 轮后松开 → 触发全部校准
HIDcmdResponseIfChanged();        // 处理上位机指令（去重后回应）
HIDStateUpdate(2);                // 每 2 轮真正发一次 HID 报文
delayMicroseconds(500);           // 主循环节拍
```

要点：

- `HIDStateUpdate(2)` 是“隔轮发送”，把实际 USB 报文频率压到扫描频率的一半；
- `report` 每轮清零再重建，所以映射是“电平触发”，松手即消失；
- **扫描频率直接受 `getKeyState()` 里调试打印影响**（见第 9 节）。

---

## 4. 模拟扫描（`HallUnitManager.h`）

### 4.1 硬件拓扑

- 3 根选择线 `SEL_0..2` 选 8 个列；
- 2 根模拟读线 `RD_0(A0)`、`RD_1(A1)`；
- 每个 (列, 读线) 对应一个按键 → 8×2 = 16 个通道，实际用 14 个。

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

`buttonstate[i]` 是 0~1023 的原始 ADC 值：**越大 = 按得越深**（磁铁靠近）。

`_kbScan()` 每次把所有键刷一遍，`flagInit()` 用它给阈值一个初值。

---

## 5. 按键状态机（`getKeyState()`）

### 5.1 数据结构

```c
static uint16_t buttonstate[BUTTONCOUNT];     // 本轮原始读数
static uint16_t high_buttonflag[BUTTONCOUNT]; // 释放阈值（往上抬到这儿算松开）
static uint16_t low_buttonflag[BUTTONCOUNT];  // 按下阈值（往下压到这儿算按下）
```

### 5.2 位图的“锁存”约定（重点）

```c
static uint16_t buttonbuf;   // ★ 必须是 static
```

位**只在发生跳变的那一轮**被置位/清除；按键停在中间位置时两个条件都不成立，这一位保持原样。
所以 `buttonbuf` 一旦写成局部变量 `uint16_t buttonbuf = 0;`，每次调用都会把上一轮状态全丢掉，
表现就是“读数正常但按键时有时无、只有跳变那一瞬间能读到”——这是实际踩过的坑。

### 5.3 判定逻辑

每个键每轮做三件事：

1. **按下检测**：`s <= low` → 置位，并 `low = s-1`、`high = s + rt_deactive`；
2. **释放检测**：`s >= high` → 清位，并 `high = s+1`、`low = s - rt_active`；
3. **回锚**（自适应）：`s > low + rt_active` 时把 `low` 拉高；`s < high - rt_deactive` 时把 `high` 拉低。

参数含义（都来自 EEPROM，单位是 ADC 计数）：

| 参数 | 含义 | 默认 |
|---|---|---|
| `rt_active` | RT 触发距离：从静止要往下压多少才算按下 | 40 |
| `rt_deactive` | RT 释放距离：按下后要回抬多少才算松开 | 40 |
| `high_deadzone` | 上死区：接近满行程时强制判“松开” | 20 |
| `low_deadzone` | 下死区：接近零点时强制判“按下” | 20 |
| `bt_maximum` / `bt_minimum` | 校准得到的满行程/零点 | 校准后写入 |

> 注意：`bt_maximum`/`bt_minimum` 未校准（0）时，死区判断会受无符号下溢影响；
> 新板子第一次用应当先跑一次校准（旋钮按键 → `calibrateAll()`）。

### 5.4 校准

`calibrateKey(key)`：记录该键的 min/max，等用户按到底再松到底，最后把两个值写回 EEPROM
（`MAX_VALUE` / `MIN_VALUE`）并 `parameterRefresh(key)` 重新载入。

---

## 6. 映射与 HID 报文（`KeyManager.h` / `SubHID.h`）

### 6.1 `key_map`

```c
uint8_t key_map[BUTTONCOUNT][MAXIMUM_BINDABLE_KEY][2];  // [按键][槽][device, offset]
```

- 每键最多 `MAXIMUM_BINDABLE_KEY = 12` 条映射，**按顺序**生效；
- 槽的 `device == NO_DEVICE(0)` 表示到此为止（后面的槽不再读）。

### 6.2 device / offset 语义

| device | 含义 | offset 的解释 |
|---|---|---|
| 1 `KEYBOARD` | 键盘 | 高 4 位 = `keyboardKeys[]` 下标，低 4 位 = 该字节里的 bit |
| 2 `CONTROLLER` | 手柄 | 按钮 bit（`xboxButtons`） |
| 3 `MOUSE` | 鼠标 | 按钮 bit（`mouseButtons`） |
| 4 `MULTIMEDIA` | 多媒体 | bit0 静音 / bit1 音量- / bit2 音量+ |

键盘用“下标 + 位”的压缩方式：`keyboardKeys[]` 共 14 字节，前 13 字节是普通键、最后一字节是修饰键。
例：`offset = 0x04` → 第 0 字节 bit4；修饰键要用 `13*16 + bit`（即 `208 + bit`）换算。

### 6.3 报文

`HIDConReport_t`（`SubHID.h`）是整个复合设备的报文结构，`keyOperation()` 只负责往上“或”位；
真正的发送在 `SubHID.cpp`（Report ID 1/2/3 分别对应手柄/键盘/鼠标，多媒体系另外一条）。

### 6.4 编码器（`EncoderManager.h`）

- 正交解码用 16 项状态表（`enc_table`），A/B 接 `ENC_L_A/ENC_L_B`（带内部上拉），优先挂外部中断，挂不上就退化成轮询；
- 4 个四分之一状态算一格（`ENCODER_STEPS_PER_DETENT`）；
- 旋钮没有独立的 HID 用法，而是**模拟成一个多媒体按键脉冲**：逆时针 → 音量增(bit2)，顺时针 → 音量减(bit1)，脉冲保持 `ENCODER_PULSE_LOOPS` 轮，这样上位机/系统能收到一次“按下-松开”；
- 旋钮的按键（`ENC_BT`）：按住超过 10 轮再松开 → 调用传入的函数（`loop` 里传的是 `calibrateAll`），所以是“按住再松”才触发，快速点一下不会校准。

---

## 7. EEPROM 布局（`EEPROMManager.h`）

```
偏移 0                     参数区：BUTTONCOUNT * PSOOSK = 14 * 12 = 168 字节
                           每键 12 字节：[0]=MAX [2]=MIN [4]=RT_ACTIVE
                           [6]=RT_DEACTIVE [8]=UPPER [10]=LOWER（每个 uint16_t 占 2 字节）
偏移 168                   映射区：BUTTONCOUNT * KEYMAP_SIZE_PER_KEY = 14 * 24 = 336 字节
                           每键 12 槽 × (device, offset) 2 字节
合计                       504 字节（ATmega32u4 有 1024 字节，剩余约 520）
```

约定：

- 空槽的 `device` 不是 1~4（0 / 0xFF / 旧数据）→ 一律当空槽；
- `keyCodeInit()` 启动时把映射读进 `key_map[][]`，之后运行期都读内存；
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

### 8.2 回传约定

- **ACK**：`{0x05, 指令码, 序号, 0...}`；
- **FIN**：把收到的 8 字节**原样回传**、只把 `[0]` 换成 `0x06`；
- 查询结果：格式与下发指令一致（`[0]=0x01` 键值 / `[0]=0x02` 参数 / `[0]=0xFE` 布局）；
- **设备信息帧没有 FIN**：`{0xFF, BUTTONCOUNT, 霍尔标志, 编码器数, 设备类型, layoutKind, 保留, 固件版本}` 发完就结束；
- 固件会丢弃“与上一条完全相同”的指令（`HIDcmdResponseIfChanged`），所以上位机每条指令的 `[7]` 序号必须自增。

### 8.3 布局上传

`LayoutManager.h` 里的 `layout_custom[]` 是 PROGMEM 常量表，每项 `{x, y, w, h, flags}`（0.25u 单位）。
收到 `QUERY_LAYOUT(0xFE)` 时逐键发 `{0xFE, index, x, y, w, h, flags, 序号}`，最后发 FIN。
表用 `tools/kle2c.py` 从 keyboard-layout-editor 的 Raw data 生成，长度由 `static_assert` 对齐 `BUTTONCOUNT`。

---

## 9. 调试与排查

### 9.1 串口输出

`getKeyState()` 末尾现在有逐键打印（**每个键一行**）：

```
buttonstate  high_buttonflag  low_buttonflag  (buttonbuf & i)   ← 第 4 列表达式是错的
```

注意两点：

1. 第 4 列写的是 `(buttonbuf >> 0) & i`，实际等于 `buttonbuf & i`，**不是第 i 位**；
   想看状态要改成 `((buttonbuf >> i) & 1)`。
2. 14 个键每个 5 次 `Serial.print`，9600 波特率下每轮约 200 字节，会明显拖慢主循环
   （扫描频率从几百 Hz 掉到几 Hz），排查手感问题时建议只保留按键本身的打印，或改成每 N 轮打一次。

### 9.2 常见现象 → 可能原因

| 现象 | 先看哪里 |
|---|---|
| 按键“有时有、有时没有”，读数正常 | `buttonbuf` 是不是 `static`（第 5.2 节） |
| 某个键一直按下 / 一直松开 | 该键 `low/high` 是否贴边、`bt_maximum/minimum` 是否已校准 |
| 按下去要很深才触发 | `rt_active` 太大，或该键 `bt_minimum` 没校准 |
| 松手后还粘着 | `rt_deactive` 太大，或 `low_deadzone` 让低值被判按下 |
| 读数整体偏移 / 某个键数值异常 | 该键的模拟通道、磁铁位置、`_kbScan()` 的通道表 |
| 上位机连上就报 3000ms 超时 | 设备信息帧没有 FIN，上位机要按“收到 0xFF 帧”结束（见 8.2） |
| 布局显示不出来 | 设备信息 `[5]` 是否为 `0xFF`、`layout_custom` 是否与 `BUTTONCOUNT` 一致 |

### 9.3 改代码时的注意点

- 改 `BUTTONCOUNT` 要同时检查：`key_map` 维度、EEPROM 计算（第 7 节）、`_kbScan` 的通道表、布局表长度；
- 改 `keyboardKeys[]` 的用法（14 字节）注意别越界到 `mouseButtons`；
- `case` 里声明变量用 `{}` 包起来（AVR 上不加会影响后面几个 case 的执行）；
- 固件会丢弃重复指令：新增“只发一次就够”的指令时，上位机的序号要变化。

---

## 10. 一页速查

```
setup: 引脚 → Serial → flagInit → keyParameterInit → keyCodeInit
loop : 清报文 → getKeyState(扫描+状态机) → 编码器 → 旋钮按键 → 指令 → 发送 → 500us

按键数据: ADC(0..1023) → buttonstate[i]
          → 与 low/high 比较 → buttonbuf 位（static 锁存）
          → keyOperation() 查 key_map → report 各字段
          → HIDStateUpdate(2) 每两轮发一次

EEPROM : [0..167] 参数   [168..503] 映射（每键 12 槽 × 2 字节）
指令   : Report ID 4，8 字节；ACK=0x05、FIN=0x06、查询结果 0x01/0x02/0xFE、设备信息 0xFF
```
