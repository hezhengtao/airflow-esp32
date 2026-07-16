#ifndef _TIMER_H
#define _TIMER_H
#include "sys.h"

void TIM3_Init(u16 arr,u16 psc);
void TIM8_Config(u32 psc,u32 arr);
void TIM10_Config(u16 arr,u16 psc);
void delay_ms(u16 time);//ºÚµ•»Ìº˛—” ±
#endif
