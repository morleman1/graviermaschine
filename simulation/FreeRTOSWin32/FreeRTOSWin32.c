
// --------------------------------------------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
// --------------------------------------------------------------------------------------------------------------------
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include "LibL6474.h"
#include "LibL6474Config.h"
#include "Console.h"
#include "Spindle.h"
// -----------Begin PFP------------------------------------------------------------------------------------------------
// STM32 HAL simulation macros
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_BUSY 2
#define HAL_TIMEOUT 3

#define HAL_UNLOCKED 0x00U
#define HAL_LOCKED 0x01U

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1

// Mock timer channels
#define TIM_CHANNEL_1 0x0000U
#define TIM_CHANNEL_2 0x0004U
#define TIM_CHANNEL_3 0x0008U
#define TIM_CHANNEL_4 0x000CU



// Define the TIM instances 
#ifndef TIM1
#define TIM1 ((TIM_TypeDef *)1)
#define TIM2 ((TIM_TypeDef *)2)
#define TIM3 ((TIM_TypeDef *)3)
#define TIM4 ((TIM_TypeDef *)4)
#endif

// Define missing HAL timer macros
#define __HAL_TIM_SET_COMPARE(htim, channel, compare) \
    do { \
        printf("[SIM] TIM%d: Set Compare for Channel %d to %u\n", \
           (int)((htim)->Instance), \
           (channel) == TIM_CHANNEL_1 ? 1 : ((channel) == TIM_CHANNEL_2 ? 2 : ((channel) == TIM_CHANNEL_3 ? 3 : 4)), \
           (unsigned int)(compare)); \
    } while(0)

//---------------------------------------------------------------------------------------------------------------------


#define HAL_MAX_DELAY      0xFFFFFFFFU

#define STEPS_PER_MM 100                // Adjust based on your stepper motor and mechanics
#define MIN_DISTANCE_FROM_REFERENCE 200 // Minimum steps (2mm) to move away from reference

// Global handles and variables (similar to main.c)
TaskHandle_t RunMotorTaskHandle;
TaskHandle_t InitComponentsTaskHandle = NULL;
L6474_Handle_t stepperHandle = NULL;
SpindleHandle_t spindleHandle = NULL;
ConsoleHandle_t consoleHandle = NULL;

TaskHandle_t stepperMoveTaskHandle = NULL;
volatile bool stepperMoving = false; // for async movement
volatile bool stepperCancelRequested = false;
volatile int targetPosition = 0;

static void (*stepperDoneCallback)(L6474_Handle_t) = NULL;
static L6474_Handle_t stepperCallbackHandle = NULL;

static int spindleEnabled = 0;
static int spindleDirection = 0; // 0 = forward, 1 = backward
static float spindleCurrentRPM = 0.0f;

int currentPosition = 0;
int referencePosition = 0;
bool referenceComplete = false;

// Base parameters for stepper motor
L6474_BaseParameter_t param = {
    .stepMode = 0x01, // Full step mode
    .OcdTh = 0x02,    // Overcurrent threshold
    .TimeOnMin = 5,   // Minimum on time
    .TimeOffMin = 10, // Minimum off time
    .TorqueVal = 50,  // Torque setting
    .TFast = 2        // Fast decay time
};

// Function prototypes
static int stepAsync(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h);
int StepperHandler(int argc, char **argv, void *ctx);
int SpindleHandler(int argc, char **argv, void *ctx);
int StepperMove(int absPos);
int StepperMoveRelative(int relPos);
int StepperMoveWithSpeed(int absPos, int speedMmPerMin);
int StepperReference(int timeout);
int StepperReset(void);
int StepperCancel(void);
int StepperPosition(void);
int StepperStatus(void);
int SpindleStart(int rpm);
int SpindleStop(void);
int SpindleStatus(void);
int StepperConfigHandler(const char *param, int argc, char **argv, int startIndex);
static int CapabilityFunc(int argc, char **argv, void *ctx);
void InitComponentsTask(void *pvParameters);
void StepperMoveTask(void *pvParameters);

