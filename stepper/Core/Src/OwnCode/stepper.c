// implement updates from gelhorns
// understand the functions, rewrite them
// reformat everything
// figure out the mm, steps and time per turn so it is dynamic and correct
// Steppercontext as -> or . ?

#include "stepper.h"
#include "LibL6474.h"

#define TRACKLENGTH 295 // mm
#define MM_PER_TURN 4

typedef enum
{
    scsINIT = 0,
    scsREF,
    scsDIS,
    scsENA,
    scsFLT
} StepperState;

static struct
{
    int INIT;
    int REF;
    int DIS;
    int ENA;
    int FLT;
} scs = {scsINIT, scsREF, scsDIS, scsENA, scsFLT};

typedef struct
{
    L6474_Handle_t h;
    int is_powered; // change
    int is_referenced;
    int is_running;
    int error_code;
    void (*done_callback)(L6474_Handle_t);
    int remaining_pulses;
    TIM_HandleTypeDef *htim1;
    TIM_HandleTypeDef *htim4;
    StepperState state;
    L6474x_StepMode_t step_mode;
    int pos_min;
    int pos_max;
    int position_ref_steps;
    float mm_per_turn;
    float steps_per_turn;
    float mm_per_step;
    float mm_per_sec;

} StepperContext_t;

typedef struct
{
    void *pPWM;
    int dir;
    unsigned int numPulses;
    void (*doneClb)(L6474_Handle_t);
    L6474_Handle_t h;
} StepTaskParams;

static int StepTimerCancelAsync(void *pPWM);
void SetSpeed(StepperContext_t *StepperContext, int steps_per_sec);
int Powerena(StepperContext_t *StepperContext, int argc, char **argv);
void TimerStart(unsigned int pulses);

L6474x_Platform_t p;
StepperContext_t StepperContext;

//---------Basic---Functions--------------
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
    HAL_StatusTypeDef status = 0;
    for (unsigned int i = 0; i < length; i++)
    {
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);
        status |= HAL_SPI_TransmitReceive(pIO, (uint8_t *)pTX + i, (uint8_t *)pRX + i, 1, HAL_MAX_DELAY);
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
        HAL_Delay(1);
    }
    if (status != HAL_OK)
        return -1;
    return 0;
}

static void StepDriverReset(void *pGPO, int ena)
{
    (void)pGPO;
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);
    return;
}

