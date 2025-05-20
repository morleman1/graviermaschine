################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c 

OBJS += \
./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o 

C_DEPS += \
./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.d 


# Each subdirectory must supply rules for building sources it contributes
FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o: C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F746xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Stepper" -I"C:/Users/Max/Documents/local_projects/graviermaschine/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1:
	-$(RM) ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.cyclo ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.d ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.su

.PHONY: clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

