/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/

#include<string.h>
#include<stdint.h>
#include "stm32f4xx.h"

#include"FreeRTOS.h"
#include "task.h"

#define TRUE 1
#define FALSE 0

#define NOT_PRESSED  FALSE
#define PRESSED  TRUE

//Task functions prototypes
void led_task_handler (void *params);
void button_handler (void );

//function prototypes
static void prvSetupHardware(void);
static void prvSetupUart(void);
void prvSetupGpio(void);
void printmsg(char *msg); 	//msg: message

//global variables
uint8_t button_status_flag = NOT_PRESSED;

int main(void)
{
	DWT ->CTRL |= (1 << 0); 	// Enable the cycle counting(CYCCNT) in DWT_CTRL for SEGGER Systemview Timestamp maintaining

	//1. Reset the RCC Clock configuration to the default reset state
	//HSI ON, HSE, PLL OFF, system clock = 16 MHz, CPU clock = 16 MHz
	RCC_DeInit();

	//2. Update the systemcoreclock variable
	//SystemCoreClock = 16000000;
	SystemCoreClockUpdate();

	prvSetupHardware();

	//start recording
	SEGGER_SYSVIEW_Conf();
	SEGGER_SYSVIEW_Start();

	//let's create led_task
	xTaskCreate(led_task_handler, "LED-TASK", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	//start the scheduler
	vTaskStartScheduler();

	for(;;);
}

void led_task_handler(void *params)
{
	while(1){
		if(button_status_flag == PRESSED)
			GPIO_WriteBit(GPIOB, GPIO_Pin_0, Bit_SET);		//turn on led
		else
			GPIO_WriteBit(GPIOB, GPIO_Pin_0, Bit_RESET);	//turn off led
	}

}

void button_handler(void )
{
	button_status_flag ^= 1;
}

static void prvSetupHardware(void)	//prv : private, board or any perip related init
{
	//Setup led and button
	prvSetupGpio();

	//Setup UART3
	prvSetupUart();

}

void printmsg(char *msg)
{
	for(uint32_t i = 0; i < strlen(msg); i++){
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) != SET);
		USART_SendData(USART3, msg[i]);
	}

}

static void prvSetupUart(void)
{
	GPIO_InitTypeDef gpio_uart_pins;
	USART_InitTypeDef uart3_init;

	//1. enable the UART3 and GPIOD perip clock
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	//PD8 is UART3_TX, PD9 is UART3_RX,

	//2. Alternate func conf of MCU pins to behave as UART3 TX and RX
	//making zero each member element of the structure
	memset(&gpio_uart_pins, 0, sizeof(gpio_uart_pins));

	gpio_uart_pins.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	gpio_uart_pins.GPIO_Mode = GPIO_Mode_AF;
	gpio_uart_pins.GPIO_PuPd = GPIO_PuPd_UP;

	GPIO_Init(GPIOD, &gpio_uart_pins);

	//3. AF mode settings for the pins
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3); //PD8
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3); //PD9

	//4. UART parameter initializations
	//making zero each member element of the structure
	memset(&uart3_init, 0, sizeof(uart3_init));

	uart3_init.USART_BaudRate = 115200;
	uart3_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	uart3_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	uart3_init.USART_Parity = USART_Parity_No;
	uart3_init.USART_StopBits = USART_StopBits_1;
	uart3_init.USART_WordLength = USART_WordLength_8b;

	USART_Init(USART3 , &uart3_init);

	//5. Enable the uart3 perip
	USART_Cmd(USART3, ENABLE);

}

void prvSetupGpio(void)
{
	//this func is board specific
	GPIO_InitTypeDef led_init, button_init;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	led_init.GPIO_Pin = GPIO_Pin_0;
	led_init.GPIO_Mode = GPIO_Mode_OUT;
	led_init.GPIO_OType = GPIO_OType_PP;
	led_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
	led_init.GPIO_Speed = GPIO_Low_Speed;

	GPIO_Init(GPIOB, &led_init);

	button_init.GPIO_Pin = GPIO_Pin_13;
	button_init.GPIO_Mode = GPIO_Mode_IN;
	button_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
	button_init.GPIO_Speed = GPIO_Low_Speed;

	GPIO_Init(GPIOC, &button_init);

	// interrupt configuration for the button (PC13)
	//1. system configuration for exti line (SYSCFG settings)
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource13);

	//2. EXTI line configuration
	EXTI_InitTypeDef exti_init;

	exti_init.EXTI_Line = EXTI_Line13;
	exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
	exti_init.EXTI_Trigger = EXTI_Trigger_Rising;
	exti_init.EXTI_LineCmd = ENABLE;

	EXTI_Init(&exti_init);

	//3. NVIC settings (IRQ settings for the selected line(13))
	NVIC_SetPriority(EXTI15_10_IRQn, 5);
	NVIC_EnableIRQ(EXTI15_10_IRQn);

}

void EXTI15_10_IRQHandler(void)
{
	traceISR_ENTER();

	//1. clear the interrupt pending bit of the EXTI line(13)
	EXTI_ClearITPendingBit(EXTI_Line13);

	button_handler();

	traceISR_EXIT();

}















