/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include <stdlib.h>
#include "Console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Spindle.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
TaskHandle_t InitComponentsTaskHandle = NULL;
L6474_Handle_t stepperHandle = NULL;
SpindleHandle_t spindleHandle = NULL;
ConsoleHandle_t consoleHandle = NULL;

TaskHandle_t stepperMoveTaskHandle = NULL;
volatile bool stepperMoving = false; // für async movement
volatile bool stepperCancelRequested = false;
volatile int targetPosition = 0;

static void (*stepperDoneCallback)(L6474_Handle_t) = NULL;
static L6474_Handle_t stepperCallbackHandle = NULL;

L6474_BaseParameter_t param = {
    .stepMode = 0x01, // full step mode
    .OcdTh = 0x02,    // overcurrent threshold
    .TimeOnMin = 5,   // minimum on time
    .TimeOffMin = 10, // minimum off time
    .TorqueVal = 50,  // torque setting
    .TFast = 2        // fast decay time
};

static int spindleEnabled = 0;
static int spindleDirection = 0; // 0 = forwärts, 1 = rückwärts
static float spindleCurrentRPM = 0.0f;

int currentPosition = 0;
int referencePosition = 0;
bool referenceComplete = false;
#define STEPS_PER_MM 100                // adjust depending on your stepper motor and gear ratio
#define MIN_DISTANCE_FROM_REFERENCE 200 // Minimum steps (2mm) to move away from reference

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */

static int stepAsync(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h);
static int cancelStep(void *pPWM);

int StepperMoveRelative(int relPos);
int StepperMoveWithSpeed(int absPos, int speedMmPerMin);
int StepperConfigHandler(const char *param, int argc, char **argv, int startIndex);
int StepperMove(int absPos);
int StepperReference(int timeout);
int StepperReset(void);
int SpindleStart(int rpm);
int SpindleStop(void);
int StepperPosition(void);
int SpindleStatus(void);
int StepperCancel(void);
int StepperStatus(void);
void InitComponentsTask(void *pvParameters);

extern void initialise_stdlib_abstraction(void);
void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  __asm volatile("bkpt #0");
  for (;;)
  {
    ;
  }
}
/*-----------------------------------------------------------*/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pcTaskName;
  (void)pxTask;

  taskDISABLE_INTERRUPTS();
  __asm volatile("bkpt #0");
  for (;;)
  {
    ;
  }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void *StepLibraryMalloc(unsigned int size)
{
  return malloc(size);
}

static void StepLibraryFree(const void *const ptr)
{
  free((void *)ptr);
}

static int StepDriverSpiTransfer(void *pIO, char *pRX, const char *pTX, unsigned int length)
{
  (void)pIO; // intentionally unused
  for (unsigned int i = 0; i < length; i++)
  {
    HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)pTX + i, (uint8_t *)pRX + i, 1, HAL_MAX_DELAY) != HAL_OK) // prüft ob Transmit erfolgreich war
    {
      return -1;
    }
    HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_SET);
  }
  return 0;
}

static int StepDriverReset(void *pGPO, const int ena)
{
  (void)pGPO;

  if (ena)
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_RESET);
  else
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_SET);

  return 0;
}

static void StepLibraryDelay(unsigned int ms)
{
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static int Step(void *pPWM, int dir, unsigned int numPulses)
{
  (void)pPWM;
  HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, (dir > 0));
  for (unsigned int i = 0; i < numPulses; i++)
  {
    HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 1);
    StepLibraryDelay(1);
    HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 0);
    StepLibraryDelay(1);
  }
  return 0;
}

static int cancelStep(void *pPWM)
{
  (void)pPWM;
  if (!stepperMoving)
  {
    return 0;
  }

  printf("Cancelling movement via library callback\r\n");
  int result = L6474_StopMovement(stepperHandle);

  if (stepperDoneCallback != NULL)
  {
    stepperDoneCallback(stepperCallbackHandle);
  }

  stepperMoving = false;
  stepperCancelRequested = false;

  return result;
}

void SPINDLE_SetDirection(SpindleHandle_t h, void *context, int backward)
{
  (void)h;
  (void)context;
  spindleDirection = backward;

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

  printf("Setting spindle duty cycle: %.2f%%\r\n", dutyCycle * 100.0f);

  uint32_t pulse = (uint32_t)(dutyCycle * htim2.Init.Period);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse);
}

void SPINDLE_EnaPWM(SpindleHandle_t h, void *context, int ena)
{
  (void)h;
  (void)context;

  spindleEnabled = ena;

  if (ena)
  {
    printf("Enabling spindle\r\n");

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

    if (spindleDirection)
    {
      // rückwärts
      HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, GPIO_PIN_SET);
    }
    else
    {
      // forwärts
      HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, GPIO_PIN_RESET);
    }
  }
  else
  {
    printf("Disabling spindle\r\n");

    // PMW aus
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);

    HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, GPIO_PIN_RESET);
  }
}

