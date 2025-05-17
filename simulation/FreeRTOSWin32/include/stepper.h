#ifndef INC_STEPPER_H_
#define INC_STEPPER_H_

#include "main.h"
#include "LibL6474.h"
#include "LibL6474Config.h"
//#include "stm32f7xx_hal_spi.h"
//#include "stm32f7xx_hal_tim.h"
#include "stm32f7xx_hal_rcc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "Console.h"

#include "stm32f7xx_hal.h"

#include "timers.h"
#include "math.h"

void InitStepper(ConsoleHandle_t hconsole, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* htim1, TIM_HandleTypeDef* htim4);


#endif  /* INC_STEPPER_H_ */