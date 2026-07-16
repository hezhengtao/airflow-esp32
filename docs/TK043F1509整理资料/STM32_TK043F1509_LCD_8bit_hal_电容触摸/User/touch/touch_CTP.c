#include "touch_CTP.h"
#include <stdio.h>
#include "LCD.h"


void GUI_TOUCH_X_ActivateX(void) {}
void GUI_TOUCH_X_ActivateY(void) {}

#define  IIC_delay  1
/****************************************************************************************
																	电容屏 I2C 底层驱动

SDA一般只有在SCL位低电平的时候才能变化，因为起始，停止信号是在SCL为高的时候SDA发生变化，为了区别，所以一般在写入/读出数据时只能在SCL为低时SDA发生变化。这点在编程的时候要特别注意
	1.IIC总线空闲的时候，SDA和SCL都为高电平，所以硬件电路中一般在SDA信号线上接一个电阻后再接VCC，注意，这里由于只工作在主机模式，没涉及切换，所以SCK直是输出模式，不用串电阻。
	
	   应答信号分为主机应答 和从机应答 ，这两者有着很大的区别。主机应答是在主机从从机中读取数据时每次读取完一个字节的数据后主机给从机的一个应答信号，表示主机已收到数据了。
	而从机应答是指主机给从机发送数据时从机给主机的应答，给一个应答就代表从机已经收到了数据，为主机接下来的工作做个判断。主机在核查从机的应答信号的时候，必须先将SDA总线拉高，释放总线。
     非应答信号是主机给从机的，当读取完一字节数据以后，主机不再去读取数据就给从机一个非应答信号，接着一个停止信号，直接给停止信号也是可以结束此次读操作，但是会对后面的操作带来影响。
	   
	所以不能图省事，养成好的习惯，非应答信号在需要的时候必须给。
	
	低电平变化，高电平稳定，所以SCL为高电平时读取SDA的数据
***************************************************************************************/
	
//*************************************************
//函数名称 : void Touch_GPIO_Config(void)  
//功能描述 : 设置触屏的IIC引脚,用软件模拟的方法实现IIC功能

//*************************************************
void Touch_GPIO_Config(void) 
{
	GPIO_InitTypeDef GPIO_Initure;

	__HAL_RCC_GPIOB_CLK_ENABLE();//Enable GPIOs clock
	
	/* Common GPIO configuration */
	
  GPIO_Initure.Mode      = GPIO_MODE_OUTPUT_PP;  //推挽输出
  GPIO_Initure.Pull      = GPIO_PULLUP;
  GPIO_Initure.Speed     = GPIO_SPEED_FREQ_HIGH;
	/* config IIC SCK*/
	GPIO_Initure.Pin = GPIO_PIN_13;		
	HAL_GPIO_Init(GPIOB, &GPIO_Initure);
	
	GPIO_Initure.Mode      = GPIO_MODE_OUTPUT_OD;  //开漏输出
  GPIO_Initure.Pull      = GPIO_PULLUP;
  GPIO_Initure.Speed     = GPIO_SPEED_FREQ_HIGH;
	/* config IIC SDA */
	GPIO_Initure.Pin = GPIO_PIN_15;		
	HAL_GPIO_Init(GPIOB, &GPIO_Initure);
	

	I2C1_SCL(1);
	I2C1_SDA(1);
	I2C1_Stop();
}
void I2C1_Delay_us(u16 cnt)
{
	volatile	u16 i;
	for(i=0;i<cnt;i++);
}

//void EXTI4_IRQHandler(void)
//{
//	if(EXTI_GetITStatus(EXTI_Line4) != RESET)
//	{
//		EXTI->PR = EXTI_Line4;  				// 清除中断标示
//    keyId = 1;
//	}
//}
int SDA_read_Bit(void)
{
	int a;
	
	a=GPIOB->IDR & GPIO_PIN_15;
	
	return a;
}
/************************************************
*	函 数 名: I2C_Start
*	功能说明: CPU发起I2C总线启动信号
在SCK高电平期间，SDA一个下降沿，即构成了IIC的起始信号
//START:when CLK is high,DATA change form high to low
**************************************************/ 
void I2C1_Start(void)  
{ 
	I2C1_SDA(1);
	I2C1_SCL(1);
	I2C1_Delay_us(IIC_delay*5);
	I2C1_SDA(0);
	I2C1_Delay_us(IIC_delay);
	I2C1_SCL(0); //钳住I2C总线，准备发送或接收数据
} 

/****************************************************
*	函 数 名: I2C_Stop
*	功能说明: CPU发起I2C总线停止信号
在SCK高电平期间，SDA一个上升沿，即构成了IIC的终止信号
//STOP:when CLK is high DATA change form low to high
****************************************************/
void I2C1_Stop(void)  
{ 

	I2C1_SDA(0);
	I2C1_SCL(0);
	I2C1_Delay_us(IIC_delay);
	I2C1_SDA(1);
	I2C1_SCL(1);
	I2C1_Delay_us(IIC_delay);
}

