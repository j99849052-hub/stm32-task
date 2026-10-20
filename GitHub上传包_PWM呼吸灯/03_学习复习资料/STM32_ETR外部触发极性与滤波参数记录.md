# ETR 外部触发：极性与滤波参数

记录日期：2026-10-19。适用当前 STM32F103C8T6、STM32F10x 标准外设库；依据用户参数截图、当前标准库实现及 ST RM0008。

直观记忆版见 [极性的参数理解](极性的参数理解.md)：极性选择“以哪种电平变化的边沿为计数节拍”，滤波确认“这次变化是否足够稳定”。

2026-10-20 复习图：[高清 PNG](STM32_ETR极性与滤波参数理解复习图.png) / [可缩放 SVG](STM32_ETR极性与滤波参数理解复习图.svg)。

## 1. 两个参数分别是什么

```c
uint16_t TIM_ExtTRGPolarity, uint16_t ExtTRGFilter
```

- ETR = External Trigger，外部触发。
- Ext = External，外部；TRG = Trigger，触发。
- Polarity：极性，决定输入是否反相、哪个电平或边沿有效。
- Filter：滤波；ExtTRGFilter 是数字输入滤波配置，不是创建一个外接滤波器。
- uint16_t 是 16 位无符号整数类型。参数类型可以容纳较大的数，但每个参数的合法取值还受接口规定限制。

这些参数用于 TIM_ETRConfig、TIM_ETRClockMode1Config、TIM_ETRClockMode2Config。Config = Configuration，配置；Clock Mode = 时钟模式。它们控制 ETR 路径，不是通道输入 TI 的 TIM_ICPolarity 参数，也不是 EXTI 的边沿选择接口。

## 2. TIM_ExtTRGPolarity：两个合法值

| 应传入的宏 | 宏对应整数 | 极性含义 | 用 ETR 作外部计数时钟时的理解 |
| --- | --- | --- | --- |
| TIM_ExtTRGPolarity_NonInverted | 0x0000 | Non-Inverted，不反相；高电平或上升沿有效 | 选择引脚信号的上升沿 0→1。 |
| TIM_ExtTRGPolarity_Inverted | 0x8000 | Inverted，反相；低电平或下降沿有效 | 选择引脚信号的下降沿 1→0。 |

“电平有效”还是“边沿有效”，取决于使用哪一种工作模式；不要把所有触发控制都理解为沿计数。

注意：这里不能用裸数字 1 代替 Inverted。对应的 ETP（External Trigger Polarity，外部触发极性）位在 TIMx_SMCR 的第 15 位，标准库宏已编码为 0x8000。SMCR = Slave Mode Control Register，从模式控制寄存器。优先使用宏，不记硬编码。

极性为 0x0000 不表示“把输入引脚置低电平”；这是配置编码，不是给 GPIO 输出赋值。

## 3. ExtTRGFilter：0～15，十六进制 0x00～0x0F

该值是滤波配置编号，不是“延时多少毫秒”，也不是“统一连续采样多少次”。每个编号同时规定采样频率和连续采样次数 N。

| 十进制 | 可写的十六进制 | 采样频率 fSAMPLING | 连续有效采样次数 N |
| --- | --- | --- | --- |
| 0 | 0x00 | 不启用数字滤波；手册注明按 fDTS 采样 | 不作连续 N 次滤波判定 |
| 1 | 0x01 | fCK_INT | 2 |
| 2 | 0x02 | fCK_INT | 4 |
| 3 | 0x03 | fCK_INT | 8 |
| 4 | 0x04 | fDTS / 2 | 6 |
| 5 | 0x05 | fDTS / 2 | 8 |
| 6 | 0x06 | fDTS / 4 | 6 |
| 7 | 0x07 | fDTS / 4 | 8 |
| 8 | 0x08 | fDTS / 8 | 6 |
| 9 | 0x09 | fDTS / 8 | 8 |
| 10 | 0x0A | fDTS / 16 | 5 |
| 11 | 0x0B | fDTS / 16 | 6 |
| 12 | 0x0C | fDTS / 16 | 8 |
| 13 | 0x0D | fDTS / 32 | 5 |
| 14 | 0x0E | fDTS / 32 | 6 |
| 15 | 0x0F | fDTS / 32 | 8 |

十六进制 A、B、C、D、E、F 分别代表十进制 10、11、12、13、14、15；例如写 15 和写 0x0F 完全相同。

