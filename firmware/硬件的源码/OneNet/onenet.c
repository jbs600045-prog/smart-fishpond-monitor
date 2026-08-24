/**
 * 文件名  : onenet.c
 * 作者    : 金波胜
 * 日期    : 2026-6-16
 * 功能    : OneNET MQTT 云平台通信协议层
 *
 * 上传 19项: 气泵/湿度/温度/报警/溶解氧/风扇/调速值/烟雾/pH值/
 *            气压/水泵/烟雾阈值/水温/气温阈值/溶解氧阈值/pH阈值/
 *            水温阈值/控制模式
 * 下发   : App远程控制+阈值调节+模式切换
 **/

/*========== 单片机头文件 ==========*/
#include "stm32f10x.h"

/*========== 网络设备 ==========*/
#include "esp8266.h"

/*========== 协议层 ==========*/
#include "onenet.h"
#include "mqttkit.h"

/*========== 加密算法 ==========*/
#include "base64.h"
#include "hmac_sha1.h"

/*========== 硬件驱动 ==========*/
#include "usart.h"
#include "delay.h"
#include "beep.h"
#include "fan.h"
#include "relay.h"

/*========== C标准库 ==========*/
#include <string.h>
#include <stdio.h>
#include "cJSON.h"

/*==================== OneNET 平台参数  ====================*/
#define PROID         "YOUR_PRODUCT_ID"            /* 产品ID */
#define ACCESS_KEY    "'YOUR_ACCESS_KEY"  /* 访问密钥 */
#define DEVICE_NAME   "YOUR_DEVICE_NAME"                     /* 设备名称 */

char devid[16];          /* 设备注册后获得的 device_id */
char key[48];            /* 设备注册后获得的 key */

extern unsigned char esp8266_buf[512];   /* ESP8266 接收缓冲区 */

/*==================== 引用 main.c 全局变量 ====================*/
extern float water_temp;           /* 水温 */
extern float bmp_pressure;         /* 大气压 */
extern float bmp_temp;             /* BMP280 芯片温度 */
extern float do_value;             /* 溶解氧 */
extern float ph_value;             /* pH值 */
extern u8    air_temp, air_humi;   /* 空气温湿度 */
extern float mq2_vol;              /* 烟雾浓度 */
extern float smog_th;              /* 烟雾阈值 */
extern u8    aerator_status;       /* 气泵状态 */
extern u8    pump_status;          /* 水泵状态 */
extern u8    fan_adj;              /* 风扇调速值 */
extern u8    Fan_Status;           /* 风扇开关状态(fan.c) */
extern uint8_t Aerator_Status;     /* 气泵运行状态(fan.c) */
extern u8    auto_mode;            /* 控制模式: 1=自动 0=手动 */
extern u8    alarm_flag;           /* 报警标志: 1=报警 0=正常 */

/* 阈值变量 (可App远程修改) */
extern float do_low_th;            /* 溶解氧低阈值 */
extern float do_high_th;           /* 溶解氧高阈值 */
extern float ph_low_th;            /* pH低阈值 */
extern float ph_high_th;           /* pH高阈值 */
extern float water_temp_high_th;   /* 水温高阈值 */
extern float water_temp_low_th;    /* 水温低阈值 */
extern float air_temp_high_th;     /* 气温高阈值 */

/*==================== URL特殊字符编码 ====================*/
static unsigned char OTA_UrlEncode(char *sign)
{
    char sign_t[40];
    unsigned char i = 0, j = 0;
    unsigned char sign_len = strlen(sign);

    if(sign == (void *)0 || sign_len < 28)
        return 1;

    /* 备份原始签名 */
    for(; i < sign_len; i++)
    {
        sign_t[i] = sign[i];
        sign[i] = 0;
    }
    sign_t[i] = 0;

    /* 逐个字符检查并替换 */
    for(i = 0, j = 0; i < sign_len; i++)
    {
        switch(sign_t[i])
        {
            case '+': strcat(sign + j, "%2B"); j += 3; break;
            case ' ': strcat(sign + j, "%20"); j += 3; break;
            case '/': strcat(sign + j, "%2F"); j += 3; break;
            case '?': strcat(sign + j, "%3F"); j += 3; break;
            case '%': strcat(sign + j, "%25"); j += 3; break;
            case '#': strcat(sign + j, "%23"); j += 3; break;
            case '&': strcat(sign + j, "%26"); j += 3; break;
            case '=': strcat(sign + j, "%3D"); j += 3; break;
            default:  sign[j] = sign_t[i]; j++; break;
        }
    }
    sign[j] = 0;
    return 0;
}

