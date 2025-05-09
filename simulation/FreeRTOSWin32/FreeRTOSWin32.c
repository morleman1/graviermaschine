
// --------------------------------------------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>

// --------------------------------------------------------------------------------------------------------------------
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include "LibL6474.h"
#include "LibL6474Config.h"

// -----------Begin PFP------------------------------------------------------------------------------------------------
#define HAL_MAX_DELAY      0xFFFFFFFFU


TaskHandle_t RunMotorTaskHandle;
TaskHandle_t InitMotorTaskHandle;

void InitMotor();

// --------------------------------------------------------------------------------------------------------------------
SPI_HandleTypeDef hspi1 = { .Instance = SPI1, .ErrorCode = 0, .Lock = HAL_UNLOCKED, .hdmarx = 0, .hdmatx = 0 };
TIM_HandleTypeDef htim1 = { .Instance = TIM1, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim2 = { .Instance = TIM2, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim4 = { .Instance = TIM4, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };

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


// remove this in case it is implemented by the Gated Mode Master Slave Timers in your Controller Logic!
// --------------------------------------------------------------------------------------------------------------------
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim)
// --------------------------------------------------------------------------------------------------------------------
{
    (void)htim;
}


/* USER CODE BEGIN 0 */
// --------------------------------------------------------------------------------------------------------------------
static void* StepLibraryMalloc(unsigned int size)
{
    return malloc(size);
}

static void StepLibraryFree(const void* const ptr)
{
    free((void*)ptr);
}

static int StepDriverSpiTransfer(void* pIO, char* pRX, const char* pTX, unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_RESET); // Select the SPI device
        if (HAL_SPI_TransmitReceive(pIO, pTX + i, pRX + i, 1, HAL_MAX_DELAY) != HAL_OK) 
        {
            HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_SET); // Deselect the SPI device
            return -1; // Error during SPI transfer
        }
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, GPIO_PIN_SET); // Deselect the SPI device

    }
    return 0;
}



static int StepDriverReset(void* pGPO, const int ena)
{
    (void)pGPO;

    if (ena)
        HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_RESET); // Release reset
    else
        HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, GPIO_PIN_SET); // Assert reset

    return 0;
}

static int StepLibraryDelay(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

static int Step(void* pPWM, int dir, unsigned int numPulses)
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

void InitMotorTask(void)
{
    L6474x_Platform_t p;

    p.malloc = StepLibraryMalloc;
    p.free = StepLibraryFree;
    p.transfer = StepDriverSpiTransfer;
    p.reset = StepDriverReset;
    p.sleep = StepLibraryDelay;
    p.step = Step;
    // p.cancelStep = StepTimerCancelAsync;

    // Create motor driver instance
    L6474_Handle_t h = L6474_CreateInstance(&p, &hspi1, NULL, NULL);
    if (h == NULL)
    {
        printf("Failed to create motor driver instance\r\n");
    }

    L6474_BaseParameter_t param = {
        .stepMode = 0x01,
        .OcdTh = 0x02,
        .TimeOnMin = 5,
        .TimeOffMin = 10,
        .TorqueVal = 50,
        .TFast = 2 };


    // reset all and take all initialization steps
    int result = 0;
    result |= L6474_ResetStandBy(h);
    result |= L6474_SetBaseParameter(&param);
    result |= L6474_Initialize(h, &param);
    result |= L6474_SetPowerOutputs(h, 1);

    if (result != 0)
    {
        printf("Motor initialization failed with error code: %d\r\n", result);
    }

    if (result == 0)
    {
        while (1)
        {
            result |= L6474_StepIncremental(h, -1);
        }
    }
    else
    {
        // error handling
    }
    return 0;
}
// --------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
// --------------------------------------------------------------------------------------------------------------------
{
    printf("Hello World\r\n");



    /* USER CODE BEGIN 2 */
    xTaskCreate(InitMotorTask, "InitMotorTask", 2000, NULL, 2, &InitMotorTaskHandle);

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
}