此函数接收的是未移位编号 0～15，标准库内部会把它左移 8 位写入 ETF 字段。ETF = External Trigger Filter，外部触发滤波。不要自行传入 0x0F00，也不要把 IC 滤波位掩码直接填到这里。

### 两种时钟符号怎么读

- f 表示频率；SAMPLING 是采样；fSAMPLING 是滤波器实际采样频率。
- CK_INT 是 Internal Clock，内部时钟；CK 是 Clock，INT 在这里是 Internal，不是 Interrupt。
- fDTS 是手册中滤波/死区时基的频率符号；其基准由 TIM_ClockDivision 的 CKD 配置得到，不另编造逐字母英文全称。CKD = Clock Division，时钟分频。

| TIM_ClockDivision 设置 | fDTS |
| --- | --- |
| TIM_CKD_DIV1 | fCK_INT |
| TIM_CKD_DIV2 | fCK_INT / 2 |
| TIM_CKD_DIV4 | fCK_INT / 4 |

这里的 CKD 不是时基 PSC，也不是 ETR 外部触发预分频器。PSC = Prescaler，预分频器。即使 CNT 的计数来源改为外部 ETR，滤波采样仍使用定时器内部时钟派生的时基。

## 4. N 次采样怎样过滤毛刺

假设滤波输出原先为 0，现设置 N=4。只有连续采到 4 个 1，才承认输入已经稳定变成 1；中途又采到 0，连续高电平的确认过程会被打断。这不是“收到 4 个脉冲才计数”。

```text
短暂毛刺：采样值  0 0 1 1 0 0 0  → 未连续达到 4 个 1，不承认变高
稳定变化：采样值  0 0 1 1 1 1 1  → 连续达到 4 个 1，承认变高
```

对本表的较大编号，采样间隔总体更长、稳定性验证时间更长，更能拒绝短毛刺，但也增加延迟，并可能漏掉本来想计数的窄脉冲。不能简单理解为“数越大越准确、永远选最大”。

稳定性验证的时间尺度约为 N / fSAMPLING；精确输出延迟还受采样相位、同步处理等影响，不能把该估算当作保证的脉冲宽度门限。

例如 fCK_INT=72 MHz、TIM_CKD_DIV1、ExtTRGFilter=0x0F：

```text
fDTS = 72 MHz
fSAMPLING = 72 MHz / 32 = 2.25 MHz
N = 8
N / fSAMPLING ≈ 3.56 微秒
```

所以 0x0F 不是 15 毫秒消抖；该条件下仍是微秒量级，不能依靠它解决全部机械按键抖动。毫秒级抖动通常还需合适的软件或硬件消抖。

## 5. 外部时钟模式 2 的标准库示例

```c
TIM_ETRClockMode2Config(
    TIM2,
    TIM_ExtTRGPSC_OFF,
    TIM_ExtTRGPolarity_Inverted,
    0x0F
);
```

- TIM2：选择定时器 2。
- TIM_ExtTRGPSC_OFF：外部触发预分频关闭，即 ETR 不作此级分频；不等于关闭外部触发输入。
- TIM_ExtTRGPolarity_Inverted：选择反相极性，按引脚下降沿理解有效计数边沿。
- 0x0F：按上表的采样频率和 N=8 做数字滤波。

这只是 ETR 外部时钟路径配置示例，不是完整初始化程序。还需要核对引脚、输入配置、外设时钟、PSC/ARR、计数使能等。后级 PSC 若再分频，CNT 不一定每个外部有效边沿都加 1。本记录未替当前工程切换时钟模式、未修改接线、未编译或烧录。

## 资料

- [ST RM0008 参考手册](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)：通用定时器 TIMx_SMCR 的 ETP、ETF、ETPS 与 TIMx_CR1 的 CKD 字段。
- `D:\STM32project\6-1_TimerInterrupt\Library\stm32f10x_tim.h`：极性宏定义与 IS_TIM_EXT_FILTER 范围检查。
- `D:\STM32project\6-1_TimerInterrupt\Library\stm32f10x_tim.c`：TIM_ETRConfig 和 TIM_ETRClockMode2Config。
- 本次原图：`C:\Users\JL\AppData\Local\Temp\codex-clipboard-f4456501-131c-4c33-a1b6-741b2b8ef6ef.png`。
