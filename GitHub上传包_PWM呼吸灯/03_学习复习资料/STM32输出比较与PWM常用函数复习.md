# 输出比较与 PWM：当前阶段需要掌握的函数

整理日期：2026-10-20。针对 STM32F103C8T6、Keil、STM32F10x 标准外设库。以下是按当前学习阶段筛选的清单，不是要求背完所有 TIM 函数。未修改或烧录工程。

复习图：[高清 PNG](STM32输出比较与PWM常用函数复习图.png) / [可缩放 SVG](STM32输出比较与PWM常用函数复习图.svg)。结构体各成员另见 [初始化结构体复习](STM32_PWM输出比较初始化结构体复习.md)。

已补充第二张：[函数调用参数与三种结构体参数对照图](STM32_PWM函数调用与结构体参数对照复习图.png) / [可缩放 SVG](STM32_PWM函数调用与结构体参数对照复习图.svg)。第二张按当前 `D:\STM32project\PWM LED` 实际保存代码标注，不把学习建议当作工程已经使用的配置。

## 当前呼吸灯工程的结构体实参速查

### TIM_TimeBaseInit(TIM2, &timeBaseConfig)

第一个参数指定 TIM2，第二个参数传时基配置结构体的地址；`&` 表示取地址。结构体变量名可以不同，成员名须与库一致。下面是当前 PWM.c 中保存的取值：

| 结构体 TIM_TimeBaseInitTypeDef 的成员 | 当前值 | 对应意思 |
| --- | --- | --- |
| TIM_Prescaler | 720-1 | 写入 PSC=719，计数时钟除以 720。 |
| TIM_Period | 100-1 | 写入 ARR=99，向上计数 0～99，共 100 拍一周期。 |
| TIM_CounterMode | TIM_CounterMode_Up | 向上计数。 |
| TIM_ClockDivision | TIM_CKD_DIV1 | 滤波/死区采样时基不分频；不是计数时基的 PSC 分频。 |
| TIM_RepetitionCounter | 0 | 重复计数配置；TIM2 无此功能，此成员不生效。 |

若定时器实际时钟为工程所配置的 72 MHz，则 PWM 频率为 72 MHz / 720 / 100 = 1 kHz。这里依据工程时钟配置计算，未用仪器测量。

### TIM_OCStructInit(&outputConfig) → TIM_OC1Init(TIM2, &outputConfig)

前者只填结构体默认值，后者才把所填输出比较配置写入 TIM2 通道 1。当前源码先填默认值，再显式设置四项。

| 结构体 TIM_OCInitTypeDef 的成员 | 当前值 | 对应意思 |
| --- | --- | --- |
| TIM_OCMode | TIM_OCMode_PWM1 | PWM 模式 1；向上计数时 CNT<CCR 为有效。 |
| TIM_OutputState | TIM_OutputState_Enable | 开启主输出通道。 |
| TIM_Pulse | 0 | CCR1 初始为 0；当前 PWM1、高有效条件下初始高占比为 0%。 |
| TIM_OCPolarity | TIM_OCPolarity_High | 有效时输出高电平。 |
| TIM_OutputNState | TIM_OutputNState_Disable（默认） | 互补输出关闭；TIM2 不使用此成员。 |
| TIM_OCNPolarity | TIM_OCPolarity_High（库默认赋值，数值与 TIM_OCNPolarity_High 相同） | 互补输出极性默认高有效；TIM2 不使用此成员。 |
| TIM_OCIdleState | TIM_OCIdleState_Reset（默认） | 高级定时器主输出空闲目标为低；TIM2 不使用。 |
| TIM_OCNIdleState | TIM_OCNIdleState_Reset（默认） | 高级定时器互补输出空闲目标为低；TIM2 不使用。 |

如果手动填写互补输出极性，应使用该成员自己的 TIM_OCNPolarity_High/Low 宏，不根据默认值恰好相同就混用其他宏。

### GPIO_Init(GPIOA, &gpioConfig)

第一个参数是 GPIO 端口，第二个参数是 GPIO_InitTypeDef 的地址；不是传 TIM2。

| 结构体 GPIO_InitTypeDef 的成员 | 当前 PWM 引脚配置 | 对应意思 |
| --- | --- | --- |
| GPIO_Pin | GPIO_Pin_0 | 选择 PA0。TIM2 默认映射下通道 1 对应该引脚。 |
| GPIO_Mode | GPIO_Mode_AF_PP | 复用推挽输出：由定时器控制高低电平，输出级可主动拉高、拉低。 |
| GPIO_Speed | GPIO_Speed_50MHz | GPIO 输出速度等级；不是 PWM 频率被设成 50 MHz。 |

### 不传结构体的调用也要看清参数

