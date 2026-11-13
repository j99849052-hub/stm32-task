/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stddef.h> // size_t、offsetof：字节长度和结构体成员偏移
#include "OLED.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
//保存三个等级各自持续的毫秒数

typedef struct
{
	RTC_TimeTypeDef startTime; // 开始时间：时、分、秒
	RTC_DateTypeDef startDate; // 开始日期：年、月、日

    uint32_t weakMs;   // 弱光累计毫秒数
    uint32_t mediumMs; // 中光累计毫秒数
    uint32_t strongMs; // 强光累计毫秒数
} LightRecord;

// Flash里的一份完整保存数据；目前只定义类型，后续再接入读写
// Storage表示存储，Snapshot表示某个时刻的数据快照
// 修改字段布局时需要更新formatVersion，并同步保存/恢复校验逻辑
typedef struct
{
    uint32_t magic;         // 数据识别标记：区分本工程记录和空白/其他内容
    uint32_t formatVersion; // 数据格式版本，避免把旧格式当成新格式读取
    uint32_t sequence;      // 保存序号：后续在两份有效备份中选择较新的一份

    float strongThreshold; // 强/中分界，单位V
    float weakThreshold;   // 中/弱分界，单位V
    uint32_t historyCount; // 有效历史条数，必须为0～3
    LightRecord history[3];// 三条完整历史，包含开始日期、时间和各级累计时长

    uint32_t checksum;      // 校验值：读取时检查数据是否完整，后续实现计算
} StorageSnapshot;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STORAGE_MAGIC          0x4C4C4F47UL // 本工程数据识别标记
#define STORAGE_FORMAT_VERSION 1UL          // 当前存储格式版本

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint16_t adcData[2] = {0};
uint8_t settingMode = 0;//这是按键的状态设置，0为正常，1调强中，2调中弱
float strongThreshold = 0.7f;
float weakThreshold = 2.3f;
GPIO_PinState keyLast = GPIO_PIN_SET;//上一次读的电平
GPIO_PinState keyStable = GPIO_PIN_SET;//消抖后稳定电平
uint32_t keyChangeTime = 0;//电平最近一次变化的时间

uint8_t recording = 0; // 0：停止记录，1：正在记录
GPIO_PinState recordLast = GPIO_PIN_SET;   // 上次读取的PB1电平
GPIO_PinState recordStable = GPIO_PIN_SET; // 消抖后认可的PB1状态

uint32_t recordChangeTime = 0; // PB1最近变化的时间
uint32_t recordPressTime = 0;  // 确认按下的时间

uint8_t recordLongHandled = 0; // 本次长按是否已经处理，防止反复切换
uint32_t displayTime = 0; // OLED与串口刷新时间

uint32_t weakMs = 0;   // 弱光累计时间
uint32_t mediumMs = 0; // 中光累计时间
uint32_t strongMs = 0; // 强光累计时间

uint32_t lastCountTime = 0; // 上一次检查计时的时间
char lastLevel = 'M';       // 上一轮的光照等级：W弱、M中、S强

LightRecord history[3];    // 最多存三条记录
uint8_t historyCount = 0; // 当前已保存的记录条数：0～3
uint8_t historyView = 0; // 0：实时界面，1～3：对应三条历史记录
RTC_TimeTypeDef recordStartTime; // 暂存正在进行的这条记录的开始时间
RTC_DateTypeDef recordStartDate; // 暂存本次开始时的日期

//校准模块
uint8_t clockSetting = 0; // 0：正常界面，1：校时界面

GPIO_PinState clockKeyLast = GPIO_PIN_SET;   // 上次读到的PB10电平
GPIO_PinState clockKeyStable = GPIO_PIN_SET; // 消抖后认可的状态
uint32_t clockKeyChangeTime = 0;             // 最近一次电平变化时间
RTC_DateTypeDef editDate; // 暂存待设置的日期，不直接改变RTC
RTC_TimeTypeDef editTime; // 暂存待设置的时间
uint32_t clockPotEntry = 0; // 进入当前设置项时，电位器的原始读数
uint8_t clockPotMoved = 0;  // 0：尚未转动，1：已转动，允许修改


// 定时设置页面：当前只验证进入/退出，尚未启用RTC闹钟
uint8_t scheduleSetting = 0; // 0：正常界面，1：定时设置页面
GPIO_PinState scheduleKeyLast = GPIO_PIN_SET;
GPIO_PinState scheduleKeyStable = GPIO_PIN_SET;
uint32_t scheduleKeyChangeTime = 0;
RTC_TimeTypeDef scheduleStartTime = {0}; // 已保存的定时开始时间，目前尚未提交设置
RTC_TimeTypeDef editScheduleStart;      // 编辑中的候选开始时间
uint32_t schedulePotEntry = 0;          // 进入当前项时的旋钮位置
uint8_t schedulePotMoved = 0;           // 旋钮明显移动后才修改候选值
RTC_TimeTypeDef scheduleStopTime = {0}; // 已保存的定时停止时间
RTC_TimeTypeDef editScheduleStop;       // 编辑中的候选停止时间
uint8_t scheduleConfigured = 0;         // 已确认一组有效时间，不代表闹钟已启用
uint8_t scheduleTimeError = 0;          // 开始、停止时间相同时提示错误
volatile uint8_t rtcAlarmPending = 0; // 闹钟中断置1，主循环读取并处理
uint8_t scheduleEnabled = 0;     // 0关闭，1启用每天定时记录
uint8_t editScheduleEnabled = 0; // 候选开关，确认前不改变实际设置
uint8_t scheduleWaitingStop = 0; // 0等待开始，1等待停止
uint8_t storageReady = 0;       // 已确认Flash身份并检查存储区域
uint8_t storageSavePending = 0; // 需要保存；主循环在停止/退出设置后执行
int8_t storageActiveSlot = -1; // -1无有效副本，0副本A，1副本B
uint32_t storageSequence = 0;  // 当前有效副本的保存序号
uint8_t clockValid = 0; // 只有校时成功或本次连续供电下恢复后，才认为时间可信
uint8_t historyDateFilter = 0; // 历史页PB0切换：全部/当前开始日期
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
HAL_StatusTypeDef RTC_SetNextAlarm(RTC_TimeTypeDef targetTime);
HAL_StatusTypeDef Record_Start(void);
void Record_StopAndSave(void);
void Schedule_HandleAlarm(void);

HAL_StatusTypeDef Flash_ReadJEDECID(uint8_t id[3]);
HAL_StatusTypeDef Flash_ReadData(uint32_t address, uint8_t *data, uint16_t length);
HAL_StatusTypeDef Flash_EraseSector(uint32_t address);
HAL_StatusTypeDef Flash_ProgramPage(uint32_t address, const uint8_t *data, uint16_t length);
const char *Flash_TestBlankSector(void);
uint32_t Storage_Checksum(const void *data, size_t length);
HAL_StatusTypeDef Storage_Pack(StorageSnapshot *snapshot, uint32_t sequence);
HAL_StatusTypeDef Storage_Init(void);
HAL_StatusTypeDef Storage_Save(void);
HAL_StatusTypeDef Clock_InitSession(void);
HAL_StatusTypeDef Clock_Set(RTC_DateTypeDef date, RTC_TimeTypeDef time);
void Clock_StoreBackupDate(const RTC_DateTypeDef *date);
HAL_StatusTypeDef Clock_BeginEdit(uint32_t potRaw);
void History_Next(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 设置下一次闹钟。只安排目标时刻，不改变RTC当前时间。
HAL_StatusTypeDef RTC_SetNextAlarm(RTC_TimeTypeDef targetTime)
{
    RTC_AlarmTypeDef alarm = {0}; // 准备闹钟配置

    // 拒绝非法时分秒，防止关闭原闹钟后才发现参数错误
    if ((targetTime.Hours > 23U) ||
        (targetTime.Minutes > 59U) || (targetTime.Seconds > 59U))
    {
        return HAL_ERROR;
    }

    // F103只有一个闹钟：先停用旧安排，检查停用是否成功
    HAL_StatusTypeDef status = HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    if (status != HAL_OK)
    {
        return status;
    }

    // 清除旧通知及中断挂起，避免旧事件被误认为新闹钟到时
    rtcAlarmPending = 0;
    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);

    alarm.Alarm = RTC_ALARM_A;    // 选择F103唯一的闹钟A
    alarm.AlarmTime = targetTime; // 复制目标时分秒

    // 启用闹钟中断，到时进入已有的回调函数
    // 此F1驱动会将早于当前时间的目标安排在明天；每天需要重新安排。
    return HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN);
}


