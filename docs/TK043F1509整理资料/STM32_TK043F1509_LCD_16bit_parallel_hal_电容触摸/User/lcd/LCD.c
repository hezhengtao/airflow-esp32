#include "stm32f1xx.h"
#include "LCD.h"
#include <stdio.h>
#include "ASCII.h"
#include "GB1616.h"	//16*16汉字字模

/*
硬件平台：STM32F103VCT6评估板（最小系统板）
FSMC用到的引脚
* PD4-FSMC_NOE  : LCD-RD
* PD5-FSMC_NWE  : LCD-WR
* PD7-FSMC_NE1  : LCD-CS
* PD11-FSMC_A16 : LCD-DC( DC又名DCX或RS )
* FSMC-D0~D15   : PD 14 15 0 1,PE 7 8 9 10 11 12 13 14 15,PD 8 9 10   

另外两个液晶屏相关的引脚  RESET=PD13；BL_CTR=PA1（背光开关，Lcd_Light_ON、OFF）
*/
 void LCD_GPIO_Config(void)
{
  GPIO_InitTypeDef GPIO_Initure;
  
      /* Enable GPIOs clock */
	__HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_FSMC_CLK_ENABLE();			//使能FSMC时钟
  

  GPIO_Initure.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_Initure.Mode      = GPIO_MODE_AF_PP; 
	
/*   config tft data lines base on FSMC
	   data lines,FSMC-D0~D15: PD 14 15 0 1,PE 7 8 9 10 11 12 13 14 15,PD 8 9 10
 */	
  
	GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14|GPIO_PIN_15; 
  HAL_GPIO_Init(GPIOD, &GPIO_Initure);

	GPIO_Initure.Pin=  GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE,&GPIO_Initure);

	
	 /* config tft control lines base on FSMC
	 * PD4-FSMC_NOE  :LCD-RD
   * PD5-FSMC_NWE  :LCD-WR
	 * PD7-FSMC_NE1  :LCD-CS
   * PD11-FSMC_A16 :LCD-DC
	 */
    GPIO_Initure.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_7|GPIO_PIN_11; 
    HAL_GPIO_Init(GPIOD, &GPIO_Initure);


	    /* Common GPIO configuration */
  GPIO_Initure.Mode      = GPIO_MODE_OUTPUT_PP;  //推挽输出
  GPIO_Initure.Pull      = GPIO_PULLUP;
  GPIO_Initure.Speed     = GPIO_SPEED_FREQ_HIGH;

    /* config LCD rst gpio 液晶屏复位引脚*/
    GPIO_Initure.Pin = GPIO_PIN_13;		
    HAL_GPIO_Init(GPIOD, &GPIO_Initure);
    
    
		/* config LCD back_light gpio 背光开关 */
    GPIO_Initure.Pin = GPIO_PIN_1 ; 	 
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);  

		GPIO_Initure.Mode      = GPIO_MODE_INPUT;  //下拉输入
		GPIO_Initure.Pull      = GPIO_PULLDOWN;
		GPIO_Initure.Speed     = GPIO_SPEED_FREQ_HIGH;
    /* config center key“中心”  按键 */
    GPIO_Initure.Pin = GPIO_PIN_0;		
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);
}

static void LCD_FSMC_Config(void)       //FSMC 配置函数
{
	
	SRAM_HandleTypeDef  LCD_FSMC;
  FSMC_NORSRAM_TimingTypeDef Timing;
	LCD_FSMC.Instance = FSMC_NORSRAM_DEVICE;
  LCD_FSMC.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  
  /* SRAM device configuration */  
  Timing.AddressSetupTime      = 0x02;
  Timing.AddressHoldTime       = 0x00;
  Timing.DataSetupTime         = 0x05;
  Timing.BusTurnAroundDuration = 0x00;
  Timing.CLKDivision           = 0x00;
  Timing.DataLatency           = 0x00;
  Timing.AccessMode            = FSMC_ACCESS_MODE_B;
 
  LCD_FSMC.Init.NSBank=FSMC_NORSRAM_BANK1    ;     					        //使用NE1
	LCD_FSMC.Init.DataAddressMux=FSMC_DATA_ADDRESS_MUX_DISABLE; 	//地址/数据线不复用
	LCD_FSMC.Init.MemoryType=FSMC_MEMORY_TYPE_NOR;   				       //NOR
	LCD_FSMC.Init.MemoryDataWidth=FSMC_NORSRAM_MEM_BUS_WIDTH_16; 	//16位数据宽度
	LCD_FSMC.Init.BurstAccessMode=FSMC_BURST_ACCESS_MODE_DISABLE; //是否使能突发访问,仅对同步突发存储器有效,此处未用到
	LCD_FSMC.Init.WaitSignalPolarity=FSMC_WAIT_SIGNAL_POLARITY_LOW;//等待信号的极性,仅在突发模式访问下有用
	LCD_FSMC.Init.WaitSignalActive=FSMC_WAIT_TIMING_BEFORE_WS;   	//存储器是在等待周期之前的一个时钟周期还是等待周期期间使能NWAIT
	LCD_FSMC.Init.WriteOperation=FSMC_WRITE_OPERATION_ENABLE;    	//存储器写使能
	LCD_FSMC.Init.WaitSignal=FSMC_WAIT_SIGNAL_DISABLE;           	//等待使能位,此处未用到
	LCD_FSMC.Init.ExtendedMode=FSMC_EXTENDED_MODE_DISABLE;        	//读写使用相同的时序
	LCD_FSMC.Init.AsynchronousWait=FSMC_ASYNCHRONOUS_WAIT_DISABLE;	//是否使能同步传输模式下的等待信号,此处未用到
	LCD_FSMC.Init.WriteBurst=FSMC_WRITE_BURST_DISABLE;           	  //禁止突发写
  
  /* SRAM controller initialization */
  HAL_SRAM_Init(& LCD_FSMC, &Timing, &Timing);		
		
}

