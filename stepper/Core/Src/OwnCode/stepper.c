#include "stepper.h"
#include "LibL6474.h"

#define STEPS_PER_TURN 200
#define RESOLUTION 16
#define MM_PER_TURN 4

typedef enum {
    scsINIT = 0,
    scsREF,
    scsDIS,
    scsENA,
    scsFLT
} StepperState;

static struct {
    int INIT;
    int REF;
    int DIS;
    int ENA;
    int FLT;
} scs = {scsINIT, scsREF, scsDIS, scsENA, scsFLT};

typedef struct {
    L6474_Handle_t h;
    int is_powered; //change
    int is_referenced;
    int is_running;
    int error_code;
    void (*done_callback)(L6474_Handle_t);
    int remaining_pulses;
    TIM_HandleTypeDef* htim1;
    TIM_HandleTypeDef* htim4;
    StepperState state;
} StepperContext_t;

typedef struct {
    void* pPWM;
    int dir;
    unsigned int numPulses;
    void (*doneClb)(L6474_Handle_t);
    L6474_Handle_t h;
} StepTaskParams;



static int StepTimerCancelAsync(void* pPWM);
void set_speed(StepperContext_t* StepperContext, int steps_per_second);

L6474x_Platform_t p;
StepperContext_t StepperContext;


//---------Basic---Functions--------------
static void* StepLibraryMalloc(unsigned int size) 
{ 
    return malloc(size); 
}

static void StepLibraryFree(const void* const ptr) 
{ 
    free((void*)ptr); 
}

static int StepDriverSpiTransfer(void* pIO, char* pRX, const char* pTX, unsigned int length) {
    HAL_StatusTypeDef status = 0;
    for (unsigned int i = 0; i < length; i++) {
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);
        status |= HAL_SPI_TransmitReceive(pIO, (uint8_t*)pTX + i, (uint8_t*)pRX + i, 1, 10000);
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
        HAL_Delay(1);
    }
    if (status != HAL_OK) return -1;
    return 0;
}

static void StepDriverReset(void* pGPO, int ena) {
    (void)pGPO;
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
    StepperContext.is_running = 1;
    StepperContext.done_callback = doneClb;
    HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);
    start_tim1(numPulses);
    return 0;
}

static int StepTimerCancelAsync(void* pPWM) {
    (void)pPWM;
    if (StepperContext.is_running) {
        HAL_TIM_OnePulse_Stop_IT(StepperContext.htim1, TIM_CHANNEL_1);
        StepperContext.done_callback(StepperContext.h);
    }
    return 0;
}
//---------basic-functions-end-------------

//-----Command-specific-functions---------
static int Reset(StepperContext_t* StepperContext) {
    L6474_BaseParameter_t param;
    param.stepMode = smMICRO16;
    param.OcdTh = ocdth6000mA;
    param.TimeOnMin = 0x29;
    param.TimeOffMin = 0x29;
    param.TorqueVal = 0x11;
    param.TFast = 0x19;

    int result = 0;
    result |= L6474_ResetStandBy(StepperContext->h);
    result |= L6474_Initialize(StepperContext->h, &param);
    result |= L6474_SetPowerOutputs(StepperContext->h, 0);

    StepperContext->is_powered = 0;
    StepperContext->is_referenced = 0;
    StepperContext->is_running = 0;
    StepperContext->error_code = 0;
    StepperContext->state = scs.REF; // Transition INIT -> REF

    return result;
}

