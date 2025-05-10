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

L6474_BaseParameter_t param = {
    .stepMode = 0x01, // Full step mode
    .OcdTh = 0x02,    // Overcurrent threshold
    .TimeOnMin = 5,   // Minimum on time
    .TimeOffMin = 10, // Minimum off time
    .TorqueVal = 50,  // Torque setting
    .TFast = 2        // Fast decay time
};

static int spindleEnabled = 0;
static int spindleDirection = 0; // 0 = forward, 1 = backward
static float spindleCurrentRPM = 0.0f;

int currentPosition = 0;
int referencePosition = 0;
bool referenceComplete = false;
#define STEPS_PER_MM 100                // Adjust based on your stepper motor and mechanics
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

int StepperMove(int absPos);
int StepperReference(int timeout);
int StepperReset(void);
int SpindleStart(int rpm);
int SpindleStop(void);
int SpindleStatus(void);
void InitComponentsTask(void *pvParameters); // Renamed from InitStepperTask

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
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_RESET); // Release reset
  else
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_SET); // Assert reset

  return 0;
}

static void StepLibraryDelay(unsigned int ms)
{
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static int Step(void *pPWM, int dir, unsigned int numPulses)
{

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

/*static void StepTimerCancelAsync(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{

}*/

// Set the spindle direction
void SPINDLE_SetDirection(SpindleHandle_t h, void *context, int backward)
{
  (void)h;
  (void)context;
  spindleDirection = backward;

  // GPIO polarity might need adjustment based on your H-bridge
  if (backward)
  {
    printf("Setting spindle direction: backward (counter-clockwise)\r\n");
  }
  else
  {
    printf("Setting spindle direction: forward (clockwise)\r\n");
  }
}

// Set the PWM duty cycle
void SPINDLE_SetDutyCycle(SpindleHandle_t h, void *context, float dutyCycle)
{
  (void)h;
  (void)context;

  printf("Setting spindle duty cycle: %.2f%%\r\n", dutyCycle * 100.0f);

  // Configure TIM2 for PWM
  // Adjust channel based on your hardware setup
  uint32_t pulse = (uint32_t)(dutyCycle * htim2.Init.Period);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse);
}

// Enable/disable the spindle
void SPINDLE_EnaPWM(SpindleHandle_t h, void *context, int ena)
{
  (void)h;
  (void)context;

  spindleEnabled = ena;

  if (ena)
  {
    printf("Enabling spindle\r\n");

    // Enable PWM output
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

    // Enable H-bridge based on direction
    if (spindleDirection)
    {
      // Backward direction
      HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, GPIO_PIN_SET);
    }
    else
    {
      // Forward direction
      HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, GPIO_PIN_RESET);
    }
  }
  else
  {
    printf("Disabling spindle\r\n");

    // Disable PWM output
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);

    // Disable both H-bridge outputs
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
  p.step = Step;
  // p.cancelStep = StepTimerCancelAsync;

  // Create stepper motor driver instance
  stepperHandle = L6474_CreateInstance(&p, &hspi1, NULL, &htim2);
  if (stepperHandle == NULL)
  {
    printf("Failed to create stepper motor driver instance\r\n");
    Error_Handler();
  }

  // reset all and take all initialization steps
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

  // Initialize Spindle
  SpindlePhysicalParams_t s;
  s.maxRPM = 9000.0f;
  s.minRPM = -9000.0f;
  s.absMinRPM = 1600.0f;
  s.setDirection = SPINDLE_SetDirection;
  s.setDutyCycle = SPINDLE_SetDutyCycle;
  s.enaPWM = SPINDLE_EnaPWM;
  s.context = NULL;

  // Create spindle instance (use consoleHandle that was created in main)
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

  // Keep this task alive
  while (1)
  {
     vTaskDelay(pdMS_TO_TICKS(100)); // Sleep periodically
  }
}

void StepperHandler(int argc, char **argv, void *ctx)
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

    int absPos = atoi(argv[1]);
    int result = StepperMove(absPos);
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
      // Perform reference run
      result = StepperReference(timeout);
    }

    // Handle power output state after reference
    if (result == 0 && !keepEnabled)
    {
      // Turn off power if not keeping enabled
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
  else
  {
    printf("Unknown stepper command: %s\r\n", argv[0]);
    return -1;
  }
}

void SpindleHandler(int argc, char **argv, void *ctx)
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
  // Implementation for moving stepper to absolute position
  printf("Moving stepper to position: %d\r\n", absPos);

  // Check if reference has been completed
  if (!referenceComplete)
  {
    printf("Error: Reference run required before absolute movement\r\n");
    return -1;
  }

  // Calculate number of steps to move
  int stepsToMove = absPos - currentPosition;

  if (stepsToMove == 0)
  {
    // Already at the target position
    return 0;
  }

  printf("Moving %d steps\r\n", stepsToMove);

  // Move the stepper motor
  if (L6474_StepIncremental(stepperHandle, stepsToMove) != 0)
  {
    printf("Movement failed\r\n");
    return -1;
  }

  // Update current position
  currentPosition = absPos;

  return 0;
}

int StepperReference(int timeout)
{
  // Implementation for reference run
  printf("Performing reference run\r\n");

  // Define variables
  bool referenceSwitchHit = false;
  int result = 0;
  // Add timeout implementation
  TickType_t startTime = xTaskGetTickCount();
  TickType_t timeoutTicks = timeout * 1000 / portTICK_PERIOD_MS;

  // Check if reference switch is already active
  referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_SET);

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
    referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_SET);
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
    referenceSwitchHit = (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_SET);
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
  // Implementation for stepper reset
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

// Implementation of spindle functions
int SpindleStart(int rpm)
{
  // Implementation for starting spindle at given RPM
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
  // Implementation for stopping spindle
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
  // Implementation for getting spindle status
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

// --------------------------------------------------------------------------------------------------------------------
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
