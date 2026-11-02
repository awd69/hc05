# STM32F103 + HC-05 蓝牙遥控

用手机蓝牙遥控小车。单片机收到手机发来的一个字母，就按字母让车动。

- 芯片：STM32F103C8T6
- 蓝牙：HC-05（普通蓝牙，不是 BLE）
- 开发环境：VS Code + EIDE（队友用 CubeMX + Keil 也能用，见下面「给队友的移植步骤」）

---

## 一、接线

HC-05 只接 4 根线：

| HC-05 | 接到单片机 | 说明 |
|---|---|---|
| VCC | **5V** | 模块板上有稳压，接 5V 更稳 |
| GND | **GND** | 必须共地，不共地就收不到数据 |
| TXD | **PA10** | 模块发 → 单片机收 |
| RXD | **PA9** | 单片机发 → 模块收 |
| EN / KEY | 不用接 | 接 3.3V 会进 AT 模式，反而不能正常收发 |

```
   HC-05                 STM32 最小系统板
  ┌──────┐             ┌──────────────┐
  │ VCC  ├─────────────┤ 5V           │
  │ GND  ├─────────────┤ GND          │
  │ TXD  ├─────────────┤ PA10 (RX)    │
  │ RXD  ├─────────────┤ PA9  (TX)    │
  │ EN   ├── 空着                       │
  └──────┘             └──────────────┘
```

**记住一句话：模块的 TX 接单片机的 RX，模块的 RX 接单片机的 TX，两根线要交叉。**

串口参数：**9600，8 数据位，无校验，1 停止位**（就是 HC-05 出厂默认值）。

---

## 二、手机怎么连

1. 手机蓝牙设置里先搜到 HC-05 并配对（密码一般是 `1234` ）
2. 装一个**蓝牙串口助手**，比如「Serial Bluetooth Terminal」「蓝牙串口助手」
   - ⚠️ 一定要用**经典蓝牙串口**的 APP，**BLE 调试助手用不了**
3. 在 APP **里面**点连接 HC-05（系统显示"已配对"不代表连上了）
4. 连上之后，HC-05 板上的灯会从「快闪」变成「慢闪两下」

---

## 三、怎么用

手机 APP 里发**一个字母**就行，**不需要换行**（收到一个字母就执行一次）。

| 发什么 | 车怎么动 |
|---|---|
| `F` | 往前 |
| `B` | 往后 |
| `L` | 往左平移 |
| `R` | 往右平移 |
| `Q` | 原地左转 |
| `E` | 原地右转 |
| `S` | 停车 |
| `P` | 只回复 `PONG`，用来测通不通 |

小写也可以（`f` 和 `F` 一样）。

### Serial Bluetooth Terminal 按键设置（就是这种用法）

把按键设成：

| 设置项 | 选什么 |
|---|---|
| Action | **Send**（或者 Send w/o Newline，都行） |
| Data | 就是那个字母，比如 `F` |
| Repeat | `Infinite`，Delay **100 ms** |

按住按键 → 每 100ms 发一次 → 车一直跑；松手 → 不再发 → 自动停车。

> 注意：因为现在是“发一个字母就执行”，所以**不能发单词**（比如 `PING`）。
> 发 `PING` 会被当成 P、I、N、G 四个单独命令，后面三个会回一个错误提示。

**安全设计：0.5 秒没收到新命令，车自动停车**，防止手机断线或者松手了车还在跑。
速度、停车时间都在 `Core/Src/hc05.c` 里改：

```c
#define SPEED      500    /* 遥控速度，按你电机 PWM 范围改 */
#define STOP_TIME  500    /* 0.5 秒没命令就停车（手机每 100ms 发一次） */
```

---

## 四、想让车真的动起来（改一个函数）

现在 `hc05.c` 里这个函数是空的，所以车不会动：

```c
static void Car_SetVector(int16_t vx, int16_t vy, int16_t wz)
{
  /* 在这里写电机的代码 */
}
```

三个参数的含义：`vx` = 前后，`vy` = 左右平移，`wz` = 转身。

### 如果队友已经写好麦轮函数了

```c
static void Car_SetVector(int16_t vx, int16_t vy, int16_t wz)
{
  Mecanum_SetVector(vx, vy, wz);      /* 换成他函数的真名 */
}
```

### 如果只有 4 个电机的 PWM 函数

麦轮的标准算法就这 4 行：

```c
static void Car_SetVector(int16_t vx, int16_t vy, int16_t wz)
{
  int16_t lf = vx + vy + wz;    /* 左前 */
  int16_t rf = vx - vy - wz;    /* 右前 */
  int16_t lb = vx - vy + wz;    /* 左后 */
  int16_t rb = vx + vy - wz;    /* 右后 */

  Motor_SetPWM(1, lf);          /* 换成你的电机函数 */
  Motor_SetPWM(2, rf);
  Motor_SetPWM(3, lb);
  Motor_SetPWM(4, rb);
}
```

### 还没有电机，只想先验一下

```c
static void Car_SetVector(int16_t vx, int16_t vy, int16_t wz)
{
  HC05_Printf("vx=%d vy=%d wz=%d\r\n", vx, vy, wz);   /* 发回手机看 */
}
```