void LCD_delay(volatile u32 time)                  //延时函数
{
	volatile unsigned short i,j;
	for(i=0;i<time;i++)
		for(j=0;j<1000;j++)	;
}  
  
static void LCD_Rst(void)                 //液晶屏复位
{			
    Clr_Rst;
    LCD_delay(500);					   
    Set_Rst;		 	 
    LCD_delay(500);	
}
static void WriteComm(u16 CMD)            //写命令
{			
	*(__IO u16 *) (Bank1_LCD_C) = CMD;
}
static void WriteData(u16 tem_data)       //写数据
{			
	*(__IO u16 *) (Bank1_LCD_D) = tem_data;
}

/**********************************************
Lcd初始化函数
***********************************************/
void Lcd_Initialize(void)
{	
LCD_GPIO_Config();
LCD_FSMC_Config();
LCD_Rst();
	
WriteComm(0xFF00);WriteData(0x00AA); 
WriteComm(0xFF01);WriteData(0x0055); 
WriteComm(0xFF02);WriteData(0x00A5); 
WriteComm(0xFF03);WriteData(0x0080); 

WriteComm(0xF200);WriteData(0x0000); 
WriteComm(0xF201);WriteData(0x0084); 
WriteComm(0xF202);WriteData(0x0008);

WriteComm(0x6F00);WriteData(0x000A); 
WriteComm(0xF400);WriteData(0x0013);

WriteComm(0xFF00);WriteData(0x00AA); 
WriteComm(0xFF01);WriteData(0x0055); 
WriteComm(0xFF02);WriteData(0x00A5); 
WriteComm(0xFF03);WriteData(0x0000);

//Enable Page 1
WriteComm(0xF000);WriteData(0x0055); 
WriteComm(0xF001);WriteData(0x00AA); 
WriteComm(0xF002);WriteData(0x0052); 
WriteComm(0xF003);WriteData(0x0008);
WriteComm(0xF004);WriteData(0x0001);

//AVDD Set AVDD 5.2V
WriteComm(0xB000);WriteData(0x000C); 
WriteComm(0xB001);WriteData(0x000C);
WriteComm(0xB002);WriteData(0x000C);

//AVDD ratio
WriteComm(0xB600);WriteData(0x0046); 
WriteComm(0xB601);WriteData(0x0046);
WriteComm(0xB602);WriteData(0x0046); 

//AVEE  -5.2V
WriteComm(0xB100);WriteData(0x000C); 
WriteComm(0xB101);WriteData(0x000C);
WriteComm(0xB102);WriteData(0x000C);

//AVEE ratio
WriteComm(0xB700);WriteData(0x0026); 
WriteComm(0xB701);WriteData(0x0026); 
WriteComm(0xB702);WriteData(0x0026); 

//VCL  -2.5V   
WriteComm(0xB200);WriteData(0x0000); 
WriteComm(0xB201);WriteData(0x0000);
WriteComm(0xB202);WriteData(0x0000);

//VCL ratio
WriteComm(0xB800);WriteData(0x0034); 
WriteComm(0xB801);WriteData(0x0034);
WriteComm(0xB802);WriteData(0x0034);

//VGH 15V  (Free pump) 
WriteComm(0xBF00);WriteData(0x0001);
WriteComm(0xB300);WriteData(0x0008);
WriteComm(0xB301);WriteData(0x0008); 
WriteComm(0xB302);WriteData(0x0008);  

//VGH ratio
WriteComm(0xB900);WriteData(0x0026); 
WriteComm(0xB901);WriteData(0x0026);
WriteComm(0xB902);WriteData(0x0026); 

//VGL_REG -10V 
WriteComm(0xB500);WriteData(0x0008);
WriteComm(0xB501);WriteData(0x0008);
WriteComm(0xB502);WriteData(0x0008);

//WriteComm(0xC200);WriteData(0x0003);

//VGLX ratio
WriteComm(0xBA00);WriteData(0x0036); 
WriteComm(0xBA01);WriteData(0x0036); 
WriteComm(0xBA02);WriteData(0x0036); 

//VGMP/VGSP 4.5V/0V
WriteComm(0xBC00);WriteData(0x0000); 
WriteComm(0xBC01);WriteData(0x0080); 
WriteComm(0xBC02);WriteData(0x0000);

//VGMN/VGSN -4.5V/0V
WriteComm(0xBD00);WriteData(0x0000); 
WriteComm(0xBD01);WriteData(0x0080); 
WriteComm(0xBD02);WriteData(0x0000);

//VCOM 
WriteComm(0xBE00);WriteData(0x0000); 
WriteComm(0xBE01);WriteData(0x0055); // 6A

//Gamma Setting
WriteComm(0xD100);WriteData(0x0000); 
WriteComm(0xD101);WriteData(0x0001); 
WriteComm(0xD102);WriteData(0x0000); 
WriteComm(0xD103);WriteData(0x001C); 
WriteComm(0xD104);WriteData(0x0000); 
WriteComm(0xD105);WriteData(0x004E); 
WriteComm(0xD106);WriteData(0x0000); 
WriteComm(0xD107);WriteData(0x006A); 
WriteComm(0xD108);WriteData(0x0000); 
WriteComm(0xD109);WriteData(0x0085); 
WriteComm(0xD10A);WriteData(0x0000); 
WriteComm(0xD10B);WriteData(0x00AB); 
WriteComm(0xD10C);WriteData(0x0000); 
WriteComm(0xD10D);WriteData(0x00C4); 
WriteComm(0xD10E);WriteData(0x0000); 
WriteComm(0xD10F);WriteData(0x00FC); 
WriteComm(0xD110);WriteData(0x0001); 
WriteComm(0xD111);WriteData(0x0023); 
WriteComm(0xD112);WriteData(0x0001); 
WriteComm(0xD113);WriteData(0x0061); 
WriteComm(0xD114);WriteData(0x0001); 
WriteComm(0xD115);WriteData(0x0094); 
WriteComm(0xD116);WriteData(0x0001); 
WriteComm(0xD117);WriteData(0x00E4); 
WriteComm(0xD118);WriteData(0x0002); 
WriteComm(0xD119);WriteData(0x0027); 
WriteComm(0xD11A);WriteData(0x0002); 
WriteComm(0xD11B);WriteData(0x0029); 
WriteComm(0xD11C);WriteData(0x0002); 
WriteComm(0xD11D);WriteData(0x0065); 
WriteComm(0xD11E);WriteData(0x0002);
WriteComm(0xD11F);WriteData(0x00A6);  
WriteComm(0xD120);WriteData(0x0002); 
WriteComm(0xD121);WriteData(0x00CA); 
WriteComm(0xD122);WriteData(0x0002); 
WriteComm(0xD123);WriteData(0x00FD); 
WriteComm(0xD124);WriteData(0x0003); 
WriteComm(0xD125);WriteData(0x001D); 
WriteComm(0xD126);WriteData(0x0003); 
WriteComm(0xD127);WriteData(0x004D); 
WriteComm(0xD128);WriteData(0x0003); 
WriteComm(0xD129);WriteData(0x006A); 
WriteComm(0xD12A);WriteData(0x0003); 
WriteComm(0xD12B);WriteData(0x0095); 
WriteComm(0xD12C);WriteData(0x0003); 
WriteComm(0xD12D);WriteData(0x00AC); 
WriteComm(0xD12E);WriteData(0x0003); 
WriteComm(0xD12F);WriteData(0x00CB); 
WriteComm(0xD130);WriteData(0x0003); 
WriteComm(0xD131);WriteData(0x00EA); 
WriteComm(0xD132);WriteData(0x0003); 
WriteComm(0xD133);WriteData(0x00EF); 

WriteComm(0xD200);WriteData(0x0000); 
WriteComm(0xD201);WriteData(0x0001); 
WriteComm(0xD202);WriteData(0x0000); 
WriteComm(0xD203);WriteData(0x001C); 
WriteComm(0xD204);WriteData(0x0000); 
WriteComm(0xD205);WriteData(0x004E); 
WriteComm(0xD206);WriteData(0x0000); 
WriteComm(0xD207);WriteData(0x006A); 
WriteComm(0xD208);WriteData(0x0000); 
WriteComm(0xD209);WriteData(0x0085); 
WriteComm(0xD20A);WriteData(0x0000); 
WriteComm(0xD20B);WriteData(0x00AB); 
WriteComm(0xD20C);WriteData(0x0000); 
WriteComm(0xD20D);WriteData(0x00C4); 
WriteComm(0xD20E);WriteData(0x0000); 
WriteComm(0xD20F);WriteData(0x00FC); 
WriteComm(0xD210);WriteData(0x0001); 
WriteComm(0xD211);WriteData(0x0023); 
WriteComm(0xD212);WriteData(0x0001); 
WriteComm(0xD213);WriteData(0x0061); 
WriteComm(0xD214);WriteData(0x0001); 
WriteComm(0xD215);WriteData(0x0094); 
WriteComm(0xD216);WriteData(0x0001); 
WriteComm(0xD217);WriteData(0x00E4); 
WriteComm(0xD218);WriteData(0x0002); 
WriteComm(0xD219);WriteData(0x0027); 
WriteComm(0xD21A);WriteData(0x0002); 
WriteComm(0xD21B);WriteData(0x0029); 
WriteComm(0xD21C);WriteData(0x0002); 
WriteComm(0xD21D);WriteData(0x0065); 
WriteComm(0xD21E);WriteData(0x0002);
WriteComm(0xD21F);WriteData(0x00A6);  
WriteComm(0xD220);WriteData(0x0002); 
WriteComm(0xD221);WriteData(0x00CA); 
WriteComm(0xD222);WriteData(0x0002); 
WriteComm(0xD223);WriteData(0x00FD); 
WriteComm(0xD224);WriteData(0x0003); 
WriteComm(0xD225);WriteData(0x001D); 
WriteComm(0xD226);WriteData(0x0003); 
WriteComm(0xD227);WriteData(0x004D); 
WriteComm(0xD228);WriteData(0x0003); 
WriteComm(0xD229);WriteData(0x006A); 
WriteComm(0xD22A);WriteData(0x0003); 
WriteComm(0xD22B);WriteData(0x0095); 
WriteComm(0xD22C);WriteData(0x0003); 
WriteComm(0xD22D);WriteData(0x00AC); 
WriteComm(0xD22E);WriteData(0x0003); 
WriteComm(0xD22F);WriteData(0x00CB); 
WriteComm(0xD230);WriteData(0x0003); 
WriteComm(0xD231);WriteData(0x00EA); 
WriteComm(0xD232);WriteData(0x0003); 
WriteComm(0xD233);WriteData(0x00EF); 

WriteComm(0xD300);WriteData(0x0000); 
WriteComm(0xD301);WriteData(0x0001); 
WriteComm(0xD302);WriteData(0x0000); 
WriteComm(0xD303);WriteData(0x001C); 
WriteComm(0xD304);WriteData(0x0000); 
WriteComm(0xD305);WriteData(0x004E); 
WriteComm(0xD306);WriteData(0x0000); 
WriteComm(0xD307);WriteData(0x006A); 
WriteComm(0xD308);WriteData(0x0000); 
WriteComm(0xD309);WriteData(0x0085); 
WriteComm(0xD30A);WriteData(0x0000); 
WriteComm(0xD30B);WriteData(0x00AB); 
WriteComm(0xD30C);WriteData(0x0000); 
WriteComm(0xD30D);WriteData(0x00C4); 
WriteComm(0xD30E);WriteData(0x0000); 
WriteComm(0xD30F);WriteData(0x00FC); 
WriteComm(0xD310);WriteData(0x0001); 
WriteComm(0xD311);WriteData(0x0023); 
WriteComm(0xD312);WriteData(0x0001); 
WriteComm(0xD313);WriteData(0x0061); 
WriteComm(0xD314);WriteData(0x0001); 
WriteComm(0xD315);WriteData(0x0094); 
WriteComm(0xD316);WriteData(0x0001); 
WriteComm(0xD317);WriteData(0x00E4); 
WriteComm(0xD318);WriteData(0x0002); 
WriteComm(0xD319);WriteData(0x0027); 
WriteComm(0xD31A);WriteData(0x0002); 
WriteComm(0xD31B);WriteData(0x0029); 
WriteComm(0xD31C);WriteData(0x0002); 
WriteComm(0xD31D);WriteData(0x0065); 
WriteComm(0xD31E);WriteData(0x0002);
WriteComm(0xD31F);WriteData(0x00A6);  
WriteComm(0xD320);WriteData(0x0002); 
WriteComm(0xD321);WriteData(0x00CA); 
WriteComm(0xD322);WriteData(0x0002); 
WriteComm(0xD323);WriteData(0x00FD); 
WriteComm(0xD324);WriteData(0x0003); 
WriteComm(0xD325);WriteData(0x001D); 
WriteComm(0xD326);WriteData(0x0003); 
WriteComm(0xD327);WriteData(0x004D); 
WriteComm(0xD328);WriteData(0x0003); 
WriteComm(0xD329);WriteData(0x006A); 
WriteComm(0xD32A);WriteData(0x0003); 
WriteComm(0xD32B);WriteData(0x0095); 
WriteComm(0xD32C);WriteData(0x0003); 
WriteComm(0xD32D);WriteData(0x00AC); 
WriteComm(0xD32E);WriteData(0x0003); 
WriteComm(0xD32F);WriteData(0x00CB); 
WriteComm(0xD330);WriteData(0x0003); 
WriteComm(0xD331);WriteData(0x00EA); 
WriteComm(0xD332);WriteData(0x0003); 
WriteComm(0xD333);WriteData(0x00EF); 

WriteComm(0xD400);WriteData(0x0000); 
WriteComm(0xD401);WriteData(0x0001); 
WriteComm(0xD402);WriteData(0x0000); 
WriteComm(0xD403);WriteData(0x001C); 
WriteComm(0xD404);WriteData(0x0000); 
WriteComm(0xD405);WriteData(0x004E); 
WriteComm(0xD406);WriteData(0x0000); 
WriteComm(0xD407);WriteData(0x006A); 
WriteComm(0xD408);WriteData(0x0000); 
WriteComm(0xD409);WriteData(0x0085); 
WriteComm(0xD40A);WriteData(0x0000); 
WriteComm(0xD40B);WriteData(0x00AB); 
WriteComm(0xD40C);WriteData(0x0000); 
WriteComm(0xD40D);WriteData(0x00C4); 
WriteComm(0xD40E);WriteData(0x0000); 
WriteComm(0xD40F);WriteData(0x00FC); 
WriteComm(0xD410);WriteData(0x0001); 
WriteComm(0xD411);WriteData(0x0023); 
WriteComm(0xD412);WriteData(0x0001); 
WriteComm(0xD413);WriteData(0x0061); 
WriteComm(0xD414);WriteData(0x0001); 
WriteComm(0xD415);WriteData(0x0094); 
WriteComm(0xD416);WriteData(0x0001); 
WriteComm(0xD417);WriteData(0x00E4); 
WriteComm(0xD418);WriteData(0x0002); 
WriteComm(0xD419);WriteData(0x0027); 
WriteComm(0xD41A);WriteData(0x0002); 
WriteComm(0xD41B);WriteData(0x0029); 
WriteComm(0xD41C);WriteData(0x0002); 
WriteComm(0xD41D);WriteData(0x0065); 
WriteComm(0xD41E);WriteData(0x0002);
WriteComm(0xD41F);WriteData(0x00A6);  
WriteComm(0xD420);WriteData(0x0002); 
WriteComm(0xD421);WriteData(0x00CA); 
WriteComm(0xD422);WriteData(0x0002); 
WriteComm(0xD423);WriteData(0x00FD); 
WriteComm(0xD424);WriteData(0x0003); 
WriteComm(0xD425);WriteData(0x001D); 
WriteComm(0xD426);WriteData(0x0003); 
WriteComm(0xD427);WriteData(0x004D); 
WriteComm(0xD428);WriteData(0x0003); 
WriteComm(0xD429);WriteData(0x006A); 
WriteComm(0xD42A);WriteData(0x0003); 
WriteComm(0xD42B);WriteData(0x0095); 
WriteComm(0xD42C);WriteData(0x0003); 
WriteComm(0xD42D);WriteData(0x00AC); 
WriteComm(0xD42E);WriteData(0x0003); 
WriteComm(0xD42F);WriteData(0x00CB); 
WriteComm(0xD430);WriteData(0x0003); 
WriteComm(0xD431);WriteData(0x00EA); 
WriteComm(0xD432);WriteData(0x0003); 
WriteComm(0xD433);WriteData(0x00EF); 

WriteComm(0xD500);WriteData(0x0000); 
WriteComm(0xD501);WriteData(0x0001); 
WriteComm(0xD502);WriteData(0x0000); 
WriteComm(0xD503);WriteData(0x001C); 
WriteComm(0xD504);WriteData(0x0000); 
WriteComm(0xD505);WriteData(0x004E); 
WriteComm(0xD506);WriteData(0x0000); 
WriteComm(0xD507);WriteData(0x006A); 
WriteComm(0xD508);WriteData(0x0000); 
WriteComm(0xD509);WriteData(0x0085); 
WriteComm(0xD50A);WriteData(0x0000); 
WriteComm(0xD50B);WriteData(0x00AB); 
WriteComm(0xD50C);WriteData(0x0000); 
WriteComm(0xD50D);WriteData(0x00C4); 
WriteComm(0xD50E);WriteData(0x0000); 
WriteComm(0xD50F);WriteData(0x00FC); 
WriteComm(0xD510);WriteData(0x0001); 
WriteComm(0xD511);WriteData(0x0023); 
WriteComm(0xD512);WriteData(0x0001); 
WriteComm(0xD513);WriteData(0x0061); 
WriteComm(0xD514);WriteData(0x0001); 
WriteComm(0xD515);WriteData(0x0094); 
WriteComm(0xD516);WriteData(0x0001); 
WriteComm(0xD517);WriteData(0x00E4); 
WriteComm(0xD518);WriteData(0x0002); 
WriteComm(0xD519);WriteData(0x0027); 
WriteComm(0xD51A);WriteData(0x0002); 
WriteComm(0xD51B);WriteData(0x0029); 
WriteComm(0xD51C);WriteData(0x0002); 
WriteComm(0xD51D);WriteData(0x0065); 
WriteComm(0xD51E);WriteData(0x0002);
WriteComm(0xD51F);WriteData(0x00A6);  
WriteComm(0xD520);WriteData(0x0002); 
WriteComm(0xD521);WriteData(0x00CA); 
WriteComm(0xD522);WriteData(0x0002); 
WriteComm(0xD523);WriteData(0x00FD); 
WriteComm(0xD524);WriteData(0x0003); 
WriteComm(0xD525);WriteData(0x001D); 
WriteComm(0xD526);WriteData(0x0003); 
WriteComm(0xD527);WriteData(0x004D); 
WriteComm(0xD528);WriteData(0x0003); 
WriteComm(0xD529);WriteData(0x006A); 
WriteComm(0xD52A);WriteData(0x0003); 
WriteComm(0xD52B);WriteData(0x0095); 
WriteComm(0xD52C);WriteData(0x0003); 
WriteComm(0xD52D);WriteData(0x00AC); 
WriteComm(0xD52E);WriteData(0x0003); 
WriteComm(0xD52F);WriteData(0x00CB); 
WriteComm(0xD530);WriteData(0x0003); 
WriteComm(0xD531);WriteData(0x00EA); 
WriteComm(0xD532);WriteData(0x0003); 
WriteComm(0xD533);WriteData(0x00EF); 

WriteComm(0xD600);WriteData(0x0000); 
WriteComm(0xD601);WriteData(0x0001); 
WriteComm(0xD602);WriteData(0x0000); 
WriteComm(0xD603);WriteData(0x001C); 
WriteComm(0xD604);WriteData(0x0000); 
WriteComm(0xD605);WriteData(0x004E); 
WriteComm(0xD606);WriteData(0x0000); 
WriteComm(0xD607);WriteData(0x006A); 
WriteComm(0xD608);WriteData(0x0000); 
WriteComm(0xD609);WriteData(0x0085); 
WriteComm(0xD60A);WriteData(0x0000); 
WriteComm(0xD60B);WriteData(0x00AB); 
WriteComm(0xD60C);WriteData(0x0000); 
WriteComm(0xD60D);WriteData(0x00C4); 
WriteComm(0xD60E);WriteData(0x0000); 
WriteComm(0xD60F);WriteData(0x00FC); 
WriteComm(0xD610);WriteData(0x0001); 
WriteComm(0xD611);WriteData(0x0023); 
WriteComm(0xD612);WriteData(0x0001); 
WriteComm(0xD613);WriteData(0x0061); 
WriteComm(0xD614);WriteData(0x0001); 
WriteComm(0xD615);WriteData(0x0094); 
WriteComm(0xD616);WriteData(0x0001); 
WriteComm(0xD617);WriteData(0x00E4); 
WriteComm(0xD618);WriteData(0x0002); 
WriteComm(0xD619);WriteData(0x0027); 
WriteComm(0xD61A);WriteData(0x0002); 
WriteComm(0xD61B);WriteData(0x0029); 
WriteComm(0xD61C);WriteData(0x0002); 
WriteComm(0xD61D);WriteData(0x0065); 
WriteComm(0xD61E);WriteData(0x0002);
WriteComm(0xD61F);WriteData(0x00A6);  
WriteComm(0xD620);WriteData(0x0002); 
WriteComm(0xD621);WriteData(0x00CA); 
WriteComm(0xD622);WriteData(0x0002); 
WriteComm(0xD623);WriteData(0x00FD); 
WriteComm(0xD624);WriteData(0x0003); 
WriteComm(0xD625);WriteData(0x001D); 
WriteComm(0xD626);WriteData(0x0003); 
WriteComm(0xD627);WriteData(0x004D); 
WriteComm(0xD628);WriteData(0x0003); 
WriteComm(0xD629);WriteData(0x006A); 
WriteComm(0xD62A);WriteData(0x0003); 
WriteComm(0xD62B);WriteData(0x0095); 
WriteComm(0xD62C);WriteData(0x0003); 
WriteComm(0xD62D);WriteData(0x00AC); 
WriteComm(0xD62E);WriteData(0x0003); 
WriteComm(0xD62F);WriteData(0x00CB); 
WriteComm(0xD630);WriteData(0x0003); 
WriteComm(0xD631);WriteData(0x00EA); 
WriteComm(0xD632);WriteData(0x0003); 
WriteComm(0xD633);WriteData(0x00EF); 


//Enable Page 0
WriteComm(0xF000);WriteData(0x0055); 
WriteComm(0xF001);WriteData(0x00AA); 
WriteComm(0xF002);WriteData(0x0052); 
WriteComm(0xF003);WriteData(0x0008);
WriteComm(0xF004);WriteData(0x0000);

//Display control
WriteComm(0xB100);WriteData(0x00CC);// MCU I/F & mipi cmd mode用CC， RGB I/F  & mipi video mode 用FC
WriteComm(0xB101);WriteData(0x0000); 

WriteComm(0xB400);WriteData(0x0010); 

//Source hold time
WriteComm(0xB600);WriteData(0x0005);

//Gate EQ control
WriteComm(0xB700);WriteData(0x0070); 
WriteComm(0xB701);WriteData(0x0070);

//Source EQ control (Mode 2)
WriteComm(0xB800);WriteData(0x0001); 
WriteComm(0xB801);WriteData(0x0005); 
WriteComm(0xB802);WriteData(0x0005); 
WriteComm(0xB803);WriteData(0x0005);

//Inversion mode  (C)
WriteComm(0xBC00);WriteData(0x0002); //02:2DOT
WriteComm(0xBC01);WriteData(0x0000); 
WriteComm(0xBC02);WriteData(0x0000); 

//BOE's Setting (default) 
WriteComm(0xCC00);WriteData(0x0003); 
WriteComm(0xCC01);WriteData(0x0050); 
WriteComm(0xCC02);WriteData(0x0050); 

//Porch Adjust
WriteComm(0xBD00);WriteData(0x0001); 
WriteComm(0xBD01);WriteData(0x0084); 
WriteComm(0xBD02);WriteData(0x0007); 
WriteComm(0xBD03);WriteData(0x0031); 
WriteComm(0xBD04);WriteData(0x0000); 

WriteComm(0xBE00);WriteData(0x0001); 
WriteComm(0xBE01);WriteData(0x0084); 
WriteComm(0xBE02);WriteData(0x0007); 
WriteComm(0xBE03);WriteData(0x0031); 
WriteComm(0xBE04);WriteData(0x0000);

WriteComm(0xBF00);WriteData(0x0001); 
WriteComm(0xBF01);WriteData(0x0084); 
WriteComm(0xBF02);WriteData(0x0007); 
WriteComm(0xBF03);WriteData(0x0031); 
WriteComm(0xBF04);WriteData(0x0000);

WriteComm(0x3500);WriteData(0x0000); 
WriteComm(0x3600);WriteData(0x0000); 
WriteComm(0x3A00);WriteData(0x0077);	 //0x77:24BIT

//SLEEP OUT 
WriteComm(0x1100);
LCD_delay(20);
//DISPLY ON
WriteComm(0x2900);
LCD_delay(20);

	
WriteComm(0x3600);WriteData(0x00A3);//这个AB的高5位是旋转屏幕用的，D3位是设置RGB与BGR顺序，低两位是相互镜像X轴与Y轴
WriteComm(0x3A00);WriteData(0x0055);//如果用24位，则是77，如果你用16位STM32的FSMC，77改为55

	Lcd_Light_ON;  //打开背光


}
/**********************************************
函数名：开窗函数(操作窗口)

入口参数：XStart x方向的起点
          Xend   x方向的终点
					YStart y方向的起点
          Yend   y方向的终点

这个函数的意义是：开一个矩形框，方便接下来往这个框填充数据
注意：xStart、yStart、Xend、Yend随着屏幕的旋转而改变，位置是矩形框的四个角
***********************************************/
void BlockWrite(unsigned int Xstart,unsigned int Xend,unsigned int Ystart,unsigned int Yend) 
{
	WriteComm(0x2a00);   
	WriteData(Xstart>>8);
	WriteComm(0x2a01); 
	WriteData(Xstart);
	WriteComm(0x2a02); 
	WriteData(Xend>>8);
	WriteComm(0x2a03); 
	WriteData(Xend);

	WriteComm(0x2b00);   
	WriteData(Ystart>>8);
	WriteComm(0x2b01); 
	WriteData(Ystart);
	WriteComm(0x2b02); 
	WriteData(Yend>>8);
	WriteComm(0x2b03); 
	WriteData(Yend);
	WriteComm(0x2c00);
}
/**********************************************
函数名：Lcd矩形填充函数

入口参数：xStart x方向的起始点
          ySrart y方向的终止点
          xLong  要选定矩形的x方向长度
          yLong  要选定矩形的y方向长度
          Color  要填充的颜色
***********************************************/
void Lcd_ColorBox(u16 xStart,u16 yStart,u16 xLong,u16 yLong,u16 Color)
{
	u32 temp;

	BlockWrite(xStart,xStart+xLong-1,yStart,yStart+yLong-1);
	for (temp=0; temp<xLong*yLong; temp++)
	{
		*(__IO u16 *) (Bank1_LCD_D) = Color;
	}
}
/******************************************
函数名：Lcd图像填充
功能：向Lcd指定位置填充图像
入口参数：
					(x,y): 图片左上角起始坐标
					(pic_H,pic_V): 图片的宽高
					 pic  指向存储图片数组的指针
******************************************/
void LCD_Fill_Pic(u16 x, u16 y,u16 pic_H, u16 pic_V, const unsigned short int* pic)
{
  unsigned long i;

	BlockWrite(x,x+pic_H-1,y,y+pic_V-1);
	for (i = 0; i < pic_H*pic_V; i++)
	{
		*(__IO u16 *) (Bank1_LCD_D) = pic[i];
	}
}
//=============== 在x，y 坐标上打一个颜色为Color的点 ===============//
void DrawPixel(u16 x, u16 y, u16 Color)
{
	BlockWrite(x,x,y,y);
	*(__IO u16 *) (Bank1_LCD_D) = Color;
}
/**********画水平线**********
x0:    水平线起始坐标
x1:    水平线结束坐标
y:     水平线y坐标
Color: 线段颜色
*****************************/
void LCD_draw_HLine(u16 x0,u16 x1,u16 y,u16 Color)
{
	u16 i;
	BlockWrite(x0,x1,y,y);
	for(i=0;i<x1-x0+1;i++)
		{
			WriteData(Color);  
		}
}
/**********画垂直线**********
x:     垂直线x坐标
y0:    垂直线起始坐标
y1:    垂直线结束坐标
Color: 线段颜色
*****************************/
void LCD_draw_VLine(u16 x,u16 y0,u16 y1,u16 Color)
{
	u16 i;
	BlockWrite(x,x,y0,y1);
	for(i=0;i<y1-y0+1;i++)
		{
			WriteData(Color); 
		}
}
/**********电池电量显示**********
x,y :      电池显示位置坐标
power_per: 电池显示的百分比，范围为0~4
*********************************/
void draw_Bat(u16 x,u16 y,u8 power_per)
{
	u8 i,j;
	LCD_draw_VLine(x,y,y+5,BLACK);
	LCD_draw_VLine(x,y+5+10,y+5+10+5,BLACK);
	
	LCD_draw_HLine(x,x+28,y,BLACK);
	LCD_draw_HLine(x,x+28,y+20,BLACK);
	
	LCD_draw_VLine(x+28,y,y+5,BLACK);
	LCD_draw_VLine(x+28,y+5+10,y+5+10+5,BLACK);
	
	LCD_draw_HLine(x+28,x+28+3,y+5,BLACK);
	LCD_draw_HLine(x+28,x+28+3,y+5+10,BLACK);
	
	LCD_draw_VLine(x+28+3,y+5,y+5+10,BLACK);
	
	for(j=0;j<power_per;j++)
	{
		BlockWrite(x+3,x+3+4,y+3,y+3+14);
		for(i=0;i<6*15;i++)
			{
				WriteData(BLACK);   
			}
		x=x+6;
	}
}
/**********8*16字体 ASCII码 显示*************
(x,y): 显示字母的起始坐标
num:   要显示的字符:" "--->"~"
fColor 前景色
bColor 背景色
flag:  有背景色(1)无背景色(0)
*********************************************/
void SPILCD_ShowChar(unsigned short x,unsigned short y,unsigned char num, unsigned int fColor, unsigned int bColor,unsigned char flag) 
{       
	unsigned char temp;
	unsigned int pos,i,j;  

	num=num-' ';//得到偏移后的值
	i=num*16; 	
	for(pos=0;pos<16;pos++)
		{
			temp=nAsciiDot[i+pos];	//调通调用ASCII字体
			for(j=0;j<8;j++)
		   {                 
		        if(temp&0x80)
							DrawPixel(x+j,y,fColor);
						else 
							if(flag) DrawPixel(x+j,y,bColor); //如果背景色标志flag为1
							temp<<=1; 
		    }
			 y++;
		}		 
}  

