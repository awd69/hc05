/**
  ******************************************************************************
  * @file    hc05.c
  * @brief   HC-05 蓝牙遥控：串口中断收字节，主循环解析命令
  *
  * 用法就 3 步：
  *   1. HC05_Init(&huart1);          // 初始化
  *   2. while(1) { HC05_Poll(); }    // 主循环里一直调
  *   3. 改 Car_SetVector() 里那行注释，接上队友的电机代码
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hc05.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* 内部变量 ------------------------------------------------------------------*/

#define RX_MAX   64      /* 收到的字节先存这里，64 个够用 */

static UART_HandleTypeDef *bt_uart;   /* 蓝牙接在哪个串口上 */

static uint8_t rx_byte;               /* 中断里收到的 1 个字节 */
static uint8_t rx_buf[RX_MAX];        /* 收到的字节都先放进这个数组 */
static uint8_t rx_w = 0;              /* 写指针：写到第几个格子 */
static uint8_t rx_r = 0;              /* 读指针：从第几个格子读 */

static uint8_t  is_moving = 0;        /* 1 = 车正在动 */
static uint32_t last_time = 0;        /* 上次收到命令的时间（毫秒） */

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  初始化：记住是哪个串口，然后打开第一次接收
 */
void HC05_Init(UART_HandleTypeDef *huart)
{
  bt_uart = huart;

  rx_w = 0;
  rx_r = 0;

  /* 顺便打开“串口出错”中断，万一溢出能自己恢复 */
  __HAL_UART_ENABLE_IT(huart, UART_IT_ERR);

  /* 先收第 1 个字节，后面每收一个就在回调里再接一个 */
  HAL_UART_Receive_IT(huart, &rx_byte, 1);
}

/**
 * @brief  往手机发一串字
 */
void HC05_SendString(const char *str)
{
  if (bt_uart == 0)
  {
    return;
  }
  HAL_UART_Transmit(bt_uart, (uint8_t *)str, strlen(str), 100);
}

/**
 * @brief  往手机发带数字的话，例如 HC05_Printf("speed=%d\r\n", 300);
 */
void HC05_Printf(const char *fmt, ...)
{
  char buf[96];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  HC05_SendString(buf);
}

/**
 * @brief  从 rx_buf 里取一个字符（不等待，没有就返回 0）
 * @note   命令就是单个字母，收到一个就执行一个，不用等回车换行。
 *         如果你的 APP 会多传 \r \n，这两个符号直接丢掉。
 */
static char ReadChar(void)
{
  char ch;

  while (rx_r != rx_w)                  /* 数组里还有没读的字节 */
  {
    ch = (char)rx_buf[rx_r];            /* 读一格 */
    rx_r++;
    if (rx_r >= RX_MAX)
    {
      rx_r = 0;                         /* 读到尾巴就绕回开头 */
    }

    if ((ch == '\r') || (ch == '\n'))   /* 回车换行不用管 */
    {
      continue;
    }

    return ch;
  }

  return 0;
}

/* 命令 → 麦轮 ---------------------------------------------------------------*/

#define SPEED      500    /* 遥控速度，按你电机 PWM 范围改 */
#define STOP_TIME  500    /* 多久没收到命令就自动停车（毫秒）
                           * 手机每 100ms 发一次，松手后 0.5 秒就停 */

/**
 * @brief  麦轮速度：vx=前后，vy=左右平移，wz=自转
 * @note   【融合时只改这一个函数】把下面那行注释换成队友的电机函数
 *         停车时也会调它，参数是 (0, 0, 0)
 */
static void Car_SetVector(int16_t vx, int16_t vy, int16_t wz)
{
  /* Mecanum_SetVector(vx, vy, wz);   ← 换成电机驱动的函数 */

  (void)vx;
  (void)vy;
  (void)wz;
}

/**
 * @brief  设速度，顺便记一下时间（用来判断要不要自动停车）
 */
static void Car_Move(int16_t vx, int16_t vy, int16_t wz)
{
  Car_SetVector(vx, vy, wz);

  if ((vx == 0) && (vy == 0) && (wz == 0))
  {
    is_moving = 0;          /* 停下了 */
  }
  else
  {
    is_moving = 1;          /* 还在跑 */
  }

  last_time = HAL_GetTick();
}

/**
 * @brief  收到一个字母，就做对应的动作
 */
static void HandleCmd(char c)
{
  switch (c)
  {
    case 'F': case 'f': Car_Move( SPEED,     0,     0); break;   /* 前 */
    case 'B': case 'b': Car_Move(-SPEED,     0,     0); break;   /* 后 */
    case 'L': case 'l': Car_Move(     0,  SPEED,     0); break;   /* 左平移 */
    case 'R': case 'r': Car_Move(     0, -SPEED,     0); break;   /* 右平移 */
    case 'Q': case 'q': Car_Move(     0,     0,  SPEED); break;   /* 左转圈 */
    case 'E': case 'e': Car_Move(     0,     0, -SPEED); break;   /* 右转圈 */
    case 'S': case 's': Car_Move(     0,     0,     0); break;   /* 停 */
    case 'P': case 'p': HC05_SendString("PONG\r\n");    break;   /* 试试通不通 */
    default:            HC05_SendString("? F B L R Q E S\r\n"); break;
  }
}

/**
 * @brief  主循环里一直调：把收到的命令执行掉
 */
void HC05_Poll(void)
{
  char c;

  c = ReadChar();
  while (c != 0)
  {
    HandleCmd(c);
    c = ReadChar();
  }

  /* 一段时间没收到命令就自动停车，防止车跑飞 */
  if ((is_moving == 1) && ((HAL_GetTick() - last_time) >= STOP_TIME))
  {
    Car_SetVector(0, 0, 0);
    is_moving = 0;
  }
}

/* 串口中断回调（这几个函数是 HAL 自己调的，你不用调）----------------------*/

/**
 * @brief  每收到 1 个字节就进这里：存起来，再把接收重新打开
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((bt_uart == 0) || (huart->Instance != bt_uart->Instance))
  {
    return;
  }

  rx_buf[rx_w] = rx_byte;         /* 存一个字节 */
  rx_w++;
  if (rx_w >= RX_MAX)
  {
    rx_w = 0;                     /* 写到尾巴就绕回开头 */
  }

  if (rx_w == rx_r)               /* 两个指针碰上 = 满了，丢掉最老的一个 */
  {
    rx_r++;
    if (rx_r >= RX_MAX)
    {
      rx_r = 0;
    }
  }

  /* 一定要再开一次，否则只能收到这 1 个字节 */
  HAL_UART_Receive_IT(bt_uart, &rx_byte, 1);
}

/**
 * @brief  串口出错（溢出、帧错）时进这里：重新打开接收，别让它卡死
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if ((bt_uart == 0) || (huart->Instance != bt_uart->Instance))
  {
    return;
  }
  HAL_UART_Receive_IT(bt_uart, &rx_byte, 1);
}
