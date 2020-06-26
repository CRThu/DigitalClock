#include "sys.h"
#include "delay.h"
#include "led.h"
#include "includes.h"
#include "key.h"
#include "lcd.h"
#include "rtc.h"
#include "usart.h"
#include "timer.h"
#include "beep.h"
#include "math.h"

// RTC Calibration
//#define SET_NOW_TIME
#ifdef SET_NOW_TIME
#define NOW_YEAR    2020
#define NOW_MONTH   6
#define NOW_DAY     27
#define NOW_HOUR    00
#define NOW_MINUTE  21
#define NOW_SECOND  20
#endif

// UP TO 10 HOURS
#define TIME_GETHOUR(T)			(((T/1000)/3600)%10)
#define TIME_GETMINUTE(T)		(((T/1000)%3600)/60)
#define TIME_GETSECOND(T)		((T/1000)%60)
#define TIME_GET10MSEC(T)		((T%1000)/10)

//LCD显示任务
//设置任务优先级
#define DIGITAL_CLK_TASK_PRIO       7
//设置任务堆栈大小
#define DIGITAL_CLK_STK_SIZE        128
//任务堆栈	
OS_STK DIGITAL_CLK_TASK_STK[DIGITAL_CLK_STK_SIZE];
//任务函数
void digital_clock_task(void *pdata);

//LCD秒表任务
//设置任务优先级
#define STOPWATCH_TASK_PRIO         6
//设置任务堆栈大小
#define STOPWATCH_STK_SIZE          128
//任务堆栈
OS_STK STOPWATCH_TASK_STK[STOPWATCH_STK_SIZE];
//任务函数
void stopwatch_task(void *pdata);

//按键任务
//设置任务优先级
#define KEY_TASK_PRIO               5
//设置任务堆栈大小
#define KEY_STK_SIZE                128
//任务堆栈
OS_STK KEY_TASK_STK[KEY_STK_SIZE];
//任务函数
void key_task(void *pdata);

OS_EVENT *Sec_Int_Event;
OS_EVENT *LCD_Mutex_Event;
u8 err;

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    // Initial
	delay_init();
	uart_init(115200);
	LED_Init();
	LCD_Init();             // 注意:LCD代码耦合UART模块
	RTC_Init();
	TIM3_Int_Init(10-1,7200-1);   //10Khz的计数频率，计数到10为1ms
    BEEP_Init();
    KEY_Init();
    
    #ifdef SET_NOW_TIME
    RTC_Set(NOW_YEAR,NOW_MONTH,NOW_DAY,NOW_HOUR,NOW_MINUTE,NOW_SECOND);  //设置时间
    #endif
    
	OSInit();
	Sec_Int_Event=OSSemCreate(1);
	LCD_Mutex_Event=OSMutexCreate(4,&err);
 	OSTaskCreate(digital_clock_task,(void *)0,(OS_STK*)&DIGITAL_CLK_TASK_STK[DIGITAL_CLK_STK_SIZE-1],DIGITAL_CLK_TASK_PRIO);
 	OSTaskCreate(stopwatch_task,(void *)0,(OS_STK*)&STOPWATCH_TASK_STK[STOPWATCH_STK_SIZE-1],STOPWATCH_TASK_PRIO);
 	OSTaskCreate(key_task,(void *)0,(OS_STK*)&KEY_TASK_STK[KEY_STK_SIZE-1],KEY_TASK_PRIO);
	OSStart();
}
uint8_t digital_initial = 1;
_calendar_obj calendar_display = {0};
void LCD_DIGITAL_CLOCK_RENDER()
{
    
    if(digital_initial)
    {
        POINT_COLOR=BLUE;
        LCD_ShowChar64(48+48*2,80,11,0);
        LCD_ShowChar64(48+48*5,80,11,0);
        LCD_ShowChar64(48+48*2,80+64,10,0);
        LCD_ShowChar64(48+48*5,80+64,10,0);
        
        LCD_ShowNum64(48+48*0,80,0,2);
        LCD_ShowNum64(48+48*3,80,0,2);
        LCD_ShowNum64(48+48*6,80,0,2);
        LCD_ShowNum64(48+48*0,80+64,0,2);
        LCD_ShowNum64(48+48*3,80+64,0,2);
        LCD_ShowNum64(48+48*6,80+64,0,2);
        digital_initial = 0;
    }
    else
    {
        POINT_COLOR=BLUE;
        if(calendar_display.w_year != calendar.w_year)
            LCD_ShowNum64(48+48*0,80,calendar.w_year%100,2);
        if(calendar_display.w_month != calendar.w_month)
            LCD_ShowNum64(48+48*3,80,calendar.w_month,2);
        if(calendar_display.w_date != calendar.w_date)
            LCD_ShowNum64(48+48*6,80,calendar.w_date,2);
        if(calendar_display.hour != calendar.hour)
            LCD_ShowNum64(48+48*0,80+64,calendar.hour,2);
        if(calendar_display.min != calendar.min)
            LCD_ShowNum64(48+48*3,80+64,calendar.min,2);
        if(calendar_display.sec != calendar.sec)
            LCD_ShowNum64(48+48*6,80+64,calendar.sec,2);
        
        calendar_display = calendar;
    }
}

