#include "Spindle.h"
#include "Console.h"
#include "FreeRTOSConfig.h"
#include "main.h"

SpindleHandle_t spindleHandle = NULL;


static int spindleEnabled = 0;
static int spindleDirection = 0; // 0 = forwärts, 1 = rückwärts
static float spindleCurrentRPM = 0.0f;

int SpindleStart(int rpm);
int SpindleStop(void);
int SpindleStatus(void);



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

void SpindleInit(SpindleHandle_t handle)
{
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

  
  CONSOLE_RegisterCommand(consoleHandle, "spindle", "Spindle control commands", SpindleHandler, NULL);
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