void InitComponentsTask(void *pvParameters)
{
  L6474x_Platform_t p;
  p.malloc = StepLibraryMalloc;
  p.free = StepLibraryFree;
  p.transfer = StepDriverSpiTransfer;
  p.reset = StepDriverReset;
  p.sleep = StepLibraryDelay;
  // only if async is disabled
  // p.step = Step;
  p.stepAsync = stepAsync;
  p.cancelStep = cancelStep;

  stepperHandle = L6474_CreateInstance(&p, &hspi1, NULL, &htim2);
  if (stepperHandle == NULL)
  {
    printf("Failed to create stepper motor driver instance\r\n");
    Error_Handler();
  }

  int result = 0;
  result |= L6474_ResetStandBy(stepperHandle);
  result |= L6474_SetBaseParameter(&param);
  result |= L6474_Initialize(stepperHandle, &param);
  result |= L6474_SetPowerOutputs(stepperHandle, 1);

  if (result != 0)
  {
    printf("Stepper initialization failed with error code: %d\r\n", result);
    Error_Handler();
  }

  printf("Stepper initialization complete\r\n");

  // Spindel init 
  SpindlePhysicalParams_t s;
  s.maxRPM = 9000.0f;
  s.minRPM = -9000.0f;
  s.absMinRPM = 1600.0f;
  s.setDirection = SPINDLE_SetDirection;
  s.setDutyCycle = SPINDLE_SetDutyCycle;
  s.enaPWM = SPINDLE_EnaPWM;
  s.context = NULL;

  spindleHandle = SPINDLE_CreateInstance(4 * configMINIMAL_STACK_SIZE,
                                         configMAX_PRIORITIES - 3,
                                         consoleHandle, &s);

  if (spindleHandle == NULL)
  {
    printf("Failed to create spindle controller instance\r\n");
    Error_Handler();
  }

  printf("Spindle initialization complete\r\n");
  printf("All components initialized successfully\r\n");

  // keep task alive
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(100)); // destress
  }
}

int StepperHandler(int argc, char **argv, void *ctx)
{
  if (argc < 1)
  {
    printf("Invalid number of arguments\r\n");
    return -1;
  }

  if (strcmp(argv[0], "move") == 0)
  {
    if (argc < 2)
    {
      printf("Missing position argument\r\n");
      return -1;
    }

    int pos = atoi(argv[1]);
    bool isRelative = false;
    bool isAsync = false;
    int speed = 0;

    // Parse flags
    for (int i = 2; i < argc; i++)
    {
      if (strcmp(argv[i], "-r") == 0)
      {
        isRelative = true;
      }
      else if (strcmp(argv[i], "-a") == 0)
      {
        isAsync = true;
      }
      else if (strcmp(argv[i], "-s") == 0)
      {
        if (i + 1 < argc)
        {
          speed = atoi(argv[i + 1]);
          i++; // skip the next argument as its the speed value
        }
      }
    }

    // relativ -> absolut
    if (isRelative)
    {
      pos = currentPosition + pos;
    }

    int result = 0;

    if (stepperMoving)
    {
      printf("Error: Stepper already moving\r\n");
      return -1;
    }

    if (isRelative)
    {
      result = StepperMoveRelative(pos);
    }
    else if (speed > 0)
    {
      result = StepperMoveWithSpeed(pos, speed);
    }
    else if (isAsync)
    {

      // Asynchronous movement
      int stepsToMove = pos - currentPosition;
      int dir = (stepsToMove > 0) ? 1 : 0;
      unsigned int numSteps = abs(stepsToMove);

      result = stepAsync(NULL, dir, numSteps, NULL, stepperHandle);
    }
    else
    {
      // Standard movement
      result = StepperMove(pos);
    }

    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "reference") == 0)
  {
    bool keepEnabled = false;
    bool skipReference = false;
    int timeout = 0;

    // Parse flags
    for (int i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "-e") == 0)
      {
        keepEnabled = true;
      }
      else if (strcmp(argv[i], "-s") == 0)
      {
        skipReference = true;
      }
      else if (strcmp(argv[i], "-t") == 0)
      {
        if (i + 1 < argc)
        {
          timeout = atoi(argv[i + 1]);
          i++; // Skip the next argument as it's the timeout value
        }
      }
    }

    int result;
    if (skipReference)
    {
      // Skip reference run and set current position as reference
      printf("Skipping reference run, using current position\r\n");
      currentPosition = 0;
      referencePosition = 0;
      referenceComplete = true;
      result = 0;
    }
    else
    {

      result = StepperReference(timeout);
    }


    if (result == 0 && !keepEnabled)
    {
      L6474_SetPowerOutputs(stepperHandle, 0);
    }

    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "reset") == 0)
  {
    int result = StepperReset();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "position") == 0)
  {
    int result = StepperPosition();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "cancel") == 0)
  {
    int result = StepperCancel();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "status") == 0)
  {
    int result = StepperStatus();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "config") == 0)
  {
    if (argc < 2)
    {
      printf("Missing config parameter\r\n");
      return -1;
    }

    int result = StepperConfigHandler(argv[1], argc, argv, 2);
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else
  {
    printf("Unknown stepper command: %s\r\n", argv[0]);
    return -1;
  }
}