- `TIM_SetCompare1(TIM2, compareValue)`：TIM2 是定时器；第二项是通道 1 的 CCR 数值。当前代码通过自定义封装 `PWM_SetCompare1(value)` 调用，循环传入 0～100，得到高占空比从 0% 到 100% 再返回。这个封装名不是标准库函数。
- `TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable)`：第二项是预装载开关宏，不是结构体；当前保存源码未主动调用这句。本复习图保留它作为后续学习内容，未擅自加入完成工程。
- `TIM_Cmd(TIM2, ENABLE)`：第二项是计数器使能状态，不是比较值。
- `TIM_CCxCmd(TIM2, TIM_Channel_1, TIM_CCx_Disable)`：依次是定时器、通道编号宏、通道开关宏。
- `TIM_OC1PolarityConfig(TIM2, TIM_OCPolarity_Low)`：依次是定时器、输出极性宏。
- `TIM_ARRPreloadConfig(TIM2, ENABLE)`：依次是定时器、ARR 预装载开关。
- `TIM_SetAutoreload(TIM2, newArr)`：第二项是 ARR 值，不是毫秒数。

在当前保存的 main.c 中，每 10 ms 调整一步：0～100 再 100～0，共两段各 101 次延时，名义呼吸循环约 2.02 秒，加上程序执行开销；不是硬件 PWM 的 1 ms 载波周期。

## 1. 函数名拆解

- TIM：Timer，定时器。
- OC：Output Compare，输出比较；PWM：Pulse Width Modulation，脉宽调制。
- Init：Initialization，初始化；Struct：Structure，结构体。
- Config：Configuration，配置；Cmd：Command，控制命令；Ctrl：Control，控制。
- CC：Capture/Compare，捕获/比较；CCR：Capture/Compare Register，捕获/比较寄存器。
- ARR：Auto-Reload Register，自动重装载寄存器；PSC：Prescaler，预分频器。
- Preload：预装载；Polarity：极性；Compare：比较；Auto-reload：自动重装载。
- 函数名称中的 x 不总是占位符：TIM_OCxInit 在讲解中代指一族函数，实际要写 TIM_OC1Init 等；但 TIM_CCxCmd 的 x 就是实际函数名的一部分，不能改成 TIM_CC1Cmd。

## 2. 现在先学会的四组核心函数

### TIM_OCStructInit：填默认配置

```c
void TIM_OCStructInit(TIM_OCInitTypeDef* TIM_OCInitStruct);

TIM_OCInitTypeDef outputConfig;
TIM_OCStructInit(&outputConfig);
```

只给内存中的输出比较配置结构体赋默认值，不开启时钟、不配置引脚、不启动定时器。之后还要改成 PWM 模式、开启主输出、填写 Pulse 和输出极性；默认并不是已经能输出 PWM。

### TIM_OC1Init / 2Init / 3Init / 4Init：把配置写入相应通道

```c
void TIM_OC1Init(TIM_TypeDef* TIMx, TIM_OCInitTypeDef* TIM_OCInitStruct);

TIM_OC1Init(TIM2, &outputConfig);
```

第一个参数 TIM2 选定时器；函数名中的 1 选通道 1。使用前先填写好结构体。此函数写入模式、输出使能、极性、CCR 初值等，不替代 GPIO 复用、时钟及整个时基初始化。

### TIM_SetCompare1 / 2 / 3 / 4：运行中改变比较值

```c
void TIM_SetCompare1(TIM_TypeDef* TIMx, uint16_t Compare1);

TIM_SetCompare1(TIM2, 30);
```

写入通道 1 的 CCR1。适合在呼吸灯、舵机等程序中反复调整脉宽，不需要每调一次就重新执行整套初始化。开了 CCR 预装载时，新比较值要在更新事件发生后进入实际比较使用的寄存器。

示例限定 PWM1、高有效、边沿对齐向上计数、ARR=99：CCR=30 对应高电平占空比 30%；CCR=70 对应 70%。30 并不在所有配置下都表示 30%，也不是直接表示 30 微秒。

### TIM_OC1PreloadConfig / 2 / 3 / 4：控制 CCR 预装载

```c
void TIM_OC1PreloadConfig(TIM_TypeDef* TIMx, uint16_t TIM_OCPreload);

TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
```

参数用 TIM_OCPreload_Enable / TIM_OCPreload_Disable，不是 ENABLE / DISABLE。这组函数控制对应通道比较寄存器的预装载，在 PWM 学习中应掌握并使用。

启用后，运行中写入新 CCR 值先暂存，到更新事件时再生效；连续边沿对齐 PWM 的常见情况就是在周期边界切换，避免随意在半个周期内改变比较值造成异常脉宽。关闭预装载不是“定时器不能再输出波形”，而是不再通过该预装载机制等待更新；不要反过来理解为没开预装载才算 PWM。

## 3. 通道对应表

| 通道 | 初始化 | 修改比较值 | 比较值预装载 |
| --- | --- | --- | --- |
| CH1 | TIM_OC1Init | TIM_SetCompare1 | TIM_OC1PreloadConfig |
| CH2 | TIM_OC2Init | TIM_SetCompare2 | TIM_OC2PreloadConfig |
| CH3 | TIM_OC3Init | TIM_SetCompare3 | TIM_OC3PreloadConfig |
| CH4 | TIM_OC4Init | TIM_SetCompare4 | TIM_OC4PreloadConfig |

