################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################
-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include User/subdir.mk
-include Startup/subdir.mk
-include Peripheral/src/subdir.mk
-include Debug/subdir.mk
-include Core/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
GPIO_Toggle.hex \

SECONDARY_LIST += \
GPIO_Toggle.lst \

SECONDARY_SIZE += \
GPIO_Toggle.siz \


# All Target
all: 
	$(MAKE) --no-print-directory main-build 

main-build: GPIO_Toggle.elf secondary-outputs

# Tool invocations
GPIO_Toggle.elf: $(OBJS) $(USER_OBJS_ESCAPE)
	@	riscv32-wch-elf-gcc -march=rv32imafc_xw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fsingle-precision-constant -Wunused -Wuninitialized -g -gdwarf-4 -T "d:\ch32v307_ms_prj\SRC\Ld\Link.ld" -nostartfiles -Xlinker --gc-sections -Wl,-Map,"GPIO_Toggle.map" --specs=nano.specs --specs=nosys.specs -o "GPIO_Toggle.elf" $(OBJS) $(LIBS)
GPIO_Toggle.hex: GPIO_Toggle.elf
	@	riscv32-wch-elf-objcopy -O ihex "GPIO_Toggle.elf" "GPIO_Toggle.hex"
GPIO_Toggle.lst: GPIO_Toggle.elf
	@	riscv32-wch-elf-objdump --all-headers --demangle --disassemble -M xw "GPIO_Toggle.elf" > "GPIO_Toggle.lst"
GPIO_Toggle.siz: GPIO_Toggle.elf
	riscv32-wch-elf-size --format=berkeley "GPIO_Toggle.elf"

# Other Targets
clean:
	-$(RM) $(DIR_OBJS) $(SECONDARY_FLASH)$(SECONDARY_LIST)$(SECONDARY_SIZE) GPIO_Toggle.elf
	-$(RM) $(DIR_EXPANDS) $(CALLGRAPH_DOT)
	-$(RM) $(DIR_DEPS)
secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets