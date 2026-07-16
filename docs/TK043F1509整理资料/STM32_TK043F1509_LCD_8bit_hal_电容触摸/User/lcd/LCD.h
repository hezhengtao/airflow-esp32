#include "stm32f1xx.h"

#define XSIZE_PHYS 800
#define YSIZE_PHYS 480

#define Bank1_LCD_D    ((u32)0x60010000)    //Disp Data ADDR
#define Bank1_LCD_C    ((u32)0x60000000)	  //Disp Reg ADDR

#define Set_Rst GPIOD->BSRR = GPIO_PIN_13;
#define Clr_Rst GPIOD->BRR  = GPIO_PIN_13;

#define Lcd_Light_ON   GPIOA->BSRR = GPIO_PIN_1;
#define Lcd_Light_OFF  GPIOA->BRR  = GPIO_PIN_1;

typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

//*************  24位色（1600万色）定义 *************//
//#define WHITE          0xFFFFFF
//#define BLACK          0x000000
//#define BLUE           0x0000FF
//#define BLUE2          0x3F3FFF
//#define RED            0xFF0000
//#define MAGENTA        0xFF00FF
//#define GREEN          0x00FF00
//#define CYAN           0x00FFFF
//#define YELLOW         0xFFFF00

//*************  18位色（26万色）定义 *************//
//#define White          0x3FFFF
//#define Black          0x00000
//#define Blue           0x3F000
//#define Blue2          0x3F3CF
//#define Red            0x0003F
//#define Magenta        0x3F03F
//#define Green          0x0FFC0
//#define Cyan           0x3FFC0
//#define Yellow         0x0FFFF						

////*************  16位色定义 *************//
#define WHITE						0xFFFF
#define BLACK						0x0000	  
#define BLUE						0x001F  
#define BRED						0XF81F
#define GRED						0XFFE0
#define GBLUE						0X07FF
#define RED							0xF800
#define MAGENTA					0xF81F
#define GREEN						0x07E0
#define CYAN						0x7FFF
#define YELLOW					0xFFE0
#define BROWN						0XBC40 //棕色
#define BRRED						0XFC07 //棕红色
#define GRAY						0X8430 //灰色

//Lcd初始化及其低级控制函数
void LCD_delay(volatile u32 time);
void Lcd_Initialize(void);
//Lcd高级控制函数
void Lcd_ColorBox(u16 x,u16 y,u16 xLong,u16 yLong,u16 Color);
void DrawPixel(u16 x, u16 y, u16 Color);
void draw_Bat(u16 x,u16 y,u8 power_per);
void LCD_Fill_Pic(u16 x, u16 y,u16 pic_H, u16 pic_V, const unsigned char* pic);
void BlockWrite(unsigned int Xstart,unsigned int Xend,unsigned int Ystart,unsigned int Yend);
void LCD_PutString(unsigned short x, unsigned short y, char *s, unsigned int fColor, unsigned int bColor,unsigned char flag);