/*==================== OneNET 鉴权: HMAC-SHA1 签名算法 ====================*/
#define METHOD  "sha1"
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et,
    char *access_key, char *dev_name, char *authorization_buf,
    unsigned short authorization_buf_len, _Bool flag)
{
    size_t olen = 0;
    char sign_buf[64];
    char hmac_sha1_buf[64];
    char access_key_base64[64];
    char string_for_signature[72];

    /* 参数校验 */
    if(ver == (void *)0 || res == (void *)0 || et < 1564562581 ||
       access_key == (void *)0 || authorization_buf == (void *)0 ||
       authorization_buf_len < 120)
        return 1;

    /* 步骤1: Base64解码 access_key 得到原始密钥字节 */
    memset(access_key_base64, 0, sizeof(access_key_base64));
    BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64),
                  &olen, (unsigned char *)access_key, strlen(access_key));

    /* 步骤2: 构建签名原文 */
    memset(string_for_signature, 0, sizeof(string_for_signature));
    if(flag)    /* 注册设备用 (不含 device_name) */
        snprintf(string_for_signature, sizeof(string_for_signature),
                 "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
    else        /* 设备登录用 (含 device_name) */
        snprintf(string_for_signature, sizeof(string_for_signature),
                 "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);


    /* 步骤3: HMAC-SHA1 签名 */
    memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));
    hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
              (unsigned char *)string_for_signature, strlen(string_for_signature),
              (unsigned char *)hmac_sha1_buf);

    /* 步骤4: Base64编码 + URL编码 + 拼接 */
    olen = 0;
    memset(sign_buf, 0, sizeof(sign_buf));
    BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen,
                  (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));

    OTA_UrlEncode(sign_buf);

    /* 拼接鉴权字符串 */
    if(flag)
        snprintf(authorization_buf, authorization_buf_len,
                 "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s",
                 ver, res, et, METHOD, sign_buf);
    else
        snprintf(authorization_buf, authorization_buf_len,
                 "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s",
                 ver, res, dev_name, et, METHOD, sign_buf);

    return 0;
}

/*==================== 设备注册 (首次使用, 仅需执行一次) ====================*/
_Bool OneNET_RegisterDevice(void)
{
    _Bool result = 1;
    unsigned short send_len = 11 + strlen(DEVICE_NAME);
    char *send_ptr = NULL, *data_ptr = NULL;
    char authorization_buf[144];

    send_ptr = malloc(send_len + 240);
    if(send_ptr == NULL)
        return result;

    /* TCP连接 OneNET 设备注册服务器 (HTTP) */
    while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
        DelayXms(500);

    /* 生成注册用鉴权Token */
    OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
                         authorization_buf, sizeof(authorization_buf), 1);

    /* 构造 HTTP POST 请求 */
    snprintf(send_ptr, 240 + send_len,
             "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
             "Authorization:%s\r\n"
             "Host:ota.heclouds.com\r\n"
             "Content-Type:application/json\r\n"
             "Content-Length:%d\r\n\r\n"
             "{\"name\":\"%s\"}",
             authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);

    ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));

    /* 解析服务器响应, 提取 device_id 和 key */
    data_ptr = (char *)ESP8266_GetIPD(250);
    if(data_ptr)
        data_ptr = strstr(data_ptr, "device_id");

    if(data_ptr)
    {
        char name[16];
        int pid = 0;
        if(sscanf(data_ptr,
                  "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"",
                  devid, name, &pid, key) == 4)
        {
            UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
            result = 0;
        }
    }

    free(send_ptr);
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");    /* 关闭HTTP连接 */
    return result;
}