static void StepLibraryDelay(unsigned int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static int StepTimerAsync(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{
    (void)pPWM;
    (void)h;
    StepperContext.is_running = 1;
    StepperContext.done_callback = doneClb;
    HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);
    TimerStart(numPulses);
    return 0;
}

static int StepTimerCancelAsync(void *pPWM)
{
    (void)pPWM;
    if (StepperContext.is_running)
    {
        if (HAL_TIM_OnePulse_Stop_IT(StepperContext.htim1, TIM_CHANNEL_1) != HAL_OK)
        {
            printf("Error: Failed to stop the timer\r\n");
            return -1;
        }
        StepperContext.done_callback(StepperContext.h);
        StepperContext.is_running = 0;
    }
    return 0;
}
//---------basic-functions-end-------------

//-----Command-specific-functions---------
/*
static int Step(void *pPWM, int dir, unsigned int numPulses)
{
    HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, dir); // set direction
    // generate pulses
    for (unsigned int i = 0; i < numPulses; i++)
    {
        HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 1);
        stepLibraryDelay(1);
        HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 0);
        stepLibraryDelay(1);
    }

    return 0;
}
*/
// WIP kind of done, parameters need to  be looked at
static int Reset(StepperContext_t *StepperContext)
{
    L6474_BaseParameter_t param;
    param.stepMode = smMICRO16;
    param.OcdTh = ocdth6000mA; // 3000mA ? cause requirements
    param.TimeOnMin = 0x29;
    param.TimeOffMin = 0x29;
    param.TorqueVal = 0x11;
    param.TFast = 0x19;
    int result = 0;

    result |= L6474_SetBaseParameter(&param); // changable
    result |= L6474_ResetStandBy(StepperContext->h);
    result |= L6474_Initialize(StepperContext->h, &param);
    result |= L6474_SetPowerOutputs(StepperContext->h, 0);

    if (result != 0)
    {
        printf("Failed to reset\r\n");
        return -1;
        StepperContext->state = scs.FLT;
    }
    StepperContext->is_powered = 0;
    StepperContext->is_referenced = 0;
    StepperContext->is_running = 0;
    StepperContext->error_code = 0;
    StepperContext->state = scs.REF; // Transition INIT -> REF

    StepperContext->mm_per_turn = MM_PER_TURN;
    StepperContext->steps_per_turn = (float)(1 << (StepperContext->step_mode + 2));
    StepperContext->mm_per_step = StepperContext->mm_per_turn / StepperContext->steps_per_turn;

    return result;
}
// Finished but maybe look at it again
static int Reference(StepperContext_t *StepperContext, int argc, char **argv)
{
    // Allow reference from REF state only
    if (!(StepperContext->state == scs.REF))
    {
        printf("Reference run not allowed in current state\r\n");
        return -1;
    }
    int step_amt = 0;
    uint32_t track_timer_stop = 0;
    int result = 0;
    int poweroutput = 0;
    uint32_t timeout_ms = 0;
    if (argc == 2)
    {
        if (strcmp(argv[1], "-s") == 0)
        {
            StepperContext->is_referenced = 1;
            L6474_SetAbsolutePosition(StepperContext->h, 0);
            StepperContext->state = scs.DIS; // REF -> DIS
            return result;
        }
        else if (strcmp(argv[1], "-e") == 0)
        {
            poweroutput = 1;
            StepperContext->state = scs.ENA; // REF -> ENA
        }
        else
        {
            printf("Invalid argument for reference\r\n");
            return -1;
        }
    }
    else if (argc == 3)
    {
        if (strcmp(argv[1], "-t") == 0)
        {
            timeout_ms = atoi(argv[2]) * 1000; // s to ms
        }
        else
        {
            printf("Invalid argument for reference time\r\n");
            return -1;
        }
    }

    const uint32_t start_time = HAL_GetTick();
    result |= L6474_SetPowerOutputs(StepperContext->h, 1);

    // is switch already pressed?
    if (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET)
    {
        // if yes, move away from it as long as it is pressed
        while (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET)
        {
            // if still pressed, wait
            L6474_StepIncremental(StepperContext->h, 1);
            if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms)
            {
                StepTimerCancelAsync(NULL);
                printf("Timeout while moving away from the switch\r\n");
                StepperContext->error_code = 1;
                StepperContext->state = scs.FLT;
                return -1;
            }
        }
        StepTimerCancelAsync(NULL);
    }
    // move to reference switch
    while (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET)
    {
        L6474_StepIncremental(StepperContext->h, -1);
        if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms)
        {
            StepTimerCancelAsync(NULL);
            printf("Timeout while waiting for reference switch\r\n");
            StepperContext->error_code = 2;
            StepperContext->state = scs.FLT;
            return -1;
        }
    }
    StepTimerCancelAsync(NULL);
    // move to limit switch from reference switch
    const uint32_t track_timer_start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(LIMIT_SWITCH_GPIO_Port, LIMIT_SWITCH_Pin) != GPIO_PIN_RESET)
    {
        L6474_StepIncremental(StepperContext->h, 1);
        step_amt += 1;
        if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms)
        {
            StepTimerCancelAsync(NULL);
            printf("Timeout while moving to limit switch\r\n");
            StepperContext->error_code = 2;
            StepperContext->state = scs.FLT;
            return -1;
        }
    }
    track_timer_stop = HAL_GetTick();
    StepTimerCancelAsync(NULL);

    // calc parameters from full run
    StepperContext->mm_per_step = (TRACKLENGTH / step_amt);
    StepperContext->mm_per_sec = (TRACKLENGTH / ((track_timer_stop - track_timer_start) * 1000.0f));
    StepperContext->is_referenced = 1;

    L6474_SetAbsolutePosition(StepperContext->h, 0);
    result |= L6474_SetPowerOutputs(StepperContext->h, poweroutput); // set power output if -e flag was set
    StepperContext->is_powered = poweroutput;
    // After reference, go to DIS or ENA depending on poweroutput
    StepperContext->state = poweroutput ? scs.ENA : scs.DIS;
    return result;
}
// WIP kind of done
static int Position(StepperContext_t *StepperContext, int argc, char **argv)
{
    if (StepperContext->state == scs.FLT)
    {
        printf("Stepper in fault state\r\n");
        return -1;
    }
    int position;
    L6474_GetAbsolutePosition(StepperContext->h, &position);
    printf("Current position: %d\n\r", (position));
    return 0;
}
// WIP kind of done
static int Status(StepperContext_t *StepperContext, int argc, char **argv)
{
    L6474_Status_t driverStatus;
    L6474_GetStatus(StepperContext->h, &driverStatus);

    unsigned int statusBits = 0;
    statusBits |= (driverStatus.DIR ? (1 << 0) : 0);
    statusBits |= (driverStatus.HIGHZ ? (1 << 1) : 0);
    statusBits |= (driverStatus.NOTPERF_CMD ? (1 << 2) : 0);
    statusBits |= (driverStatus.OCD ? (1 << 3) : 0);
    statusBits |= (driverStatus.ONGOING ? (1 << 4) : 0);
    statusBits |= (driverStatus.TH_SD ? (1 << 5) : 0);
    statusBits |= (driverStatus.TH_WARN ? (1 << 6) : 0);
    statusBits |= (driverStatus.UVLO ? (1 << 7) : 0);
    statusBits |= (driverStatus.WRONG_CMD ? (1 << 8) : 0);

    printf("0x%01X\r\n", StepperContext->state);
    printf("0x%04X\r\n", statusBits);
    printf("%d\r\n", StepperContext->is_running);

    return 0;
}
// WIP
static int Move(StepperContext_t *StepperContext, int argc, char **argv)
{
    // Preconditions
    if (StepperContext->state != scs.ENA)
    {
        printf("Error: Stepper not enabled (state=%d)\r\n", StepperContext->state);
        return -1;
    }
    if (StepperContext->is_powered != 1)
    {
        printf("Error: Stepper not powered\r\n");
        return -1;
    }
    if (StepperContext->is_referenced != 1)
    {
        printf("Error: Stepper not referenced\r\n");
        return -1;
    }
    if (argc < 2)
    {
        printf("Error: Invalid number of arguments\r\n");
        return -1;
    }

    // Parse arguments
    float target_position = atof(argv[1]);
    int speed = 1000; // Default speed in mm/min
    int is_relative = 0;
    int is_async = 0;

    for (int i = 2; i < argc;)
    {
        if (strcmp(argv[i], "-s") == 0)
        {
            if (i == argc - 1)
            {
                printf("Error: Missing speed value after -s flag\r\n");
                return -1;
            }
            speed = atoi(argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "-r") == 0)
        {
            is_relative = 1;
            i++;
        }
        else if (strcmp(argv[i], "-a") == 0)
        {
            is_async = 1;
            i++;
        }
        else
        {
            printf("Error: Invalid flag '%s'\r\n", argv[i]);
            return -1;
        }
    }

    // Cap speed to valid mechanical limits
    if (speed < 1)
    {
        printf("Error: Speed too low\r\n");
        return -1;
    }
    int max_speed = (int)(StepperContext->mm_per_sec * 60); // Convert mm/sec to mm/min
    if (speed > max_speed)
    {
        printf("Warning: Speed capped to maximum (%d mm/min)\r\n", max_speed);
        speed = max_speed;
    }

    // Calculate steps per second
    float steps_per_second = (speed * StepperContext->steps_per_turn) / (60.0f * StepperContext->mm_per_turn);
    SetSpeed(StepperContext, (int)steps_per_second);

    // Get current position
    int current_steps;
    L6474_GetAbsolutePosition(StepperContext->h, &current_steps);
    float current_position = (current_steps * StepperContext->mm_per_turn) / StepperContext->steps_per_turn;

    // Calculate target position
    float final_position = is_relative ? current_position + target_position : target_position;
    if (final_position < StepperContext->pos_min || final_position > StepperContext->pos_max)
    {
        printf("Error: Target position out of range\r\n");
        return -1;
    }

    // Calculate steps to move
    int target_steps = (int)((final_position * StepperContext->steps_per_turn) / StepperContext->mm_per_turn);
    int steps_to_move = target_steps - current_steps;

    if (steps_to_move == 0)
    {
        printf("No movement required\r\n");
        return 0;
    }

    // Prevent multiple asynchronous commands
    if (is_async && StepperContext->is_running)
    {
        printf("Error: Stepper is already moving asynchronously\r\n");
        return -1;
    }

    // Perform movement
    int result = 0;
    StepperContext->is_running = 1; // Mark stepper as running
    if (is_async)
    {
        result = L6474_StepIncremental(StepperContext->h, steps_to_move);
    }
    else
    {
        result = L6474_StepIncremental(StepperContext->h, steps_to_move);
        while (StepperContext->is_running)
        {
            StepLibraryDelay(1); // Wait for movement to complete
        }
    }

    // Check for errors
    if (result != 0)
    {
        printf("Error: Movement failed\r\n");
        StepperContext->state = scsFLT;
        return -1;
    }

    printf("Movement completed. Final position: %.2f mm\r\n", final_position);
    return 0;
}
// WIP
static int Config(StepperContext_t *StepperContext, int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Invalid number of arguments\r\n");
        return -1;
    }

    int result = 0;
    int pos = -1;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0)
        {
            pos = i;
            break;
        }
    }

    if (strcmp(argv[1], "powerena") == 0)
    {
        if (pos != -1)
        {
            int ena = atoi(argv[pos + 1]);
            result = SetPower(ena);
            if (ena)
            {
                StepperContext->state = scs.ENA; // DIS -> ENA
            }
            else
            {
                StepperContext->state = scs.DIS; // ENA -> DIS
            }
        }
        else
        {
            printf("Current Powerstate: %d\r\n", StepperContext->is_powered);
        }
    }
    else if (strcmp(argv[1], "torque") == 0)
    {
        if (pos != -1)
        {
            result = L6474_SetProperty(StepperContext->h, L6474_PROP_TORQUE, atoi(argv[pos + 1]));
        }
        else
        {
            int value = 0;
            result = L6474_GetProperty(StepperContext->h, L6474_PROP_TORQUE, &value);
            printf("%d\r\n", value);
        }
    }
    else if (strcmp(argv[1], "throvercurr") == 0)
    {
        if (pos != -1)
        {
            result = L6474_SetProperty(StepperContext->h, L6474_PROP_OCDTH, atoi(argv[pos + 1]));
        }
        else
        {
            int value = 0;
            result = L6474_GetProperty(StepperContext->h, L6474_PROP_OCDTH, &value);
            printf("%d\r\n", value);
        }
    }
    else if (strcmp(argv[1], "stepmode") == 0)
    {
        if (pos != -1)
        {
            L6474x_StepMode_t step_mode_l;
            switch (atoi(argv[pos + 1]))
            {
            case 1:
                step_mode_l = smFULL;
                break;
            case 2:
                step_mode_l = smHALF;
                break;
            case 4:
                step_mode_l = smMICRO4;
                break;
            case 8:
                step_mode_l = smMICRO8;
                break;
            case 16:
                step_mode_l = smMICRO16;
                break;
            default:
                printf("Error: Invalid step mode\r\n");
                return -1;
            }
            result = L6474_SetStepMode(StepperContext->h, step_mode_l);
            if (result == 0)
            {
                StepperContext->step_mode = step_mode_l;
                StepperContext->is_referenced = 0; // Force a new reference run
                StepperContext->state = scsREF;
                printf("Step mode updated. Please perform a reference run.\r\n");
            }
        }
        else
        {
            printf("%d\r\n", StepperContext->step_mode);
        }
    }
    else if (strcmp(argv[1], "timeon") == 0 || strcmp(argv[1], "timeoff") == 0 || strcmp(argv[1], "timefast") == 0)
    {
        if (StepperContext->state != scs.ENA)
        {
            printf("Error: Parameter can be changed in scsENA only!\n");
            return -1;
        }

        int property;
        if (strcmp(argv[1], "timeon") == 0)
        {
            property = L6474_PROP_TON;
        }
        else if (strcmp(argv[1], "timeoff") == 0)
        {
            property = L6474_PROP_TOFF;
        }
        else
        {
            property = L6474_PROP_TFAST;
        }
        if (pos != -1)
        {
            result = L6474_SetProperty(StepperContext->h, property, atoi(argv[pos + 1]));
        }
        else
        {
            int value = 0;
            result = L6474_GetProperty(StepperContext->h, property, &value);
            printf("%d\r\n", value);
        }
    }
    else if (strcmp(argv[1], "mmperturn") == 0)
    {
        if (pos != -1)
        {
            StepperContext->mm_per_step = atof(argv[pos + 1]) / (float)pow(2, StepperContext->mm_per_step + 2);
        }
        else
        {
            printf("%f\r\n", (StepperContext->mm_per_step * (float)pow(2, StepperContext->mm_per_step + 2)));
        }
    }
    else if (strcmp(argv[1], "posmin") == 0 || strcmp(argv[1], "posmax") == 0 || strcmp(argv[1], "posref") == 0)
    {
        int *position_steps;
        if (strcmp(argv[1], "posmin") == 0)
        {
            position_steps = &StepperContext->pos_min;
        }
        else if (strcmp(argv[1], "posmax") == 0)
        {
            position_steps = &StepperContext->pos_max;
        }
        else
        {
            position_steps = &StepperContext->position_ref_steps;
        }

        if (pos != -1)
        {
            float input = atof(argv[pos + 1]);
            *position_steps = (input * StepperContext->steps_per_turn) / StepperContext->mm_per_turn;
        }
        else
        {
            printf("%f\r\n", (*position_steps * StepperContext->mm_per_turn) / StepperContext->steps_per_turn);
        }
    }
    else
    {
        printf("Invalid config parameter\r\n");
        return -1;
    }

    return result;
}

