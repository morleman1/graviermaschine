#include "main.h"
#include "init.h"
#include "LibL6474.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "LibL6474Config.h"
#include "stm32f7xx_hal_spi.h"
#include "stm32f7xx_hal_tim.h"
#include "FreeRTOS.h"
#include "task.h"

L6474_Handle_t stepperHandle = NULL;
TaskHandle_t stepperMoveTaskHandle = NULL;
volatile bool stepperMoving = false; // für async movement
volatile bool stepperCancelRequested = false;
volatile int targetPosition = 0;

StepperContext stepper_ctx;

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

int currentPosition = 0;
int referencePosition = 0;
bool referenceComplete = false;
#define STEPS_PER_MM 100                // adjust depending on your stepper motor and gear ratio
#define MIN_DISTANCE_FROM_REFERENCE 200 // Minimum steps (2mm) to move away from reference

int StepperMoveRelative(int relPos);
int StepperMoveWithSpeed(int absPos, int speedMmPerMin);
int StepperConfigHandler(const char *param, int argc, char **argv, int startIndex);
int StepperMove(int absPos);
int StepperReference(int timeout);
int StepperReset(void);
int StepperCancel(void);
int StepperStatus(void);
int StepperPosition(void);
static int stepAsync(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h);
static int cancelStep(void *pPWM);
int Step(void* pPWM, int dir, unsigned int numPulses);



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
int Step(void* pPWM, int dir, unsigned int numPulses) {
  (void)pPWM; // Unused in this implementation

  // Set direction pin
  HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

  // Generate pulses
  for (unsigned int i = 0; i < numPulses; ++i) {
    // Set STEP_PULSE pin high
    HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, GPIO_PIN_SET);
    StepLibraryDelay(1); // 1 ms High

    // Set STEP_PULSE pin low
    HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, GPIO_PIN_RESET);
    StepLibraryDelay(1); // 1 ms Low
  }

  return 0; // Success
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


void InitStepper(ConsoleHandle_t consoleHandle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* htim1, TIM_HandleTypeDef* tim4_handle)
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

  stepperHandle = L6474_CreateInstance(&p, &hspi1, NULL, &htim1);

  CONSOLE_RegisterCommand(consoleHandle, "stepper", "Standard stepper command followed by subcommands", StepperHandler, NULL);

}