/*==================== MQTT设备登录 OneNET ====================*/
_Bool OneNet_DevLink(void)
{
    MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
    unsigned char *dataPtr;
    char authorization_buf[160];
    _Bool status = 1;           /* 默认失败 */

    /* 生成设备登录鉴权Token */
    OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, DEVICE_NAME,
                         authorization_buf, sizeof(authorization_buf), 0);

    UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
                            "NAME: %s, PROID: %s, KEY:%s\r\n",
                DEVICE_NAME, PROID, authorization_buf);

    /* 构建MQTT CONNECT报文 */
    if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME,
                          1000, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
    {
        ESP8266_SendData(mqttPacket._data, mqttPacket._len);   /* 发送 CONNECT */
        dataPtr = ESP8266_GetIPD(250);                          /* 等待 CONNACK 响应 */

        if(dataPtr != NULL)
        {
            if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
            {
                switch(MQTT_UnPacketConnectAck(dataPtr))        /* 解析返回码 */
                {
                    case 0: UsartPrintf(USART_DEBUG, "Tips: 连接成功\r\n"); status = 0; break;
                    case 1: UsartPrintf(USART_DEBUG, "WARN: 协议错误\r\n"); break;
                    case 2: UsartPrintf(USART_DEBUG, "WARN: 非法clientid\r\n"); break;
                    case 3: UsartPrintf(USART_DEBUG, "WARN: 服务器不可用\r\n"); break;
                    case 4: UsartPrintf(USART_DEBUG, "WARN: 用户名密码错误\r\n"); break;
                    case 5: UsartPrintf(USART_DEBUG, "WARN: 未授权(token非法)\r\n"); break;
                    default: UsartPrintf(USART_DEBUG, "ERR: 未知错误\r\n"); break;
                }
            }
        }
        MQTT_DeleteBuffer(&mqttPacket);       /* 释放MQTT报文内存 */
    }
    else
        UsartPrintf(USART_DEBUG, "WARN: MQTT_PacketConnect Failed\r\n");

    return status;
}

/*==================== 构造上传JSON (18项数据) ====================*/
unsigned char OneNet_FillBuf(char *buf)
{
    char text[64];

    strcpy(buf, "{\"id\":\"123\",\"params\":{");      /* JSON头部 */

    /* 1.气泵状态 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"aerator\":{\"value\":%s},", aerator_status ? "true" : "false");strcat(buf, text);
    /* 2.空气湿度 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"air_humi\":{\"value\":%d},", air_humi);strcat(buf, text);
    /* 3.空气温度 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"air_temp\":{\"value\":%d},", air_temp);strcat(buf, text);
    /* 4.报警状态 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"alarm\":{\"value\":%s},", alarm_flag ? "true" : "false");strcat(buf, text);
    /* 5.溶解氧(估算值) */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"do_value\":{\"value\":%.1f},", do_value);strcat(buf, text);
    /* 6.风扇状态 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"fan\":{\"value\":%s},", Fan_Status ? "true" : "false");strcat(buf, text);
    /* 7.风扇调速值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"fan_adj\":{\"value\":%d},", fan_adj);strcat(buf, text);
    /* 8.烟雾浓度 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"mq2_vol\":{\"value\":%.2f},", mq2_vol);strcat(buf, text);
    /* 9.pH值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"ph_value\":{\"value\":%.1f},", ph_value);strcat(buf, text);
    /* 10.大气压 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"pressure_kpa\":{\"value\":%.1f},", bmp_pressure);strcat(buf, text);
    /* 11.水泵状态 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"pump\":{\"value\":%s},", pump_status ? "true" : "false");strcat(buf, text);
    /* 12.烟雾阈值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"smog_th\":{\"value\":%.1f},", smog_th);strcat(buf, text);
    /* 13.水温 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"water_temp\":{\"value\":%.1f},", water_temp);strcat(buf, text);
    /* 14.气温高阈值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"air_temp_high_th\":{\"value\":%.1f},", air_temp_high_th);strcat(buf, text);
    /* 15.溶解氧低阈值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"do_low_th\":{\"value\":%.1f},", do_low_th);strcat(buf, text);
    /* 16.pH低阈值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"ph_low_th\":{\"value\":%.1f},", ph_low_th);strcat(buf, text);
    /* 17.水温高阈值 */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"water_temp_high_th\":{\"value\":%.1f},", water_temp_high_th);strcat(buf, text);
    /* 18.控制模式 (最后一个字段, 不加逗号) */
    memset(text, 0, sizeof(text));
    sprintf(text, "\"auto_mode\":{\"value\":%s}", auto_mode ? "true" : "false");strcat(buf, text);

    strcat(buf, "}}");    /* JSON尾部 */
    return strlen(buf);
}

