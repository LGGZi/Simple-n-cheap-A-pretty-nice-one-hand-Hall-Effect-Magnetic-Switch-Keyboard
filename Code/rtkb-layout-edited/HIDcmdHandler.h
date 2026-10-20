#ifndef HIDcmdHandler_h
#define HIDcmdHandler_h

#include <stdint.h>
#include "EEPROMManager.h"
#include "HallUnitManager.h"
#include "LayoutManager.h"
// Byte 1, CMD Code
#define CMD_SET_MAP      0x01
#define CMD_SET_PARAM    0x02
#define CMD_CALIBRATE    0x03
#define CMD_QUERY        0x04
#define CMD_ACK          0x05
#define CMD_FIN          0x06
#define CMD_REFRESH      0x07

// Byte 2, Device, when mapping keys
#define NO_DEVICE 0x00
#define MAP_TYPE_KEYBOARD   0x01
#define MAP_TYPE_MOUSE      0x02
#define MAP_TYPE_GAMEPAD    0x03
#define MAP_TYPE_CONSUMER   0x04
#define MAP_TYPE_CONTROLLER_XAXIS_LEFT 0x05
#define MAP_TYPE_CONTROLLER_YAXIS_LEFT 0x06
#define MAP_TYPE_CONTROLLER_XAXIS_RIGHT 0x07
#define MAP_TYPE_CONTROLLER_YAXIS_RIGHT 0x08
#define MAP_TYPE_MOUSE_X 0x09
#define MAP_TYPE_MOUSE_Y 0x0A
#define MAP_TYPE_CONTROLLER_LEFT_TRIGGER 0x0B
#define MAP_TYPE_CONTROLLER_RIGHT_TRIGGER 0x0C
// Byte 2, when setting parameter
#define RT_ACTIVE       0x04
#define RT_DEACTIVE     0x06
#define UPPER_DEADZONE  0x08
#define LOWER_DEADZONE  0x0A
// Byte 2, When calibrating：CQ_ALL / CQ_SINGLE
#define CQ_ALL     0x00
#define CQ_SINGLE  0x01
// Byte 2, When querying
#define QUERY_MAPPING       0x00
#define QUERY_PARAM         0x01
#define QUERY_CALIB         0x02
#define QUERY_DEVICE_INFO   0xFF
// 查询配列（布局）：[1] = QUERY_LAYOUT，固件逐键回 0xFE 帧后回 FIN
#define QUERY_LAYOUT        0xFE
// 查询指令布局：[0]=CMD_QUERY [1]=查询内容 [2]=CQ_ALL/CQ_SINGLE [3]=按键号 [7]=序号
//Byte 2, When refresh
#define REFRESH_MAPPING       0x00
#define REFRESH_PARAM         0x01

// Byte 3, 
/*  Key number(mapping, parameter setting, calibrating single, querying single),
    ROW/COL number(calibrating or querying a line of keys)*/

// Byte 4, Report data offset/parameter value high byte

// Byte 5, Binding's operation when mapping
#define MAP_DO_APPEND   0x00
#define MAP_DO_OVERRIDE 0x01
#define MAP_DO_CLEAR    0x02



// Byte 6, when MAP_DO_OVERRIDE：该按键映射区里的第几个槽（0 开始，< MAXIMUM_BINDABLE_KEY）/when MAP_DO_CLEAR：清空哪个按键，值为按键编号（0开始），BUTTONCOUNT为全部清空

uint8_t pre_cmd[8];

/*-------------------------------------------------------------
独立的设备信息报文：结构为 
[0xFF]、 [BUTTONCOUNT]、 [是否为磁轴0xF0 == y|0x0F == n]、 [编码器数量，0就是没有）]、 [设备类型（保留给后续手台主控使用）]、 
后二位保留、最后一位固件版本号
版本号格式示例 0 1 | 0 1 1 | 0 0 1  ==  1.31
前两位小数点前整数、中间三位小数点后一位、最后三位为小数点后第二位
*/
void HIDDeviceInfoUpload(){
    uint8_t resp[8] = { 0xFF, BUTTONCOUNT, 0xF0, 0x01, 0x01, LAYOUT_KIND, HOST_VERSION_REQUIRED, FIRMWARE_VERSION };
    HID().SendReport(4, resp, 8);
}

