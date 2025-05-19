################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/croutine.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/event_groups.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/list.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/stream_buffer.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c \
C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/timers.c 

OBJS += \
./FreeRTOS-Kernel/croutine.o \
./FreeRTOS-Kernel/event_groups.o \
./FreeRTOS-Kernel/list.o \
./FreeRTOS-Kernel/queue.o \
./FreeRTOS-Kernel/stream_buffer.o \
./FreeRTOS-Kernel/tasks.o \
./FreeRTOS-Kernel/timers.o 

C_DEPS += \
./FreeRTOS-Kernel/croutine.d \
./FreeRTOS-Kernel/event_groups.d \
./FreeRTOS-Kernel/list.d \
./FreeRTOS-Kernel/queue.d \
./FreeRTOS-Kernel/stream_buffer.d \
./FreeRTOS-Kernel/tasks.d \
./FreeRTOS-Kernel/timers.d 


# Each subdirectory must supply rules for building sources it contributes
FreeRTOS-Kernel/croutine.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/croutine.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/event_groups.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/event_groups.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/list.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/list.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/queue.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/stream_buffer.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/stream_buffer.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/tasks.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
FreeRTOS-Kernel/timers.o: C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/timers.c FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu18 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F746xx -DUSE_FULL_ASSERT -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include -I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1 -I../../libs/LibL6474/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Stepper" -I../../libs/LibRTOSConsole/inc -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Console" -I"C:/Users/tomke/Documents/2_Hardwarenahes_Prog/graviermaschine/stepper/Core/Inc/Spindle" -I../../libs/LibSpindle/inc -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FreeRTOS-2d-Kernel

clean-FreeRTOS-2d-Kernel:
	-$(RM) ./FreeRTOS-Kernel/croutine.cyclo ./FreeRTOS-Kernel/croutine.d ./FreeRTOS-Kernel/croutine.o ./FreeRTOS-Kernel/croutine.su ./FreeRTOS-Kernel/event_groups.cyclo ./FreeRTOS-Kernel/event_groups.d ./FreeRTOS-Kernel/event_groups.o ./FreeRTOS-Kernel/event_groups.su ./FreeRTOS-Kernel/list.cyclo ./FreeRTOS-Kernel/list.d ./FreeRTOS-Kernel/list.o ./FreeRTOS-Kernel/list.su ./FreeRTOS-Kernel/queue.cyclo ./FreeRTOS-Kernel/queue.d ./FreeRTOS-Kernel/queue.o ./FreeRTOS-Kernel/queue.su ./FreeRTOS-Kernel/stream_buffer.cyclo ./FreeRTOS-Kernel/stream_buffer.d ./FreeRTOS-Kernel/stream_buffer.o ./FreeRTOS-Kernel/stream_buffer.su ./FreeRTOS-Kernel/tasks.cyclo ./FreeRTOS-Kernel/tasks.d ./FreeRTOS-Kernel/tasks.o ./FreeRTOS-Kernel/tasks.su ./FreeRTOS-Kernel/timers.cyclo ./FreeRTOS-Kernel/timers.d ./FreeRTOS-Kernel/timers.o ./FreeRTOS-Kernel/timers.su

.PHONY: clean-FreeRTOS-2d-Kernel

