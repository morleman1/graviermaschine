#include "Spindle.h"
#include "Console.h"
#include "FreeRTOSConfig.h"
#include "main.h"
#include "stdio.h"

typedef struct {
	int direction;
	TIM_HandleTypeDef timer;
	SpindleHandle_t handle;
} SpindleContext_t;

SpindleContext_t spindleContext;

int SpindleStart(int rpm);
int SpindleStop(void);
int SpindleStatus(void);


void SPINDLE_SetDirection(SpindleHandle_t h, void *context, int backward)
{
  (void)h;
  (void)context;
  spindleContext.direction = backward;

  if (backward)
  {
    printf("Setting spindle direction: backward (counter-clockwise)\r\n");
  }
  else
  {
    printf("Setting spindle direction: forward (clockwise)\r\n");
  }
}

void SPINDLE_SetDutyCycle(SpindleHandle_t h, void *context, float dutyCycle)
{
  (void)h;
  (void)context;
  int arr = TIM2->ARR;
  if (spindleContext.direction)
   {
      TIM2->CCR3 = 0;
      TIM2->CCR4 = (int)((float)arr * dutyCycle);
   }
   else
   {
      TIM2->CCR3 = (int)((float)arr * dutyCycle);
      TIM2->CCR4 = 0;
   }
  
}

void SPINDLE_EnaPWM(SpindleHandle_t h, void *context, int ena)
{
  (void)h;
  (void)context;

  HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, ena);
  HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, ena);

  if (ena)
  {
      HAL_TIM_PWM_Start(&spindleContext.timer, TIM_CHANNEL_3);
      HAL_TIM_PWM_Start(&spindleContext.timer, TIM_CHANNEL_4);
  }
}

void InitSpindle(ConsoleHandle_t* consoleHandle, TIM_HandleTypeDef* htim)
{
  // Initialize the spindle parameters
  SpindlePhysicalParams_t s;
  s.maxRPM = 9000.0f;
  s.minRPM = -9000.0f;
  s.absMinRPM = 1600.0f;
  s.setDirection = SPINDLE_SetDirection;
  s.setDutyCycle = SPINDLE_SetDutyCycle;
  s.enaPWM = SPINDLE_EnaPWM;
  s.context = NULL;

  spindleContext.handle= SPINDLE_CreateInstance(4 * configMINIMAL_STACK_SIZE,
                                         configMAX_PRIORITIES - 3,
                                         *consoleHandle, &s);

  if (spindleContext.handle == NULL)
  {
    printf("Failed to create spindle controller instance\r\n");
    Error_Handler();
  }
  spindleContext.direction = 0;
  spindleContext.timer = *htim;
}