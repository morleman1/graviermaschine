################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/LibRTOSConsole/src/Console.c 

OBJS += \
./Core/Src/Console/Console.o 

C_DEPS += \
./Core/Src/Console/Console.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/Console/Console.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/LibRTOSConsole/src/Console.c Core/Src/Console/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-Console

clean-Core-2f-Src-2f-Console:
	-$(RM) ./Core/Src/Console/Console.cyclo ./Core/Src/Console/Console.d ./Core/Src/Console/Console.o ./Core/Src/Console/Console.su

.PHONY: clean-Core-2f-Src-2f-Console