// 手动/自动记录共用入口：读到开始日期时间后，才清零并开始
HAL_StatusTypeDef Record_Start(void)
{
    RTC_TimeTypeDef startTime;
    RTC_DateTypeDef startDate;
    if (recording != 0) return HAL_BUSY;
    if (!clockValid) return HAL_ERROR; // 不把未校准的假时间写入历史
    if (HAL_RTC_GetTime(&hrtc, &startTime, RTC_FORMAT_BIN) != HAL_OK)
        return HAL_ERROR;
    if (HAL_RTC_GetDate(&hrtc, &startDate, RTC_FORMAT_BIN) != HAL_OK)
        return HAL_ERROR;

    recordStartTime = startTime;
    recordStartDate = startDate;
    weakMs = 0;
    mediumMs = 0;
    strongMs = 0;
    lastCountTime = HAL_GetTick(); // 从实际开始时刻建立毫秒计时基准
    historyView = 0;
    historyDateFilter = 0;
    recording = 1;
    return HAL_OK;
}

// 手动/自动停止共用保存逻辑；重复调用不会重复添加历史
void Record_StopAndSave(void)
{
    if (recording == 0) return;
    recording = 0;
    history[2] = history[1]; // 先后移旧记录，避免覆盖
    history[1] = history[0];
    history[0].weakMs = weakMs;
    history[0].mediumMs = mediumMs;
    history[0].strongMs = strongMs;
    history[0].startTime = recordStartTime;
    history[0].startDate = recordStartDate;
    if (historyCount < 3) historyCount++;
    historyView = 0;
    storageSavePending = 1; // 手动/自动停止都请求保存最新历史
}

// 只在主循环调用，回调函数仍只置通知标志
void Schedule_HandleAlarm(void)
{
    if ((scheduleEnabled == 0) || (scheduleConfigured == 0)) return;

    if (scheduleWaitingStop == 0) // 【开始闹钟到时】
    {
        if (Record_Start() != HAL_OK)
        {
            Error_Handler();
            return;
        }
        settingMode = 0;      // 到时回到记录界面，不停留在设置页
        scheduleSetting = 0;
        scheduleWaitingStop = 1;
        if (RTC_SetNextAlarm(scheduleStopTime) != HAL_OK)
            Error_Handler(); // 安排停止时间；早于当前时刻则是明天
    }
    else // 【停止闹钟到时】
    {
        Record_StopAndSave();
        scheduleWaitingStop = 0;
        if (RTC_SetNextAlarm(scheduleStartTime) != HAL_OK)
            Error_Handler(); // 再安排下一次开始，形成每天循环
    }
}


// JEDEC ID读取：只查询厂商/器件信息，不擦除、不写入Flash
HAL_StatusTypeDef Flash_ReadJEDECID(uint8_t id[3])
{
    if (id == NULL) return HAL_ERROR;

    // 第一个字节发0x9F读ID命令，随后发3个占位字节，产生接收所需时钟
    uint8_t tx[4] = {0x9F, 0xFF, 0xFF, 0xFF};
    uint8_t rx[4] = {0};

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // CS拉低：选中Flash
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 4, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   // 成功或失败都释放CS

    if (status == HAL_OK)
    {
        // 发命令期间收到的rx[0]无效，后面三个才是ID
        id[0] = rx[1]; // 厂商代码
        id[1] = rx[2]; // 存储器类型
        id[2] = rx[3]; // 容量代码
    }
    return status;
}

// 仅用于已确认容量为8MB的W25Q64系列；地址为芯片内部字节地址
#define FLASH_CAPACITY_BYTES 0x800000UL
#define FLASH_TEST_ADDRESS   0x7FF000UL // 最后一个4KB扇区，仅供本次测试

static HAL_StatusTypeDef Flash_ReadStatus(uint8_t *value)
{
    uint8_t tx[2] = {0x05, 0xFF}; // 0x05：读状态寄存器1
    uint8_t rx[2] = {0};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    if (status == HAL_OK) *value = rx[1];
    return status;
}

static HAL_StatusTypeDef Flash_WaitReady(uint32_t timeoutMs)
{
    uint32_t begin = HAL_GetTick();
    while (1)
    {
        uint8_t value;
        HAL_StatusTypeDef status = Flash_ReadStatus(&value);
        if (status != HAL_OK) return status;
        if ((value & 0x01U) == 0) return HAL_OK; // BUSY位为0：内部操作完成
        if (HAL_GetTick() - begin >= timeoutMs) return HAL_TIMEOUT;
        HAL_Delay(1); // 只在启动测试/保存时等待，不在中断里调用
    }
}

static HAL_StatusTypeDef Flash_WriteEnable(void)
{
    uint8_t command = 0x06; // 每次擦除/页编程前，都需重新写使能
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, &command, 1, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    if (status != HAL_OK) return status;
    uint8_t value;
    status = Flash_ReadStatus(&value);
    if (status != HAL_OK) return status;
    return (value & 0x02U) ? HAL_OK : HAL_ERROR; // 检查WEL写使能锁存位
}

HAL_StatusTypeDef Flash_ReadData(uint32_t address, uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0) ||
        (address >= FLASH_CAPACITY_BYTES) ||
        (length > FLASH_CAPACITY_BYTES - address)) return HAL_ERROR;

    HAL_StatusTypeDef status = Flash_WaitReady(3000);
    if (status != HAL_OK) return status;
    uint8_t command[4] = {0x03, (uint8_t)(address >> 16),
                         (uint8_t)(address >> 8), (uint8_t)address};
    // F1 HAL主机全双工Receive会同时发送接收缓冲原值；先填FF作为占位数据
    memset(data, 0xFF, length);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(&hspi1, command, sizeof(command), 100);
    if (status == HAL_OK) status = HAL_SPI_Receive(&hspi1, data, length, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    return status;
}

