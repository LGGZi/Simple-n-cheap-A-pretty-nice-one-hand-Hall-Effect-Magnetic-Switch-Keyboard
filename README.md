# 磁轴单手键盘（14 键 · Arduino Leonardo）

**中文** | [English](README.en.md)

[![Platform](https://img.shields.io/badge/platform-Arduino%20Leonardo%20%2F%20ATmega32u4-00979D?logo=arduino&logoColor=white)](#硬件)
[![Switch](https://img.shields.io/badge/switch-hall%20effect%20%2F%20magnetic-e36209)](#硬件)
[![Protocol](https://img.shields.io/badge/HID-Report%20ID%204%20%C2%B7%208%20bytes-6f42c1)](#协议速查)
[![Firmware](https://img.shields.io/badge/firmware-1.04-blue)](#固件版本校验)
[![Host](https://img.shields.io/badge/host-KeyboardOG%20%E2%89%A5%201.07-2ea44f)](https://github.com/LGGZi/KeyBoard-OG-key-binding-tool)

一把单手可用的 **14 键霍尔（磁轴）** 键盘，主控 Arduino Leonardo（ATmega32u4），
固件代码都是我自己写的，上位机用了些Vibe Coding。支持 **RT（快速触发）**、**上下死区**、**校准**、**EC11 旋钮**，
参数与按键映射都存在 EEPROM 里，通过
[KeyboardOG](https://github.com/LGGZi/KeyBoard-OG-key-binding-tool) 配置：

```
上位机 ──(HID Report ID 4，8 字节指令)──> 键盘固件
        <─(ACK / FIN / 查询结果 / 设备信息)─
```

> **默认没有绑定任何键值**：拿到后请先用上位机写一遍映射。
> **第一次使用请先做一次校准**（旋钮长按，或上位机「校准全部按键」），否则死区会按错误行程计算。

---

## 功能特性

### 磁轴手感：RT + 死区

- **死区优先**判定：先看当前读数是不是进了上/下死区（接近松开端 / 压到底），
  在正常行程里才做 RT 的按下、释放与阈值跟随。
- 死区里会把对侧阈值钉在“边界 ± 行程”，所以离开死区后必须再压深 `rt_active` 才会重新触发，
  1~2 个计数的抖动不会造成**双击**。
- 参数（EEPROM 里是千分比，运行时换算成 ADC 计数）：

  | 参数 | 含义 | 范围 |
  |---|---|---|
  | `rt_active` | RT 触发行程（往下压多少算按下） | 10~500 ‰ |
  | `rt_deactive` | RT 释放行程（回抬多少算松开） | 10~500 ‰ |
  | `high_deadzone` | 上死区（接近松开端强制释放） | 0~200 ‰（0 = 关闭） |
  | `low_deadzone` | 下死区（接近压到底强制按下） | 0~200 ‰（0 = 关闭） |

  换算公式：`实际距离 = (bt_maximum - bt_minimum) × 参数 / 1000`，即“把整个键程切成 1000 份”。

### 校准

- 旋钮按键**按住超过 10 轮再松开**触发全部校准；
- 也可以由上位机单键或全部校准，界面会随固件逐键回报更新进度；
- 校准得到的极值（`MAX_VALUE` / `MIN_VALUE`，原始 ADC 0~1023）会写进 EEPROM，**掉电不丢**。

### 按键映射

- 每键最多 **12 条映射**，按顺序生效，可混用**键盘 / 鼠标 / 手柄 / 多媒体**；
- 修饰键自动换算到 HID 修饰键字节；映射是“电平触发”，按住持续上报。

### EC11 旋钮

- 正交解码（A/B → D2/D3，内部上拉），4 个四分之一状态算一格；
- 默认模拟成多媒体按键脉冲：逆时针 → 音量增，顺时针 → 音量减；
- 旋钮按键单独一脚，用于校准。

### 配列上传

- 固件把每个键的 `x / y / w / h`（0.25u）上报，上位机按真实配列画图、点图选键
  （`QUERY_LAYOUT = 0xFE`）；配列表由 `kle2c.py` 生成，长度用 `static_assert` 对齐 `BUTTONCOUNT`。

### 固件版本校验

- 设备信息帧（`0xFF`）里带两个版本，都按 **2 | 3 | 3** 位编码：

  | 字节 | 含义 |
  |---|---|
  | `[6]` | 这套固件**要求的上位机版本**（当前 `0x47` = 1.07） |
  | `[7]` | 固件版本（当前 `0x44` = 1.04） |

- 上位机连上后立即比对，不匹配时状态栏、日志、版本标签都会提示；
  同时维护“软件版本 ↔ 支持的固件范围”表，判断“固件过老 / 过新”。

---

## 快速开始

### 1. 烧录固件

Arduino IDE 打开 `Code/rtkb-layout-edited/rtkb-layout-edited.ino`，板子选 **Arduino Leonardo**，上传。

### 2. 连接上位机

1. 打开 [KeyboardOG](https://github.com/LGGZi/KeyBoard-OG-key-binding-tool)，点「刷新设备」，
   在设备下拉框里选你的键盘（只有真正回应设备信息的设备才会出现）。
2. 点「连接」：自动读取设备信息、全部映射与磁轴参数。

### 3. 校准 → 调参 → 写映射

1. 先做一次**校准**（旋钮长按，或上位机「校准全部按键」）。
2. 拖动 RT / 死区滑块（数值是满行程的千分比，`40` = 4%），写入选中按键。
3. 最后写按键映射，写完自动 `CMD_REFRESH`，不用重新插拔。

---

## 硬件

| 项目 | 说明 |
|---|---|
| 主控 | Arduino Leonardo / ATmega32u4（`arduino:avr:leonardo`） |
| 按键 | 14 个霍尔（磁轴），3 根选择线 `SEL_0..2` + 2 路模拟输入 `RD_0(A0)`、`RD_1(A1)` 分时扫描 |
| 输入方向 | ADC 0~1023，**数值越小 = 按得越深** |
| 旋钮 | EC11，A/B 接 D2/D3（内部上拉），按键为独立引脚 |
| USB | 复合设备：键盘 / 鼠标 / 手柄 / 多媒体（Report ID 1/2/3/5，指令走 Report ID 4） |

```
PCB/
   Gerber_PCB1_2026-09-07.zip            Gerber（打板用）
   BOM_Board1_Schematic1_2026-09-07.xlsx 物料清单
   SCH_Schematic1_2026-09-07.pdf         原理图
   PCB_PCB1_2026-09-07.pdf               PCB 图
   ProPrj_磁轴小键盘V2_2026-09-07.epro2  EasyEDA 工程
keytest/                                  打板后先焊一颗霍尔，用这个测通道
pre-set tools/                            不开上位机时，直接往 EEPROM 预写参数的小工具
```

---

## 从源码构建

依赖：Arduino 自带的 `EEPROM` / `HID`，无需额外库。

Arduino IDE：打开 `Code/rtkb-layout-edited/rtkb-layout-edited.ino`，板子选 **Arduino Leonardo**，上传。

命令行（arduino-cli）：

```bash
arduino-cli compile --fqbn arduino:avr:leonardo Code/rtkb-layout-edited
arduino-cli upload  --fqbn arduino:avr:leonardo -p COM5 Code/rtkb-layout-edited
```

---

## 目录结构

```
Simple-n-cheap-...-Hall-Effect-Magnetic-Switch-Keyboard/
├─ README.md / README.en.md      本文件（中文 / English）
├─ Code/rtkb-layout-edited/
│  ├─ rtkb-layout-edited.ino     setup / loop
│  ├─ Universal.h                全局配置（按键数、引脚、参数/映射数组、版本号）
│  ├─ HallUnitManager.h          模拟扫描、按键状态机（死区优先 + RT）、校准
│  ├─ KeyManager.h               映射翻译
│  ├─ EEPROMManager.h            EEPROM 布局与读写
│  ├─ HIDcmdHandler.h            上位机指令 / 查询 / 设备信息
│  ├─ LayoutManager.h            配列表 + 逐键上传
│  ├─ EncoderManager.h           EC11 解码 + 旋钮脉冲
│  └─ README.md / README.en.md   固件结构说明（代码级文档）
├─ PCB/                          Gerber / BOM / 原理图 / EasyEDA
├─ keytest/                      PCB 通道自检
└─ pre-set tools/                EEPROM 参数预写工具
```

---

## 协议速查

指令与回传都走 **Report ID 4**，数据 8 字节；`[0]` 是指令码，`[7]` 是上位机自增序号
（固件会丢弃与上一条完全相同的指令）。

| 指令 | 值 | 说明 |
|---|---|---|
| 写映射 | 0x01 | `[1]=device [2]=键 [3]=offset [4]=APPEND/OVERRIDE/CLEAR [5]=槽` |
| 写参数 | 0x02 | `[1]=参数号 [2]=键 [3..4]=值` |
| 校准 | 0x03 | `[2]=ALL/SINGLE [3]=键` |
| 查询 | 0x04 | `[1]=0 映射 / 1 参数 / 0xFE 配列 / 0xFF 设备信息` |
| ACK / FIN | 0x05 / 0x06 | 回传 |
| 刷新 | 0x07 | 从 EEPROM 重新载入 |

指令反馈约定：

1. **一条指令回两帧**：先 ACK 再 FIN；只读一帧会把 FIN 留给下一条。
2. **设备信息帧没有 FIN**：查询要用“收到 `0xFF` 帧”作为结束条件。
3. **固件丢弃重复指令**：所以每条指令的 `[7]` 必须自增。

完整字节布局、回传格式、EEPROM 映射见
[`Code/rtkb-layout-edited/README.md`](Code/rtkb-layout-edited/README.md)
（[English](Code/rtkb-layout-edited/README.en.md)）。

---

## FAQ

**Q：轻按出现双击 / 松手粘滞**

- 轻按双击通常是死区过大或 `rt_active` 太小；松手粘滞看 `rt_deactive` 与 `low_deadzone`。
- 先做一次校准，再调参数；参数范围：RT 10~500、死区 0~200。

**Q：某个键一直按下 / 读数异常**

- 该键的模拟通道、磁铁位置，或 `_kbScan()` 的通道表与实物不符。

**Q：上位机连上就报 3000 ms 超时**
- 旧版上位机bug，连接检测等待逻辑不正确，用配套版本上位机
- 设备信息帧没有 FIN，要以“收到 `0xFF` 帧”结束；用配套版本的上位机。
- 不要同时开 Arduino 串口监视器（固件开着 `DEBUG` 时打印会阻塞）。

**Q：能改成别的键数吗**

- 改 `BUTTONCOUNT` 、`key_map` 大小与参数量不能过大以超过EEPROM，键值映射与参数都在EEPROM中储存，可以通过宏定义调整单个按键的最大映射按键数、`_kbScan()`自定义按键读取、上位机tool文件夹内工具转换配列并替换代码中的layout相关代码数组（这个主要是为了上位机绘制layout，不加的话上位机直接矩阵列出所有按钮）。

---

## 待办

- [x] 改键 / 调参软件（KeyboardOG）
- [ ] 3D 打印外壳
- [ ] STM32 版本

---

## 相关工程

| 工程 | 说明 |
|---|---|
| [KeyBoard-OG-key-binding-tool](https://github.com/LGGZi/KeyBoard-OG-key-binding-tool) | 上位机（映射 / 参数 / 校准 / 配列显示） |
| [6-button-tiny-keyboard-with-Arduino](https://github.com/LGGZi/6-button-tiny-keyboard-with-Arduino) | 六键矮轴版本（每键 RGB、12 种灯效） |

---

## 致谢

- HID 复合描述符基于 Arduino 官方的 `Keyboard` / `HID` 示例改写。
- 感谢在真机上反复验证 RT / 死区 / 校准 / 编码器与版本协议的过程中提供的反馈。