/**********写一个16x16的汉字*****************
(x,y): 显示汉字的起始坐标
c[2]:  要显示的汉字
fColor 前景色
bColor 背景色
flag:  有背景色(1)无背景色(0)
*********************************************/
void PutGB1616(unsigned short x, unsigned short  y, unsigned char c[2], unsigned int fColor,unsigned int bColor,unsigned char flag)
{
	unsigned int i,j,k;
	unsigned short m;
	for (k=0;k<64;k++) { //64标示自建汉字库中的个数，循环查询内码
	  if ((codeGB_16[k].Index[0]==c[0])&&(codeGB_16[k].Index[1]==c[1]))
			{ 
    	for(i=0;i<32;i++) 
			{
				m=codeGB_16[k].Msk[i];
				for(j=0;j<8;j++) 
				{		
					if((m&0x80)==0x80) {
						DrawPixel(x+j,y,fColor);
						}
					else {
						if(flag) DrawPixel(x+j,y,bColor);
						}
					m=m<<1;
				} 
				if(i%2){y++;x=x-8;}
				else x=x+8;
		  }
		}  
	  }	
	}
/**********显示一串字*****************
(x,y): 字符串的起始坐标
*s:    要显示的字符串指针
fColor 前景色
bColor 背景色
flag:  有背景色(1)无背景色(0)
*********************************************/
void LCD_PutString(unsigned short x, unsigned short y, char *s, unsigned int fColor, unsigned int bColor,unsigned char flag) 
	{
		unsigned char l=0;
		while(*s) 
			{
				if( (unsigned char)*s < 0x80) 
						{
							SPILCD_ShowChar(x+l*8,y,*s,fColor,bColor,flag);
							s++;l++;
						}
				else
						{
							PutGB1616(x+l*8,y,(unsigned char*)s,fColor,bColor,flag);
							s+=2;l+=2;
						}
			}
	}

