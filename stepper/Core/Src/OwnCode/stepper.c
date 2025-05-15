
/*
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

struct {
  L6474_Handle_t StepperHandle;
  TIM_HandleTypeDef* htim1_handle;
  int is_powered;
  int is_referenced;
  int is_running;
  void (*done_callback)(L6474_Handle_t);
} StepperContext;

int state;
struct 
{
  int INIT;
  int REF;
  int DIS;
  int ENA;
  int FLT;
} scs;


//---------Basic---Functions--------------
static void* StepLibraryMalloc( unsigned int size )
{
     return malloc(size);
}

static void StepLibraryFree( const void* const ptr )
{
     free((void*)ptr);
}

static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
	// byte based access, so keep in mind that only single byte transfers are performed!

	HAL_StatusTypeDef status = 0;

	for ( unsigned int i = 0; i < length; i++ )
	{
		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);

		status |= HAL_SPI_TransmitReceive(pIO, (uint8_t*)pTX + i, (uint8_t*)pRX + i, 1, 10000);

		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);

		HAL_Delay(1);
	}

	if (status != HAL_OK) {
		return -1;
	}

	return 0;
}

static void StepDriverReset(void* pGPO, int ena)
{
	(void) pGPO;
	HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);

	return;
}

static void StepLibraryDelay()
{
	return;
}

static int StepTimerAsync(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h) {
	(void)pPWM;
	(void)h;

	stepper_ctx.is_running = 1;
	stepper_ctx.done_callback = doneClb;

	HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);



	start_tim1(numPulses);

	return 0;
}

static int StepTimerCancelAsync(void* pPWM)
{
	(void)pPWM;

	if (stepper_ctx.is_running) {
		HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
		stepper_ctx.done_callback(stepper_ctx.h);
	}

	return 0;
}
//---------Basic--fucntions-end-------------

//-----Command-specific-functions---------

//-----Command-specific-functions-end-----

//-----State-machine-functions---------
int Reset(StepperContext_t* StepperContext){
	L6474_BaseParameter_t param;
	param.stepMode = smMICRO16;
	param.OcdTh = ocdth6000mA;
	param.TimeOnMin = 0x29;
	param.TimeOffMin = 0x29;
	param.TorqueVal = 0x11;
	param.TFast = 0x19;

	int result = 0;

	result |= L6474_ResetStandBy(StepperContext->StepperHandle);
	result |= L6474_Initialize(StepperContext->StepperHandle, &param);
	result |= L6474_SetPowerOutputs(StepperContext->StepperHandle, 0);

  state = scs.REF;
	StepperContext->is_powered = 0;
	StepperContext->is_referenced = 0;
	StepperContext->is_running = 0;

	return result;
}


static int Reference(StepperContext* StepperContext, int argc, char** argv) {
	int result = 0;
	int poweroutput = 0;
	uint32_t timeout_ms = 0;
	if (argc == 2) {
		if (strcmp(argv[1], "-s") == 0) {
			//Referenzfahrt überspringen
			stepper_ctx->is_referenced = 1;
			L6474_SetAbsolutePosition(stepper_ctx->h, 0);
			return result;
		}
		else if (strcmp(argv[1], "-e") == 0) {
			// Power aktiv lassen
			poweroutput = 1;
      state = scs.ENA;
		}
		else {
			printf("Invalid argument for reference\r\n");
			return -1;
		}
	}
	else if(argc == 3){
		if (strcmp(argv[1], "-t") == 0) {
			timeout_ms = atoi(argv[2]) * 1000;
		}
		else {
			printf("Invalid argument for reference\r\n");
			return -1;
		}
	}

	const uint32_t start_time = HAL_GetTick();
	result |= L6474_SetPowerOutputs(stepper_ctx->h, 1);
	if(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
		// already at reference
		set_speed(stepper_ctx, 500);
		L6474_StepIncremental(stepper_ctx->h, 100000000);
		while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET){
			if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
				StepTimerCancelAsync(NULL);
				printf("Timeout while waiting for reference switch\r\n");
				return -1;
			}
		}
		StepTimerCancelAsync(NULL);
	}
	L6474_StepIncremental(stepper_ctx->h, -1000000000);
	while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET){
		if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
			StepTimerCancelAsync(NULL);
			printf("Timeout while waiting for reference switch\r\n");
			return -1;
		}
	}
	StepTimerCancelAsync(NULL);

	stepper_ctx->is_referenced = 1;
	L6474_SetAbsolutePosition(stepper_ctx->h, 0);
	result |= L6474_SetPowerOutputs(stepper_ctx->h, poweroutput);
	return result;
}

//---------Stepper-Init----------------
void InitStepper(ConsoleHandle_t consoleHandle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* htim1, TIM_HandleTypeDef* tim4_handle)
{

  HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
	HAL_TIM_PWM_Start(tim4_handle, TIM_CHANNEL_4);


  L6474x_Platform_t p;
  p.malloc = StepLibraryMalloc;
  p.free = StepLibraryFree;
  p.transfer = StepDriverSpiTransfer;
  p.reset = StepDriverReset;
  p.sleep = StepLibraryDelay;
  // only if async is disabled
  // p.step = Step;
  p.stepAsync  = StepTimerAsync;
  p.cancelStep = StepTimerCancelAsync;

  StepperContext.StepperHandle = L6474_CreateInstance(&p, &hspi1, NULL, &htim1);

  CONSOLE_RegisterCommand(consoleHandle, "stepper", "Standard stepper command followed by subcommands", StepperHandler, NULL);

  reset(StepperContext);
	stepper_ctx->is_powered = 1;
	stepper_ctx->is_referenced = 1;

	return L6474_SetPowerOutputs(stepper_ctx->h, 1);
  state = scs.INIT;
}

*/