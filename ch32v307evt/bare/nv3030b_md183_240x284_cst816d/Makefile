# nv3030b_md183_240x284_cst816d - CH32V307VCT6 (CH32V307 EVT) build/flash
#
# TK018F3716 module: NV3030B 240x284 LCD on soft (bit-banged) SPI +
# CST816D touch on bit-banged I2C. Ported from the tc212-kit project
# of the same name.
#
# Targets:   make            build build/nv3030b_md183_240x284_cst816d.elf
#            make hex        build build/nv3030b_md183_240x284_cst816d.hex
#            make flash      program build/nv3030b_md183_240x284_cst816d.hex via WCH-Link (OpenOCD)
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

PROJ := nv3030b_md183_240x284_cst816d

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
SRC   := $(abspath $(ROOT)/../../../drivers/SRC)
BUILD := build

# --- Include paths ---
INCLUDES := \
    -I$(SRC)/Debug \
    -I$(SRC)/Core \
    -I$(ROOT)/User \
    -I$(SRC)/Peripheral/inc

# --- Compile flags ---
ARCHFLAGS := -march=rv32imafc_xw -mabi=ilp32f -msmall-data-limit=8 -msave-restore
CFLAGS    := $(ARCHFLAGS) -fmax-errors=20 -O2 -fmessage-length=0 -fsigned-char \
             -ffunction-sections -fdata-sections -fno-common -fsingle-precision-constant \
             -Wunused -Wuninitialized -std=gnu99
CFLAGS    += $(INCLUDES) -MMD -MP

# --- Sources -> objects ---
PROJ_SRCS := $(wildcard $(ROOT)/User/*.c)
SHRD_SRCS := $(wildcard $(SRC)/Core/*.c) \
             $(wildcard $(SRC)/Debug/*.c) \
             $(wildcard $(SRC)/Peripheral/src/*.c)
ASMS      := $(filter %/startup_ch32v30x_D8C.S,$(wildcard $(SRC)/Startup/*.S))

OBJS := $(patsubst $(ROOT)/%,build/%,$(PROJ_SRCS:.c=.o)) \
        $(patsubst $(SRC)/%.c,build/src_%.o,$(SHRD_SRCS)) \
        $(patsubst $(SRC)/%.S,build/src_%.o,$(ASMS))

$(BUILD)/%.o: $(ROOT)/%.c
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
		-Wl,-Map,$(BUILD)/$(PROJ).map --specs=nano.specs --specs=nosys.specs \
		-o $@ $(OBJS) -lm

$(HEX): $(ELF)
	"$(OBJCOPY)" -O ihex $< $@

flash: $(HEX)
	"$(OPENOCD)" -f "$(OPENOCD_CFG)" -c "program $(abspath $(HEX)) verify reset exit"

size: $(ELF)
	"$(SIZE)" $<

clean:
	$(RM) -r $(BUILD)

.PHONY: all hex flash size clean
