# STM32 GPIO 外接 LED 点灯实验

机甲杯电控初级任务学习工程。使用 STM32 开发板，通过标准外设库配置 GPIO，控制外接 LED 周期性闪烁。

## 硬件与开发环境

- 开发板：正点原子 STM32 精英板 V2。
- 工程目标芯片：STM32F103ZE。
- 开发工具：Keil MDK5，ARM Compiler 6.24。
- 芯片支持包：Keil STM32F1xx_DFP 2.4.1。
- 软件库：STM32F10x 标准外设库（SPL）。
- 下载器：ST-Link，SWD 接口。

## 接线与运行效果

```text
开发板 3.3V → 限流电阻 → LED 正极 → LED 负极 → PE0
```

GPIOE 的 PE0 配置为通用推挽输出。该接法为低电平点亮：

- `GPIO_ResetBits(GPIOE, GPIO_Pin_0)`：输出低电平，LED 点亮。
- `GPIO_SetBits(GPIOE, GPIO_Pin_0)`：输出高电平，LED 熄灭。
- 当前源码两处延时均为 `Delay_ms(100)`，即亮约 100 毫秒、灭约 100 毫秒。

LED 需串联合适的限流电阻。面包板同一个内部连通的五孔组属于同一电气节点，电源正极与 GND 不可直接接入同组。接线前断电，并检查杜邦线接触是否牢固。

## 工程结构

```text
.
├── .gitignore
├── README.md
├── Project/
│   ├── project.uvprojx
│   ├── project.uvoptx
│   ├── LED.c
│   ├── start/
│   ├── Library/
│   ├── System/
│   ├── User/
│   ├── RTE/
│   └── DebugConfig/
├── docs/
│   └── GPIO、按键上下拉、工程结构等复习资料
└── media/
    └── LED演示.mp4
```

`Project/LED.c` 包含本实验的 `main()`。`start/` 提供启动文件、CMSIS 和系统时钟配置；`Library/` 提供标准外设库；`System/` 提供延时模块；`User/` 提供库配置与中断处理文件。

## 编译与下载

1. 安装上述 Keil、编译器与 STM32F1 芯片支持包。
2. 打开 `Project/project.uvprojx`，选择 `Target_1`。
3. 执行 Rebuild，构建程序。
4. 接好开发板电源、ST-Link 与 LED 电路。
5. 使用工程中的 ST-Link 配置下载。Flash 算法对应高密度 512KB 器件，并已配置下载后复位运行。
6. 观察外接 LED 闪烁；必要时按开发板 RESET。

编译输出目录由 Keil 自动生成，未纳入版本管理。主机环境不同或下载器不同，可能需要重新选择本机编译器及 ST-Link。

## 学习资料与演示

- [演示视频](media/LED演示.mp4)
- [GPIO 配置流程图](docs/STM32_GPIO_流程复习图.svg)
- [按键连接形式](docs/按键的常见连接形式.svg)
- [GPIO 按键与上下拉笔记](docs/GPIO按键输入与上下拉复习.md)
- [初学者术语表](docs/STM32_初学者术语对照表.md)
- [工程架构与启动流程](docs/STM32工程架构与启动流程复习图.svg)
- [系统架构图](docs/STM32系统架构复习图.svg)

复习资料由学习对话辅助整理，硬件接线和寄存器细节应结合实际开发板手册与芯片参考手册核对。

## 版本管理说明

保留 `.uvprojx`、`.uvoptx`、源码、头文件、启动汇编及配套配置文件；忽略 `Objects/`、`Listings/`、编译产物、`.uvguix.*` 个人界面设置和临时备份。

自定义链接脚本 `.sct`、CubeMX 配置 `.ioc`、以后新增的源码与头文件不在忽略范围内。