/*==================== 上传数据到OneNET (MQTT PUBLISH, QoS=1) ====================*/
void OneNet_SendData(void)
{
    char buf[640];                           /* JSON payload缓冲区 */
    char topic_buf[56];                      /* 发布主题缓冲区 */
    static unsigned short pub_id = 100;      /* 报文ID (静态, 递增) */

    memset(buf, 0, sizeof(buf));
    OneNet_FillBuf(buf);                     /* 构建JSON */

    /* 发布主题: $sys/{产品ID}/{设备名}/thing/property/post */
    snprintf(topic_buf, sizeof(topic_buf),
             "$sys/%s/%s/thing/property/post", PROID, DEVICE_NAME);

    UsartPrintf(USART_DEBUG, "SEND id=%d: %s\r\n", pub_id, buf);

    /* 构建MQTT PUBLISH报文, 通过ESP8266发送 */
    {
        MQTT_PACKET_STRUCTURE pkt = {NULL, 0, 0, 0};
        if(MQTT_PacketPublish(pub_id, topic_buf, buf, strlen(buf),
                              MQTT_QOS_LEVEL1, 0, 1, &pkt) == 0)
        {
            ESP8266_SendData(pkt._data, pkt._len);   /* 发送 RAW 数据 */
            MQTT_DeleteBuffer(&pkt);                  /* 释放内存 */
            UsartPrintf(USART_DEBUG, "Send OK\r\n");
        }
        else
            UsartPrintf(USART_DEBUG, "WARN: PacketPublish Failed\r\n");
    }

    if(++pub_id == 0) pub_id = 1;    /* 递增报文ID (0保留不使) */
}

/*==================== 发布消息到指定主题 (用于set_reply回复) ====================*/
void OneNET_Publish(const char *topic, const char *msg)
{
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};
    UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);

    if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg),
                          MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
    {
        ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);
        MQTT_DeleteBuffer(&mqtt_packet);
    }
}

/*==================== 订阅属性设置主题 (接收App下发指令) ====================*/
void OneNET_Subscribe(void)
{
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};
    char topic_buf[56];
    const char *topic = topic_buf;

    /* 订阅主题: $sys/{产品ID}/{设备名}/thing/property/set */
    snprintf(topic_buf, sizeof(topic_buf),
             "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);
    UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);

    if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL1, &topic, 1,
                            &mqtt_packet) == 0)
    {
        unsigned char *suback_ptr;
        ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);
        MQTT_DeleteBuffer(&mqtt_packet);

        suback_ptr = ESP8266_GetIPD(200);     /* 等待 SUBACK 确认 */
        if(suback_ptr != NULL)
        {
            if(MQTT_UnPacketSubscribe(suback_ptr) == 0)
                UsartPrintf(USART_DEBUG, "Subscribe OK\r\n");
            else
                UsartPrintf(USART_DEBUG, "WARN: Subscribe Failed!\r\n");
        }
        else
            UsartPrintf(USART_DEBUG, "WARN: No SUBACK response!\r\n");
    }
}

