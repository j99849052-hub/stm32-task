#include "stm32f10x.h"

void PWM_Init()
{
	//初始化并使能TIM2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	TIM_InternalClockConfig(TIM2);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1; //clock division时钟分频器，设置一个时钟频率用来多个滤波器采样，当采样都相同说明电平稳定，才输出频率越低采样越准，但延迟高
	TIM_TimeBaseInitStructure.TIM_CounterMode =TIM_CounterMode_Up ;
	TIM_TimeBaseInitStructure.TIM_Period = 100-1;//周期就是ARR自动重装器的值，以下三个是时基单元的重要函数
	TIM_TimeBaseInitStructure.TIM_Prescaler = 720-1;//预分频器的值  //利用公式 72MHz/(PSC+1)/(ARR+1)
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter =0 ;//重复计时器的值，赋值N，则计数器溢出N+1次，才产生一次更新事件（不过TIM2通用定时器无此功能
	
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);
	TIM_Cmd(TIM2,ENABLE);
	
	
	
	TIM_OCInitTypeDef TIM_OCInitStructure; //部分结构体属性不需要使用，故使用StructureInit给他们赋初始值
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;   //设置输出比较模式，像是冻结，强制有无效电平，以及PWM1，2模式
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;//设置有效电平为高电平or低电平
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;//输出状态使能,即允许TIM输出
	TIM_OCInitStructure.TIM_Pulse =0;  //脉冲，但事实上用于设置CCR的值，用于控制占空比的主要参数
	TIM_OC1Init(TIM2,&TIM_OCInitStructure);
	
	//判断OC输出是借用哪个Pin的复用功能，去配置那个引脚，发现TIM2的输出CH是在PA0，故用PA0输出PWM
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
}

//上方封装函数实现改变CCR的值改变占空比，下方则通过Setcompare函数实现CCR的动态变化，实现呼吸灯的效果
void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM2,Compare);
}