// WIP kind of very done
int SetPower(int ena)
{
    if ((StepperContext.state != scs.ENA) && (StepperContext.state != scs.DIS))
    {
        printf("Power can only be set in state ENA or DIS\r\n");
        return -1;
    }
    if (ena != 0 && ena != 1)
    {
        printf("Invalid argument for ena\r\n");
        return -1;
    }
    StepperContext.is_powered = ena;
    return L6474_SetPowerOutputs(StepperContext.h, ena);
}
// WIP kind of done
void SetSpeed(StepperContext_t *StepperContext, int steps_per_sec)
{
    // Get the system clock frequency (e.g., 72 MHz)
    int clk = HAL_RCC_GetHCLKFreq();

    // Calculate the timer period for the desired step frequency
    // Multiply by 2 because the timer toggles once for the rising edge and once for the falling edge
    int timer_period = clk / (steps_per_sec * 2);

    // Initialize the prescaler to 0
    int i = 0;

    // make upscaler bigger until reload value is <= 16 bit
    while ((timer_period / (i + 1)) > 65535)
        i++;

    // Set the prescaler value to slow down the timer's counting rate
    __HAL_TIM_SET_PRESCALER(StepperContext->htim4, i);

    // Set the auto-reload value to define the timer period
    // Subtract 1 because the timer counts from 0 to ARR
    __HAL_TIM_SET_AUTORELOAD(StepperContext->htim4, (timer_period / (i + 1)) - 1);

    // ARR = Auto-reload register, we set it to 50% duty cycle
    StepperContext->htim4->Instance->CCR4 = StepperContext->htim4->Instance->ARR / 2;
}
// WIP kind of done
void TimerStart(unsigned int pulse_count)
{
    // Limit the number of pulses to a maximum of 16 bit
    int active_pulses = (pulse_count >= 65535) ? 65535 : pulse_count;
    StepperContext.remaining_pulses = pulse_count - active_pulses;
    if (active_pulses > 1)
    {
        // set timer params and start it
        HAL_TIM_OnePulse_Stop_IT(StepperContext.htim1, TIM_CHANNEL_1);
        __HAL_TIM_SET_AUTORELOAD(StepperContext.htim1, active_pulses);
        HAL_TIM_GenerateEvent(StepperContext.htim1, TIM_EVENTSOURCE_UPDATE);
        HAL_TIM_OnePulse_Start_IT(StepperContext.htim1, TIM_CHANNEL_1);
        __HAL_TIM_ENABLE(StepperContext.htim1);
    }
    else
    {
        // signal completion
        StepperContext.done_callback(StepperContext.h);
    }
}
// WIP kind of done
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    // check if pulse is finished
    if ((StepperContext.done_callback != 0) && ((htim->Instance->SR & (1 << 2)) == 0))
    {
        // process remaining pulses
        if (StepperContext.remaining_pulses > 0)
        {
            TimerStart(StepperContext.remaining_pulses);
        }
        else
        {
            // finished
            StepperContext.done_callback(StepperContext.h);
            StepperContext.is_running = 0;
        }
    }
}