/************************************** 
*	函 数 名: I2C_Ack
*	功能说明: CPU产生一个ACK信号
*	形    参：无
*	返 回 值: 无 
**************************************/
void I2C1_Ack(void) 
{ 
	I2C1_SCL(0);
	I2C1_SDA(0);
	I2C1_Delay_us(IIC_delay);
	I2C1_SCL(1);
	I2C1_Delay_us(IIC_delay);
	I2C1_SCL(0);
} 
/*
*************************************************
*	函 数 名: I2C_NoAck
*	功能说明: CPU产生1个NACK信号
*	形    参：无
*	返 回 值: 无
*************************************************
*/
void I2C1_NoAck(void)
{
	I2C1_SCL(0);
	I2C1_SDA(1);
	I2C1_Delay_us(IIC_delay);
	I2C1_SCL(1);
	I2C1_Delay_us(IIC_delay);
	I2C1_SCL(0);	
}
/*************************************************************
*	函 数 名: I2C_WaitAck
*	功能说明: CPU产生一个时钟，并读取器件的ACK应答信号
*	形    参：无
*	返 回 值: 返回0表示正确应答，1表示无器件响应
*************************************************************/
uint8_t I2C1_WaitAck(void)
{ 
	__IO uint16_t t = 0;
	I2C1_SDA(1);
	I2C1_Delay_us(IIC_delay);	
	I2C1_SCL(1);	
	I2C1_Delay_us(IIC_delay);	
	while(I2C1_SDA_Read())
	{
		t++;
		if(t>50)
		{
			I2C1_Stop();
			return 1;
		}	
	}
	I2C1_SCL(0);
	return 0; 
}

void I2C1_Send_Byte(uint8_t dat)
{
	__IO uint8_t i;
	I2C1_SCL(0);
	for(i=0; i<8; i++)
	{		
		if(dat & 0x80)
		{
			I2C1_SDA(1);
		}
		else
		{
			I2C1_SDA(0);
		}
		I2C1_Delay_us(IIC_delay);
		I2C1_SCL(1);	
		I2C1_Delay_us(IIC_delay);
		I2C1_SCL(0);
		I2C1_Delay_us(IIC_delay);
		dat <<= 1;	
	}
}

/************************************************
*	函 数 名: I2C_Read_Byte
*	功能说明: CPU从I2C总线设备读取8bit数据
*	形    参：无
*	返 回 值: 读到的数据
*************************************************/
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
uint8_t I2C1_Read_Byte(uint8_t ack)
{
	
	unsigned char i,receive=0;
	I2C1_SDA(1);
  for(i=0;i<8;i++ )
	{
		I2C1_SCL(0); 
    I2C1_Delay_us(IIC_delay*6);
		I2C1_SCL(1);
    receive<<=1;
    if(I2C1_SDA_Read())receive++;   
		I2C1_Delay_us(IIC_delay*6); 
   }		

   if (!ack)  I2C1_NoAck();//发送nACK
   else       I2C1_Ack(); //发送ACK   
    
	 return receive;
}



/****************************************************************************************
																	电容屏 FT6336芯片驱动

***************************************************************************************/


uint8_t FT6206_Read_Reg(uint8_t *pbuf,uint32_t len)
{
	
	int8_t i=0;

	I2C1_Start();
	I2C1_Send_Byte(FT6206_ADDR);
	I2C1_WaitAck();	
	
	I2C1_Send_Byte(0);
	I2C1_WaitAck();	
  I2C1_Stop();
  
	I2C1_Start();
	I2C1_Send_Byte(FT6206_ADDR+1);
	I2C1_WaitAck();	
	
	for(i=0;i<len;i++)
	{
		if(i==(len-1))  *(pbuf+i)=I2C1_Read_Byte(0);
		else            *(pbuf+i)=I2C1_Read_Byte(1);
	}		
	I2C1_Stop();
  
	return 0;
}

u8 buf[33];

void Touch_Test(void)
{
	char i;

	u16 touchX=0,touchY=0;
	FT6206_Read_Reg((uint8_t*)&buf, 32);//一般小屏读一个点就够了，节省资源，这里写32,把5个点都读出来
	i = buf[2]&0x0f;
	if((i<1)) return ;//当小于1个点时，也就是说没有触摸点，则直接返回
	
		for(i=0;i<5;i++)
	{
		
		touchX = (int16_t)(buf[5+6*i] & 0x0F)<<8 | (int16_t)buf[6+6*i];//x坐标
		touchY = (int16_t)(buf[3+6*i] & 0x0F)<<8 | (int16_t)buf[4+6*i];//y坐标
		
//		if((touchX>0)&&(touchX<=480))
		{
			switch(i) //分别显示5点触摸
			{
				case 0 :
					Lcd_ColorBox(touchX,480-touchY,2,2,RED);
					break ;
				case 1 :
					Lcd_ColorBox(touchX,480-touchY,2,2,GREEN);
					break ;
				case 2 :
					Lcd_ColorBox(touchX,480-touchY,2,2,BLUE);
					break ;
				case 3 :
					Lcd_ColorBox(touchX,480-touchY,2,2,YELLOW);
					break ;
				case 4 :
					Lcd_ColorBox(touchX,480-touchY,2,2,MAGENTA);
					break ;
				default  :
					break ;
				
			}

//		printf("touchX= %d \r\n",touchX);
//		printf("touchY= %d \r\n",touchY);
		}
	}

}



