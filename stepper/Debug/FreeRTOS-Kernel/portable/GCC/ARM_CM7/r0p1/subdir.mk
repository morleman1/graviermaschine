################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
<<<<<<< HEAD
C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c 
=======
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c 
>>>>>>> ffdde72ac9ab55556c7181c7d453721a4c6d8d6d

OBJS += \
./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o 

C_DEPS += \
./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.d 


# Each subdirectory must supply rules for building sources it contributes
<<<<<<< HEAD
FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o: C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
=======
FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.c FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
>>>>>>> ffdde72ac9ab55556c7181c7d453721a4c6d8d6d

clean: clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1:
	-$(RM) ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.cyclo ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.d ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.o ./FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/port.su

.PHONY: clean-FreeRTOS-2d-Kernel-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