static int StepperHandler(int argc, char **argv, void *ctx)
{
    StepperContext_t *StepperContext = (StepperContext_t *)ctx;
    int result = 0;

    if (argc == 0)
    {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
    if (strcmp(argv[0], "move") == 0)
    {
        result = Move(StepperContext, argc, argv);
    }
    else if (strcmp(argv[0], "reset") == 0)
    {
        result = Reset(StepperContext);
    }
    else if (strcmp(argv[0], "config") == 0)
    {
        result = Config(StepperContext, argc, argv);
    }
    else if (strcmp(argv[0], "reference") == 0)
    {
        result = Reference(StepperContext, argc, argv);
    }
    else if (strcmp(argv[0], "cancel") == 0)
    {
        result = StepTimerCancelAsync(NULL);
    }
    else if (strcmp(argv[0], "position") == 0)
    {
        result = Position(StepperContext, argc, argv);
    }
    else if (strcmp(argv[0], "status") == 0)
    {
        result = Status(StepperContext, argc, argv);
    }
    else
    {
        printf("Invalid command\r\n");
        return -1;
    }
    if (result == 0)
    {
        printf("OK\r\n");
    }
    else
    {
        printf("FAIL\r\n");
    }
    return result;
}

void InitStepper(ConsoleHandle_t hconsole, SPI_HandleTypeDef *hspi1,
                 TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim4)
{

    HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
    HAL_TIM_PWM_Start(htim4, TIM_CHANNEL_4);

    p.malloc = StepLibraryMalloc;
    p.free = StepLibraryFree;
    p.transfer = StepDriverSpiTransfer;
    p.reset = StepDriverReset;
    p.sleep = StepLibraryDelay;
    // p.step = Step;
    p.stepAsync = StepTimerAsync;
    p.cancelStep = StepTimerCancelAsync;

    StepperContext.h = L6474_CreateInstance(&p, hspi1, NULL, htim1);
    StepperContext.htim1 = htim1;
    StepperContext.htim4 = htim4;
    StepperContext.state = scs.INIT;
    StepperContext.error_code = 0;

    StepperContext.step_mode = smMICRO16;
    StepperContext.mm_per_turn = 4.0f;
    StepperContext.steps_per_turn = (float)(1 << (StepperContext.step_mode + 2));
    StepperContext.mm_per_step = StepperContext.mm_per_turn / StepperContext.steps_per_turn;

    CONSOLE_RegisterCommand(hconsole, "stepper", "Stepper main Command", StepperHandler, &StepperContext);
}