void LCD_ANALOG_CLOCK_CIR(u16 r)
{
    POINT_COLOR=BLACK;
    LCD_Draw_Circle(240,450,r+2);
    LCD_Draw_Circle(240,450,r+1);
    LCD_Draw_Circle(240,450,r);
    LCD_Draw_Circle(240,450,r-1);
    LCD_Draw_Circle(240,450,r-2);
}

// clk:0-60 -> deg:0:360
// deg=clk*6
// x=cos(deg*pi/180)
// y=sin(deg*pi/180)
void LCD_ANALOG_CLOCK_LINE(u16 l, double deg, u16 color)
{
    if(deg >= 0.0)
    {
        double dx=sin((double)deg/180.0*3.14);
        double dy=cos((double)deg/180.0*3.14);
        POINT_COLOR=color;

        LCD_DrawLine(240+dy*2,450+dx*2,240+l*dx+dy*2,450-l*dy+dx*2);
        LCD_DrawLine(240+dy*1,450+dx*1,240+l*dx+dy*1,450-l*dy+dx*1);
        LCD_DrawLine(240,450,240+l*dx,450-l*dy);
        LCD_DrawLine(240-dy*1,450-dx*1,240+l*dx-dy*1,450-l*dy-dx*1);
        LCD_DrawLine(240-dy*2,450-dx*2,240+l*dx-dy*2,450-l*dy-dx*2);
    }
}

double hour_deg = -1.0;
double hour_deg_display = -1.0;
void LCD_ANALOG_CLOCK_HOUR()
{
    hour_deg = (double)calendar.hour*30+(double)calendar.min*0.5;
    if(hour_deg_display != hour_deg)
    {
        LCD_ANALOG_CLOCK_LINE(100,hour_deg_display,WHITE);
        LCD_ANALOG_CLOCK_LINE(100,hour_deg,BLACK);
        hour_deg_display = hour_deg;
    }
}

double minute_deg = -1.0;
double minute_deg_display = -1.0;
void LCD_ANALOG_CLOCK_MINUTE()
{
    minute_deg = (double)calendar.min*6+(double)calendar.sec*0.1;
    if(minute_deg_display != minute_deg)
    {
        LCD_ANALOG_CLOCK_LINE(150,minute_deg_display,WHITE);
        LCD_ANALOG_CLOCK_LINE(150,minute_deg,BLACK);
        minute_deg_display = minute_deg;
    }
}

uint8_t analog_initial = 1;
void LCD_ANALOG_CLOCK_RENDER()
{
    
    if(analog_initial)
    {
        LCD_ANALOG_CLOCK_CIR(175);
        analog_initial = 0;
    }
    else
    {
        LCD_ANALOG_CLOCK_HOUR();
        LCD_ANALOG_CLOCK_MINUTE();
    }
}

uint8_t stopwatch_img_initial = 1;
uint8_t stopwatch_img_display = 0;
void LCD_STOPWATCH_IMG_RENDER()
{
    if(stopwatch_img_initial)
    {
        stopwatch_img_initial = 0;
        LCD_ShowStopWatch(32,610,1,RED,WHITE);
    }
    else
    {
        if(stopwatch_img_display != Timer_GetStatus())
        {
            if(Timer_GetStatus())
                LCD_ShowStopWatch(32,610,0,GREEN,WHITE);
            else
                LCD_ShowStopWatch(32,610,0,RED,WHITE);
            stopwatch_img_display = Timer_GetStatus();
        }
    }
}

