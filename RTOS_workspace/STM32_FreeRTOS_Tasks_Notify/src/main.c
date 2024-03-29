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

TaskHandle_t xTaskHandle1 = NULL;
TaskHandle_t xTaskHandle2 = NULL;

//Task functions prototypes
void vtask_led_handler (void *params);
void vtask_button_handler (void *params);

//function prototypes
static void prvSetupHardware(void);
static void prvSetupUart(void);
void prvSetupGpio(void);
void printmsg(char *msg); 	//msg: message
void rtos_delay(uint32_t );

//global variables
char usr_msg[250] = {0};

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

	sprintf(usr_msg, "This is Demo of Task Notify APIs\r\n");
	printmsg(usr_msg);

	//start recording
	SEGGER_SYSVIEW_Conf();
	SEGGER_SYSVIEW_Start();

	//3. lets create 2 tasks
	xTaskCreate(vtask_led_handler, "TASK-LED", 500, NULL, 2, &xTaskHandle1);

	xTaskCreate(vtask_button_handler, "TASK-BUTTON", 500, NULL, 2, &xTaskHandle2);

	//4. start the scheduler
	vTaskStartScheduler();

	for(;;);
}

void vtask_led_handler(void *params)
{
	while(1){
		//lets wait until we receive any notification event from task_button
		if(xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE){
			//we received the notification so lets toggle the led
			GPIO_ToggleBits(GPIOB, GPIO_Pin_0);
			sprintf(usr_msg, "Notification is received \r\n");
			printmsg(usr_msg);
		}
	}

}

void vtask_button_handler(void *params)
{
	while(1){
		if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13)){
			//if button is pressed

			//wait here for 100ms to compensate for the button debouncing
			rtos_delay(100); //wait for 100 ticks

			//send notification to task_led
			xTaskNotify(xTaskHandle1, 0, eNoAction);

		}

	}
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

}

void rtos_delay(uint32_t delay_in_ms)
{
	uint32_t current_tick_count = xTaskGetTickCount();

	//convertion ms to # of ticks
	uint32_t delay_in_ticks = (delay_in_ms * configTICK_RATE_HZ) / 1000;

	while(xTaskGetTickCount() < (current_tick_count + delay_in_ticks));

}