/*==================== 解析并执行App下发的控制指令 ====================*/
void OneNet_RevPro(unsigned char *cmd)
{
    char *req_payload = NULL;
    char *cmdid_topic = NULL;
    unsigned short topic_len = 0;
    unsigned short req_len = 0;
    unsigned char qos = 0;
    static unsigned short pkt_id = 0;
    unsigned char type = 0;
    short result = 0;
    char req_id[64] = {0};
    cJSON *raw_json, *params_json, *item_json, *val_json;

    type = MQTT_UnPacketRecv(cmd);        /* 识别MQTT报文类型 */
    switch(type)
    {
        case MQTT_PKT_PUBLISH:             /* App下发了属性设置指令 */
            result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len,
                                          &req_payload, &req_len, &qos, &pkt_id);
            if(result == 0)
            {
                UsartPrintf(USART_DEBUG, "topic: %s, payload: %s\r\n",
                            cmdid_topic, req_payload);

                raw_json = cJSON_Parse(req_payload);     /* 解析JSON */
                if(raw_json == NULL)
                {
                    UsartPrintf(USART_DEBUG, "ERR: JSON Parse Failed!\r\n");
                }
                else
                {
                    /* 提取请求ID (兼容字符串/数字) */
                    {
                        cJSON *id_json = cJSON_GetObjectItem(raw_json, "id");
                        if(id_json != NULL)
                        {
                            if(id_json->type == cJSON_String && id_json->valuestring != NULL)
                                strncpy(req_id, id_json->valuestring, sizeof(req_id) - 1);
                            else if(id_json->type == cJSON_Number)
                                snprintf(req_id, sizeof(req_id), "%.0f", id_json->valuedouble);
                        }
                    }
                    UsartPrintf(USART_DEBUG, "RevPro req_id=[%s]\r\n", req_id);

                    params_json = cJSON_GetObjectItem(raw_json, "params");
                    if(params_json != NULL)
                    {
                        /*==== 气泵开关 (收到即切手动模式) ====*/
                        item_json = cJSON_GetObjectItem(params_json, "aerator");
                        if(item_json != NULL)
                        {
                            val_json = cJSON_GetObjectItem(item_json, "value");
                            if(val_json == NULL) val_json = item_json;
                            if(val_json->type == cJSON_True || val_json->valueint == 1)
                            {
                                Aerator_Set(AERATOR_ON);       /* PA9=HIGH → MOS导通 → 气泵开 */
                                aerator_status = 1; auto_mode = 0;
                            }
                            else
                            {
                                Aerator_Set(AERATOR_OFF);      /* PA9=LOW → MOS截止 → 气泵关 */
                                aerator_status = 0; auto_mode = 0;
                            }
                        }

                        /*==== 水泵开关 ====*/
                        item_json = cJSON_GetObjectItem(params_json, "pump");
                        if(item_json != NULL)
                        {
                            val_json = cJSON_GetObjectItem(item_json, "value");
                            if(val_json == NULL) val_json = item_json;
                            if(val_json->type == cJSON_True || val_json->valueint == 1)
                            {
                                Relay_Pump_Set(RELAY_PUMP_ON);     /* PA10=LOW → 继电器吸合 */
                                pump_status = 1; auto_mode = 0;
                            }
                            else
                            {
                                Relay_Pump_Set(RELAY_PUMP_OFF);    /* PA10=HIGH → 继电器断开 */
                                pump_status = 0; auto_mode = 0;
                            }
                        }

                        /*==== 风扇开关 ====*/
                        item_json = cJSON_GetObjectItem(params_json, "fan");
                        if(item_json != NULL)
                        {
                            val_json = cJSON_GetObjectItem(item_json, "value");
                            if(val_json == NULL) val_json = item_json;
                            if(val_json->type == cJSON_True || val_json->valueint == 1)
                            {
                                Fan_Set(FAN_ON);                   /* TIM1_CH1 PWM 99% */
                                fan_adj = 99; auto_mode = 0;
                            }
                            else
                            {
                                Fan_Set(FAN_OFF);                  /* TIM1_CH1 PWM 0% */
                                fan_adj = 0; auto_mode = 0;
                            }
                        }

                        /*==== 风扇调速 ====*/
                        item_json = cJSON_GetObjectItem(params_json, "fan_adj");
                        if(item_json != NULL)
                        {
                            val_json = cJSON_GetObjectItem(item_json, "value");
                            if(val_json == NULL) val_json = item_json;
                            fan_adj = val_json->valueint;
                            Fan_Adj(fan_adj); auto_mode = 0;
                        }

                        /*==== 控制模式切换 ====*/
                        item_json = cJSON_GetObjectItem(params_json, "auto_mode");
                        if(item_json != NULL)
                        {
                            val_json = cJSON_GetObjectItem(item_json, "value");
                            if(val_json == NULL) val_json = item_json;
                            if(val_json->type == cJSON_True || val_json->valueint == 1)
                                auto_mode = 1;     /* 自动控制 */
                            else
                                auto_mode = 0;     /* 手动控制(App) */
                        }

                        /*==== 阈值调节 (共8项) ====*/
                        item_json = cJSON_GetObjectItem(params_json, "smog_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          smog_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "do_low_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          do_low_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "do_high_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          do_high_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "ph_low_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          ph_low_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "ph_high_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          ph_high_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "water_temp_high_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          water_temp_high_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "water_temp_low_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          water_temp_low_th = val_json->valuedouble; }

                        item_json = cJSON_GetObjectItem(params_json, "air_temp_high_th");
                        if(item_json != NULL)
                        { val_json = cJSON_GetObjectItem(item_json, "value");
                          if(val_json == NULL) val_json = item_json;
                          air_temp_high_th = val_json->valuedouble; }
                    }
                    cJSON_Delete(raw_json);      /* 释放JSON对象 */

                    /* 回复OneNET: 指令已执行 */
                    if(req_id[0] != '\0')
                    {
                        char reply_topic[56];
                        char reply_payload[128];
                        snprintf(reply_topic, sizeof(reply_topic),
                                 "$sys/%s/%s/thing/property/set_reply",
                                 PROID, DEVICE_NAME);
                        snprintf(reply_payload, sizeof(reply_payload),
                                 "{\"id\":\"%s\",\"code\":0,\"msg\":\"success\"}",
                                 req_id);
                        OneNET_Publish(reply_topic, reply_payload);
                    }
                }

                MQTT_FreeBuffer(cmdid_topic);     /* 释放MQTT解包内存 */
                MQTT_FreeBuffer(req_payload);
            }
        break;

        case MQTT_PKT_SUBACK:    /* 订阅确认(已处理) */
        break;

        default:                 /* 其他报文忽略 */
        break;
    }

    ESP8266_Clear();             /* 清空ESP8266接收缓存 */
}
