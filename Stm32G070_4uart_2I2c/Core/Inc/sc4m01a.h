/**
  ******************************************************************************
  * @file    sc4m01a.h
  * @brief   SC-4M01A 六合一空气质量模块驱动
  *          UART 协议：9600 bps, 8N1，无校验
  *          模块每 1 秒主动上报 14 字节帧
  ******************************************************************************
  */

#ifndef __SC4M01A_H__
#define __SC4M01A_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SC4M01A_FRAME_LEN   14u
#define SC4M01A_HDR0        0x2Cu
#define SC4M01A_HDR1        0xE4u

/* 解析后的数据（整数，避免浮点） */
typedef struct {
    uint16_t tvoc;    /* TVOC，μg/m³，范围 0~2000                 */
    uint16_t hcho;    /* 甲醛（等效），μg/m³，范围 0~1000          */
    uint16_t co2;     /* CO₂（等效），ppm，范围 400~5000            */
    uint8_t  aqi;     /* 空气质量指数，1~6                          */
    int16_t  temp10;  /* 温度 × 10，单位 0.1°C（可为负值）          */
    uint16_t humi10;  /* 湿度 × 10，单位 0.1%RH                    */
} SC4M01A_Data_t;

/* 有状态解析器，持有跨帧残留字节 */
typedef struct {
    uint8_t  buf[SC4M01A_FRAME_LEN * 2]; /* 最多缓存 2 帧数据 */
    uint8_t  len;
} SC4M01A_Parser_t;

/**
 * @brief 初始化解析器（清零即可）
 */
void SC4M01A_ParserInit(SC4M01A_Parser_t *p);

/**
 * @brief 向解析器喂入原始字节，解析出最新一帧
 * @param p    解析器状态
 * @param in   新收到的字节数组
 * @param n    字节数
 * @param data 解析成功时输出数据
 * @return 1：本次调用解析到至少一帧有效数据；0：未获得完整帧
 */
int SC4M01A_Feed(SC4M01A_Parser_t *p, const uint8_t *in, uint16_t n,
                 SC4M01A_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __SC4M01A_H__ */
