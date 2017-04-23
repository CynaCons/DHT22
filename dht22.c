/*
 * dht22.c
 *
 *  Created on: Jan 3, 2015
 *      Author: peter
 *
 *  Modified on: April 23, 2017
 *  	Modifier: Constantin CHABIRAND ; cynako@gmail.com
 *
 *  This file can be used in an STM32 project using ST's HAL library in order to setup
 *  and use the DHT22 temperature and humidity sensor.
 */

#include "dht22.h"

#define DHT22_StartIT()	if(HAL_TIM_IC_Start_IT(&handle->timHandle, handle->timChannel) != HAL_OK) return DHT22_ERROR
#define DHT22_StopIT()	if(HAL_TIM_IC_Stop_IT(&handle->timHandle, handle->timChannel) != HAL_OK) return DHT22_ERROR

/* ---- PRIVATE FUNCTION DECLARATION -------------------------------------*/
void DHT22_Prv_ProcessReceivedData(DHT22_HandleTypeDef* handle);

/**
 * Initialize the HW and peripherals.
 * The setup is done according the data specified by the used in the handle structure.
 *
 *@param handle pointer to a handle structure filled by the user.
 *
 * @return DHT22_ERROR : An error has happened during the process.
 * 		   DHT22_OK    : Init successful.
 */
DHT22_RESULT DHT22_Init(DHT22_HandleTypeDef* handle) {
	/* Zero the lastVal and bitPos */
	handle->lastVal = 0;
	handle->bitPos = 0;
	handle->timerFreq = HAL_RCC_GetPCLK1Freq();

	/* Init the  TIM IC */
	handle->timHandle.Init.Period = 0xFFFF;
	handle->timHandle.Init.Prescaler = 0;
	handle->timHandle.Init.ClockDivision = 0;
	handle->timHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	if (HAL_TIM_IC_Init(&handle->timHandle) != HAL_OK) {
		return DHT22_ERROR;
	}

	/* Configure the IC Channel */
	handle->timICHandle.ICPolarity = TIM_ICPOLARITY_FALLING;
	handle->timICHandle.ICSelection = TIM_ICSELECTION_DIRECTTI;
	handle->timICHandle.ICPrescaler = TIM_ICPSC_DIV1;
	handle->timICHandle.ICFilter = 0;
	if (HAL_TIM_IC_ConfigChannel(&handle->timHandle, &handle->timICHandle,
			handle->timChannel) != HAL_OK) {
		return DHT22_ERROR;
	}
	return DHT22_OK;
}


/**
 * Set the PIN in ouput mode to send the trigger signal.
 *
 * @param handle pointer to the handle structure filled by the user.
 */
void DHT22_SetPinOUT(DHT22_HandleTypeDef* handle) {
	HAL_NVIC_DisableIRQ(handle->timerIRQn);
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = handle->gpioPin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(handle->gpioPort, &GPIO_InitStruct);
}


/**
 * Set the PIN in input mode for data reception.
 *
 * The PIN will be in "input Open Drain", meaning it's state (high or low)
 * 		will adapt to the sensor data line state.
 * The pin state will be "driven" by the sensor data line.
 *
 *
 * @param handle pointer to the handle structure filled by the user.
 */
void DHT22_SetPinIN(DHT22_HandleTypeDef* handle) {
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = handle->gpioPin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = handle->gpioAlternateFunction;
	HAL_GPIO_Init(handle->gpioPort, &GPIO_InitStruct);
	HAL_NVIC_EnableIRQ(handle->timerIRQn);
	HAL_NVIC_SetPriority(handle->timerIRQn, 0, 0);
}


/**
 * To be implemented.
 */
DHT22_RESULT DHT22_DeInit(DHT22_HandleTypeDef* handle) {
	return DHT22_OK;
}


/**
 * Send the trigger signal and start the IRQs on the TIM IC Channel.
 *
 * @param handle pointer to the handle structure filled by the user.
 *
 * @return DHT22_OK
 */
DHT22_RESULT DHT22_InitiateTransfer(DHT22_HandleTypeDef* handle) {

	/* Send the trigger signal (pull down the data line for 2 sec then release) */
	DHT22_SetPinOUT(handle);
	HAL_GPIO_WritePin(handle->gpioPort, handle->gpioPin, GPIO_PIN_RESET);
	HAL_Delay(2);

	/* Release the data line and setup the pin for data reception. */
	DHT22_SetPinIN(handle);

	/* Empty the data reception buffer (bitsRX) and reset the bit position index (bitPos) */
	handle->bitPos = -1;
	handle->irq_count = 0;
	for (int i = 0; i < 5; i++) {
		handle->bitsRX[i] = 0;
	}

	handle->state = DHT22_RECEIVING;
	DHT22_StartIT();

	return DHT22_OK;
}


/**
 * Process the received data.
 *
 * This function should be called during the IRQ Handler for the associated timer.
 *
 * @param handle pointer to the handle structure filled by the user.
 */
