################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/libs/LibSpindle/src/Spindle.c 

OBJS += \
./Core/Src/Spindle/Spindle.o 

C_DEPS += \
./Core/Src/Spindle/Spindle.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/Spindle/Spindle.o: C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/libs/LibSpindle/src/Spindle.c Core/Src/Spindle/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Stepper" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-Spindle

clean-Core-2f-Src-2f-Spindle:
	-$(RM) ./Core/Src/Spindle/Spindle.cyclo ./Core/Src/Spindle/Spindle.d ./Core/Src/Spindle/Spindle.o ./Core/Src/Spindle/Spindle.su

.PHONY: clean-Core-2f-Src-2f-Spindle