//收到指令立刻回的应答，格式与上位机发送的一致（Byte1 换成 CMD_ACK，其余保留）
void HIDSendAck(const uint8_t *cmdBytes) {
    uint8_t ack[8] = { CMD_ACK, cmdBytes[0], cmdBytes[7], 0, 0, 0, 0, 0 };
    HID().SendReport(4, ack, 8);
}
//一条指令处理完毕回的完成标志，格式同样与上位机发送的一致（Byte1 换成 CMD_FIN，其余原样回传），
//这样上位机既能确认收到了，也能确认执行完了（校准这种阻塞操作特别需要）
void HIDSendFin(const uint8_t *cmdBytes) {
    uint8_t fin[8];
    for(uint8_t i = 0; i < 8; i++){
        fin[i] = cmdBytes[i];
    }
    fin[0] = CMD_FIN;
    HID().SendReport(4, fin, 8);
}
//校准用：Byte3 写上刚校准完的按键号，上位机可以据此显示逐个校准的进度
void HIDSendCalibFin(const uint8_t *cmdBytes, uint8_t key) {
    uint8_t fin[8];
    for(uint8_t i = 0; i < 8; i++){
        fin[i] = cmdBytes[i];
    }
    fin[0] = CMD_FIN;
    fin[2] = key;
    HID().SendReport(4, fin, 8);
}

// ------------------------------------------------------------
// 查询上传：把查到的内容逐条发回去，每条都构造成"和上位机下发时一样的 8 字节"，
// 上位机可以直接复用下发那一套字段位置来解析：
//   键值：[0]=CMD_SET_MAP,   [1]=device, [2]=key, [3]=offset, [4]=MAP_DO_OVERRIDE, [5]=slot
//   参数：[0]=CMD_SET_PARAM, [1]=parameter, [2]=key, [3]=valueHi, [4]=valueLo
// 读 EEPROM 的动作在 EEPROMManager 里，这里只负责"读一条发一条"，保证原子性
// ------------------------------------------------------------
void HIDSendMapItem(uint8_t device, uint8_t key, uint8_t offset, uint8_t slot) {
    uint8_t resp[8] = { CMD_SET_MAP, device, key, offset, MAP_DO_OVERRIDE, slot, 0, 0 };
    HID().SendReport(4, resp, 8);
}
void HIDSendParamItem(uint8_t parameter, uint8_t key, uint16_t value) {
    uint8_t resp[8] = { CMD_SET_PARAM, parameter, key, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF), 0, 0, 0 };
    HID().SendReport(4, resp, 8);
}

void HIDQueryUpload(uint8_t queryWhat, uint8_t queryScope, uint8_t key) {
    if(queryScope == CQ_SINGLE && !(key < BUTTONCOUNT)){
        return;   //单键查询的按键号非法
    }
    uint8_t first_key = (queryScope == CQ_SINGLE) ? key : 0;
    uint8_t last_key  = (queryScope == CQ_SINGLE) ? key : (BUTTONCOUNT - 1);
    const uint8_t params[4] = { RT_ACTIVE, RT_DEACTIVE, UPPER_DEADZONE, LOWER_DEADZONE };

    for(uint8_t k = first_key; k <= last_key; k++){
        if(queryWhat == QUERY_PARAM){
            for(uint8_t i = 0; i < 4; i++){
                HIDSendParamItem(params[i], k, keyParameterGetValue(k, params[i]));
            }
        }else{   //QUERY_MAPPING
            for(uint8_t slot = 0; slot < MAXIMUM_BINDABLE_KEY; slot++){
                uint8_t offset = 0;
                uint8_t device = keyMapGetFromEEPROM(k, slot, &offset);
                if(device == NO_DEVICE){
                    break;   //空槽：本按键的映射到此为止
                }
                HIDSendMapItem(device, k, offset, slot);
            }
        }
    }
}
/**
 * 解析 8 字节 HID 报告（Report ID 4 的数据部分）
 * @param report 8字节数组指针
 */
