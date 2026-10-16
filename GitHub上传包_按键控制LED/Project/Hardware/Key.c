#include "stm32f10x.h"
#include "Delay.h"

void Key_Init()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE,ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOE,&GPIO_InitStructure);
}	

uint8_t Key_GetNum()  //也即按一次，KEYNUM赋值为1，按两次KeyNUM赋值为2，利用1开灯，2关灯
{
	uint8_t KeyNum = 0;
	if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_0)==0)//一开始上拉输入设为1，不进入if，一按就进入if，然后按紧while，松开进入赋值
	{
		Delay_ms(20);//进行按键消抖处理
		while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_0)==0); //当按下不松开时，会一直卡在这一步while而不会进入下一步
		Delay_ms(20);//按键消抖
		KeyNum = 1;//即当按键按下时消抖后，若PE0为低电平，则卡在上面那步while，使KeyNum等于0且不会继续，当按下后KeyNum等于1
	}
	//当上面那一步实现赋值1才会进行这一步，也即按键按下了一次，灯开才会开始这一步
	if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_6)==0)
	{
		Delay_ms(20);//进行按键消抖处理
		while(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_6)==0); //当PE0为低电平时，会一直卡在这一步while而不会进入下一步
		Delay_ms(20);//按键消抖
		KeyNum = 2; //当按下两次了，则对KeyNum赋值为2
	}	
	return KeyNum;
}		