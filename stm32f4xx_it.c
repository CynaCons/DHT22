/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @author  Ac6
 * @version V1.0
 * @date    02-Feb-2015
 * @brief   Default Interrupt Service Routines.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm32f4xx.h"
#include "stm32f4xx_it.h"

#include "dht22.h"

extern DHT22_HandleTypeDef dht;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


void TIM3_IRQHandler(void){
/bin/bash: q: command not found
    /* Execute the DHT22 IRQ Handler before clearing the IRQ Flags */
    DHT22_InterruptHandler(&dht);

    /* Optimised IRQ Flag clearing without callbacks for processing speed */
    if(__HAL_TIM_GET_FLAG(&dht.timHandle, TIM_FLAG_CC1) != RESET)
    {
        if(__HAL_TIM_GET_IT_SOURCE(&dht.timHandle, TIM_IT_CC1) !=RESET)
        {
            {
                __HAL_TIM_CLEAR_IT(&dht.timHandle, TIM_IT_CC1);
            }
        }
    }
}


/******************************************************************************/
/*            	  	    Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief  This function handles SysTick Handler, but only if no RTOS defines it.
 * @param  None
 * @retval None
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}
