#ifndef __TIMER_H
#define __TIMER_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEKս��STM32������
//��ʱ�� ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//�޸�����:2012/9/3
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2009-2019
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////   


extern uint32_t counter;

void TIM3_Int_Init(u16 arr,u16 psc);
void Timer_Start(void);
uint32_t Timer_Stop(void);
uint8_t Timer_GetStatus(void);
uint32_t Timer_GetCounter(void);
void Timer_SetCounter(uint32_t c);
void Timer_ClearCounter(void);

#endif