发 `F`，手机收到 `vx=500 vy=0 wz=0`，说明整条链路和解析都对了。

---

## 五、程序里怎么调用

```c
#include "hc05.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  HC05_Init(&huart1);        /* ① 初始化 */

  while (1)
  {
    HC05_Poll();             /* ② 一直调，它自己会收命令、执行动作 */
    /* 下面放 PID、里程计、别的任务 */
  }
}
```

就这 4 个函数，别的都不用管：

```c
void HC05_Init(UART_HandleTypeDef *huart);   /* 初始化，传 &huart1 */
void HC05_SendString(const char *str);       /* 发字符串 */
void HC05_Printf(const char *fmt, ...);      /* 发带数字的话 */
void HC05_Poll(void);                        /* 主循环里一直调 */
```

---

## 六、给队友的移植步骤（CubeMX + Keil）

**只需要两个文件**：`hc05.c` 和 `hc05.h`，HAL 库用他自己的就行。

> 📦 已经打包好了，直接发这个文件夹给他：**[`给队友/`](给队友/)**
> 里面有这两个文件的副本 + 一份更详细的《移植说明.md》。
> （改代码记得改 `Core/Src/hc05.c`，改完把副本同步一下）

### 1. CubeMX 里检查 3 项

| 项目 | 设置 |
|---|---|
| USART1 | Asynchronous，**9600**，8N1 |
| 引脚 | PA9 = TX，PA10 = RX |
| **NVIC Settings** | 勾上 **USART1 global interrupt** ⚠️ 不勾就永远收不到数据 |

然后点 GENERATE CODE。

### 2. Keil 里加文件

1. `hc05.c` 放 `Core/Src`，`hc05.h` 放 `Core/Inc`
2. Keil 左边 `Application/User/Core` 分组 → 右键 → **Add Existing Files** → 选 `hc05.c`
3. 编译

### 3. `main.c` 里加 3 处（**写在 USER CODE 区里**，不然 CubeMX 重新生成会被删）

```c
/* USER CODE BEGIN Includes */
#include "hc05.h"
/* USER CODE END Includes */

  /* USER CODE BEGIN 2 */
  HC05_Init(&huart1);
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HC05_Poll();
  /* USER CODE END WHILE */
```

### 4. 改 `Car_SetVector()` 接他的电机代码

---

## 七、常见问题

**1. 手机能看到单片机发的字，但发命令没反应（收到的字节数一直是 0）**

挨个查：
- 手机 APP 里**真的点了发送**吗（只在输入框打字不算）
- HC-05 的 **TXD 是不是接在 PA10** 上
- 两根线有没有**交叉**（TXD→PA10，RXD→PA9）
- 有没有**共地**（HC-05 的 GND 和单片机的 GND 要连一起）

**2. 手机上全是乱码**

波特率对不上。程序里 `usart.c` 是 9600，HC-05 默认也是 9600，两边必须一样。
如果 HC-05 被人改过波特率，就得查它的资料重新设回来。

**3. 编译报 `multiple definition of 'HAL_UART_RxCpltCallback'`**

工程里有**两份**这个函数。`hc05.c` 里已经写了一份，把自己工程里那份删掉，或者把两边的逻辑合并成一份。

**4. 用 CubeMX 重新生成代码后，蓝牙不动了**

说明你的调用没写在 `USER CODE BEGIN / END` 之间，被覆盖掉了。`hc05.c` 和 `hc05.h` 不会被覆盖，放心。

**5. `while(1)` 里千万别放 `HAL_Delay()`**

放了之后自动停车会失效，命令响应也会变慢。要用延时就用 `HAL_GetTick()` 自己算时间。

---

## 八、上传 GitHub 要传哪些文件

**要传：**

```
Core/                      你的代码（main.c / hc05.c / usart.c ...）
.eide/                     EIDE 的工程配置
STM32F103C8TX_FLASH.ld     链接脚本
hc05.ioc                   CubeMX 工程（以后改引脚用）
.mxproject
.gitignore
README.md
```

**不要传：**

```
build/                     编译出来的东西
MDK-ARM/hc05/              Keil 的中间文件
.settings/                 个人设置
```

⚠️ 注意：这个工程的 HAL 驱动库放在工程外面（`../../../STM32Cube/STM32Cube_FW_F1_V1.8.7`），
所以别人 clone 下来**直接编译会失败**。想让仓库能独立编译，需要把 HAL 拷一份到 `Drivers/` 里。

如果只是发给队友，直接发 `hc05.c` 和 `hc05.h` 两个文件最快。

---

## 九、文件说明

| 文件 | 作用 |
|---|---|
| `Core/Src/hc05.c` | 蓝牙驱动：收字节、拆命令、调电机 |
| `Core/Inc/hc05.h` | 上面这个文件的接口声明 |
| `Core/Src/main.c` | 主程序：初始化 + 主循环 |
| `Core/Src/usart.c` | 串口配置（波特率在这里改成 9600） |
| `hc05.ioc` | CubeMX 工程文件 |
