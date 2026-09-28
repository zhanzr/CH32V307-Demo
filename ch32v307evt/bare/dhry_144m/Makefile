# dhry_144m - CH32V307VCT6 (CH32V307 EVT) Dhrystone 2.1 @ 144 MHz
#
# Targets:   make            build build/dhry_144m.elf
#            make hex        build build/dhry_144m.hex
#            make flash      program build/dhry_144m.hex via WCH-Link (OpenOCD)
#            make size       print section sizes
#            make clean      remove build/
#
# Toolchain: MounRiver Studio 2 bundled RISC-V GCC 15 ("riscv32-wch-elf").
#            Run from Git Bash / MSYS2 (bash + make + rm on PATH), or add
#            <MRS2>\resources\app\resources\win32\others\Build_Tools\Make\bin
#            to PATH to use the bundled make.exe.
#            Override with: make MRS2=... OPENOCD=... OPENOCD_CFG=...
#
# Flashing needs a WCH-Link (wlink) probe on the board's SWDIO/SDIO pins.
#
# Optimization flags match the appkit-tc234 reference benchmarks
# (dhry_200m / coremark_200m): -Ofast -ffp-contract=fast -funroll-loops.

PROJ := dhry_144m

.DEFAULT_GOAL := all

# --- Toolchain ---
MRS2        ?= D:/MounRiver/MounRiver_Studio2/resources/app/resources/win32
GCC15_BIN   ?= $(MRS2)/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
CC          := $(GCC15_BIN)/riscv32-wch-elf-gcc
OBJCOPY     := $(GCC15_BIN)/riscv32-wch-elf-objcopy
SIZE        := $(GCC15_BIN)/riscv32-wch-elf-size
OPENOCD     ?= $(MRS2)/components/WCH/OpenOCD/OpenOCD/bin/openocd.exe
OPENOCD_CFG ?= $(MRS2)/components/WCH/OpenOCD/OpenOCD/bin/wch-riscv.cfg

# --- Paths ---
ROOT  := $(CURDIR)
SRC   := $(abspath $(ROOT)/../../../SRC)
BUILD := build

# --- Include paths (from .cproject) ---
INCLUDES := \
    -I$(SRC)/Debug \
    -I$(SRC)/Core \
    -I$(ROOT)/User \
    -I$(SRC)/Peripheral/inc

# --- Compile flags (appkit-tc234 aligned) ---
ARCHFLAGS := -march=rv32imafc_xw -mabi=ilp32f -msmall-data-limit=8 -msave-restore
CFLAGS    := $(ARCHFLAGS) -fmax-errors=20 -Ofast -ffp-contract=fast -funroll-loops \
             -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections \
             -fno-common -fsingle-precision-constant -Wunused -Wuninitialized -std=gnu11
CFLAGS    += $(INCLUDES) -MMD -MP

# --- Sources -> objects ---
PROJ_SRCS := $(wildcard $(ROOT)/User/*.c)
SHRD_SRCS := $(wildcard $(SRC)/Core/*.c) \
             $(wildcard $(SRC)/Debug/*.c) \
             $(wildcard $(SRC)/Peripheral/src/*.c)
ASMS      := $(filter %/startup_ch32v30x_D8C.S,$(wildcard $(SRC)/Startup/*.S))

OBJS := $(patsubst $(ROOT)/User/%.c,build/User/%.o,$(PROJ_SRCS)) \
        $(patsubst $(SRC)/%.c,build/src_%.o,$(SHRD_SRCS)) \
        $(patsubst $(SRC)/%.S,build/src_%.o,$(ASMS))

$(BUILD)/User/%.o: $(ROOT)/User/%.c
	@mkdir -p $(dir $@)
	"$(CC)" $(CFLAGS) -c -o $@ $<

$(BUILD)/src_%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	"$(CC)" $(CFLAGS) -c -o $@ $<

$(BUILD)/src_%.o: $(SRC)/%.S
	@mkdir -p $(dir $@)
	"$(CC)" $(ARCHFLAGS) -c -o $@ $<

# --- Header dependencies (generated with -MMD -MP) ---
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

# --- Link / hex ---
ELF := $(BUILD)/$(PROJ).elf
HEX := $(BUILD)/$(PROJ).hex
LDS := $(SRC)/Ld/Link.ld

all: $(ELF)
hex: $(HEX)

$(ELF): $(OBJS)
	@mkdir -p $(dir $@)
	"$(CC)" $(ARCHFLAGS) -T $(LDS) -nostartfiles -Xlinker --gc-sections \
		-Wl,-Map,$(BUILD)/$(PROJ).map --specs=nano.specs -u _printf_float \
		--specs=nosys.specs -o $@ $(OBJS) -lm

$(HEX): $(ELF)
	"$(OBJCOPY)" -O ihex $< $@

flash: $(HEX)
	"$(OPENOCD)" -f "$(OPENOCD_CFG)" -c "program $(abspath $(HEX)) verify reset exit"

size: $(ELF)
	"$(SIZE)" $<

clean:
	$(RM) -r $(BUILD)

.PHONY: all hex flash size clean