static int Reference(StepperContext_t* StepperContext, int argc, char** argv) {
    // Allow reference from REF, DIS, ENA
    if (!(StepperContext->state == scs.REF || StepperContext->state == scs.DIS || StepperContext->state == scs.ENA)) {
        printf("Reference run not allowed in current state\r\n");
        return -1;
    }
    int result = 0;
    int poweroutput = 0;
    uint32_t timeout_ms = 0;
    if (argc == 2) {
        if (strcmp(argv[1], "-s") == 0) {
            StepperContext->is_referenced = 1;
            L6474_SetAbsolutePosition(StepperContext->h, 0);
            StepperContext->state = scs.DIS; // REF -> DIS
            return result;
        } else if (strcmp(argv[1], "-e") == 0) {
            poweroutput = 1;
            StepperContext->state = scs.ENA; // REF -> ENA
        } else {
            printf("Invalid argument for reference\r\n");
            return -1;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "-t") == 0) {
            timeout_ms = atoi(argv[2]) * 1000;
        } else {
            printf("Invalid argument for reference\r\n");
            return -1;
        }
    }

    const uint32_t start_time = HAL_GetTick();
    result |= L6474_SetPowerOutputs(StepperContext->h, 1);
    if (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
        set_speed(StepperContext, 500);
        L6474_StepIncremental(StepperContext->h, 100000000);
        while (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
            if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
                StepTimerCancelAsync(NULL);
                printf("Timeout while waiting for reference switch\r\n");
                StepperContext->error_code = 1;
                StepperContext->state = scs.FLT;
                return -1;
            }
        }
        StepTimerCancelAsync(NULL);
    }
    L6474_StepIncremental(StepperContext->h, -1000000000);
    while (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET) {
        if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
            StepTimerCancelAsync(NULL);
            printf("Timeout while waiting for reference switch\r\n");
            StepperContext->error_code = 2;
            StepperContext->state = scs.FLT;
            return -1;
        }
    }
    StepTimerCancelAsync(NULL);

    StepperContext->is_referenced = 1;
    L6474_SetAbsolutePosition(StepperContext->h, 0);
    result |= L6474_SetPowerOutputs(StepperContext->h, poweroutput);

    // After reference, go to DIS or ENA depending on poweroutput
    StepperContext->state = poweroutput ? scs.ENA : scs.DIS;
    return result;
}

static int Position( StepperContext_t* StepperContext, int argc, char** argv) {
            int position;
        L6474_GetAbsolutePosition(StepperContext->h, &position);
        printf("Current position: %d\n\r", (position * MM_PER_TURN) / (STEPS_PER_TURN * RESOLUTION));
}

static int Status(StepperContext_t* StepperContext, int argc, char** argv) {

    const char* state_str[] = {"INIT", "REF", "DIS", "ENA", "FLT"};
        printf("State: %s, Power: %d, Referenced: %d, Running: %d, Error: %d\r\n",
            state_str[StepperContext->state],
            StepperContext->is_powered,
            StepperContext->is_referenced,
            StepperContext->is_running,
            StepperContext->error_code);
    return 0;
}

static int Move(StepperContext_t* StepperContext, int argc, char** argv) {
    if (StepperContext->state != scs.ENA) {
        printf("Stepper not enabled (state=%d)\r\n", StepperContext->state);
        return -1;
    }
    if (StepperContext->is_powered != 1) {
        printf("Stepper not powered\r\n");
        return -1;
    }
    if (StepperContext->is_referenced != 1) {
        printf("Stepper not referenced\r\n");
        return -1;
    }
    if (argc < 2) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }

    int position = atoi(argv[1]);
    int speed = 1000;
    int is_async = 0;
    int is_relative = 0;

    for (int i = 2; i < argc;) {
        if (strcmp(argv[i], "-a") == 0) {
            is_async = 1;
            i++;
        } else if (strcmp(argv[i], "-r") == 0) {
            is_relative = 1;
            i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i == argc - 1) {
                printf("Invalid number of arguments\r\n");
                return -1;
            }
            speed = atoi(argv[i + 1]);
            i += 2;
        } else {
            printf("Invalid Flag\r\n");
            return -1;
        }
    }

    int steps_per_second = (speed * STEPS_PER_TURN * RESOLUTION) / (60 * MM_PER_TURN);
    if (steps_per_second < 1) {
        printf("Speed too small\r\n");
        return -1;
    }
    set_speed(StepperContext, steps_per_second);

    int steps = (position * STEPS_PER_TURN * RESOLUTION) / MM_PER_TURN;
    if (!is_relative) {
        int absolute_position;
        L6474_GetAbsolutePosition(StepperContext->h, &absolute_position);
        steps -= absolute_position;
    }

    if (is_async) {
        return L6474_StepIncremental(StepperContext->h, steps);
    } else {
        int result = L6474_StepIncremental(StepperContext->h, steps);
        while (StepperContext->is_running);
        return result;
    }
}

static int Config(StepperContext_t* StepperContext, int argc, char** argv) {
    if (argc < 2) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
    if (strcmp(argv[1], "powerena") == 0) {
        return powerena(StepperContext, argc, argv);
    } else {
        printf("Invalid command\r\n");
        return -1;
    }
}

//-----Command-specific-functions-end-----

//-----helper-functions---------
int powerena(StepperContext_t* StepperContext, int argc, char** argv) {
    if (argc == 2) {
        printf("Current Powerstate: %d\r\n", StepperContext->is_powered);
        return 0;
    } else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
        int ena = atoi(argv[3]);
        if (ena != 0 && ena != 1) {
            printf("Invalid argument for powerena\r\n");
            return -1;
        }
        StepperContext->is_powered = ena;
        if (ena) {
            StepperContext->state = scs.ENA; // DIS -> ENA
        } else {
            StepperContext->state = scs.DIS; // ENA -> DIS
        }
        return L6474_SetPowerOutputs(StepperContext->h, ena);
    } else {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
}