HAL_StatusTypeDef Flash_EraseSector(uint32_t address)
{
    // 传入扇区起点
    if ((address >= FLASH_CAPACITY_BYTES) || ((address & 0xFFFU) != 0))
        return HAL_ERROR;
    HAL_StatusTypeDef status = Flash_WaitReady(3000);
    if (status != HAL_OK) return status;
    status = Flash_WriteEnable();
    if (status != HAL_OK) return status;
    uint8_t command[4] = {0x20, (uint8_t)(address >> 16),
                         (uint8_t)(address >> 8), (uint8_t)address};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(&hspi1, command, sizeof(command), 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    if (status != HAL_OK) return status;
    return Flash_WaitReady(3000); // 擦除完成后整扇区应为FF
}

HAL_StatusTypeDef Flash_ProgramPage(uint32_t address, const uint8_t *data, uint16_t length)
{
    // 一次只写一页，且不能跨越256字节页边界
    if ((data == NULL) || (length == 0) || (length > 256U) ||
        (address >= FLASH_CAPACITY_BYTES) ||
        (length > FLASH_CAPACITY_BYTES - address) ||
        ((address & 0xFFU) + length > 256U)) return HAL_ERROR;
    HAL_StatusTypeDef status = Flash_WaitReady(3000);
    if (status != HAL_OK) return status;
    status = Flash_WriteEnable();
    if (status != HAL_OK) return status;
    uint8_t command[4] = {0x02, (uint8_t)(address >> 16),
                         (uint8_t)(address >> 8), (uint8_t)address};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(&hspi1, command, sizeof(command), 100);
    if (status == HAL_OK)
        status = HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    if (status != HAL_OK) return status;
    return Flash_WaitReady(100);
}

// 返回提示文字给主循环打印；未确认整扇区空白时，绝不擦除
const char *Flash_TestBlankSector(void)
{
    const uint8_t pattern[] = {0x12, 0x34, 0x56, 0x78, 0xA5, 0x5A, 0x00, 0xFF};
    uint8_t block[128];
    uint8_t blank = 1;
    uint8_t alreadyTested = 1;

    // 先读完整个测试扇区，既保护旧数据，也防止每次开机重复擦写
    for (uint32_t offset = 0; offset < 4096U; offset += sizeof(block))
    {
        if (Flash_ReadData(FLASH_TEST_ADDRESS + offset, block, sizeof(block)) != HAL_OK)
            return "Flash test FAIL: pre-read\r\n";
        for (uint32_t i = 0; i < sizeof(block); i++)
        {
            uint32_t position = offset + i;
            if (block[i] != 0xFF) blank = 0;
            uint8_t expected = (position < sizeof(pattern)) ? pattern[position] : 0xFF;
            if (block[i] != expected) alreadyTested = 0;
        }
    }
    if (alreadyTested != 0)
        return "Flash test pattern retained; no erase/write\r\n";
    if (blank == 0)
        return "Flash test SKIP: sector has existing data\r\n";

    if (Flash_EraseSector(FLASH_TEST_ADDRESS) != HAL_OK)
        return "Flash test FAIL: sector erase\r\n";
    // 擦除后再次核对全部为FF，识别保护/命令未生效等情况
    for (uint32_t offset = 0; offset < 4096U; offset += sizeof(block))
    {
        if (Flash_ReadData(FLASH_TEST_ADDRESS + offset, block, sizeof(block)) != HAL_OK)
            return "Flash test FAIL: erase readback\r\n";
        for (uint32_t i = 0; i < sizeof(block); i++)
            if (block[i] != 0xFF) return "Flash test FAIL: erase verify\r\n";
    }
    if (Flash_ProgramPage(FLASH_TEST_ADDRESS, pattern, sizeof(pattern)) != HAL_OK)
        return "Flash test FAIL: page program\r\n";
    if (Flash_ReadData(FLASH_TEST_ADDRESS, block, sizeof(pattern)) != HAL_OK)
        return "Flash test FAIL: final read\r\n";
    if (memcmp(block, pattern, sizeof(pattern)) != 0)
        return "Flash test FAIL: compare\r\n";
    return "Flash test PASS: erase/write/read matched\r\n";
}

// CRC-32/ISO-HDLC：检查数据是否意外变化，不用于加密
// data指向待校验字节，length是字节数；这里不包括checksum字段本身
uint32_t Storage_Checksum(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 1U) != 0)
                crc = (crc >> 1) ^ 0xEDB88320UL;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

// 把已确认的阈值和已停止保存的历史打包；不含正在进行的记录或候选设置
HAL_StatusTypeDef Storage_Pack(StorageSnapshot *snapshot, uint32_t sequence)
{
    if ((snapshot == NULL) || (historyCount > 3U)) return HAL_ERROR;
    // 用完整的正向条件同时排除NaN等非法浮点值
    if (!((strongThreshold >= 0.0f) && (strongThreshold < weakThreshold) &&
          (weakThreshold <= 3.3f))) return HAL_ERROR;

    // 先清零，包括结构体填充字节和未使用的历史，保证校验结果确定
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->magic = STORAGE_MAGIC;
    snapshot->formatVersion = STORAGE_FORMAT_VERSION;
    snapshot->sequence = sequence;
    snapshot->strongThreshold = strongThreshold;
    snapshot->weakThreshold = weakThreshold;
    snapshot->historyCount = historyCount;

    for (uint8_t i = 0; i < historyCount; i++)
    {
        // 分别复制各成员，保留清零后的结构体填充字节
        snapshot->history[i].startTime = history[i].startTime;
        snapshot->history[i].startDate = history[i].startDate;
        snapshot->history[i].weakMs = history[i].weakMs;
        snapshot->history[i].mediumMs = history[i].mediumMs;
        snapshot->history[i].strongMs = history[i].strongMs;
    }

    // offsetof得到checksum前面有多少字节，只校验它之前的内容
    snapshot->checksum = Storage_Checksum(snapshot, offsetof(StorageSnapshot, checksum));
    return HAL_OK;
}

// 两个正式存储扇区；0x7FF000仍保留为前面的擦写测试区
#define STORAGE_SLOT_A 0x7FD000UL
#define STORAGE_SLOT_B 0x7FE000UL
_Static_assert(sizeof(StorageSnapshot) <= 256U, "Snapshot must fit one Flash page");

static uint32_t Storage_SlotAddress(uint8_t slot)
{
    return (slot == 0) ? STORAGE_SLOT_A : STORAGE_SLOT_B;
}

static uint8_t Storage_IsValid(const StorageSnapshot *snapshot)
{
    if ((snapshot->magic != STORAGE_MAGIC) ||
        (snapshot->formatVersion != STORAGE_FORMAT_VERSION) ||
        (snapshot->historyCount > 3U)) return 0;
    if (snapshot->checksum != Storage_Checksum(snapshot, offsetof(StorageSnapshot, checksum)))
        return 0;
    if (!((snapshot->strongThreshold >= 0.0f) &&
          (snapshot->strongThreshold < snapshot->weakThreshold) &&
          (snapshot->weakThreshold <= 3.3f))) return 0;

    for (uint32_t i = 0; i < snapshot->historyCount; i++)
    {
        const LightRecord *record = &snapshot->history[i];
        const RTC_DateTypeDef *date = &record->startDate;
        if ((record->startTime.Hours > 23U) ||
            (record->startTime.Minutes > 59U) || (record->startTime.Seconds > 59U) ||
            (date->Year > 99U) || (date->Month < 1U) || (date->Month > 12U)) return 0;
        uint8_t days = 31;
        if ((date->Month == 4U) || (date->Month == 6U) ||
            (date->Month == 9U) || (date->Month == 11U)) days = 30;
        else if (date->Month == 2U)
        {
            uint16_t year = 2000U + date->Year;
            days = ((year % 400U == 0U) ||
                    ((year % 4U == 0U) && (year % 100U != 0U))) ? 29U : 28U;
        }
        if ((date->Date < 1U) || (date->Date > days)) return 0;
    }
    return 1;
}

// writable=1表示整扇区空白，或带本版本识别头且包外仍为空白。
// 校验失败的本工程副本可重写；不认识的数据绝不自动擦除。
static HAL_StatusTypeDef Storage_ReadSlot(uint8_t slot, StorageSnapshot *snapshot,
                                          uint8_t *writable)
{
    uint32_t address = Storage_SlotAddress(slot);
    HAL_StatusTypeDef status = Flash_ReadData(address, (uint8_t *)snapshot, sizeof(*snapshot));
    if (status != HAL_OK) return status;
    uint8_t blank = 1;
    uint8_t tailBlank = 1;
    uint8_t block[128];
    for (uint32_t offset = 0; offset < 4096U; offset += sizeof(block))
    {
        status = Flash_ReadData(address + offset, block, sizeof(block));
        if (status != HAL_OK) return status;
        for (uint32_t i = 0; i < sizeof(block); i++)
        {
            if (block[i] != 0xFFU)
            {
                blank = 0;
                if (offset + i >= sizeof(*snapshot)) tailBlank = 0;
            }
        }
    }
    *writable = blank || ((snapshot->magic == STORAGE_MAGIC) &&
                         (snapshot->formatVersion == STORAGE_FORMAT_VERSION) && tailBlank);
    return HAL_OK;
}

