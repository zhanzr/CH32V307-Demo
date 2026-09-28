# CH32V307 Demo

Demo and benchmark projects for the WCH **CH32V307** RISC-V MCU, built and
flashed from the command line (no IDE required).

## Layout

```
ch32v307evt/    CH32V307 EVT evaluation board (see its README.md)
ch32v307mini/   CH32V307 mini board (see its README.md)
drivers/        WCH EVT example projects + shared SRC tree (periph lib,
                Core, Debug, Startup, linker scripts)
NetLib/         WCH Ethernet library (wchnet, chip-level, shared by boards)
```

## Building

Each board project has a `Makefile`: `make`, `make hex`, `make flash`,
`make size`, `make clean`. Flashing uses the MounRiver-bundled OpenOCD with a
WCH-Link probe; serial console is 115200 8N1. See the board READMEs for
details.

## Highlights

- Benchmarks: `dhry_144m` (1.310 DMIPS/MHz), `coremark_144m` (414.9 it/s) -
  in `ch32v307evt/bare/`.
- `eth_http_server`: embedded web server (static IP, LED + ADC JSON API) -
  in `ch32v307evt/bare/`, site from `ch32v307evt/e_server/`.
