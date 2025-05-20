################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/main.c \
../Core/Src/newlib_abs.c \
../Core/Src/stm32f7xx_hal_msp.c \
../Core/Src/stm32f7xx_it.c \
../Core/Src/system_stm32f7xx.c 

OBJS += \
./Core/Src/main.o \
./Core/Src/newlib_abs.o \
./Core/Src/stm32f7xx_hal_msp.o \
./Core/Src/stm32f7xx_it.o \
./Core/Src/system_stm32f7xx.o 

C_DEPS += \
./Core/Src/main.d \
./Core/Src/newlib_abs.d \
./Core/Src/stm32f7xx_hal_msp.d \
./Core/Src/stm32f7xx_it.d \
./Core/Src/system_stm32f7xx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Stepper" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/newlib_abs.cyclo ./Core/Src/newlib_abs.d ./Core/Src/newlib_abs.o ./Core/Src/newlib_abs.su ./Core/Src/stm32f7xx_hal_msp.cyclo ./Core/Src/stm32f7xx_hal_msp.d ./Core/Src/stm32f7xx_hal_msp.o ./Core/Src/stm32f7xx_hal_msp.su ./Core/Src/stm32f7xx_it.cyclo ./Core/Src/stm32f7xx_it.d ./Core/Src/stm32f7xx_it.o ./Core/Src/stm32f7xx_it.su ./Core/Src/system_stm32f7xx.cyclo ./Core/Src/system_stm32f7xx.d ./Core/Src/system_stm32f7xx.o ./Core/Src/system_stm32f7xx.su

.PHONY: clean-Core-2f-Src