HAL_StatusTypeDef Storage_Init(void)
{
    StorageSnapshot a, b;
    uint8_t writableA, writableB;
    storageReady = 0;
    storageActiveSlot = -1;
    storageSequence = 0;
    HAL_StatusTypeDef status = Storage_ReadSlot(0, &a, &writableA);
    if (status != HAL_OK) return status;
    status = Storage_ReadSlot(1, &b, &writableB);
    if (status != HAL_OK) return status;
    uint8_t validA = Storage_IsValid(&a);
    uint8_t validB = Storage_IsValid(&b);
    const StorageSnapshot *latest = NULL;
    if (validA) { latest = &a; storageActiveSlot = 0; }
    // 无符号差值支持序号回绕；两份副本正常只相差一次保存
    if (validB && (!validA || ((b.sequence - a.sequence != 0U) &&
                              (b.sequence - a.sequence < 0x80000000UL))))
    {
        latest = &b;
        storageActiveSlot = 1;
    }
    if (latest != NULL)
    {
        strongThreshold = latest->strongThreshold;
        weakThreshold = latest->weakThreshold;
        historyCount = (uint8_t)latest->historyCount;
        memcpy(history, latest->history, sizeof(history));
        storageSequence = latest->sequence;
    }
    // 无有效包时保留默认阈值/空历史；至少有一个可写扇区才允许首次保存
    if ((latest == NULL) && !writableA && !writableB) return HAL_ERROR;
    storageReady = 1;
    return HAL_OK;
}

HAL_StatusTypeDef Storage_Save(void)
{
    if (!storageReady || recording) return HAL_BUSY; // 不在记录过程中擦写
    StorageSnapshot snapshot, verify;
    HAL_StatusTypeDef status = Storage_Pack(&snapshot, storageSequence + 1U);
    if (status != HAL_OK) return status;
    if (!Storage_IsValid(&snapshot)) return HAL_ERROR;

    // 优先写另一份，绝不先擦掉当前有效副本
    uint8_t target = (storageActiveSlot == 0) ? 1U : 0U;
    uint8_t writable;
    status = Storage_ReadSlot(target, &verify, &writable);
    if (status != HAL_OK) return status;
    if (!writable && storageActiveSlot < 0)
    {
        target = 1U; // 首次保存：A被其他数据占用时尝试B
        status = Storage_ReadSlot(target, &verify, &writable);
        if (status != HAL_OK) return status;
    }
    if (!writable) return HAL_ERROR; // 不擦除未知内容，也不破坏旧有效副本

    uint32_t address = Storage_SlotAddress(target);
    status = Flash_EraseSector(address);
    if (status != HAL_OK) return status;
    status = Flash_ProgramPage(address, (const uint8_t *)&snapshot, sizeof(snapshot));
    if (status != HAL_OK) return status;
    status = Flash_ReadData(address, (uint8_t *)&verify, sizeof(verify));
    if (status != HAL_OK) return status;
    if (!Storage_IsValid(&verify) || memcmp(&verify, &snapshot, sizeof(snapshot)) != 0)
        return HAL_ERROR;
    // 只有读回确认成功，才将新副本记为当前有效副本
    storageActiveSlot = (int8_t)target;
    storageSequence = snapshot.sequence;
    return HAL_OK;
}

// LSE外部32.768kHz时钟；备份域数据有效时恢复日历，无备用供电则冷启动需校时
#define CLOCK_BACKUP_MARK 0xC10CUL

static uint8_t Clock_DateValid(const RTC_DateTypeDef *date)
{
    if ((date->Year > 99U) || (date->Month < 1U) || (date->Month > 12U)) return 0;
    uint8_t days = 31;
    if ((date->Month == 4U) || (date->Month == 6U) ||
        (date->Month == 9U) || (date->Month == 11U)) days = 30;
    else if (date->Month == 2U)
    {
        uint16_t year = 2000U + date->Year;
        days = ((year % 400U == 0U) ||
                ((year % 4U == 0U) && (year % 100U != 0U))) ? 29U : 28U;
    }
    return (date->Date >= 1U) && (date->Date <= days);
}

void Clock_StoreBackupDate(const RTC_DateTypeDef *date)
{
    if (!clockValid || !Clock_DateValid(date)) return;
    uint16_t packed = (uint16_t)(((uint16_t)date->Year << 9) |
                                ((uint16_t)date->Month << 5) | date->Date);
    if ((HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == CLOCK_BACKUP_MARK) &&
        (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2) == packed) &&
        (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR3) == (uint16_t)~packed)) return;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0); // 更新中断时，下次启动不误认有效
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR4, date->WeekDay);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, packed);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR3, (uint16_t)~packed);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, CLOCK_BACKUP_MARK);
}

HAL_StatusTypeDef Clock_Set(RTC_DateTypeDef date, RTC_TimeTypeDef time)
{
    if (!Clock_DateValid(&date) || (time.Hours > 23U) ||
        (time.Minutes > 59U) || (time.Seconds > 59U)) return HAL_ERROR;
    clockValid = 0;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0);
    HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
    if (status != HAL_OK) return status;
    status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
    if (status != HAL_OK) return status;
    clockValid = 1;
    Clock_StoreBackupDate(&date);
    return HAL_OK;
}

HAL_StatusTypeDef Clock_InitSession(void)
{
    // 关闭上一会话遗留的闹钟；定时功能在本次上电确认后才启用
    HAL_StatusTypeDef status = HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    if (status != HAL_OK) return status;
    rtcAlarmPending = 0;
    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    clockValid = 0;

    uint16_t packed = (uint16_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2);
    RTC_DateTypeDef date = {0};
    date.Year = (uint8_t)(packed >> 9);
    date.Month = (uint8_t)((packed >> 5) & 0x0FU);
    date.Date = (uint8_t)(packed & 0x1FU);
    date.WeekDay = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR4);

    // LSE可在VBAT供电下继续运行；标记与日期校验通过才恢复，不能假定一定有备用电池
    if ((HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == CLOCK_BACKUP_MARK) &&
        (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR3) == (uint16_t)~packed) &&
        Clock_DateValid(&date))
    {
        // 仅恢复HAL的软件日期，不能用SetDate截掉尚未处理的跨天计数
        hrtc.DateToUpdate = date;
        clockValid = 1;
        RTC_TimeTypeDef time;
        status = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
        if (status == HAL_OK) status = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
        if (status == HAL_OK) Clock_StoreBackupDate(&date);
        else clockValid = 0;
    }
    else
    {
        // 冷启动给编辑器一个合法起点，但明确标为未校时，不当作真实时间使用
        RTC_TimeTypeDef time = {0};
        date.Year = 26;
        date.Month = 11;
        date.Date = 10;
        status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
        if (status == HAL_OK) status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0);
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return status;
}

HAL_StatusTypeDef Clock_BeginEdit(uint32_t potRaw)
{
    HAL_StatusTypeDef status = HAL_RTC_GetTime(&hrtc, &editTime, RTC_FORMAT_BIN);
    if (status != HAL_OK) return status;
    status = HAL_RTC_GetDate(&hrtc, &editDate, RTC_FORMAT_BIN);
    if (status != HAL_OK) return status;
    clockPotEntry = potRaw;
    clockPotMoved = 0;
    clockSetting = 1;
    settingMode = 0;
    historyView = 0;
    return HAL_OK;
}

