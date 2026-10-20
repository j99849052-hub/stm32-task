# PWM 输出比较初始化结构体：TIM_OCInitTypeDef

记录日期：2026-10-20。适用当前 STM32F103C8T6 与 STM32F10x 标准外设库；不是 HAL 结构体。本次只整理资料，未改动、编译或烧录学习工程。

复习图：[高清 PNG](STM32_PWM输出比较初始化结构体复习图.png) / [可缩放 SVG](STM32_PWM输出比较初始化结构体复习图.svg)。

配套函数清单：[输出比较与 PWM 常用函数复习](STM32输出比较与PWM常用函数复习.md) / [函数复习图](STM32输出比较与PWM常用函数复习图.png)。

## 1. 先认识名字

- TIM：Timer，定时器。
- OC：Output Compare，输出比较。定时器比较计数值与比较值，按所选模式改变内部输出参考信号；PWM 是其工作模式之一。
- PWM：Pulse Width Modulation，脉宽调制。
- Init：Initialization，初始化；Struct：Structure，结构体；TypeDef 对应 C 的 typedef 类型定义命名，不是一个额外硬件模块。
- Mode：模式；Output：输出；State：状态；Pulse：脉冲；Polarity：极性；Idle：空闲。
- N：这里是互补输出的命名标记，例如 CH1N；不另编造固定的英文单词展开。Complementary Output 是互补输出。
- CNT：Counter，计数器；CCR：Capture/Compare Register，捕获/比较寄存器；ARR：Auto-Reload Register，自动重装载寄存器；PSC：Prescaler，预分频器。

TIM_OCInitTypeDef 是输出比较初始化结构体，不只用于 PWM。截图中的八个成员均为 uint16_t（16 位无符号整数），但各成员的合法赋值由接口规定，不能因为是整数就随便填数。

## 2. 普通 PWM 先掌握这四个成员

| 成员 | 用途 | 常用赋值与理解 |
| --- | --- | --- |
| TIM_OCMode | 选择输出比较模式 | PWM 常用 TIM_OCMode_PWM1 或 TIM_OCMode_PWM2；有效/无效状态如何变化由模式与计数方向决定。 |
| TIM_OutputState | 开关该主输出通道 | TIM_OutputState_Enable 允许主通道输出；TIM_OutputState_Disable 禁止该通道输出。不是启动计数器，也不是给引脚写高低电平。 |
| TIM_Pulse | 设置捕获/比较寄存器 CCRx 的初始值 | 当前接口范围 0～65535；例如通道 1 写入 CCR1。不是直接填百分比，也不是直接填微秒。 |
| TIM_OCPolarity | 配置主输出有效极性 | TIM_OCPolarity_High：有效时为高；TIM_OCPolarity_Low：有效时为低。这里是输出极性，不是 ETR 输入的计数边沿选择。 |

当前本地标准库中 TIM_OCMode 的六种合法值：

| 宏 | 作用 |
| --- | --- |
| TIM_OCMode_Timing | 不因比较匹配改变输出参考信号；仍可产生比较匹配相关标志/请求。 |
| TIM_OCMode_Active | 匹配 CNT=CCR 时，把输出参考信号置为有效。 |
| TIM_OCMode_Inactive | 匹配 CNT=CCR 时，把输出参考信号置为无效。 |
| TIM_OCMode_Toggle | 每次匹配 CNT=CCR，翻转输出参考信号。 |
| TIM_OCMode_PWM1 | 向上计数时，CNT<CCR 有效，否则无效。 |
| TIM_OCMode_PWM2 | 向上计数时，CNT<CCR 无效，否则有效。 |

Active/Inactive 是内部有效/无效状态，不应不看输出极性就直接等同于引脚高/低。当前 TIM_OC1Init 的模式范围检查不包含强制输出模式；不要把别的接口支持的宏混填到这里。

## 3. Pulse、模式、极性一起决定波形

以下只讨论正常输出、边沿对齐、向上计数、无死区/刹车影响的主输出。

设 ARR=99，CNT 每周期依次取 0～99，共 100 拍；TIM_Pulse=30，也就是 CCR=30。

| 配置 | CNT=0～29 | CNT=30～99 | 高电平占比 |
| --- | --- | --- | --- |
| PWM1 + High | 高 | 低 | 30% |
| PWM1 + Low | 低 | 高 | 70% |
| PWM2 + High | 低 | 高 | 70% |

所以只说“Pulse=30 就是 30%”不严谨，还需要 ARR、模式、极性与计数方式等条件。

