/****************************************Copyright (c)****************************************************
** 
**                                      
**
**--------------File Info---------------------------------------------------------------------------------
** File name:			main.c
** modified Date:  		2019-8-12
** Last Version:		V0.1
** Descriptions:		  main 函数调用
** Author : xiao chen
** Historical Version :
** 好钜润科技，芯片事业部----深圳龙华应用分部
*********************************************************************************************************/
#include "main.h"
#include "LCD.h"
#include "SPI.h"
#include "MM_T035.h"
#include "stdio.h"
/********************************************************************************************************
**函数信息 ：int main (void)                          
**功能描述 ：无
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/

int main(void)
{	
  RemapVtorTable();
	SystemClk_HSEInit(RCC_PLLMul_20);//启动PLL时钟，12MHz*20=240MHz
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//2：2，全局性函数，仅需设置一次
	UartInit(UART1,460800);      //配置串口1，波特率为460800
	printf(" Welcome to use HJR TK499! \r\n");
				
	TIM8_Config(3000,24000);     //配置定时器8，在定时器中断里闪灯，0.3秒中断一次

	LCD_Initial();               //LCD初始化函数
	
	Lcd_ColorBox(0,0,800,480,BLUE);//用蓝色清屏
	printf(" Welcome to use HJR TK499! \r\n");
	LCD_PutString(80,20,"型号：TK043F1508",RED,YELLOW,0);
	LCD_PutString(10,60,"Welcome to use HJR TK499 and SPI LCD!",RED,YELLOW,1);
	LCD_PutString(10,80,"深圳市好钜润科技有限公司",RED,YELLOW,1);
	LCD_PutString(10,100,"电话：0755-21006150",RED,YELLOW,1);
	

	LCD_Fill_Pic(480, 0,320, 480, (u32*)gImage_MM_T035);//用DMA模式 显示一幅图片	

	
while(1)//无限循环
	{	
	}
}


