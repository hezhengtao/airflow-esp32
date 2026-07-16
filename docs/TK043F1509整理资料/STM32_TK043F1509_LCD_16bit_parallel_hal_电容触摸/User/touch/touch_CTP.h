
#ifndef  __TOUCH_CTP_H__
#define  __TOUCH_CTP_H__
#include "stm32f1xx.h"
#include "stdio.h"



#define I2C1_SCL(x) 	\
						if (x)	\
						GPIOB->BSRR = GPIO_PIN_13;	\
						else		\
						GPIOB->BRR  = GPIO_PIN_13;
						
#define I2C1_SDA(x)	\
						if (x)	\
						GPIOB->BSRR = GPIO_PIN_15;	\
						else		\
						GPIOB->BRR  = GPIO_PIN_15;

#define	I2C1_SDA_Read()  	  	SDA_read_Bit()   
						
#define FT6206_ADDR		0x70		//地址为0x38要移一位 
						
void Touch_GPIO_Config(void); 
int SDA_read_Bit(void);
void I2C1_Start(void);
void I2C1_Stop(void);
void I2C1_Ack(void);
void I2C1_NoAck(void);

void I2C1_Send_Byte(uint8_t dat);
uint8_t I2C1_Read_Byte(uint8_t ack);
uint8_t I2C1_WaitAck(void);


static uint8_t FT6206_Write_Reg(uint8_t startaddr,uint8_t *pbuf,uint32_t len);
uint8_t FT6206_Read_Reg(uint8_t *pbuf,uint32_t len);

int GUI_TOUCH_X_MeasureX(void); 
int GUI_TOUCH_X_MeasureY(void);

void Touch_Test(void);

#endif                                     

