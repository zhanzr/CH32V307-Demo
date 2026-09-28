# dhry_144m

**Dhrystone 2.1** benchmark for the CH32V307 EVT board (CH32V307VCT6), running
at the maximum core frequency **144 MHz** with the same aggressive GCC speed
optimization as the appkit-tc234 reference benchmarks (`dhry_200m`).

## Build / flash (CLI)

```
make          # build build/dhry_144m.elf
make hex      # build build/dhry_144m.hex
make flash    # program via WCH-Link + OpenOCD (rebuilds hex first)
make size     # print section sizes
make clean    # remove build/
```

Serial: **115200 8N1** on USART1 (PA9 TX). The benchmark repeats: one run,
then a ~20 s pause (PC13 LED toggles between runs).

## Compiler flags

```
-march=rv32imafc_xw -mabi=ilp32f -msave-restore -Ofast -ffp-contract=fast -funroll-loops
```

- `-Ofast`, `-ffp-contract=fast`, `-funroll-loops` match the
  appkit-tc234 `dhry_200m` / `coremark_200m` flags for comparable scores.
- `-msmall-data-limit=8 -fsingle-precision-constant` kept from the original
  MounRiver project configuration.
- Toolchain: MounRiver Studio 2 bundled `riscv32-wch-elf-gcc` **GCC 15.2.0**.

Run: 2,000,000 runs (`RUN_NUMBER` in `dhry.h`).

## Results

Captured 2026-09-29, CH32V307 @ **144 MHz**, GCC 15.2.0
`-Ofast -ffp-contract=fast -funroll-loops`, flash cached:

```
MicroSecond for one run through Dhrystone[19-6051]:	 3.016
Dhrystones per Second:	331565.000
DMIPS/MHz:	1.310
```

All output validation values matched (`Int_Glob: 5/5`, `Arr_2_Glob[8][7]:
2000010 = Number_Of_Runs + 10`, ...).

| Metric               | Value      |
|----------------------|------------|
| Microseconds / run   | 3.016 µs   |
| Dhrystones / second  | 331,565    |
| DMIPS / MHz          | 1.310      |
| Total DMIPS @ 144 MHz| 188.6      |

### Historical (older IDE builds, GCC 15.2.0)

| Flags  | Dhrystones/s | DMIPS/MHz |
|--------|--------------|-----------|
| -Ofast | 332,502      | 1.314     |
| -O3    | 337,154      | 1.333     |

The appkit-aligned flag set does not beat plain `-O3` here (loop unrolling
does not pay off for this core/I-cache mix); the numbers are within ~1 %.
The flags are kept aligned with the appkit-tc234 reference for cross-board
comparability.