void set_speed(StepperContext_t* StepperContext, int steps_per_second) {
    int clk = HAL_RCC_GetHCLKFreq();
    int quotient = clk / (steps_per_second * 2);
    int i = 0;
    while ((quotient / (i + 1)) > 65535) i++;
    __HAL_TIM_SET_PRESCALER(StepperContext->htim4, i);
    __HAL_TIM_SET_AUTORELOAD(StepperContext->htim4, (quotient / (i + 1)) - 1);
    StepperContext->htim4->Instance->CCR4 = StepperContext->htim4->Instance->ARR / 2;
}

static int Initialize(StepperContext_t* StepperContext) {
    Reset(StepperContext);
    StepperContext->is_powered = 1;
    StepperContext->is_referenced = 1;
    StepperContext->state = scs.INIT;
    return L6474_SetPowerOutputs(StepperContext->h, 1);
}

static int StepperHandler(int argc, char** argv, void* ctx) {
    StepperContext_t* StepperContext = (StepperContext_t*)ctx;
    int result = 0;

    if (argc == 0) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
    if (strcmp(argv[0], "move") == 0) {
        result = Move(StepperContext, argc, argv);
    } else if (strcmp(argv[0], "reset") == 0) {
        result = Reset(StepperContext);
    } else if (strcmp(argv[0], "config") == 0) {
        result = Config(StepperContext, argc, argv);
    } else if (strcmp(argv[0], "reference") == 0) {
        result = Reference(StepperContext, argc, argv);
    } else if (strcmp(argv[0], "cancel") == 0) {
        result = StepTimerCancelAsync(NULL);
    } else if (strcmp(argv[0], "init") == 0) {
        result = Initialize(StepperContext);
    } else if (strcmp(argv[0], "position") == 0) {
        result = Position(StepperContext, argc, argv);
    } else if (strcmp(argv[0], "status") == 0) {
        result = Status(StepperContext, argc, argv);
    } else {
        printf("Invalid command\r\n");
        return -1;
    }
    if (result == 0) {
        printf("OK\r\n");
    } else {
        printf("FAIL\r\n");
    }
    return result;
}

void start_tim1(int pulses) {
    int current_pulses = (pulses >= 65535) ? 65535 : pulses;
    StepperContext.remaining_pulses = pulses - current_pulses;
    if (current_pulses != 1) {
        HAL_TIM_OnePulse_Stop_IT(StepperContext.htim1, TIM_CHANNEL_1);
        __HAL_TIM_SET_AUTORELOAD(StepperContext.htim1, current_pulses);
        HAL_TIM_GenerateEvent(StepperContext.htim1, TIM_EVENTSOURCE_UPDATE);
        HAL_TIM_OnePulse_Start_IT(StepperContext.htim1, TIM_CHANNEL_1);
        __HAL_TIM_ENABLE(StepperContext.htim1);
    } else {
        StepperContext.done_callback(StepperContext.h);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim) {
    if ((StepperContext.done_callback != 0) && ((htim->Instance->SR & (1 << 2)) == 0)) {
        if (StepperContext.remaining_pulses > 0) {
            start_tim1(StepperContext.remaining_pulses);
        } else {
            StepperContext.done_callback(StepperContext.h);
            StepperContext.is_running = 0;
        }
    }
}

void InitStepper(ConsoleHandle_t hconsole, SPI_HandleTypeDef* hspi1, 
    TIM_HandleTypeDef* htim1, TIM_HandleTypeDef* htim4) {

    HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
    HAL_TIM_PWM_Start(htim4, TIM_CHANNEL_4);

    p.malloc     = StepLibraryMalloc;
    p.free       = StepLibraryFree;
    p.transfer   = StepDriverSpiTransfer;
    p.reset      = StepDriverReset;
    p.sleep      = StepLibraryDelay;
    p.stepAsync  = StepTimerAsync;
    p.cancelStep = StepTimerCancelAsync;

    StepperContext.h = L6474_CreateInstance(&p, hspi1, NULL, htim1);
    StepperContext.htim1 = htim1;
    StepperContext.htim4 = htim4;
    StepperContext.state = scs.INIT;
    StepperContext.error_code = 0;

    CONSOLE_RegisterCommand(hconsole, "stepper", "Stepper main Command", StepperHandler, &StepperContext);
}