各通道有自己的 CCR；同一个定时器的普通 PWM 通道共用计数器、PSC、ARR，所以改周期通常会影响同一定时器的所有这些通道。

## 4. 下一步会用、遇到能查的四项

| 函数 | 示例 | 用途与注意 |
| --- | --- | --- |
| TIM_OC1PolarityConfig | TIM_OC1PolarityConfig(TIM2, TIM_OCPolarity_Low); | 单独改通道 1 输出极性。High 高有效、Low 低有效；不是选择输入计数边沿。通道 2/3/4 有对应函数。 |
| TIM_CCxCmd | TIM_CCxCmd(TIM2, TIM_Channel_1, TIM_CCx_Disable); | 控制一个主通道使能。参数用 TIM_CCx_Enable / Disable，不是直接填 ENABLE / DISABLE。只关这一通道不会自动停止计数器，也不能据此保证引脚一定为低。 |
| TIM_ARRPreloadConfig | TIM_ARRPreloadConfig(TIM2, ENABLE); | 控制 ARR 的预装载。它影响计数周期参数何时生效；不是 CCR 的预装载开关。 |
| TIM_SetAutoreload | TIM_SetAutoreload(TIM2, 199); | 写 ARR，改变计数周期。若启用 ARR 预装载，要等更新事件后生效；固定 CCR 时，占空比也可能跟着变。 |

运行中需要改变 PSC 时再查 TIM_PrescalerConfig。TIM_PSCReloadMode_Update 表示等待更新生效，TIM_PSCReloadMode_Immediate 会生成更新事件，可能影响计数相位及相关标志，不应当作无副作用的立即改数。

## 5. 必须配合、但不专属于输出比较的函数

- RCC_APB1PeriphClockCmd：为当前 TIM2 开外设时钟；TIM1 的外设时钟在另一条总线上，不能照搬。
- GPIO_Init：将实际通道引脚配置为合适的复用输出。TIM2 默认映射下 CH1 在 PA0；需要结合当前重映射和接线确认。
- TIM_InternalClockConfig：若使用内部时钟计时，选择当前定时器的内部时钟计数路径，不是启动计数。
- TIM_TimeBaseInit：配置计数方向、PSC 和 ARR 等时基属性。
- TIM_Cmd(TIM2, ENABLE)：启动计数器。它与 TIM_CCxCmd 的单通道开关不是一回事。

普通连续硬件 PWM 不需要每一拍都进入中断翻转 GPIO；配置好后由定时器硬件持续生成。

## 6. 实际学习顺序

```text
时钟、引脚、时基准备好
  → TIM_OCStructInit 填默认值
  → 填写 PWM1、OutputState、Pulse、Polarity
  → TIM_OC1Init 写入通道
  → TIM_OC1PreloadConfig 开 CCR 预装载
  → TIM_Cmd 启动计数
  → 运行中 TIM_SetCompare1 调脉宽
```

需要更新参数在首次计数前就装载到有效寄存器时，可在配置结束后使用 TIM_GenerateEvent(TIM2, TIM_EventSource_Update) 产生一次更新；这不是启动计数。它可能产生更新标志，若后续开更新中断，应先处理/清除初始化遗留标志。ARR 与 CCR 预装载具体何时启用也要考虑，不能机械复制调用顺序。

## 7. 只在 TIM1 高级输出或特殊模式再学

- TIM_CtrlPWMOutputs(TIM1, ENABLE)：打开高级定时器主输出总门控。当前 C8T6 的 TIM2/3/4 不需要也不应照加这句。MOE = Main Output Enable，主输出使能。
- TIM_BDTRConfig：Break and Dead-Time Register，刹车与死区寄存器相关配置。做互补功率驱动时再系统学习，不能只靠开启互补输出来保证上下桥臂安全。
- TIM_CCxNCmd、TIM_OC1NPolarityConfig 等：互补通道开关和极性。C8T6 的 TIM1 通道 1～3 有互补输出，通道 4 没有。
- TIM_ForcedOC1Config、TIM_SelectOCxM：强制输出状态或单独切换输出比较模式。不是呼吸灯入门的优先项。
- TIM_OC1FastConfig、TIM_ClearOC1Ref、TIM_CCPreloadControl：快速响应、参考信号清除或高级控制预装载等特殊用途，按需再查；不要把 ClearOC1Ref 当作清中断标志，也不要把 Fast 当作提高 PWM 频率的普通开关。

## 一句话记忆

**StructInit 填表，OCnInit 配通道，SetCompareN 改脉宽，OCnPreloadConfig 管何时生效。**

## 核对依据

- 当前工程 `D:\STM32project\6-1_TimerInterrupt\Library\stm32f10x_tim.h` 的声明及宏。
- 同目录 `stm32f10x_tim.c` 的对应实现。
- [ST RM0008 参考手册](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：输出比较、PWM、CCR 与 ARR 预装载、主输出门控。