// --------------------------------------------------------------------------------------------------------------------
SPI_HandleTypeDef hspi1 = { .Instance = SPI1, .ErrorCode = 0, .Lock = HAL_UNLOCKED, .hdmarx = 0, .hdmatx = 0 };
TIM_HandleTypeDef htim1 = { .Instance = TIM1, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim2 = { .Instance = TIM2, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim4 = { .Instance = TIM4, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };

TIM_HandleTypeDef htim3 = { .Instance = TIM3, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 }; // Added TIM3

// --------------------------------------------------------------------------------------------------------------------
void* pvPortMalloc(size_t xSize)
// --------------------------------------------------------------------------------------------------------------------
{
    void* p = malloc(xSize);
    return p;
}

// --------------------------------------------------------------------------------------------------------------------
void vPortFree(void* pv)
// --------------------------------------------------------------------------------------------------------------------
{
    free(pv);
}

// --------------------------------------------------------------------------------------------------------------------
size_t xPortGetFreeHeapSize(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return 0xFFFFFFFF;
}

// --------------------------------------------------------------------------------------------------------------------
size_t xPortGetMinimumEverFreeHeapSize(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return 0xFFFFFFFF;
}

//! No implementation needed, but stub provided in case application already calls vPortInitialiseBlocks
// --------------------------------------------------------------------------------------------------------------------
void vPortInitialiseBlocks(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return;
}

// --------------------------------------------------------------------------------------------------------------------
void vAssertCalled(const char* const pcFileName, unsigned long ulLine)
// --------------------------------------------------------------------------------------------------------------------
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

// --------------------------------------------------------------------------------------------------------------------
void vApplicationMallocFailedHook(void)
// --------------------------------------------------------------------------------------------------------------------
{
    taskDISABLE_INTERRUPTS();
    for (;;) { ; }
}

// --------------------------------------------------------------------------------------------------------------------
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName)
// --------------------------------------------------------------------------------------------------------------------
{
    (void)pcTaskName;
    (void)pxTask;

    taskDISABLE_INTERRUPTS();
    for (;;) { ; }
}




// remove this in case it is implemented by the Gated Mode Master Slave Timers in your Controller Logic!
// --------------------------------------------------------------------------------------------------------------------
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim)
// --------------------------------------------------------------------------------------------------------------------
{
    (void)htim;
}


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
  (void)pIO; //intentionally unused
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
  (void)pPWM; //intentionally unused
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
    return 0; // Nothing to cancel
  }

  printf("Cancelling movement via library callback\r\n");
  int result = L6474_StopMovement(stepperHandle);

  // Call the callback to signal completion if needed
  if (stepperDoneCallback != NULL)
  {
    stepperDoneCallback(stepperCallbackHandle);
  }

  stepperMoving = false;
  stepperCancelRequested = false;

  return result;
}

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
  // only if async is disabled
  // p.step = Step;
  p.stepAsync = stepAsync;
  p.cancelStep = cancelStep;

  // Create stepper motor driver instance
  stepperHandle = L6474_CreateInstance(&p, &hspi1, NULL, &htim2);
  if (stepperHandle == NULL)
  {
    printf("Failed to create stepper motor driver instance\r\n");
    //Error_Handler();
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
    //Error_Handler();
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
    //Error_Handler();
  }

  printf("Spindle initialization complete\r\n");
  printf("All components initialized successfully\r\n");

  // Keep this task alive
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(100)); // Sleep periodically
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
          i++; // Skip the next argument as it's the speed value
        }
      }
    }

    // If relative movement, convert to absolute
    if (isRelative)
    {
      pos = currentPosition + pos;
    }

    int result = 0;

    // Check if stepper is already moving
    if (stepperMoving)
    {
      printf("Error: Stepper already moving\r\n");
      return -1;
    }

    // Execute the appropriate movement function
    if (isRelative)
    {
      // Use the dedicated relative movement function
      result = StepperMoveRelative(pos);
    }
    else if (speed > 0)
    {
      // Movement with specified speed
      result = StepperMoveWithSpeed(pos, speed);
    }
    else if (isAsync)
    {

      // Asynchronous movement - calculate direction and steps
      int stepsToMove = pos - currentPosition;
      int dir = (stepsToMove > 0) ? 1 : 0;
      unsigned int numSteps = abs(stepsToMove);

      // Call the stepAsync function with the proper parameters
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
  // Check if the parameter is valid
  if (!param)
  {
    printf("Missing config parameter\r\n");
    return -1;
  }

  // Default: no value provided (read mode)
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

    // Parameters posmax, posmin, posref, and mmperturn are float values
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
      // Set step mode
      if (value < 0 || value > 4)
      {
        printf("Invalid value for stepmode - must be between 0 and 4\r\n");
        return -1;
      }
      return L6474_SetStepMode(stepperHandle, (L6474x_StepMode_t)value);
    }
    else
    {
      // Get step mode
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
    // Check if output is enabled (can't change this parameter while enabled)
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
    // Check if output is enabled (can't change this parameter while enabled)
    L6474x_State_t state;
    L6474_GetState(stepperHandle, &state);
    if (writeMode && state == stENABLED)
    {
      printf("Cannot change timeon while outputs are enabled\r\n");
      return -1;
    }

    if (writeMode)
    {
      // Set on time
      if (value < 0 || value > 255)
      {
        printf("Invalid value for timeon - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TON, value);
    }
    else
    {
      // Get on time
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
    // Check if output is enabled (can't change this parameter while enabled)
    L6474x_State_t state;
    L6474_GetState(stepperHandle, &state);
    if (writeMode && state == stENABLED)
    {
      printf("Cannot change timefast while outputs are enabled\r\n");
      return -1;
    }

    if (writeMode)
    {
      // Set fast decay time
      if (value < 0 || value > 255)
      {
        printf("Invalid value for timefast - must be between 0 and 255\r\n");
        return -1;
      }
      return L6474_SetProperty(stepperHandle, L6474_PROP_TFAST, value);
    }
    else
    {
      // Get fast decay time
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
    // This is a custom parameter that's not directly mapped to L6474
    static float mmPerTurn = 1.0f; // Default: 1mm per turn

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
    // This is a custom parameter that's not directly mapped to L6474
    static int stepsPerTurn = 200; // Default: 200 steps per turn for many steppers

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
    // These are custom parameters for position limits
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

// Task to handle asynchronous stepper movement
void StepperMoveTask(void *pvParameters)
{
  int target = targetPosition;

  // Calculate number of steps to move
  int stepsToMove = target - currentPosition;

  if (stepsToMove == 0 || stepperCancelRequested)
  {
    // Already at the target position or cancelled
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

  // Move the stepper motor
  if (L6474_StepIncremental(stepperHandle, stepsToMove) != 0)
  {
    printf("Movement failed\r\n");
    stepperMoving = false;
    stepperCancelRequested = false;
    vTaskDelete(NULL);
    return;
  }

  // Check if cancelled during movement
  if (stepperCancelRequested)
  {
    printf("Movement was cancelled during execution\r\n");
    stepperCancelRequested = false;
  }
  else
  {
    // Update current position only if not cancelled
    currentPosition = target;
    printf("Movement completed\r\n");
  }

  stepperMoving = false;
  vTaskDelete(NULL);
}

int StepperMoveWithSpeed(int absPos, int speedMmPerMin)
{
  // Implementation for moving with specific speed
  printf("Moving to position %d at speed %d mm/min\r\n", absPos, speedMmPerMin);

  // Check if reference has been completed
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

  // Calculate number of steps to move
  int stepsToMove = absPos - currentPosition;

  if (stepsToMove == 0)
  {
    // Already at the target position
    return 0;
  }

  // Convert speed from mm/min to steps/sec
  float speedStepsPerSec = (speedMmPerMin * STEPS_PER_MM) / 60.0f;

  // Cap the speed if necessary
  if (speedStepsPerSec < 10) // Minimum speed (adjust as needed)
  {
    printf("Speed capped to minimum\r\n");
    speedStepsPerSec = 10;
  }
  else if (speedStepsPerSec > 1000) // Maximum speed (adjust as needed)
  {
    printf("Speed capped to maximum\r\n");
    speedStepsPerSec = 1000;
  }

  // Calculate delay between steps in milliseconds
  int delayMs = (int)(1000.0f / speedStepsPerSec);

  printf("Moving %d steps with %d ms delay between steps\r\n", stepsToMove, delayMs);

  // Move the stepper motor one step at a time with controlled speed
  int stepDirection = (stepsToMove > 0) ? 1 : -1;
  int remainingSteps = abs(stepsToMove);

  while (remainingSteps > 0)
  {
    if (L6474_StepIncremental(stepperHandle, stepDirection) != 0)
    {
      printf("Movement failed\r\n");
      return -1;
    }

    // Update position
    currentPosition += stepDirection;
    remainingSteps--;

    // Delay for proper speed
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }

  return 0;
}

static int stepAsync(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{
  // Save the callback for when movement completes
  stepperDoneCallback = doneClb;
  stepperCallbackHandle = h;

  // Set direction
  HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);

  // Update our global tracking variables
  stepperMoving = true;
  stepperCancelRequested = false;

  // Calculate target position (needed by StepperMoveAsync)
  int stepsToMove = dir ? (int)numPulses : -(int)numPulses;
  targetPosition = currentPosition + stepsToMove;

  printf("Starting movement: dir=%d, steps=%u\r\n", dir, numPulses);

  // The library will generate the stepping pulses using appropriate timing
  // The key is we're NOT creating a task here - we're using the library's built-in
  // functionality to handle the stepping
  int result = L6474_StepIncremental(h, stepsToMove);

  if (result != 0)
  {
    stepperMoving = false;
    return result;
  }

  return 0;
}

int StepperStatus(void)
{
  // Implementation for getting stepper status

  // 1. Get the internal state machine state
  // Map our application states to the required hex values:
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
  // Implementation for cancelling stepper movement
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
  // Implementation for reference run
  printf("Performing reference run\r\n");

  // Define variables
  bool referenceSwitchHit = false;

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
  // Implementation for getting current position in mm

  // Convert the current position from steps to mm
  float positionMm = (float)currentPosition / STEPS_PER_MM;

  // Output the position as a float
  printf("%.3f\r\n", positionMm);

  return 0;
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
//----------------------------------VisualStudio mandatory for testing-----------------------------------------------


// --------------------------------------------------------------------------------------------------------------------
static int CapabilityFunc(int argc, char** argv, void* ctx)
// --------------------------------------------------------------------------------------------------------------------
{
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
        0 // has stepper cancel
    );
    return 0;
}
// --------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
// --------------------------------------------------------------------------------------------------------------------
{
    printf("Stepper & Spindle Control Simulation - Starting\r\n");

    // Initialize the console
    consoleHandle = CONSOLE_CreateInstance(4 * configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5);
    if (consoleHandle == NULL)
    {
        printf("Failed to create console instance\r\n");
        return -1;
    }

    // Register commands
    int result = 0;
    result |= CONSOLE_RegisterCommand(consoleHandle, "capability", "Shows what the program is capable of", CapabilityFunc, NULL);
    result |= CONSOLE_RegisterCommand(consoleHandle, "stepper", "Standard stepper command followed by subcommands", StepperHandler, NULL);
    result |= CONSOLE_RegisterCommand(consoleHandle, "spindle", "Spindle control commands", SpindleHandler, NULL);

    if (result != 0)
    {
        printf("Failed to register one or more commands\r\n");
        return -1;
    }

    // Create initialization task
    if (xTaskCreate(InitComponentsTask, "InitComponents", 2000, NULL, 2, &InitComponentsTaskHandle) != pdPASS)
    {
        printf("Failed to create initialization task\r\n");
        return -1;
    }

    printf("System init start\r\n");

    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    // We should never reach here if the scheduler is running
    printf("Scheduler ended unexpectedly\r\n");
    return -1;
}