uint8_t stopwatch_time_initial = 1;
uint32_t stopwatch_time_display = (uint32_t)(-1);
uint32_t stopwatch_time_now = (uint32_t)(-1);
void LCD_STOPWATCH_TIME_RENDER()
{
    if(stopwatch_time_initial)
    {
        stopwatch_time_initial = 0;
        POINT_COLOR=BLUE;
        LCD_ShowChar64(32+48*1,700,10,0);
        LCD_ShowChar64(32+48*4,700,10,0);
        
        LCD_ShowNum64(32+48*0,700,0,1);
        LCD_ShowNum64(32+48*2,700,0,2);
        LCD_ShowNum64(32+48*5,700,0,2);
        LCD_ShowNum48(32+48*7,700+54-45,0,2);
    }
    else
    {
        stopwatch_time_now = Timer_GetCounter();
        if(stopwatch_time_display/10 != stopwatch_time_now/10)
        {
            /* 0:00:00xx */
            POINT_COLOR=BLUE;
            if(TIME_GETHOUR(stopwatch_time_display) != TIME_GETHOUR(stopwatch_time_now))
                LCD_ShowNum64(32+48*0,700,TIME_GETHOUR(Timer_GetCounter()),1);
            if(TIME_GETMINUTE(stopwatch_time_display) != TIME_GETMINUTE(stopwatch_time_now))
                LCD_ShowNum64(32+48*2,700,TIME_GETMINUTE(Timer_GetCounter()),2);
            if(TIME_GETSECOND(stopwatch_time_display) != TIME_GETSECOND(stopwatch_time_now))
                LCD_ShowNum64(32+48*5,700,TIME_GETSECOND(Timer_GetCounter()),2);
            if(TIME_GET10MSEC(stopwatch_time_display) != TIME_GET10MSEC(stopwatch_time_now))
                LCD_ShowNum48(32+48*7,700+54-45,TIME_GET10MSEC(Timer_GetCounter()),2);
            
            stopwatch_time_display = stopwatch_time_now;
        }
    }
}

//LCD显示任务
void digital_clock_task(void *pdata)
{
	while(1)
	{   
        OSSemPend(Sec_Int_Event,0,&err);
        if(err == OS_ERR_NONE)
        {
            OSMutexPend(LCD_Mutex_Event,0,&err);
            if(err == OS_ERR_NONE)
            {
                LCD_DIGITAL_CLOCK_RENDER();
                LCD_ANALOG_CLOCK_RENDER();
            }
            OSMutexPost(LCD_Mutex_Event);
        }
	}
}


//LCD秒表任务
void stopwatch_task(void *pdata)
{
	while(1)
	{
        OSMutexPend(LCD_Mutex_Event,0,&err);
        if(err == OS_ERR_NONE)
        {
            LCD_STOPWATCH_IMG_RENDER();
            LCD_STOPWATCH_TIME_RENDER();
        }
        OSMutexPost(LCD_Mutex_Event);
        delay_ms(5);
    }
}


//按键任务
void key_task(void *pdata)
{
 	vu8 key=0;	
	while(1)
	{
 		key=KEY_Scan(0);
	   	if(key)
		{						   
			switch(key)
			{
				case KEY0_PRES:
                    if(Timer_GetStatus())
                    {
                        Timer_Stop();
                        LED1=1;
                    }
                    else
                    {
                        Timer_Start();
                        LED1=0;
                    }
                    break;
			}
		}
        else 
            delay_ms(10);
	}
}

// FROM RTC.C
//RTC时钟中断
//每秒触发一次  
//extern u16 tcnt; 
void RTC_IRQHandler(void)
{		 
	if (RTC_GetITStatus(RTC_IT_SEC) != RESET)//秒钟中断
	{							
		RTC_Get();//更新时间   

        LED0 = ~LED0;
        
        OSSemPost(Sec_Int_Event);
 	}
	if(RTC_GetITStatus(RTC_IT_ALR)!= RESET)//闹钟中断
	{
		RTC_ClearITPendingBit(RTC_IT_ALR);		//清闹钟中断	  	
        RTC_Get();				//更新时间   
        printf("Alarm Time:%d-%d-%d %d:%d:%d\n",calendar.w_year,calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);//输出闹铃时间	
  	} 				  								 
	RTC_ClearITPendingBit(RTC_IT_SEC|RTC_IT_OW);		//清闹钟中断
	RTC_WaitForLastTask();
}
