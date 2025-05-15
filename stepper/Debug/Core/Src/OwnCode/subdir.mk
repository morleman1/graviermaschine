################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/OwnCode/spindle.c \
<<<<<<< HEAD
../Core/Src/OwnCode/stepper.c \
../Core/Src/OwnCode/temp_stepper.c 

OBJS += \
./Core/Src/OwnCode/spindle.o \
./Core/Src/OwnCode/stepper.o \
./Core/Src/OwnCode/temp_stepper.o 

C_DEPS += \
./Core/Src/OwnCode/spindle.d \
./Core/Src/OwnCode/stepper.d \
./Core/Src/OwnCode/temp_stepper.d 
=======
../Core/Src/OwnCode/stepper.c 

OBJS += \
./Core/Src/OwnCode/spindle.o \
./Core/Src/OwnCode/stepper.o 

C_DEPS += \
./Core/Src/OwnCode/spindle.d \
./Core/Src/OwnCode/stepper.d 
>>>>>>> ffdde72ac9ab55556c7181c7d453721a4c6d8d6d


# Each subdirectory must supply rules for building sources it contributes
Core/Src/OwnCode/%.o Core/Src/OwnCode/%.su Core/Src/OwnCode/%.cyclo: ../Core/Src/OwnCode/%.c Core/Src/OwnCode/subdir.mk
<<<<<<< HEAD
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/maxor/Documents/local_projects/HW-nahes_Programmieren/Graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
=======
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
>>>>>>> ffdde72ac9ab55556c7181c7d453721a4c6d8d6d

clean: clean-Core-2f-Src-2f-OwnCode

clean-Core-2f-Src-2f-OwnCode:
<<<<<<< HEAD
	-$(RM) ./Core/Src/OwnCode/spindle.cyclo ./Core/Src/OwnCode/spindle.d ./Core/Src/OwnCode/spindle.o ./Core/Src/OwnCode/spindle.su ./Core/Src/OwnCode/stepper.cyclo ./Core/Src/OwnCode/stepper.d ./Core/Src/OwnCode/stepper.o ./Core/Src/OwnCode/stepper.su ./Core/Src/OwnCode/temp_stepper.cyclo ./Core/Src/OwnCode/temp_stepper.d ./Core/Src/OwnCode/temp_stepper.o ./Core/Src/OwnCode/temp_stepper.su
=======
	-$(RM) ./Core/Src/OwnCode/spindle.cyclo ./Core/Src/OwnCode/spindle.d ./Core/Src/OwnCode/spindle.o ./Core/Src/OwnCode/spindle.su ./Core/Src/OwnCode/stepper.cyclo ./Core/Src/OwnCode/stepper.d ./Core/Src/OwnCode/stepper.o ./Core/Src/OwnCode/stepper.su
>>>>>>> ffdde72ac9ab55556c7181c7d453721a4c6d8d6d

.PHONY: clean-Core-2f-Src-2f-OwnCode

