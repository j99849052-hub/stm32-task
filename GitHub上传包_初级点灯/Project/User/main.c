#include "stm32f10x.h"                  // Device header

int main(void)
{	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);//时钟的设置
	GPIO_InitTypeDef GPIO_InitStructure;//设置结构体变量
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;//推挽输出的模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_5;//引脚数
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;//设置输出的速度
	GPIO_Init(GPIOB,&GPIO_InitStructure);//由于GPIOInit第二个参数是结构体指针，故取址
	GPIO_SetBits(GPIOB,GPIO_Pin_5);//Setbit是高电平
	//GPIO_ResetBits(GPIOB,GPIO_Pin_5);//Resetbit低电平
	
}