void DHT22_InterruptHandler(DHT22_HandleTypeDef* handle) {

	/* Read the current value from the TIM register */
	uint16_t currentTimValue = HAL_TIM_ReadCapturedValue(
			&handle->timHandle,
			handle->timChannel
			);

	handle->irq_count++;

	/* Time ellapsed in timer ticks since the last IRQ (falling edge was received) */
	uint16_t timeEllapsedInTicks;


	/* Determine the number of timer ticks since last IRQ */
	if (currentTimValue > handle->lastVal){
		timeEllapsedInTicks = currentTimValue - handle->lastVal;
	}else{
		// Take timer overflow into consideration.
		timeEllapsedInTicks = 65535 + currentTimValue - handle->lastVal;
	}
	//Save the value in the handle structure for next IRQ processing.
	handle->lastVal = currentTimValue;


	/**
	 * Calculate the time ellapsed in micro seconds with the following formula:
	 *
	 * 		TIME = (1000000.0 / FREQ ) * TIME_ELLAPSED_SINCE_LAST_IRQ_IN_CPU_TICKS
	 * 		TIME = (PERIOD IN µs ) * TIME_ELLAPSED_SINCE_LAST_IRQ_IN_CPU_TICKS
	 */
	float timeEllapsedInMicroSec = (1000000.0 / (handle->timerFreq * 2)) * timeEllapsedInTicks;


	/**
	 * PLEASE keep in mind that an IRQ is fired when a falling edge is received.
	 * Refer to the following wiki for a detailed process:
	 * 		http://www.waveshare.com/wiki/DHT22_Temperature-Humidity_Sensor
	 *
	 * The data reception is based on the time ellapsed since the previous data bit was received.
	 *
	 * For a "0" data bit, it takes approximately 75ms (50ms low, 25 ms high)
	 * For a "1" data bit, it takes approximately 100ms (50ms low, 70 ms high)
	 *
	 * The first two IRQ signal the beginning of the data transfer.
	 */
	switch(handle->irq_count){
	case 1:
		// First IRQ, nothing to do. The sensor is answering to our trigger signal.
		return;

	case 2:
		// Second IRQ, prepare the data reception. Now the sensor will answer the data.
		handle->bitPos = 0;
		return;

	default:
		if(timeEllapsedInMicroSec > 100){
			// Last IRQ was more than 100ms ago. It's a "1" data bit.
			handle->bitsRX[handle->bitPos / 8] |= 1 << (7 - handle->bitPos % 8);
			handle->bitPos++;
		}else{
			// Last IRQ was less than 100ms. It's a "0" data bit.
			handle->bitsRX[handle->bitPos / 8] &= ~(1<< (7 - handle->bitPos % 8));
			handle->bitPos++;
		}
		break;
	}

	/**
	 *  Once the data reception is complete (when 5 bytes have been received)
	 *  	the data will be split as the following:
	 *
	 *  TEMPERATURE : 12 bits ; HUMIDITY : 12 ; CRC : 8 bits
	 *
	 *  The CRC is the sum of the 4 other bytes.
	 */
	DHT22_Prv_ProcessReceivedData(handle);
}


/**
 * Start a data transfer and process the received data from the sensor.
 * 		The reiceived data will be stored in the handle structure (temp and hum fields).
 *
 * 	@param handle pointer to the handle structure filled by the user.
 *
 * 	@return DHT22_OK : Operation success. Data available in the handle structure.
 * 			DHT22_CRC_ERROR: The calculated CRC does not match the received one.
 * 			DHT22_ERROR: Operation failed.
 */
DHT22_RESULT DHT22_ReadData(DHT22_HandleTypeDef* handle) {

	DHT22_InitiateTransfer(handle);
	uint32_t startTick=HAL_GetTick();
	while (handle->state == DHT22_RECEIVING && HAL_GetTick()-startTick<1000);
	if(handle->crcErrorFlag==1){
		return DHT22_CRC_ERROR;
	}
	if(handle->state!=DHT22_RECEIVED){
		return DHT22_ERROR;
	}else{
		handle->state=DHT22_READY;
	}
	return DHT22_OK;
}


/**
 * Process the received data to determine the temperature, humidity and CRC.
 * The data processed is stored in the handle structure (temp and hum fields).
 *
 * @param handle pointer the the handler structure.
 * @return none.
 */
void DHT22_Prv_ProcessReceivedData(DHT22_HandleTypeDef* handle) {
	/**
	 *  Once the data reception is complete (when 5 bytes have been received)
	 *  the data will be split as the following:
	 *
	 *  TEMPERATURE : 12 bits ; HUMIDITY : 12 ; CRC : 8 bits
	 *
	 *  The CRC is the sum of the 4 other bytes.
	 */
	if (handle->bitPos == 40) {
		handle->bitPos = -1;
		HAL_TIM_IC_Stop_IT(&handle->timHandle, handle->timChannel);
		uint8_t sum = 0;
		for (int i = 0; i < 4; i++) {
			sum += handle->bitsRX[i];
		}

		// Check that the received CRC matches the calculated one.
		if (sum == handle->bitsRX[4]) {
			handle->crcErrorFlag = 0;

			// Process the data to determine the temperature value.
			int16_t temp10 = 0;
			if ((handle->bitsRX[2] & 0x80) == 0x80) {
				temp10 |= (handle->bitsRX[2] & 0x7F) << 8;
				temp10 |= handle->bitsRX[3];
				temp10 *= -1;
			} else {
				temp10 |= handle->bitsRX[2] << 8;
				temp10 |= handle->bitsRX[3];
			}
			handle->temp = 0.1 * temp10;

			// Process the data to determine the humdidity value.
			int16_t hum10 = 0;
			if ((handle->bitsRX[0] & 0x80) == 0x80) {
				hum10 |= (handle->bitsRX[0] & 0x7F) << 8;
				hum10 |= handle->bitsRX[1];
				hum10 *= -1;
			} else {
				hum10 |= handle->bitsRX[0] << 8;
				hum10 |= handle->bitsRX[1];
			}
			handle->hum = 0.1 * hum10;

		} else {
			//Received CRC does not match the calculated one.
			handle->crcErrorFlag = 1;
		}
		handle->state = DHT22_RECEIVED;
	}
}

