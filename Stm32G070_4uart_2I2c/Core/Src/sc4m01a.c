/**
  ******************************************************************************
  * @file    sc4m01a.c
  * @brief   SC-4M01A 六合一空气质量模块驱动实现
  *
  * 帧格式（14 字节）：
  *   B0  B1  B2  B3  B4  B5  B6  B7  B8  B9  B10 B11 B12 CHECKSUM
  *   2Ch E4h
  *   B0~B1 : 帧头固定 0x2C 0xE4
  *   B2~B3 : TVOC  = B3*256+B2 (μg/m³)
  *   B4~B5 : HCHO  = B5*256+B4 (μg/m³)
  *   B6~B7 : CO2   = B7*256+B6 (ppm)
  *   B8    : AQI   (1~6)
  *   B9~B10: Temp  = B10×10 + B9 (×0.1°C)
  *   B11~B12: Humi = B12×10 + B11 (×0.1%RH)
  *   CHECKSUM = (B0+…+B12) & 0xFF
  ******************************************************************************
  */

#include "sc4m01a.h"
#include <string.h>

void SC4M01A_ParserInit(SC4M01A_Parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

/* 在 buf[0..len-1] 中搜索帧头，返回偏移；未找到返回 -1 */
static int find_header(const uint8_t *buf, uint8_t len)
{
    for (int i = 0; i < (int)len - 1; i++) {
        if (buf[i] == SC4M01A_HDR0 && buf[i + 1] == SC4M01A_HDR1)
            return i;
    }
    return -1;
}

static int parse_frame(const uint8_t *frame, SC4M01A_Data_t *data)
{
    /* 校验 */
    uint16_t sum = 0;
    for (int i = 0; i < SC4M01A_FRAME_LEN - 1; i++)
        sum += frame[i];
    if ((sum & 0xFFu) != frame[SC4M01A_FRAME_LEN - 1])
        return 0;

    data->tvoc   = (uint16_t)((uint16_t)frame[3] * 256u + frame[2]);
    data->hcho   = (uint16_t)((uint16_t)frame[5] * 256u + frame[4]);
    data->co2    = (uint16_t)((uint16_t)frame[7] * 256u + frame[6]);
    data->aqi    = frame[8];
    data->temp10 = (int16_t)((int16_t)frame[10] * 10 + frame[9]);
    data->humi10 = (uint16_t)((uint16_t)frame[12] * 10u + frame[11]);

    return 1;
}

int SC4M01A_Feed(SC4M01A_Parser_t *p, const uint8_t *in, uint16_t n,
                 SC4M01A_Data_t *data)
{
    int got_frame = 0;

    /* 追加新字节，防止溢出：只保留最新的 FRAME_LEN*2 字节 */
    if ((uint16_t)p->len + n > sizeof(p->buf)) {
        uint16_t keep = sizeof(p->buf) - n;
        if (keep < p->len)
            memmove(p->buf, p->buf + (p->len - keep), keep);
        p->len = (uint8_t)(p->len < keep ? p->len : keep);
    }
    memcpy(p->buf + p->len, in, n);
    p->len = (uint8_t)(p->len + n);

    /* 反复查找并消费帧，保留最后一帧结果 */
    while (1) {
        int hdr = find_header(p->buf, p->len);
        if (hdr < 0) {
            /* 无帧头：保留最后 1 字节（可能是帧头第一字节） */
            if (p->len > 0) {
                p->buf[0] = p->buf[p->len - 1];
                p->len = 1;
            }
            break;
        }

        /* 丢弃帧头前的垃圾字节 */
        if (hdr > 0) {
            memmove(p->buf, p->buf + hdr, p->len - (uint8_t)hdr);
            p->len = (uint8_t)(p->len - hdr);
        }

        /* 帧不完整，等待更多数据 */
        if (p->len < SC4M01A_FRAME_LEN)
            break;

        /* 尝试解析 */
        if (parse_frame(p->buf, data))
            got_frame = 1;

        /* 消费这 14 字节，继续找下一帧 */
        memmove(p->buf, p->buf + SC4M01A_FRAME_LEN,
                p->len - SC4M01A_FRAME_LEN);
        p->len = (uint8_t)(p->len - SC4M01A_FRAME_LEN);
    }

    return got_frame;
}