// 根据开始日期寻找下一条同日记录；index为0～historyCount-1
static uint8_t History_SameDate(uint8_t a, uint8_t b)
{
    return (history[a].startDate.Year == history[b].startDate.Year) &&
           (history[a].startDate.Month == history[b].startDate.Month) &&
           (history[a].startDate.Date == history[b].startDate.Date);
}

void History_Next(void)
{
    if (!historyDateFilter || historyView == 0)
    {
        historyView++;
        if (historyView > historyCount) historyView = 0;
        return;
    }
    uint8_t anchor = historyView - 1U;
    for (uint8_t step = 1; step <= historyCount; step++)
    {
        uint8_t index = (uint8_t)((anchor + step) % historyCount);
        if (History_SameDate(anchor, index))
        {
            historyView = index + 1U;
            return;
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  if (Clock_InitSession() != HAL_OK) Error_Handler();
  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1,(uint32_t *)adcData,2);//句柄，存放数组，存放长度
  HAL_TIM_Base_Start(&htim3);
  OLED_Init();
  OLED_ShowString(1, 1, "Light Logger");
  HAL_Delay(1000);
  OLED_Clear();

  // SPI/串口已初始化，OLED启动等待也已结束：上电读取一次芯片ID
  uint8_t flashId[3] = {0};
  char flashMessage[64];
  if (Flash_ReadJEDECID(flashId) == HAL_OK)
  {
      snprintf(flashMessage, sizeof(flashMessage), "Flash ID=%02X %02X %02X\r\n",
               (unsigned int)flashId[0], (unsigned int)flashId[1],
               (unsigned int)flashId[2]);
  }
  else
  {
      snprintf(flashMessage, sizeof(flashMessage), "Flash SPI transfer failed\r\n");
  }
  HAL_UART_Transmit(&huart1, (uint8_t *)flashMessage, strlen(flashMessage), 100);

  // 旧测试保留函数但不再开机调用；正式存储使用两个独立扇区
  if ((flashId[0] == 0xEF) && (flashId[1] == 0x40) && (flashId[2] == 0x17))
  {
      const char *message;
      if (Storage_Init() == HAL_OK)
          message = (storageActiveSlot >= 0) ? "Storage restored\r\n" : "Storage empty: defaults kept\r\n";
      else
          message = "Storage unavailable: read error or occupied sectors\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t *)message, strlen(message), 100);
  }

  // 不再在每次启动时强制写入测试日期/时间
  const char *clockMessage = clockValid ? "Clock restored from backup domain\r\n" :
                                         "Clock not set: press PB10 to calibrate\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)clockMessage, strlen(clockMessage), 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	
		uint32_t raw = adcData[0];
		uint32_t potRaw = adcData[1];
		float V = raw * 3.3f / 4095.0f;//将原始数据转换为电压
		float potV = potRaw * 3.3f / 4095.0f;
	    float candidateThreshold = potV;//设置预选的阈值
		char buffer[64];
		const char *level;

		if (V > weakThreshold)
		{
			level = "Weak";
		}
		else if (V > strongThreshold)
		{
			level = "Medium";
		}
		else
		{
			level = "Strong";
		}
		
		//按键实现电位器切换阈值处的选择（更改后我决定采用不阻塞的写法）
	/*	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET)//按下瞬间
		{
			HAL_Delay(20);//消抖

			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET)
			{
				while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET)//一直按住就while
				{
				}

				HAL_Delay(20);//松开瞬间

				if (settingMode == 0)
				{
					settingMode = 1;
				}
				else if (settingMode == 1)
				{
					if (candidateThreshold < weakThreshold)
					{
						strongThreshold = candidateThreshold;
						settingMode = 2;
					}
				}
				else
				{
					if (candidateThreshold > strongThreshold)
					{
						weakThreshold = candidateThreshold;
						settingMode = 0;
					}
				}
				
			}
		}*/
		
		uint32_t now = HAL_GetTick();
		//以下为模式计时的处理
		// 本轮距离上一轮过去了多少毫秒
		uint32_t elapsed = now - lastCountTime;
		lastCountTime = now; // 无论是否记录，都更新时间基准

		// 正在记录 才把经过的时间加到对应等级
		if (recording == 1)
		{
			// 把刚过去的时间归入上一轮观察到的等级
			if (lastLevel == 'W')
			{
				weakMs += elapsed;
			}
			else if (lastLevel == 'M')
			{
				mediumMs += elapsed;
			}
			else
			{
				strongMs += elapsed;
			}
		}

		// 保存当前等级，供下一轮累计时间时使用
		// "Weak"、"Medium"、"Strong"的首字母分别是W、M、S, level视为是字符串数组，首字母刷新
		lastLevel = level[0];

        // 计时累计完成后再处理闹钟，停止前的最后一段时间不会丢失
        if (rtcAlarmPending != 0)
        {
            rtcAlarmPending = 0; // 消费通知，自动启停在主循环执行
            Schedule_HandleAlarm();
        }
        
		// PB11：按下为0，松开为1；不阻塞地检测一次按放
        GPIO_PinState scheduleKeyNow = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11);
        if (scheduleKeyNow != scheduleKeyLast) // 电平变化，重新消抖计时
        {
            scheduleKeyLast = scheduleKeyNow;
            scheduleKeyChangeTime = now;
        }
        if ((now - scheduleKeyChangeTime >= 20U) &&
            (scheduleKeyNow != scheduleKeyStable))
        {
            scheduleKeyStable = scheduleKeyNow;
            if (scheduleKeyStable == GPIO_PIN_SET) // 确认松开，执行一次
            {
                if (scheduleSetting == 0)
                {
                    if ((recording == 0) && (clockSetting == 0))
                    {
                        editScheduleStart = scheduleStartTime; // 从已保存时间开始编辑
                        editScheduleStop = scheduleStopTime;
                        editScheduleEnabled = scheduleEnabled;
                        scheduleTimeError = 0;
                        schedulePotEntry = potRaw;
                        schedulePotMoved = 0;
                        scheduleSetting = 1; // 进入开始小时设置
                        settingMode = 0;     // 退出阈值设置
                        historyView = 0;     // 退出历史查看
                    }
                }
                else
                {
                    scheduleSetting = 0; // 再按PB11取消退出
                }
            }
        }

        // 定时编辑：1～3为开始时分秒，4～6为停止时分秒，7为最终预览
        if ((scheduleSetting >= 1) && (scheduleSetting <= 6))
        {
            int32_t change = (int32_t)potRaw - (int32_t)schedulePotEntry;
            if ((change > 40) || (change < -40)) // 排除小范围ADC波动
            {
                schedulePotMoved = 1;
            }
            if (schedulePotMoved == 1)
            {
                if (scheduleSetting == 1)
                    editScheduleStart.Hours = (uint8_t)(potRaw * 24U / 4096U);
                else if (scheduleSetting == 2)
                    editScheduleStart.Minutes = (uint8_t)(potRaw * 60U / 4096U);
                else if (scheduleSetting == 3)
                    editScheduleStart.Seconds = (uint8_t)(potRaw * 60U / 4096U);
                else if (scheduleSetting == 4)
                    editScheduleStop.Hours = (uint8_t)(potRaw * 24U / 4096U);
                else if (scheduleSetting == 5)
                    editScheduleStop.Minutes = (uint8_t)(potRaw * 60U / 4096U);
                else
                    editScheduleStop.Seconds = (uint8_t)(potRaw * 60U / 4096U);
            }
        }

        // 状态8：旋钮下半程选择OFF，上半程选择ON；明显转动后才改变候选开关
        if (scheduleSetting == 8)
        {
            int32_t change = (int32_t)potRaw - (int32_t)schedulePotEntry;
            if ((change > 40) || (change < -40))
                schedulePotMoved = 1;
            if (schedulePotMoved == 1)
                editScheduleEnabled = (potRaw >= 2048U) ? 1U : 0U;
        }

        // PB10：按下为0，松开为1
		GPIO_PinState clockKeyNow = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);

		// 按下、松开或抖动时，重新计时
		if (clockKeyNow != clockKeyLast)
		{
			clockKeyLast = clockKeyNow;
			clockKeyChangeTime = now;
		}

		// 消抖 电平稳定20ms，而且与原稳定状态不同
		if ((now - clockKeyChangeTime >= 20U) &&
			(clockKeyNow != clockKeyStable))
		{
			clockKeyStable = clockKeyNow;

			// 一次按放后执行一次
			if ((clockKeyStable == GPIO_PIN_SET) && (scheduleSetting == 0) && (scheduleEnabled == 0))
			{
				if (clockSetting == 0)
				{
					// 只有停止记录时，才允许进入校时
					if (recording == 0) // 停止记录时才允许校时
					{
						// 先读时间，再读日期，作为编辑的起点
						if (HAL_RTC_GetTime(&hrtc, &editTime, RTC_FORMAT_BIN) == HAL_OK)
						{
							if (HAL_RTC_GetDate(&hrtc, &editDate, RTC_FORMAT_BIN) == HAL_OK)
							{
								clockPotEntry = potRaw; // 记住进入年份设置时的旋钮位置
								clockPotMoved = 0;      // 先不让旋钮覆盖读取到的原年份
								clockSetting = 1;
								clockSetting = 1; // 进入年份设置
								settingMode = 0;
								historyView = 0;
							}
						}
					}
				}
				else
				{
					clockSetting = 0; // 再按一次退出，目前不修改RTC
				}
			}
		}
		
		if ((clockSetting >= 1) && (clockSetting <= 6))
		{
			uint8_t daysInMonth = 31; // 默认本月31天
			uint16_t year = 2000U + editDate.Year;

			// 4、6、9、11月只有30天
			if ((editDate.Month == 4) || (editDate.Month == 6) ||
				(editDate.Month == 9) || (editDate.Month == 11))
			{
				daysInMonth = 30;
			}
			else if (editDate.Month == 2) // 2月需要判断闰年
			{
				// 能被400整除，或能被4整除但不能被100整除，为闰年
				if ((year % 400U == 0U) ||
					((year % 4U == 0U) && (year % 100U != 0U)))
				{
					daysInMonth = 29;
				}
				else
				{
					daysInMonth = 28;
				}
			}

			// 原日期如果超过新月份上限，先修正为本月最后一天
			if (editDate.Date > daysInMonth)
			{
				editDate.Date = daysInMonth;
			}
			// 计算旋钮相对进入时的位置，变化了多少ADC计数
			int32_t potChange = (int32_t)potRaw - (int32_t)clockPotEntry;

			// 确认转动变化超过40个计数，才认为转动旋钮
			if ((potChange > 40) || (potChange < -40))
			{
				clockPotMoved = 1;
			}

			// 【已经转动】之后持续按照旋钮位置调整当前项目
			if (clockPotMoved == 1) // 已确认旋钮转动
			{
				if (clockSetting == 1) // 年
				{
					editDate.Year = (uint8_t)(potRaw * 99U / 4095U);
				}
				else if (clockSetting == 2) // 月
				{
					editDate.Month = (uint8_t)(potRaw * 12U / 4096U + 1U);
				}
				else if (clockSetting == 3) // 日，范围1～本月天数
				{
					editDate.Date =
						(uint8_t)(potRaw * daysInMonth / 4096U + 1U);
				}
				else if (clockSetting == 4) // 小时：0～23
				{
					editTime.Hours = (uint8_t)(potRaw * 24U / 4096U);
				}
				else if (clockSetting == 5) // 分钟：0～59
				{
					editTime.Minutes = (uint8_t)(potRaw * 60U / 4096U);
				}
				else if (clockSetting == 6) // 秒：0～59
				{
					editTime.Seconds = (uint8_t)(potRaw * 60U / 4096U);
				}
			}
		}
		
		//此为不阻塞写法，PB0配置
		GPIO_PinState keyNow = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);

		if (keyNow != keyLast)
		{
			keyLast = keyNow;
			keyChangeTime = now;
		}

		if ((now - keyChangeTime >= 20U) && (keyNow != keyStable))
		{
			keyStable = keyNow;
			// 确认松开PB0：定时页面优先处理，其他页面走原逻辑
            if ((keyStable == GPIO_PIN_SET) &&
                (scheduleSetting >= 1) && (scheduleSetting <= 6))
            {
                scheduleSetting++;       // 开始时分秒→停止时分秒→最终预览
                schedulePotEntry = potRaw;
                schedulePotMoved = 0;     // 新项目先保留原值
            }
            else if ((keyStable == GPIO_PIN_SET) && (scheduleSetting == 7))
            {
                // 最终确认：相同时间无法区分开始/停止，拒绝保存
                if ((editScheduleStart.Hours == editScheduleStop.Hours) &&
                    (editScheduleStart.Minutes == editScheduleStop.Minutes) &&
                    (editScheduleStart.Seconds == editScheduleStop.Seconds))
                {
                    scheduleTimeError = 1; // 保持预览，可按PB11取消后重设
                }
                else
                {
                    scheduleSetting = 8; // 时间合法，接着选择启用/关闭
                    schedulePotEntry = potRaw;
                    schedulePotMoved = 0;
                }
            }
            else if ((keyStable == GPIO_PIN_SET) && (scheduleSetting == 8))
            {
                if (editScheduleEnabled && !clockValid)
                {
                    scheduleSetting = 0;
                    if (Clock_BeginEdit(potRaw) != HAL_OK) Error_Handler();
                }
                else
                {
                // 候选值最终确认：先检查硬件设置成功，再提交软件状态
                if (editScheduleEnabled != 0)
                {
                    if (RTC_SetNextAlarm(editScheduleStart) != HAL_OK)
                        Error_Handler();
                }
                else
                {
                    if (HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A) != HAL_OK)
                        Error_Handler();
                    rtcAlarmPending = 0; // 丢弃关闭前遗留的到时通知
                    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
                    HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
                }
                scheduleStartTime = editScheduleStart;
                scheduleStopTime = editScheduleStop;
                scheduleConfigured = 1;
                scheduleEnabled = editScheduleEnabled;
                scheduleWaitingStop = 0; // 每次确认启用，先等待下一次开始
                scheduleSetting = 0;
                }
            }
            else if ((keyStable == GPIO_PIN_SET) && (scheduleSetting == 0))
			{
                if ((clockSetting == 0) && (historyView > 0))
                {
                    historyDateFilter = !historyDateFilter; // 只看当前记录同一天的历史
                }
                else if (clockSetting == 1) // 正在设置年份
				{
					clockPotEntry = potRaw; // 记录进入月份时的位置
					clockPotMoved = 0;      // 等再次转动旋钮，才修改月份
					clockSetting = 2; // 年份保留在editDate.Year，进入月份设置
				}
				else if (clockSetting == 2) // 【松开PB0】确认月份，进入日期
				{
					clockPotEntry = potRaw; // 记住进入日期设置时的旋钮位置
					clockPotMoved = 0;      // 未转动时先保留原值
					clockSetting = 3;      // 3表示调整“日”
				}
				else if (clockSetting == 3) // 松开PB0：确认日期，进入小时
				{
					clockPotEntry = potRaw; // 记住进入小时设置时的旋钮位置
					clockPotMoved = 0;      // 未转动时保留原小时
					clockSetting = 4;
				}
				else if (clockSetting == 4) // 松开PB0：确认小时，进入分钟
				{
					clockPotEntry = potRaw; // 记住进入分钟设置时的旋钮位置
					clockPotMoved = 0;      // 未转动时保留原分钟
					clockSetting = 5;
				}
				else if (clockSetting == 5) // 松开PB0：确认分钟，进入秒
                {
                    clockPotEntry = potRaw; // 记住进入秒设置时的旋钮位置
                    clockPotMoved = 0;      // 未转动时保留原秒数
                    clockSetting = 6;
                }
                else if (clockSetting == 6) // 松开PB0：确认秒，进入最终预览
                {
                    clockSetting = 7; // 预览期间旋钮不再修改候选值
                }
                else if (clockSetting == 7) // 松开PB0：确认写入日期和时间
                {
                    if (Clock_Set(editDate, editTime) != HAL_OK)
                    {
                        Error_Handler();
                    }
                    clockSetting = 0; // 两项写入成功，返回实时界面
                }
                else if (clockSetting == 0) // 不在校时，才处理原来的阈值设置
				{
					if (settingMode == 0)
					{
						settingMode = 1;
					}
					else if (settingMode == 1)
					{
						if (candidateThreshold < weakThreshold)
						{
							strongThreshold = candidateThreshold;
							settingMode = 2;
						}
					}
					else
					{
						if (candidateThreshold > strongThreshold)
						{
							weakThreshold = candidateThreshold;
							settingMode = 0;
                            storageSavePending = 1; // 两个阈值确认完毕，保存正式值
						}
					}
				}
			}


			
		}
				// PB1：按下为0，松开为1
		GPIO_PinState recordNow = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);

		// 按下、松开或抖动，重新开始消抖计时
		if (recordNow != recordLast)
		{
			recordLast = recordNow;
			recordChangeTime = now;
		}

		// 消抖电平稳定20ms，并且不同于之前的稳定状态
		if ((now - recordChangeTime >= 20U) &&
			(recordNow != recordStable))
		{
			recordStable = recordNow;

			// 开始计算长按时间
			if (recordStable == GPIO_PIN_RESET)
			{
				recordPressTime = now;
				recordLongHandled = 0; // 新的一次按下，允许触发一次
			}
			else // 松开稳定电平变为1
			{
				// 本次没有长按，而且处于停止、正常显示状态
				if ((recordLongHandled == 0) &&
					(recording == 0) &&
					(settingMode == 0)&& (clockSetting == 0) && (scheduleSetting == 0) && ((scheduleEnabled == 0) || (now - recordPressTime < 1000U)))
				{
                    History_Next(); // 全部浏览，或循环浏览选定日期的记录
				}
			}
			// 不切换记录状态
		}

		// 长按成立。稳定状态和当前读数都为按下，
		// 从确认按下起已满1秒，而且本次还没有处理过
		if ((recordStable == GPIO_PIN_RESET) &&(recordNow == GPIO_PIN_RESET)
			&&(now - recordPressTime >= 1000U) &&
			(recordLongHandled == 0)&& (clockSetting == 0) && (scheduleSetting == 0) && (scheduleEnabled == 0))
		{
            if (recording == 0) // 【手动开始】与自动记录使用同一函数
            {
                if (!clockValid)
                {
                    if (Clock_BeginEdit(potRaw) != HAL_OK) Error_Handler();
                }
                else if (Record_Start() != HAL_OK) Error_Handler();
            }
            else // 【手动停止】与自动记录使用同一保存函数
            {
                Record_StopAndSave();
            }

			recordLongHandled = 1; // 本次已处理，继续按住也不会再次切换
		}
				
        // 只在停止记录且退出设置后保存，避免每轮/记录期间反复擦写
        if (storageSavePending && (recording == 0) && (settingMode == 0) &&
            (clockSetting == 0) && (scheduleSetting == 0))
        {
            HAL_StatusTypeDef saveStatus = Storage_Save();
            storageSavePending = 0; // 失败不自动反复擦写；下次确认/停止可再次尝试
            const char *message = (saveStatus == HAL_OK) ?
                                  "Storage saved\r\n" : "Storage save FAILED\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t *)message, strlen(message), 100);
        }

		//距上次刷新过去至少200ms
		if (now - displayTime >= 200U)
		{
			displayTime = now; // 记下本次刷新时间,往前推
			
			
            RTC_TimeTypeDef rtcTime = {0};
            RTC_DateTypeDef rtcDate = {0};
            uint8_t rtcReadOk = 0;
            if (HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN) == HAL_OK)
            {
                if (HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN) == HAL_OK)
                {
                    rtcReadOk = 1;
                    Clock_StoreBackupDate(&rtcDate); // 日期变化时才更新备份寄存器，不写Flash
                }
            }
            char oledText[32];
			
			if (scheduleSetting == 8) // 最后一步：确认是否启用闹钟
            {
                OLED_ShowString(1, 1, "Timer Enable    ");
                OLED_ShowString(2, 1, editScheduleEnabled ? "ON              " : "OFF             ");
                OLED_ShowString(3, 1, "PB0: Save       ");
                OLED_ShowString(4, 1, "PB11: Cancel    ");
            }
            else if (scheduleSetting != 0) // 编辑候选时间，不修改RTC当前时间
            {
                if (scheduleSetting == 7) // 最终预览，同时显示开始/停止
                {
                    OLED_ShowString(1, 1, scheduleTimeError ? "Same time!      " : "PB0: Next       ");
                    snprintf(oledText, sizeof(oledText), "On :%02u:%02u:%02u    ",
                             (unsigned int)editScheduleStart.Hours,
                             (unsigned int)editScheduleStart.Minutes,
                             (unsigned int)editScheduleStart.Seconds);
                    OLED_ShowString(2, 1, oledText);
                    snprintf(oledText, sizeof(oledText), "Off:%02u:%02u:%02u    ",
                             (unsigned int)editScheduleStop.Hours,
                             (unsigned int)editScheduleStop.Minutes,
                             (unsigned int)editScheduleStop.Seconds);
                    OLED_ShowString(3, 1, oledText);
                }
                else
                {
                    if (scheduleSetting == 1)
                        OLED_ShowString(1, 1, "Start: Hour     ");
                    else if (scheduleSetting == 2)
                        OLED_ShowString(1, 1, "Start: Minute   ");
                    else if (scheduleSetting == 3)
                        OLED_ShowString(1, 1, "Start: Second   ");
                    else if (scheduleSetting == 4)
                        OLED_ShowString(1, 1, "Stop: Hour      ");
                    else if (scheduleSetting == 5)
                        OLED_ShowString(1, 1, "Stop: Minute    ");
                    else
                        OLED_ShowString(1, 1, "Stop: Second    ");

                    if (scheduleSetting <= 3)
                        snprintf(oledText, sizeof(oledText), "%02u:%02u:%02u        ",
                                 (unsigned int)editScheduleStart.Hours,
                                 (unsigned int)editScheduleStart.Minutes,
                                 (unsigned int)editScheduleStart.Seconds);
                    else
                        snprintf(oledText, sizeof(oledText), "%02u:%02u:%02u        ",
                                 (unsigned int)editScheduleStop.Hours,
                                 (unsigned int)editScheduleStop.Minutes,
                                 (unsigned int)editScheduleStop.Seconds);
                    OLED_ShowString(2, 1, oledText);
                    OLED_ShowString(3, 1, "PB0: Next       ");
                }
                OLED_ShowString(4, 1, "PB11: Cancel    ");
            }
            else if (clockSetting == 7) // 最终预览：PB0保存，PB10取消
            {
                OLED_ShowString(1, 1, "PB0: Save clock ");
                snprintf(oledText, sizeof(oledText), "20%02u-%02u-%02u      ",
                         (unsigned int)editDate.Year,
                         (unsigned int)editDate.Month,
                         (unsigned int)editDate.Date);
                OLED_ShowString(2, 1, oledText);
                snprintf(oledText, sizeof(oledText), "%02u:%02u:%02u        ",
                         (unsigned int)editTime.Hours,
                         (unsigned int)editTime.Minutes,
                         (unsigned int)editTime.Seconds);
                OLED_ShowString(3, 1, oledText);
                OLED_ShowString(4, 1, "PB10: Cancel    ");
            }
            else if (clockSetting != 0) // 校时界面 年份、月份设置都进入这里
			{
				OLED_ShowString(1, 1, "Clock Setup     ");

				if (clockSetting == 1)
				{
					snprintf(oledText, sizeof(oledText), "Year:20%02u       ",
							 (unsigned int)editDate.Year);
					OLED_ShowString(3, 1, "PB0: Next       ");
				}
				else if (clockSetting == 2)
				{
    					snprintf(oledText, sizeof(oledText), "Month:%02u        ",
							 (unsigned int)editDate.Month);
					// 第3行同时显示待设置的年、月，观察年份是否保持不变
					char dateText[32];

					snprintf(dateText, sizeof(dateText), "20%02u-%02u         ",
							 (unsigned int)editDate.Year,
							 (unsigned int)editDate.Month);

					OLED_ShowString(3, 1, dateText);// 月份显示代码
				}
				else if (clockSetting == 3)
				{
					snprintf(oledText, sizeof(oledText), "Day:%02u          ",
							 (unsigned int)editDate.Date);

					OLED_ShowString(3, 1, "Day editing     ");
				}
				else if (clockSetting == 4) // 小时设置界面
				{
					snprintf(oledText, sizeof(oledText), "Hour:%02u         ",
					         (unsigned int)editTime.Hours);
					OLED_ShowString(3, 1, "Hour editing    ");
				}
				else if (clockSetting == 5) // 分钟设置界面
				{
					snprintf(oledText, sizeof(oledText), "Minute:%02u       ",
					         (unsigned int)editTime.Minutes);
					OLED_ShowString(3, 1, "Minute editing  ");
				}
				else if (clockSetting == 6) // 秒设置界面
				{
					snprintf(oledText, sizeof(oledText), "Second:%02u       ",
					         (unsigned int)editTime.Seconds);
					OLED_ShowString(3, 1, "PB0: Review     ");
				}




				OLED_ShowString(2, 1, oledText);
				OLED_ShowString(4, 1, "PB10: Cancel    ");
			}
			else if ((settingMode == 0) && (historyView > 0))
			{
				// 查看历史  historyView为1时读取history[0]，类推

				uint8_t index = historyView - 1;

				//月-日 时:分:秒
				snprintf(oledText, sizeof(oledText),
						 "%c %02u-%02u %02u:%02u:%02u",
                         historyDateFilter ? 'D' : ('0' + historyView),
						 (unsigned int)history[index].startDate.Month,
						 (unsigned int)history[index].startDate.Date,
						 (unsigned int)history[index].startTime.Hours,
						 (unsigned int)history[index].startTime.Minutes,
						 (unsigned int)history[index].startTime.Seconds);
						 
				OLED_ShowString(1, 1, oledText);		 

				snprintf(oledText, sizeof(oledText), "W:%10lu s  ",
						 (unsigned long)(history[index].weakMs / 1000U));
				OLED_ShowString(2, 1, oledText);

				snprintf(oledText, sizeof(oledText), "M:%10lu s  ",
						 (unsigned long)(history[index].mediumMs / 1000U));
				OLED_ShowString(3, 1, oledText);

				snprintf(oledText, sizeof(oledText), "S:%10lu s  ",
						 (unsigned long)(history[index].strongMs / 1000U));
				OLED_ShowString(4, 1, oledText);
			}
			else if (settingMode == 0)
			{
			//实时模式的记录内容
			
				// 第1行：光敏电压 level
                if (!clockValid)
                    snprintf(oledText, sizeof(oledText), "Set clock: PB10 ");
                else if (!rtcReadOk)
                    snprintf(oledText, sizeof(oledText), "RTC read failed ");
                else
                    snprintf(oledText, sizeof(oledText), "%02u-%02u %02u:%02u:%02u  ",
                             (unsigned int)rtcDate.Month, (unsigned int)rtcDate.Date,
                             (unsigned int)rtcTime.Hours, (unsigned int)rtcTime.Minutes,
                             (unsigned int)rtcTime.Seconds);
				OLED_ShowString(1, 1, oledText);

				// 第2行：记录状态
				// REC＝Recording，正在记录；STOP＝停止
				snprintf(oledText, sizeof(oledText), "%-6s %-4s %-4s", level,
                         recording ? "REC" : "STOP", scheduleEnabled ? "AUTO" : "MAN");
				OLED_ShowString(2, 1, oledText);

				// 第3行：弱光、中光累计秒数
				snprintf(oledText, sizeof(oledText), "W:%5lu M:%5lu ",(unsigned long)(weakMs / 1000U),(unsigned long)(mediumMs / 1000U));
				OLED_ShowString(3, 1, oledText);

				// 第4行：强光累计秒数
				snprintf(oledText, sizeof(oledText), "S:%5lu A:%4lu  ",
                         (unsigned long)(strongMs / 1000U), (unsigned long)raw);
				OLED_ShowString(4, 1, oledText);
			}
			else
			{
				OLED_ShowString(1, 1, "Threshold Setup ");

				if (settingMode == 1) // 正在调整强／中分界
				{
					snprintf(oledText, sizeof(oledText),"S/M:%.3fV      ", (double)candidateThreshold);
				}
				else // 正在调整中／弱分界
				{
					snprintf(oledText, sizeof(oledText),"M/W:%.3fV      ", (double)candidateThreshold);
				}

				OLED_ShowString(2, 1, oledText);
				OLED_ShowString(3, 1, "Turn pot to edit");
				OLED_ShowString(4, 1, "Press to confirm");
			}
			

			//sprintf(oledText, "yuzhi:%.3fV", (double)potVmaxchange);
			//OLED_ShowString(4, 1, oledText);
			
			//串口打印
			sprintf(buffer, "Light=%.3fV, Pot=%.3fV, Level=%s\r\n",(double)V, (double)potV, level);
			
			HAL_UART_Transmit(&huart1, (uint8_t *)buffer,strlen(buffer), 100);
			snprintf(buffer, sizeof(buffer), "W=%lus, M=%lus, S=%lus\r\n",(unsigned long)(weakMs / 1000U),(unsigned long)(mediumMs / 1000U),(unsigned long)(strongMs / 1000U));

			HAL_UART_Transmit(&huart1, (uint8_t *)buffer,strlen(buffer), 100);
			
            if (rtcReadOk && clockValid)
            {
                snprintf(buffer, sizeof(buffer), "20%02u-%02u-%02u %02u:%02u:%02u\r\n",
                         (unsigned int)rtcDate.Year, (unsigned int)rtcDate.Month,
                         (unsigned int)rtcDate.Date, (unsigned int)rtcTime.Hours,
                         (unsigned int)rtcTime.Minutes, (unsigned int)rtcTime.Seconds);
                HAL_UART_Transmit(&huart1, (uint8_t *)buffer, strlen(buffer), 100);
            }

		
		}
		
	}

	
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE; // 闹钟只走内部中断，不占PC13输出
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 799;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 199;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB10 PB11 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// RTC闹钟到时后由HAL调用：这里只通知主循环，不打印、不操作OLED
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc)
{
    if (rtc->Instance == RTC)
    {
        rtcAlarmPending = 1; // 到时通知，后续在主循环执行记录启停
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
