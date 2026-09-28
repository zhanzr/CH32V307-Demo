# coremark_144m

**CoreMark 1.0** benchmark for the CH32V307 EVT board (CH32V307VCT6), running
at the maximum core frequency **144 MHz** with the same aggressive GCC speed
optimization as the appkit-tc234 reference benchmarks (`coremark_200m`).

## Build / flash (CLI)

```
make          # build build/coremark_144m.elf
make hex      # build build/coremark_144m.hex
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

`ITERATIONS` was raised 4000 -> 5000 (`coremark_1_0_1/ch32v307/core_portme.h`):
with the faster flags 4000 iterations finished in 9.64 s, below CoreMark's
10 s minimum for a valid result.

## Results

Captured 2026-09-29, CH32V307 @ **144 MHz**, GCC 15.2.0
`-Ofast -ffp-contract=fast -funroll-loops`, flash cached:

```
2K performance run parameters for coremark.
CoreMark Size    : 666
Total ticks      : 12052
Total time (secs): 12.052000
Iterations/Sec   : 414.868901
Iterations       : 5000
Compiler version : GCC15.2.0
Compiler flags   : -Ofast -ffp-contract=fast -funroll-loops
Memory location  : Static
seedcrc          : 0xe9f5
[0]crclist       : 0xe714
[0]crcmatrix     : 0x1fd7
[0]crcstate      : 0x8e3a
[0]crcfinal      : 0xbd59
Correct operation validated. See readme.txt for run and reporting rules.
CoreMark 1.0 : 414.868901 / GCC15.2.0 -Ofast -ffp-contract=fast -funroll-loops / Static
```

| Metric                    | Value    |
|---------------------------|----------|
| Iterations / second       | 414.87   |
| CoreMark/MHz              | 2.881    |
| Iterations                | 5000     |
| Validation                | Correct operation validated (crcfinal 0xbd59) |

### Historical (older IDE builds, GCC 15.2.0, -Ofast, 4000 iterations)

Iterations/Sec: 396.00 (crcfinal 0x65c5). The appkit-aligned flags give
+4.8 % over the previous `-Ofast` build.
