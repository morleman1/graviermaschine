################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/Code/tom_init.c \
../Core/Src/Code/tom_spindle.c \
../Core/Src/Code/tom_stepper.c 

OBJS += \
./Core/Src/Code/tom_init.o \
./Core/Src/Code/tom_spindle.o \
./Core/Src/Code/tom_stepper.o 

C_DEPS += \
./Core/Src/Code/tom_init.d \
./Core/Src/Code/tom_spindle.d \
./Core/Src/Code/tom_stepper.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/Code/%.o Core/Src/Code/%.su Core/Src/Code/%.cyclo: ../Core/Src/Code/%.c Core/Src/Code/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-Code

clean-Core-2f-Src-2f-Code:
	-$(RM) ./Core/Src/Code/tom_init.cyclo ./Core/Src/Code/tom_init.d ./Core/Src/Code/tom_init.o ./Core/Src/Code/tom_init.su ./Core/Src/Code/tom_spindle.cyclo ./Core/Src/Code/tom_spindle.d ./Core/Src/Code/tom_spindle.o ./Core/Src/Code/tom_spindle.su ./Core/Src/Code/tom_stepper.cyclo ./Core/Src/Code/tom_stepper.d ./Core/Src/Code/tom_stepper.o ./Core/Src/Code/tom_stepper.su

.PHONY: clean-Core-2f-Src-2f-Code

