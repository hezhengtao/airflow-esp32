/**
  ******************************************************************************
  * @file    main.c
  * @author  pygeo
  * @version V4.3
  * @date    2023-11-13
  * @brief   LCD display
  ******************************************************************************
  * @attention
  *
  * 应用平台:Tiky STM32F103 评估板 
  * 官网    :http://www.hjrkj.com
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx.h"
#include <stdlib.h>
#include "LCD.h"
#include "icon_128x128.h"
#include "touch_CTP.h"


int main(void)
{
  /* 系统时钟初始化成72 MHz */
  SystemClock_Config();

	Lcd_Initialize();   //LCD初始化
	Touch_GPIO_Config();//电容触摸引脚初始化
	
	Lcd_ColorBox(0,0,XSIZE_PHYS,YSIZE_PHYS,BLUE);   //全屏填充蓝色
	draw_Bat(750,10,3);  //电池电量图标
	LCD_Fill_Pic(300,160,128,128, (unsigned short int*)gImage_icon_128x128);  //显示一张图片
	
	LCD_PutString(50,20,"好钜润 www.hjrkj.com ",YELLOW,RED,1);
	LCD_PutString(50,40,"4.3inch TK043F1509 LCD",YELLOW,RED,0);
	LCD_PutString(50,60,"TEL: 0755-21006150",YELLOW,RED,0);
	LCD_PutString(50,80,"Hello world!",YELLOW,RED,0);
	LCD_PutString(50,120,"LCD Parallel mode show",YELLOW,RED,0);
	
	Lcd_ColorBox(200,150,20,20,YELLOW);  //画一个小方块
	Lcd_ColorBox(250,120,20,20,YELLOW);


	    
	while(1)
	{
		Touch_Test();//电容触摸测试函数，支持5五点触摸
		if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0))  Lcd_ColorBox(0,0,XSIZE_PHYS,YSIZE_PHYS,BLACK);//如果按下中键(PA0)，则清屏
	}	
}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 72000000
  *            HCLK(Hz)                       = 72000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            HSE PREDIV1                    = 1
  *            PLLMUL                         = 9
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
  oscinitstruct.HSEState        = RCC_HSE_ON;
  oscinitstruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
