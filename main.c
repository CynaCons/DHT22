/**
 ******************************************************************************
 * @file    main.c
 * @author  Constantin CHABIRAND (cynako@gmail.com) 
 * @version V1.0
 * @date    23-04-2017
 * @brief   Example main function for the DHT22 drivers using the HAL library
 ******************************************************************************
 */


#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "stm32f4xx_hal.h"

#include "dht22.h"


void SystemClock_Config(void);
void InitDHT22(void);


DHT22_HandleTypeDef dht;

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    Init_DHT22();

    /* It is advised to wait two seconds before starting to use the sensor */
    HAL_Delay(2000);

    while (1) {
        DHT22_ReadData(&dht);
        float temp=dht.temp;
        float humidity=dht.hum;
        HAL_Delay(3000);
    }
}



void Init_DTH22(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /*##-1- Enable peripherals and GPIO Clocks #################################*/
    /* TIMx Peripheral clock enable */
    __TIM3_CLK_ENABLE();

    /* Enable GPIO channels Clock */
    __GPIOC_CLK_ENABLE();

    /* Configure  (TIMx_Channel) in Alternate function, push-pull and 100MHz speed */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*##-2- Configure the NVIC for TIMx #########################################*/

    HAL_NVIC_SetPriority(TIM3_IRQn, 0, 1);

    /* Enable the TIM3 global Interrupt */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    /* Setup TIM3 Channel 1 for DHT22 */
    dht.gpioPin = GPIO_PIN_6;
    dht.gpioPort = GPIOC;
    dht.timChannel = TIM_CHANNEL_1;
    dht.gpioAlternateFunction = GPIO_AF2_TIM3;
    dht.timHandle.Instance = TIM3;
    dht.timerIRQn = TIM3_IRQn;

    if(DHT22_Init(&dht)!=DHT22_OK){
        // error occured
        while(1);
    }
}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
    }

    /**Initializes the CPU, AHB and APB busses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
        |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
    }

    /**Configure the Systick interrupt time
    */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick
    */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}