在 PWM1 + High、边沿对齐向上计数，且 0≤CCR≤ARR+1 的条件下：

```text
高电平占空比 = CCR / (ARR+1)
PWM 频率 = 定时器计数来源频率 / (PSC+1) / (ARR+1)
```

CCR=0 可得到 0%；CCR=ARR+1 可得到 100%，但前提是该 CCR 值能由寄存器及当前接口表示。若 ARR=65535，ARR+1=65536，不能直接塞进当前 uint16_t 的 TIM_Pulse 成员。

PWM 模式下，CNT 和 CCR 的大小关系持续参与决定输出状态；不要误以为只有相等那一瞬间才决定整个 PWM 波形。运行中可用 TIM_SetCompare1(TIM2, value) 修改通道 1 的比较值；是否立即生效还取决于比较寄存器预装载配置。

## 4. 另外四项：当前主要留给 TIM1 高级输出

| 成员 | 作用 | 赋值 |
| --- | --- | --- |
| TIM_OutputNState | 开关互补输出通道 CHxN | TIM_OutputNState_Enable / TIM_OutputNState_Disable。不会自动选择引脚或配置死区。 |
| TIM_OCNPolarity | 设置互补输出极性 | TIM_OCNPolarity_High / TIM_OCNPolarity_Low。独立配置互补输出路径的极性，不是再建一个独立 PWM 计数器。 |
| TIM_OCIdleState | 设置主输出的空闲目标电平 | TIM_OCIdleState_Set 目标为高；TIM_OCIdleState_Reset 目标为低。 |
| TIM_OCNIdleState | 设置互补输出的空闲目标电平 | TIM_OCNIdleState_Set 目标为高；TIM_OCNIdleState_Reset 目标为低。 |

对于当前 C8T6：TIM2/3/4 的普通 PWM 不使用以上四项；TIM1 的通道 1～3 具有互补输出，通道 4 没有 CH4N。通用库文件中出现 TIM8 等名字，不代表 C8T6 上也有这些定时器。

Idle 不是“PWM 正常周期里的低电平阶段”，也不是简单“TIM_Cmd(DISABLE) 后必定输出这个电平”。它与高级定时器主输出关闭时的控制有关，还要结合输出使能、关断选择、死区及引脚配置等条件；不能只设置 IdleState 就保证物理引脚一定被驱动到该电平。

MOE = Main Output Enable，主输出使能。高级定时器还需打开总输出门控，常用 TIM_CtrlPWMOutputs(TIM1, ENABLE)。这里只作区别说明，不展开功率驱动安全配置。

## 5. 最小的结构体填写示例

以下仅为 TIM2 通道 1 的结构体配置片段；假设已配置好时钟、时基和输出引脚，ARR=99、向上计数。

```c
TIM_OCInitTypeDef outputConfig;
TIM_OCStructInit(&outputConfig);
outputConfig.TIM_OCMode = TIM_OCMode_PWM1;
outputConfig.TIM_OutputState = TIM_OutputState_Enable;
outputConfig.TIM_Pulse = 30;
outputConfig.TIM_OCPolarity = TIM_OCPolarity_High;
TIM_OC1Init(TIM2, &outputConfig);
TIM_Cmd(TIM2, ENABLE);
```

TIM_OCStructInit 只给内存中的八个成员赋默认值，不启动任何硬件；默认模式为 Timing、主输出和互补输出均禁止、Pulse 为 0、两路极性为 High、两路空闲状态为 Reset。先填默认值，再覆盖所需四项，可以避免剩余成员含未初始化数据。

TIM_OC1Init 将配置写入通道 1；函数名中的 1 选通道，TIM2 参数选定时器。通道 2/3/4 分别用 TIM_OC2Init / TIM_OC3Init / TIM_OC4Init。

结构体不包含整个 PWM 系统的所有配置：时基的 PSC/ARR、引脚复用输出、时钟开启、预装载等还需另配。调用 TIM_OC1Init 不等于“PWM 所有条件已经满足”。

## 一句话记忆

**Mode 定规则，Pulse 定比较点，Polarity 定有效电平，OutputState 开通道；带 N 的看互补，Idle 看高级输出的空闲状态。**

## 依据

- 用户截图中的 TIM_OCInitTypeDef 八个成员。
- 当前工程 `D:\STM32project\6-1_TimerInterrupt\Library\stm32f10x_tim.h` 的结构体与宏。
- 同目录 `stm32f10x_tim.c` 的 TIM_OC1Init 与 TIM_OCStructInit 实现。
- [ST RM0008 参考手册](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：PWM、互补输出、输出空闲状态及输出门控。
