#include "HAL_conf.h"

#define SPI_CS(a)	\
						if (a)	\
						GPIOA->BSRR = GPIO_Pin_4;	\
						else		\
						GPIOA->BRR = GPIO_Pin_4;
#define SPI_DCLK(a)	\
						if (a)	\
						GPIOA->BSRR = GPIO_Pin_5;	\
						else		\
						GPIOA->BRR = GPIO_Pin_5;
#define SPI_SDA(a)	\
						if (a)	\
						GPIOA->BSRR = GPIO_Pin_7;	\
						else		\
						GPIOA->BRR = GPIO_Pin_7;
#define lcd_RS(a)	\
						if (a)	\
						GPIOA->BSRR = GPIO_Pin_3;	\
						else		\
						GPIOA->BRR = GPIO_Pin_3;
#define Set_Rst GPIOD->BSRR = GPIO_Pin_6
#define Clr_Rst GPIOD->BRR  = GPIO_Pin_6

//*************  24位色（1600万色）定义 *************//
#define WHITE          0xFFFFFF
#define BLACK          0x000000
#define BLUE           0x0000FF
#define BLUE2          0x3F3FFF
#define RED            0xFF0000
#define MAGENTA        0xFF00FF
#define GREEN          0x00FF00
#define CYAN           0x00FFFF
#define YELLOW         0xFFFF00						

//#define WHITE						0xFFFF
//#define BLACK						0x0000	  
//#define BLUE						0x001F  
//#define BRED						0XF81F
//#define GRED						0XFFE0
//#define GBLUE						0X07FF
//#define RED							0xF800
//#define MAGENTA						0xF81F
//#define GREEN						0x07E0
//#define CYAN						0x7FFF
//#define YELLOW						0xFFE0
//#define BROWN						0XBC40 //棕色
//#define BRRED						0XFC07 //棕红色
//#define GRAY						0X8430 //灰色
						
						

void LCD_Initial(void); //LCD初始化函数
void LCD_delay(int time);
void WriteComm(unsigned int CMD);
void WriteData(u32 dat);
void LCD_WR_REG(u16 Index,u16 CongfigTemp);
void Lcd_ColorBox(u16 xStart,u16 yStart,u16 xLong,u16 yLong,u32 Color);
//void SPILCD_DrawLine(unsigned short x1,unsigned short y1,unsigned short x2,unsigned short y2,unsigned short color);
//void SPILCD_ShowChar(unsigned short x,unsigned short y,unsigned char num, unsigned int fColor, unsigned int bColor,unsigned char flag) ;
void LCD_PutString(unsigned short x, unsigned short y, char *s, unsigned int fColor, unsigned int bColor,unsigned char flag);
void LCD_Fill_Pic(u16 x, u16 y,u16 pic_H, u16 pic_V, u32* pic);