int StepperConfigHandler(const char *param, int argc, char **argv, int startIndex)
{

  if (!param)
  {
    printf("Missing config parameter\r\n");
    return -1;
  }

  // Default: read mode
  bool writeMode = false;
  int value = 0;
  float floatValue = 0.0f;
  bool isFloatParam = false;

  // Check if -v flag is provided (write mode)
  if (startIndex < argc && strcmp(argv[startIndex], "-v") == 0)
  {
    writeMode = true;

    // Make sure we have a value after -v
    if (startIndex + 1 >= argc)
    {
      printf("Missing value for parameter %s\r\n", param);
      return -1;
    }

    // Parameters posmax, posmin, posref, and mmperturn are float values ?
    if (strcmp(param, "posmax") == 0 ||
        strcmp(param, "posmin") == 0 ||
        strcmp(param, "posref") == 0 ||
        strcmp(param, "mmperturn") == 0)
    {
      floatValue = atof(argv[startIndex + 1]);
      isFloatParam = true;
    }
    else
    {
      // All other parameters are integers
      value = atoi(argv[startIndex + 1]);
    }
  }

  // Process the particular config parameter
  if (strcmp(param, "powerena") == 0)
  {
    if (writeMode)
    {
      // Set power output state
      if (value != 0 && value != 1)
      {
        printf("Invalid value for powerena - must be 0 or 1\r\n");
        return -1;
      }
      return L6474_SetPowerOutputs(stepperHandle, value);
    }
    else
    {
      // Get power output state
      L6474x_State_t state;
      L6474_GetState(stepperHandle, &state);
      printf("%d\r\n", (state == stENABLED) ? 1 : 0);
      return 0;
    }
  }
  else if (strcmp(param, "torque") == 0)
  {
    if (writeMode)
    {
      // Set torque value (current setting)
      if (value < 0 || value > 255)
      {
        printf("Invalid value for torque - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TORQUE, value);
    }
    else
    {
      // Get torque value
      int currentVal;
      int result = L6474_GetProperty(stepperHandle, L6474_PROP_TORQUE, &currentVal);
      if (result == 0)
      {
        printf("%d\r\n", currentVal);
      }
      return result;
    }
  }
  else if (strcmp(param, "throvercurr") == 0)
  {
    if (writeMode)
    {
      // Set overcurrent threshold
      if (value < 0 || value > 3) // ocdth1500mA = 0x03
      {
        printf("Invalid value for throvercurr - must be between 0 and 3\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_OCDTH, value);
    }
    else
    {
      // Get overcurrent threshold
      int currentVal;
      int result = L6474_GetProperty(stepperHandle, L6474_PROP_OCDTH, &currentVal);
      if (result == 0)
      {
        printf("%d\r\n", currentVal);
      }
      return result;
    }
  }
  else if (strcmp(param, "stepmode") == 0)
  {
    if (writeMode)
    {

      if (value < 0 || value > 4)
      {
        printf("Invalid value for stepmode - must be between 0 and 4\r\n");
        return -1;
      }
      return L6474_SetStepMode(stepperHandle, (L6474x_StepMode_t)value);
    }
    else
    {

      L6474x_StepMode_t mode;
      int result = L6474_GetStepMode(stepperHandle, &mode);
      if (result == 0)
      {
        printf("%d\r\n", mode);
      }
      return result;
    }
  }
  else if (strcmp(param, "timeoff") == 0)
  {
    L6474x_State_t state;
    L6474_GetState(stepperHandle, &state);
    if (writeMode && state == stENABLED)
    {
      printf("Cannot change timeoff while outputs are enabled\r\n");
      return -1;
    }

    if (writeMode)
    {
      // Set off time
      if (value < 0 || value > 255)
      {
        printf("Invalid value for timeoff - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TOFF, value);
    }
    else
    {
      // Get off time
      int currentVal;
      int result = L6474_GetProperty(stepperHandle, L6474_PROP_TOFF, &currentVal);
      if (result == 0)
      {
        printf("%d\r\n", currentVal);
      }
      return result;
    }
  }
  else if (strcmp(param, "timeon") == 0)
  {

    L6474x_State_t state;
    L6474_GetState(stepperHandle, &state);
    if (writeMode && state == stENABLED)
    {
      printf("Cannot change timeon while outputs are enabled\r\n");
      return -1;
    }

    if (writeMode)
    {

      if (value < 0 || value > 255)
      {
        printf("Invalid value for timeon - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TON, value);
    }
    else
    {

      int currentVal;
      int result = L6474_GetProperty(stepperHandle, L6474_PROP_TON, &currentVal);
      if (result == 0)
      {
        printf("%d\r\n", currentVal);
      }
      return result;
    }
  }
  else if (strcmp(param, "timefast") == 0)
  {
    // Check if output is enabled
    L6474x_State_t state;
    L6474_GetState(stepperHandle, &state);
    if (writeMode && state == stENABLED)
    {
      printf("Cannot change timefast while outputs are enabled\r\n");
      return -1;
    }

    if (writeMode)
    {

      if (value < 0 || value > 255)
      {
        printf("Invalid value for timefast - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TFAST, value);
    }
    else
    {

      int currentVal;
      int result = L6474_GetProperty(stepperHandle, L6474_PROP_TFAST, &currentVal);
      if (result == 0)
      {
        printf("%d\r\n", currentVal);
      }
      return result;
    }
  }
  else if (strcmp(param, "mmperturn") == 0)
  {

    static float mmPerTurn = 1.0f; // adjust as needed

    if (writeMode)
    {
      if (floatValue <= 0)
      {
        printf("Invalid value for mmperturn - must be positive\r\n");
        return -1;
      }
      mmPerTurn = floatValue;
      return 0;
    }
    else
    {
      printf("%.3f\r\n", mmPerTurn);
      return 0;
    }
  }
  else if (strcmp(param, "stepsperturn") == 0)
  {

    static int stepsPerTurn = 200; // Default: 200 , adjust as needed

    if (writeMode)
    {
      if (value <= 0)
      {
        printf("Invalid value for stepsperturn - must be positive\r\n");
        return -1;
      }
      stepsPerTurn = value;
      return 0;
    }
    else
    {
      printf("%d\r\n", stepsPerTurn);
      return 0;
    }
  }
  else if (strcmp(param, "posmax") == 0 ||
           strcmp(param, "posmin") == 0 ||
           strcmp(param, "posref") == 0)
  {

    static float posMax = 100.0f;  // Default max position (mm)
    static float posMin = -100.0f; // Default min position (mm)
    static float posRef = 0.0f;    // Default reference position (mm)

    if (writeMode)
    {
      if (strcmp(param, "posmax") == 0)
      {
        posMax = floatValue;
      }
      else if (strcmp(param, "posmin") == 0)
      {
        posMin = floatValue;
      }
      else
      { // posref
        posRef = floatValue;
      }
      return 0;
    }
    else
    {
      if (strcmp(param, "posmax") == 0)
      {
        printf("%.3f\r\n", posMax);
      }
      else if (strcmp(param, "posmin") == 0)
      {
        printf("%.3f\r\n", posMin);
      }
      else
      { // posref
        printf("%.3f\r\n", posRef);
      }
      return 0;
    }
  }
  else
  {
    printf("Unknown config parameter: %s\r\n", param);
    return -1;
  }
}

int SpindleHandler(int argc, char **argv, void *ctx)
{
  if (argc < 1)
  {
    printf("Invalid number of arguments\r\n");
    return -1;
  }

  if (strcmp(argv[0], "start") == 0)
  {
    if (argc < 2)
    {
      printf("Missing RPM argument\r\n");
      return -1;
    }

    int rpm = atoi(argv[1]);
    int result = SpindleStart(rpm);
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "stop") == 0)
  {
    int result = SpindleStop();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else if (strcmp(argv[0], "status") == 0)
  {
    int result = SpindleStatus();
    printf("%s\r\n", result == 0 ? "OK" : "FAIL");
    return result;
  }
  else
  {
    printf("Unknown spindle command: %s\r\n", argv[0]);
    return -1;
  }
}

int StepperMove(int absPos)
{
  printf("Moving stepper to position: %d\r\n", absPos);


  if (!referenceComplete)
  {
    printf("Error: Reference run required before absolute movement\r\n");
    return -1;
  }


  int stepsToMove = absPos - currentPosition;

  if (stepsToMove == 0)
  {

    return 0;
  }

  printf("Moving %d steps\r\n", stepsToMove);

  // Move the stepper motor
  if (L6474_StepIncremental(stepperHandle, stepsToMove) != 0)
  {
    printf("Movement failed\r\n");
    return -1;
  }


  currentPosition = absPos;

  return 0;
}

static void StepperMovementComplete(L6474_Handle_t handle)
{
  printf("Asynchronous movement completed\r\n");

  // Update position
  currentPosition = targetPosition;

  // Call the library's callback if it was provided
  if (stepperDoneCallback != NULL)
  {
    stepperDoneCallback(stepperCallbackHandle);
    stepperDoneCallback = NULL; // Clear the callback
  }

  stepperMoving = false;
}


void StepperMoveTask(void *pvParameters)
{
  int target = targetPosition;


  int stepsToMove = target - currentPosition;

  if (stepsToMove == 0 || stepperCancelRequested)
  {

    stepperMoving = false;
    stepperCancelRequested = false;
    vTaskDelete(NULL);
    return;
  }

  printf("Moving %d steps\r\n", stepsToMove);

  // Check for cancellation one more time before starting movement
  if (stepperCancelRequested)
  {
    printf("Movement cancelled before starting\r\n");
    stepperMoving = false;
    stepperCancelRequested = false;
    vTaskDelete(NULL);
    return;
  }


  if (L6474_StepIncremental(stepperHandle, stepsToMove) != 0)
  {
    printf("Movement failed\r\n");
    stepperMoving = false;
    stepperCancelRequested = false;
    vTaskDelete(NULL);
    return;
  }

 
  if (stepperCancelRequested)
  {
    printf("Movement was cancelled during execution\r\n");
    stepperCancelRequested = false;
  }
  else
  {

    currentPosition = target;
    printf("Movement completed\r\n");
  }

  stepperMoving = false;
  vTaskDelete(NULL);
}

int StepperMoveWithSpeed(int absPos, int speedMmPerMin)
{

  printf("Moving to position %d at speed %d mm/min\r\n", absPos, speedMmPerMin);


  if (!referenceComplete)
  {
    printf("Error: Reference run required before absolute movement\r\n");
    return -1;
  }

  // Check if already moving
  if (stepperMoving)
  {
    printf("Error: Stepper already moving\r\n");
    return -1;
  }


  int stepsToMove = absPos - currentPosition;

  if (stepsToMove == 0)
  {

    return 0;
  }

  // Convert speed from mm/min to steps/sec
  float speedStepsPerSec = (speedMmPerMin * STEPS_PER_MM) / 60.0f;


  if (speedStepsPerSec < 10) // min speed - adjust as needed
  {
    printf("Speed capped to minimum\r\n");
    speedStepsPerSec = 10;
  }
  else if (speedStepsPerSec > 1000) // max speed - adjust as needed
  {
    printf("Speed capped to maximum\r\n");
    speedStepsPerSec = 1000;
  }

  // Calculate delay between steps in milliseconds
  int delayMs = (int)(1000.0f / speedStepsPerSec);

  printf("Moving %d steps with %d ms delay between steps\r\n", stepsToMove, delayMs);


  int stepDirection = (stepsToMove > 0) ? 1 : -1;
  int remainingSteps = abs(stepsToMove);

  while (remainingSteps > 0)
  {
    if (L6474_StepIncremental(stepperHandle, stepDirection) != 0)
    {
      printf("Movement failed\r\n");
      return -1;
    }


    currentPosition += stepDirection;
    remainingSteps--;

    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }

  return 0;
}

static int stepAsync(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{
  stepperDoneCallback = doneClb;
  stepperCallbackHandle = h;

  HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);

  stepperMoving = true;
  stepperCancelRequested = false;


  int stepsToMove = dir ? numPulses : -numPulses;
  targetPosition = currentPosition + stepsToMove;

  printf("Starting movement: dir=%d, steps=%u\r\n", dir, numPulses);

  int result = L6474_StepIncremental(h, stepsToMove);

  if (result != 0)
  {
    stepperMoving = false;
    return result;
  }
  StepperMovementComplete(h);
  return 0;
}

int StepperStatus(void)
{


  // 1. Get internal state machine state
  // Map application states to the required hex values:
  // 0x0: scsINIT - Initial state
  // 0x1: scsREF - Reference state
  // 0x2: scsDIS - Disabled state
  // 0x4: scsENA - Enabled state
  // 0x8: scsFLT - Fault state
  int stateCode;
  L6474x_State_t driverState = stINVALID;
  L6474_Status_t driverStatus;

  if (!stepperHandle)
  {
    // System not initialized
    stateCode = 0x0; // scsINIT
  }
  else
  {
    // Get the L6474 library state and status in one go
    L6474_GetState(stepperHandle, &driverState);
    L6474_GetStatus(stepperHandle, &driverStatus);

    // Determine our application state based on L6474 state and our system state
    if (driverStatus.OCD || driverStatus.TH_SD || driverStatus.UVLO)
    {
      stateCode = 0x8; // scsFLT - Fault detected
    }
    else if (driverState == stRESET)
    {
      stateCode = 0x0; // scsINIT
    }
    else if (!referenceComplete)
    {
      stateCode = 0x1; // scsREF - Reference run needed
    }
    else if (driverState == stENABLED)
    {
      stateCode = 0x4; // scsENA - Enabled and ready
    }
    else
    {
      stateCode = 0x2; // scsDIS - Disabled but initialized
    }
  }

  // 2. Get the driver status (error bits)
  int statusRegister = 0;

  if (stepperHandle)
  {
    // We already have driverStatus from above, no need to call GetStatus again

    // Map each status bit according to the specification:
    // bit 0: DIRECTION bit
    statusRegister |= (driverStatus.DIR ? 0x01 : 0);

    // bit 1: HIGH-Z Driver Output bit
    statusRegister |= (driverStatus.HIGHZ ? 0x02 : 0);

    // bit 2: NOTPERF_CMD bit
    statusRegister |= (driverStatus.NOTPERF_CMD ? 0x04 : 0);

    // bit 3: OVERCURRENT_DETECTION bit
    statusRegister |= (driverStatus.OCD ? 0x08 : 0);

    // bit 4: ONGOING bit
    statusRegister |= (driverStatus.ONGOING ? 0x10 : 0);

    // bit 5: TH_SD bit (thermal shutdown)
    statusRegister |= (driverStatus.TH_SD ? 0x20 : 0);

    // bit 6: TH_WARN bit (thermal warning)
    statusRegister |= (driverStatus.TH_WARN ? 0x40 : 0);

    // bit 7: UVLO Undervoltage Lockout bit
    statusRegister |= (driverStatus.UVLO ? 0x80 : 0);

    // bit 8: WRONG_CMD bit
    statusRegister |= (driverStatus.WRONG_CMD ? 0x100 : 0);
  }

  // 3. Get asynchronous operation flag (1 if movement is pending, 0 otherwise)
  int asyncOp = stepperMoving ? 1 : 0;

  // Output status values in the required format
  printf("0x%X\r\n", stateCode);
  printf("0x%X\r\n", statusRegister);
  printf("%d\r\n", asyncOp);

  return 0;
}

int StepperCancel(void)
{

  printf("Cancelling stepper movement\r\n");

  if (!stepperMoving)
  {
    printf("No movement to cancel - stepper is already idle\r\n");
    return 0; // Should succeed even if not moving
  }

  // Use the library's built-in movement cancellation
  int result = L6474_StopMovement(stepperHandle);

  if (result != 0)
  {
    printf("Error cancelling movement\r\n");
    return -1;
  }

  printf("Movement cancelled successfully\r\n");
  return 0;
}

int StepperReference(int timeout)
{

  printf("Performing reference run\r\n");

  // Define variables
  bool referenceSwitchHit = false;

  // Add timeout implementation
  TickType_t startTime = xTaskGetTickCount();
  TickType_t timeoutTicks = timeout * 1000 / portTICK_PERIOD_MS;

  // Check if reference switch is already active
  referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET);

  // If reference switch is already active, move away first
  if (referenceSwitchHit)
  {
    printf("Already at reference position, moving away first\r\n");

    // Move away from reference switch (positive direction)
    if (L6474_StepIncremental(stepperHandle, MIN_DISTANCE_FROM_REFERENCE) != 0)
    {
      printf("Failed to move away from reference position\r\n");
      return -1;
    }

    // Check if we successfully moved away from the switch
    referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET);
    if (referenceSwitchHit)
    {
      printf("Still at reference position after moving away, hardware error\r\n");
      return -1;
    }
  }

  // Now move towards the reference switch (negative direction) slowly
  printf("Moving towards reference position\r\n");

  // Move one step at a time until reference switch is hit
  while (!referenceSwitchHit)
  {
    // Check for timeout
    if (timeout > 0 && (xTaskGetTickCount() - startTime) > timeoutTicks)
    {
      printf("Reference run timeout\r\n");
      return -1;
    }
    // Move one step in negative direction
    if (L6474_StepIncremental(stepperHandle, -1) != 0)
    {
      printf("Failed during reference movement\r\n");
      return -1;
    }

    // Small delay between steps for slow movement
    StepLibraryDelay(10);

    // Check if reference switch is hit
    referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET);
  }

  // Reference position found
  printf("Reference position found\r\n");

  // Reset position tracking
  currentPosition = 0;
  referencePosition = 0;
  referenceComplete = true;

  return 0;
}

int StepperReset(void)
{

  printf("Resetting stepper\r\n");

  // Reset position tracking
  currentPosition = 0;
  referenceComplete = false;

  // Reset hardware by asserting and releasing the reset pin
  HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_SET);   // Assert reset
  StepLibraryDelay(10);                                                  // Small delay to ensure reset is recognized
  HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_RESET); // Release reset
  StepLibraryDelay(10);                                                  // Small delay to ensure stable state after reset

  int result = 0;
  result |= L6474_ResetStandBy(stepperHandle);
  result |= L6474_SetBaseParameter(&param);
  result |= L6474_Initialize(stepperHandle, &param);
  result |= L6474_SetPowerOutputs(stepperHandle, 1);

  return result == 0 ? 0 : -1;
}

int StepperMoveRelative(int relPos)
{
  printf("Moving stepper relatively by: %d\r\n", relPos);

  printf("Moving %d steps\r\n", relPos);

  // Move the stepper motor
  if (L6474_StepIncremental(stepperHandle, relPos) != 0)
  {
    printf("Movement failed\r\n");
    return -1;
  }

  // Update current position
  currentPosition += relPos;

  return 0;
}

int StepperPosition(void)
{


  // Convert the current position from steps to mm
  float positionMm = (float)currentPosition / STEPS_PER_MM;

  // Output the position as a float
  printf("%.3f\r\n", positionMm);

  return 0;
}


int SpindleStart(int rpm)
{

  printf("Starting spindle at %d RPM\r\n", rpm);

  // Store the target RPM for status reporting
  spindleCurrentRPM = (float)rpm;

  // Set direction based on RPM sign
  int direction = (rpm < 0) ? 1 : 0;
  spindleDirection = direction;

  // Call the platform functions that the spindle library would call
  SPINDLE_SetDirection(spindleHandle, NULL, direction);

  // Calculate duty cycle based on RPM
  // We need to convert the RPM to a duty cycle between 0.0 and 1.0
  float absRPM = (float)abs(rpm);

  // Get the configured limits from our parameters
  SpindlePhysicalParams_t *params = (SpindlePhysicalParams_t *)spindleHandle; // This is just for reference
  float maxRPM = 9000.0f;                                                     // Use the same values as in main
  float absMinRPM = 1600.0f;

  // Cap the RPM to limits
  if (absRPM < absMinRPM)
  {
    printf("Requested RPM below minimum - using minimum RPM: %.1f\r\n", absMinRPM);
    absRPM = absMinRPM;
    spindleCurrentRPM = direction ? -absMinRPM : absMinRPM;
  }

  if (absRPM > maxRPM)
  {
    printf("Requested RPM above maximum - using maximum RPM: %.1f\r\n", maxRPM);
    absRPM = maxRPM;
    spindleCurrentRPM = direction ? -maxRPM : maxRPM;
  }

  // Calculate duty cycle (linear mapping from min RPM to max RPM)
  float dutyCycle = (absRPM - absMinRPM) / (maxRPM - absMinRPM);

  // Apply the duty cycle
  SPINDLE_SetDutyCycle(spindleHandle, NULL, dutyCycle);

  // Enable the spindle
  SPINDLE_EnaPWM(spindleHandle, NULL, 1);

  spindleEnabled = 1;

  return 0;
}

int SpindleStop(void)
{

  printf("Stopping spindle\r\n");

  // Only perform stop actions if the spindle is actually running
  if (spindleEnabled)
  {
    // Disable spindle
    SPINDLE_EnaPWM(spindleHandle, NULL, 0);

    // Reset RPM tracking
    spindleCurrentRPM = 0.0f;
    spindleEnabled = 0;
  }
  else
  {
    printf("Spindle already stopped\r\n");
  }

  return 0; // Always return success
}

int SpindleStatus(void)
{
  if (spindleEnabled)
  {
    printf("RUNNING\r\n");
    printf("Direction: %s\r\n", spindleDirection ? "BACKWARD" : "FORWARD");
    printf("Current RPM: %.1f\r\n", spindleCurrentRPM);
  }
  else
  {
    printf("STOPPED\r\n");
  }

  return 0;
}

// -------------------------------------------------------------------------------------------------------------------
static int CapabilityFunc(int argc, char **argv, void *ctx)
// --------------------------------------------------------------------------------------------------------------------
{
  (void)argc;
  (void)argv;
  (void)ctx;
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\nOK",
         0, // has spindle
         0, // has spindle status
         0, // has stepper
         0, // has stepper move relative
         0, // has stepper move speed
         0, // has stepper move async
         0, // has stepper status
         0, // has stepper refrun
         0, // has stepper refrun timeout
         0, // has stepper refrun skip
         0, // has stepper refrun stay enabled
         0, // has stepper reset
         0, // has stepper position
         0, // has stepper config
         0, // has stepper config torque
         0, // has stepper config throvercurr
         0, // has stepper config powerena
         0, // has stepper config stepmode
         0, // has stepper config timeoff
         0, // has stepper config timeon
         0, // has stepper config timefast
         0, // has stepper config mmperturn
         0, // has stepper config posmax
         0, // has stepper config posmin
         0, // has stepper config posref
         0, // has stepper config stepsperturn
         0  // has stepper cancel
  );
  return 0;
}

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  consoleHandle = CONSOLE_CreateInstance(4 * configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5);
  if (consoleHandle == NULL)
  {
    printf("Failed to create console instance\r\n");
    Error_Handler();
  }

  int result = 0;
  result |= CONSOLE_RegisterCommand(consoleHandle, "capability", "Shows what the program is capable of", CapabilityFunc, NULL);
  result |= CONSOLE_RegisterCommand(consoleHandle, "stepper", "Standard stepper command followed by subcommands", StepperHandler, NULL);
  result |= CONSOLE_RegisterCommand(consoleHandle, "spindle", "Spindle control commands", SpindleHandler, NULL);

  // Check if any command registration failed
  if (result != 0)
  {
    printf("Failed to register one or more commands\r\n");
    Error_Handler();
  }

  if (xTaskCreate(&InitComponentsTask, "InitComponents", 2000, NULL, 2, &InitComponentsTaskHandle) != pdPASS)
  {
    printf("Failed to create initialization task\r\n");
    Error_Handler();
  }

  printf("System init start\r\n");

  (void)CapabilityFunc;

  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  return -1;
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct =
      {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct =
      {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig =
      {0};
  TIM_MasterConfigTypeDef sMasterConfig =
      {0};
  TIM_OC_InitTypeDef sConfigOC =
      {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4499;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct =
      {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin | LED_RED_Pin | LED_BLUE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, STEP_RSTN_Pin | STEP_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, SPINDLE_ENA_L_Pin | SPINDLE_ENA_R_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USR_BUTTON_Pin */
  GPIO_InitStruct.Pin = USR_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USR_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPINDLE_SI_R_Pin */
  GPIO_InitStruct.Pin = SPINDLE_SI_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SPINDLE_SI_R_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin LED_BLUE_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin | LED_RED_Pin | LED_BLUE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : STEP_RSTN_Pin */
  GPIO_InitStruct.Pin = STEP_RSTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(STEP_RSTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STEP_DIR_Pin */
  GPIO_InitStruct.Pin = STEP_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(STEP_DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STEP_FLAG_Pin */
  GPIO_InitStruct.Pin = STEP_FLAG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(STEP_FLAG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPINDLE_ENA_L_Pin SPINDLE_ENA_R_Pin */
  GPIO_InitStruct.Pin = SPINDLE_ENA_L_Pin | SPINDLE_ENA_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : STEP_SPI_CS_Pin */
  GPIO_InitStruct.Pin = STEP_SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(STEP_SPI_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STEP_PULSE_Pin */
  GPIO_InitStruct.Pin = STEP_PULSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(STEP_PULSE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : REFERENCE_MARK_Pin LIMIT_SWITCH_Pin */
  GPIO_InitStruct.Pin = REFERENCE_MARK_Pin | LIMIT_SWITCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SPINDLE_SI_L_Pin */
  GPIO_InitStruct.Pin = SPINDLE_SI_L_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SPINDLE_SI_L_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void vAssertCalled(const char *const pcFileName, unsigned long ulLine)
{
  volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

  /* Parameters are not used. */
  (void)ulLine;
  (void)pcFileName;

  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
     the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
     value. */
    while (ulSetToNonZeroInDebuggerToContinue == 0)
    {
    }
  }
  taskEXIT_CRITICAL();
}

int __stdout_put_char(int ch)
{
  uint8_t val = ch;
  while ((huart3.Instance->ISR & UART_FLAG_TXE) == 0)
    ;
  huart3.Instance->TDR = val;
  while ((huart3.Instance->ISR & UART_FLAG_TC) == 0)
    ;
  return 0;
}

int __stdin_get_char(void)
{
  if (huart3.Instance->ISR & UART_FLAG_ORE)
    huart3.Instance->ICR = UART_CLEAR_OREF;

  if (huart3.Instance->ISR & UART_FLAG_NE)
    huart3.Instance->ICR = UART_CLEAR_NEF;

  if (huart3.Instance->ISR & UART_FLAG_FE)
    huart3.Instance->ICR = UART_CLEAR_FEF;

  if ((huart3.Instance->ISR & UART_FLAG_RXNE) == 0)
    return -1;
  return huart3.Instance->RDR;
}
/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct =
      {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO_URO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
   */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x60000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  portENTER_CRITICAL();

  printf("HAL_ASSERT: %s:::%u\r\n", (char *)file, (unsigned int)line);
  assert(0);

  portEXIT_CRITICAL();
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
