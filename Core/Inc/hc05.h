/**
  ******************************************************************************
  * @file    hc05.h
  * @brief   HC-05 蓝牙串口模块调试驱动（基于 STM32 HAL，非阻塞）
  *
  * 设计思路：
  *   1. 串口中断每次只收 1 字节，收到后压入“环形缓冲区”并立刻重新开启接收；
  *   2. 行的拆分 / 命令解析全部放在主循环（HC05_Poll）里做，中断里绝不 sizeof/strcmp；
  *   3. 所以不存在“中断写、主循环读同一个行缓存”的竞争问题。
  *
  * 接线（HC-05 处于通信模式，EN/KEY 悬空即可）：
  *   HC-05 VCC -> 3.3V(或 5V，看模块板)
  *   HC-05 GND -> GND
  *   HC-05 TXD -> PA10 (USART1_RX)   注意是交叉接
  *   HC-05 RXD -> PA9  (USART1_TX)
  *   电平：HC-05 的 RXD 是 3.3V 逻辑，PA9 输出 3.3V，可直接连。
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HC05_H__
#define __HC05_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/* 对外就这 4 个函数 ------------------------------------------------------- */
void HC05_Init(UART_HandleTypeDef *huart);  /* 传 &huart1 */
void HC05_SendString(const char *str);      /* 往手机发一串字 */
void HC05_Printf(const char *fmt, ...);     /* 发带数字的话，如 HC05_Printf("v=%d\r\n", v); */
void HC05_Poll(void);                       /* 放在 while(1) 里，自动执行 F/B/L/R... */

#ifdef __cplusplus
}
#endif

#endif /* __HC05_H__ */
