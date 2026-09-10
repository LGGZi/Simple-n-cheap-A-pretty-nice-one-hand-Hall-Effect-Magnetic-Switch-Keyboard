#ifndef EncoderManager_h
#define EncoderManager_h

#include "Universal.h"
#include "SubHID.h"

// ---------- 方向常量 ----------
#define DIR_CW   1   // 顺时针
#define DIR_CCW -1   // 逆时针

// ---------- 格雷码状态顺序（顺时针） ----------
static const uint8_t graySeq[4] = {0, 1, 3, 2};  // 00 -> 01 -> 11 -> 10 -> 00

// ---------- 编码器状态 ----------
static struct {
    uint8_t last_raw;      // 上次处理的原始状态 (bit1=A, bit0=B)
    int8_t  step_count;    // 步进累计值 (达到 ±4 触发事件)
} enc = {0xFF, 0};

// ---------- 核心处理函数（每 loop 调用一次） ----------
int8_t processEncoder() {
    // 读取原始电平，不做去抖
    uint8_t raw = (digitalRead(ENC_L_A) << 1) | digitalRead(ENC_L_B);
    if (raw == enc.last_raw) return 0;   // 状态未变

    // 查找旧状态和新状态在格雷码序列中的索引
    int8_t idx_old = -1, idx_new = -1;
    for (int i = 0; i < 4; i++) {
        if (graySeq[i] == enc.last_raw) idx_old = i;
        if (graySeq[i] == raw) idx_new = i;
    }
    // 若任一状态无效（理论上不会），重置并返回
    if (idx_old == -1 || idx_new == -1) {
        enc.last_raw = raw;
        return 0;
    }

    // 计算方向差（处理环绕）
    int8_t diff = idx_new - idx_old;
    if (diff == -3) diff = 1;    // 0 -> 3 顺时针
    else if (diff == 3) diff = -1; // 3 -> 0 逆时针

    // 更新状态
    enc.last_raw = raw;
    // 只有 diff 为 ±1 才累加，其他情况忽略（理论上不会发生）
    if (diff == 1 || diff == -1) {
        enc.step_count += diff;
    }

    // 检查是否完成一个完整周期（4步）
    if (enc.step_count >= 4) {
        enc.step_count -= 4;     // 保留余量，避免累积误差
        return DIR_CW;           // 顺时针
    } else if (enc.step_count <= -4) {
        enc.step_count += 4;
        return DIR_CCW;          // 逆时针
    }
    return 0;   // 未完成一个周期
}

// ---------- 主刷新函数（在 loop 中调用） ----------
void encoderRotateStateRefresh() {
    int8_t dir = processEncoder();
    if (dir == 0) return;

    // 方向映射：逆时针 → 音量增(bit2)，顺时针 → 音量减(bit1)
    if (dir == DIR_CCW) {
        report.mediaButtons |= (1 << 2);
    } else if (dir == DIR_CW) {
        report.mediaButtons |= (1 << 1);
    }

    // 发送“按下”报告
    HIDCon.setState(&report);

    // 立即清除两位（模拟释放）
    report.mediaButtons &= ~((1 << 1) | (1 << 2));

    // 发送“释放”报告
    HIDCon.setState(&report);
}

// ---------- 编码器按钮（校准功能，保持不变） ----------
void ENCButtonOnPress(void (*func)()) {
    static int bt_hold = 0;
    if (digitalRead(ENC_BT) == 0) {
        bt_hold++;
    } else {
        if (bt_hold > 10) {
            bt_hold = 0;
            *func;
        }
    }
}

#endif