#ifndef EncoderManager_h
#define EncoderManager_h

#include "Universal.h"
#include "SubHID.h"
#include <Arduino.h>

// ============================================================================
// EC11 编码器：A/B 双外部中断 + 状态表解码
//
// 原来的做法是在 loop() 里每轮读一次 A/B 电平，两个问题：
//   1) 主循环一轮要发 4 条 USB 报文（还可能写 EEPROM、打印串口），轮询间隔是
//      毫秒级的。手转快一点，两次轮询之间 A/B 已经跳了 2 个状态，而老代码里
//      "diff 为 ±2" 的那一支是直接忽略 —— 于是整格整格地丢步；
//      要是刚好跳满 4 个状态，电平回到原样，连"状态变了"都看不出来。
//   2) 起步那一下同理：第一个采样点往往已经跨了 2 个状态，直接被丢掉。
//
// 现在改成：A/B 各挂一个 CHANGE 外部中断，任何一次电平跳变都不会漏；
// 中断里只查一次表累加（这张表能把机械抖动的来回跳自动抵消），
// 主循环每次取走"满一格"的量，所以每个段落感都会触发一次。
//
// 引脚：ENC_L_A = 2 (INT1)、ENC_L_B = 1 (INT3)，都是 32u4 的外部中断脚。
// 引脚如果不支持外部中断，会自动退回到主循环轮询（同一张表，逻辑一致）。
// ============================================================================

// ---------- 方向常量 ----------
#define DIR_CW   1   // 顺时针
#define DIR_CCW -1   // 逆时针

// 一个"段落感"对应几次 A/B 跳变：
//   标准 EC11（每圈 20 格）= 一个完整格雷码周期 = 4 次跳变，也就是转一格触发一次。
//   如果你的编码器是一格只跳 2 次的型号，把这里改成 2（触发次数会翻倍）。
#ifndef ENCODER_STEPS_PER_DETENT
#define ENCODER_STEPS_PER_DETENT 4
#endif

// 检测到一格之后，把"按下"这一位保持几轮。
// 主循环每 2 轮才 HIDStateUpdate 一次，保持 3 轮就一定覆盖到一次发送，
// 不会出现"转了一下，但这一位刚置上就被 reportBufferClear 清掉"的情况。
#ifndef ENCODER_PULSE_LOOPS
#define ENCODER_PULSE_LOOPS 3
#endif

// ---------- 状态转移表 ----------
// 状态编码 bit1 = A，bit0 = B（和原来的 (A << 1) | B 一致）
//   顺时针 00 -> 01 -> 11 -> 10 -> 00
//   逆时针 00 -> 10 -> 11 -> 01 -> 00
// 索引 = (上次状态 << 2) | 本次状态
//   +1 = 顺时针四分之一步，-1 = 逆时针四分之一步，0 = 没变 / 抖动回弹 / 双跳
// 抖动总是成对出现（+1 之后紧跟着 -1），累计值自己抵消，不会多算步数。
static const int8_t enc_table[16] = {
    0,  1, -1,  0,   // 上次 00
   -1,  0,  0,  1,   // 上次 01
    1,  0,  0, -1,   // 上次 10
    0, -1,  1,  0    // 上次 11
};

// ---------- 编码器状态（中断和主循环共享，必须 volatile） ----------
static volatile uint8_t enc_raw = 0;     // 当前 A/B 状态
static volatile int16_t enc_acc = 0;     // 累计的四分之一状态数
static uint8_t enc_irq_ok = 0;           // 外部中断有没有挂上
static uint8_t enc_pulse_left = 0;       // "按下"位还要保持几轮
static uint8_t enc_pulse_bit = 0;        // 保持的是哪一位

// 读一次 A/B 原始电平
static inline uint8_t encReadRaw() {
    return (uint8_t)((digitalRead(ENC_L_A) << 1) | digitalRead(ENC_L_B));
}

// 中断服务程序：只查表累加，越短越好
static void encIsr() {
    uint8_t s = encReadRaw();
    int8_t d = enc_table[((uint8_t)enc_raw << 2) | s];
    enc_raw = s;
    if (d != 0) enc_acc += d;
}

// 引脚不支持外部中断时的兜底：仍然在 loop 里采样，但用同一张表
static void encPollFallback() {
    uint8_t s = encReadRaw();
    int8_t d = enc_table[((uint8_t)enc_raw << 2) | s];
    enc_raw = s;
    if (d != 0) enc_acc += d;
}

// 第一次调用时把中断挂上。
// 必须等 pinMode() 之后（也就是 setup() 里配好引脚之后）才挂，所以放在 loop 里懒加载。
static void encInitOnce() {
    static uint8_t done = 0;
    if (done) return;
    done = 1;

    pinMode(ENC_L_A, INPUT_PULLUP);
    pinMode(ENC_L_B, INPUT_PULLUP);
    enc_raw = encReadRaw();

    if (digitalPinToInterrupt(ENC_L_A) != NOT_AN_INTERRUPT &&
        digitalPinToInterrupt(ENC_L_B) != NOT_AN_INTERRUPT) {
        attachInterrupt(digitalPinToInterrupt(ENC_L_A), encIsr, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ENC_L_B), encIsr, CHANGE);
        enc_irq_ok = 1;
    }
}

// ---------- 核心处理函数（兼容原接口：返回这次该走的方向，没有就返回 0） ----------
int8_t processEncoder() {
    encInitOnce();
    if (!enc_irq_ok) encPollFallback();

    int8_t dir = 0;

    // 取累计值并扣掉一格：这几行必须原子，
    // 否则可能和中断里的 enc_acc += 打架，丢掉一次跳变。
    noInterrupts();
    int16_t acc = enc_acc;
    if (acc >= ENCODER_STEPS_PER_DETENT) {
        enc_acc = acc - ENCODER_STEPS_PER_DETENT;
        dir = DIR_CW;
    } else if (acc <= -ENCODER_STEPS_PER_DETENT) {
        enc_acc = acc + ENCODER_STEPS_PER_DETENT;
        dir = DIR_CCW;
    }
    interrupts();

    return dir;
}

// ---------- 主刷新函数（在 loop 中调用） ----------
void encoderRotateStateRefresh() {
    int8_t dir = processEncoder();

    // 上一步的"按下"还没保持够轮数，先把这一位补上
    if (enc_pulse_left > 0) {
        report.mediaButtons |= enc_pulse_bit;
        enc_pulse_left--;
    }

    if (dir == 0) return;

    // 一格一次：逆时针 -> 音量增(bit2)，顺时针 -> 音量减(bit1)（沿用原来的映射）
    enc_pulse_bit = (dir == DIR_CCW) ? (uint8_t)(1 << 2) : (uint8_t)(1 << 1);
    enc_pulse_left = ENCODER_PULSE_LOOPS;
    report.mediaButtons |= enc_pulse_bit;
}

// ---------- 编码器按钮（校准功能） ----------
void ENCButtonOnPress(void (*func)()) {
    static int bt_hold = 0;
    if (digitalRead(ENC_BT) == 0) {
        bt_hold++;
    } else {
        if (bt_hold > 10) {
            bt_hold = 0;
            func();
        }
    }
}

#endif