void HIDcmdResponse(const uint8_t *report_cmd) {
    //先做一份快照：校准是阻塞操作，期间 USB 中断可能已经把 HIDcmd 改写成新指令，
    //如果一直引用 report_cmd，最后的 FIN 就会回成别的指令
    uint8_t cmd_buf[8];
    for(uint8_t i = 0; i < 8; i++){
        cmd_buf[i] = report_cmd[i];
    }
    HIDSendAck(cmd_buf);
    uint8_t cmd = cmd_buf[0];
    switch (cmd) {
        // ------------------------------------------------------------
        // 命令 1：修改按键映射
        // ------------------------------------------------------------
        case CMD_SET_MAP: {
            uint8_t operation = cmd_buf[4];      //Byte5
            switch (operation){
                case MAP_DO_APPEND:{
                    uint8_t device = cmd_buf[1]; //Byte2
                    uint8_t key = cmd_buf[2];    //Byte3
                    uint8_t offset = cmd_buf[3]; //Byte4
                    keyCodeAddToEEPROM(key, device, offset);
                break;
                }
                case MAP_DO_OVERRIDE:{
                    uint8_t device = cmd_buf[1];         //Byte2
                    uint8_t key = cmd_buf[2];            //Byte3
                    uint8_t offset = cmd_buf[3];         //Byte4
                    uint8_t slot = cmd_buf[5];   //Byte6：本按键的第几个映射槽
                    if(slot < MAXIMUM_BINDABLE_KEY){
                        _keyCodeSetToEEPROM(key, slot, device, offset);
                    }
                break;
                }
                case MAP_DO_CLEAR:{
                    #if DEBUG
                        Serial.print("收到清空指令\n");
                    #endif
                    uint8_t key = cmd_buf[2];           //Byte3
                    uint8_t clear_whom = cmd_buf[5];    //Byte6
                    if(clear_whom == BUTTONCOUNT){
                        keyCodeClearAll();
                    }else if(clear_whom >= 0 && clear_whom < BUTTONCOUNT){
                        keyMapClearInEEPROM(key, clear_whom);
                        //写完后用这个指令，把它后面的槽全部设成空槽
                    }
                    keyCodeInit();
                break;
                }
            }
            HIDSendFin(cmd_buf);
        break;
        }
        // ------------------------------------------------------------
        // 命令 2：修改按键参数（磁轴四个参数）
        // ------------------------------------------------------------
        case CMD_SET_PARAM: {
            // ------------------------------------------------------------
            // 在此处调用的“修改参数”函数
            // ------------------------------------------------------------
            uint8_t parameter = cmd_buf[1];
            uint8_t key = cmd_buf[2];
            uint16_t value = (uint16_t)cmd_buf[3]  << 8 | cmd_buf[4];
            keyParameterSet(key, parameter, value);
            HIDSendFin(cmd_buf);
        break;
        }

        // ------------------------------------------------------------
        // 命令 3：校准操作
        // ------------------------------------------------------------
        case CMD_CALIBRATE: {
            uint8_t mode = cmd_buf[1];
            uint8_t offset = cmd_buf[2];
            if(mode == CQ_ALL){
                for(int i = 0; i < BUTTONCOUNT; i++){
                    calibrateKey(i);
                    //每个按键校准完立刻回一个带按键号的FIN，上位机可以据此显示逐个校准的进度；
                    //最后一个（i == BUTTONCOUNT-1）就是整条指令的完成标志，不再额外回FIN
                    HIDSendCalibFin(cmd_buf, (uint8_t)i);
                }
            }else if(mode == CQ_SINGLE){
                calibrateKey(offset);
                HIDSendCalibFin(cmd_buf, offset);
            }else{
                HIDSendFin(cmd_buf);
            }
        break;
        }

        // ------------------------------------------------------------
        // 命令 4：查询请求
        // [1]=查询内容(QUERY_MAPPING/QUERY_PARAM) [2]=CQ_ALL/CQ_SINGLE [3]=按键号
        // ------------------------------------------------------------
        case CMD_QUERY: {
            uint8_t query_what  = cmd_buf[1];   //Byte2
            uint8_t query_scope = cmd_buf[2];   //Byte3
            uint8_t key        = cmd_buf[3];   //Byte4
            if(query_what == QUERY_DEVICE_INFO){
                HIDDeviceInfoUpload();
            }else if(query_what == QUERY_LAYOUT){
                // 逐键上传布局，最后回 FIN（与其它查询一致）
                HIDLayoutUpload(cmd_buf);
                HIDSendFin(cmd_buf);
            }else{
                HIDQueryUpload(query_what, query_scope, key);
                HIDSendFin(cmd_buf);
            }
        break;
        }
        // ------------------------------------------------------------
        // 命令 7：刷新请求
        // 字节2 = 刷新内容
        // ------------------------------------------------------------
        case CMD_REFRESH: {
            uint8_t refresh_what = cmd_buf[1];  //Byte2
            if(refresh_what == REFRESH_MAPPING){
                keyCodeInit();
            }else if(refresh_what == REFRESH_PARAM){
                keyParameterInit();
            }
            HIDSendFin(cmd_buf);
        break;
        }
        default:
            // 未知命令码：也回一个 FIN，免得上位机一直等在那
            HIDSendFin(cmd_buf);
        break;
    }
}

void HIDcmdResponseIfChanged(){
    if(memcmp(pre_cmd, HIDcmd, 8) != 0){
        // Serial.print("cmd get\n");
        HIDcmdResponse(HIDcmd);
        memcpy(pre_cmd, HIDcmd, 8);
    }
}
#endif