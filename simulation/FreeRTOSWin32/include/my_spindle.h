/*
 * my_spindle.h
 *
 *  Created on: May 16, 2025
 *      Author: Max
 */

#ifndef INC_SPINDLE_MY_SPINDLE_H_
#define INC_SPINDLE_MY_SPINDLE_H_

#include "Spindle.h"
#include "Console.h"
#include "FreeRTOSConfig.h"
#include "main.h"
#include "stdio.h"

void InitSpindle(ConsoleHandle_t* consoleHandle, TIM_HandleTypeDef* htim2);


#endif /* INC_SPINDLE_MY_SPINDLE_H_ */
