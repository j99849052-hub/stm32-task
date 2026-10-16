#include "stm32f10x.h"

void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOF,&GPIO_InitStructure);
	GPIO_SetBits(GPIOF,GPIO_Pin_0|GPIO_Pin_1);
}

void LED_ON(void)
{
	GPIO_ResetBits(GPIOF,GPIO_Pin_0);
}

void LED_OFF(void)
{
	GPIO_SetBits(GPIOF,GPIO_Pin_0);
}	
	
void LED_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOF,GPIO_Pin_0)==0)//如果此时读取PF0输出电平为0，即灯亮，那么置1
	{
		GPIO_SetBits(GPIOF,GPIO_Pin_0);
	}
	else
	{
		GPIO_ResetBits(GPIOF,GPIO_Pin_0);
	}
		
}	

void LED2_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOF,GPIO_Pin_1)==0)//如果此时读取PF0输出电平为0，即灯亮，那么置1
	{
		GPIO_SetBits(GPIOF,GPIO_Pin_1);
	}
	else
	{
		GPIO_ResetBits(GPIOF,GPIO_Pin_1);
	}
		
}	



void LED2_ON(void)
{
	GPIO_ResetBits(GPIOF,GPIO_Pin_1);
}

void LED2_OFF(void)
{
	GPIO_SetBits(GPIOF,GPIO_Pin_1);
}	
	