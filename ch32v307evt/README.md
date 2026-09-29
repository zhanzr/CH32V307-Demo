# CH32V307 评估板\CH32V307 Evaluation

Chip: **CH32V307VCT6** (CH32V30x_D8C, 288 KB flash / 64 KB RAM build used)

- Debug adapter: **WCH-Link** (wlink, RV mode)
- Debug serial: USART1 (PA9 TX), **115200 baud** (8N1) — all projects print
  here (`printf` retargeted in `drivers/SRC/Debug/debug.c`)

## Projects

All bare-metal projects live in the `bare/` folder and build from the command
line — no IDE needed.

| Project                       | Description                                                          |
|-------------------------------|----------------------------------------------------------------------|
| `bare/blink`                  | GPIO toggle (PA0) + chip ID / SysTick report over UART               |
| `bare/adc_temp_internal`      | Internal temperature sensor via ADC                                  |
| `bare/dhry_144m`              | Dhrystone 2.1 @ 144 MHz: 331,565 Dhrystones/s (1.310 DMIPS/MHz)      |
| `bare/coremark_144m`          | CoreMark 1.0 @ 144 MHz: 414.9 it/s                                   |
| `bare/eth_http_server`        | e_server embedded web server (static IP 192.168.5.100, WCH net lib)  |
| `bare/nv3030b_md183_240x284_cst816d` | TK018F3716 240x284 NV3030B LCD + CST816D touch, alternating soft/hardware SPI2 + I2C2 |

Shared code is **not** duplicated per project:

- `drivers/SRC/` (repo root) — Core / Debug / Peripheral library / Startup / `Ld/Link.ld`
  (288K flash + 32K RAM layout), referenced by all projects. The WCH example
  projects live next to it in `drivers/` and share the same `SRC` tree.
- `NetLib/` (repo root) — WCH Ethernet drivers + `libwchnet.a` (chip-level
  library, shared by all boards), used by `bare/eth_http_server`.
- `e_server/` (this folder) — the single-page web site served by
  `bare/eth_http_server` (packed into its `User/web_assets.h`).

## Building from the CLI

Each project ships a **Makefile** (GNU Make). Run from Git Bash / MSYS2, or
put the MounRiver bundled make on PATH:

```
C:\msys64\usr\bin\bash.exe
cd ch32v307evt/bare/<project>
make          # build build/<proj>.elf
make hex      # build build/<proj>.hex
make flash    # program build/<proj>.hex via WCH-Link (OpenOCD)
make size     # print section sizes
make clean    # remove build/
```

The Makefiles default to the **MounRiver Studio 2** bundled toolchain and
OpenOCD:

- Compiler: `<MRS2>\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin\riscv32-wch-elf-gcc.exe`
- Flasher:  `<MRS2>\resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin\openocd.exe` with `wch-riscv.cfg`

where `<MRS2>` is `D:\MounRiver\MounRiver_Studio2` by default; override with
`make MRS2=...` (or `RISCV_GCC=`/`OPENOCD=`/`OPENOCD_CFG=` for individual
tools). `make flash` runs `openocd -f wch-riscv.cfg -c "program <hex> verify
reset exit"`, which erases, writes, verifies and resets the chip.

To view the serial output: WCH-Link exposes a CDC serial port
("WCH-Link SERIAL", e.g. COM30) — open it at **115200**.

Yes, the boards can be flashed fully from the CLI: build + program + verify +
reset need nothing but `make flash` and a WCH-Link probe.

## How to add a project

1. Copy an existing project folder in `bare/` to a new name.
2. Edit `User/main.c` for your application.
3. In the new `Makefile` change `PROJ :=` (and the flag block if needed).
   Sources are picked up by wildcard from `User/`, plus the shared
   `drivers/SRC/` tree.

The leftover `.cproject` / `.project` / `.mrs` files only matter for the
optional MounRiver Studio IDE import; the CLI workflow does not use them.

## Spare SPI / I2C interfaces

Pin usage on this board: Ethernet (internal 10M PHY, fixed pins) PA1/PA2/PA7
+ RJ45 LEDs PC0/PC1, debug USART1 PA9, user LED PA0, user button PB6.
Everything else is free, which leaves:

- **SPI2 = the spare SPI**: NSS **PB12**, SCK **PB13**, MISO **PB14**,
  MOSI **PB15**. SPI1 (SCK PA5 / MISO PA6 / MOSI PA7, as used by the
  `drivers/SPI/2Lines_FullDuplex` example) is *not* fully available here -
  PA7 is taken by the Ethernet CRS_DV.
- **I2C2 = the spare I2C**: SCL **PB10**, SDA **PB11** (fixed mapping, no
  remap). I2C1's default pins are PB6/PB7 (PB6 is the button), but its
  alternate mapping PB8/PB9 (as used by the `drivers/I2C/I2C_7bit_Mode`
  example: `GPIO_Remap_I2C1`) is also fully free.

## Board

![screenshoot](board_images/board_1.png "screenshoot")
