# eth_http_server - embedded web server on the CH32V307 EVT board

Port of the `f7-demo/nucleo-f746/bare/eth_http_server` project (lwIP raw API)
to the CH32V307 EVT board, using the WCH net stack (**wchnet**, `libwchnet.a`)
in socket mode. It serves the same bundled single-page site and API:

- Site source of truth: [`../../e_server/`](../../e_server/) (adapted from the
  f746 original: **one LED on PA0**, sensor tab with the **two ADC1 internal
  channels** - Vrefint + die temperature, no VBAT channel on the CH32V307).
- `User/web_assets.h` is packed from `e_server/web/` by `build_web.py`
  (gzip page, CSS/JS inlined, 4.7 KB - no images, the board has 256 KB flash).

## Network configuration (static, no DHCP)

| Setting  | Value             |
| -------- | ----------------- |
| Board IP | **192.168.5.100** |
| Netmask  | 255.255.255.0     |
| Gateway  | **192.168.5.250** (the host PC) |
| MAC      | 02:00:00:12:34:5A (locally administered) |

The host PC (LAN IP **192.168.5.250**) is on the same LAN. Browse to:

```
http://192.168.5.100/
```

The IP/MAC are hardcoded in `User/main.c` (edit there, no DHCP, no
flash-stored configuration).

## Pages & API (same contract as the f746 port)

* **LED control** - one checkbox for the PA0 LED; clicks POST to `/api/leds`
  and re-sync.
* **ADC values** - two canvas plots (VDD estimate from VREFINT / die
  temperature) fed by the ADC1 internal channels (CH17 / CH16), 1/2/4 s
  sample interval.
* **Board info** - `arch` + `lan_ip`; `public_ip`/`geo`/`weather` are `null`
  (no HTTP/TLS client on the board).

| Route                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `GET /`              | the page: gzip, `Content-Encoding: gzip`        |
| `GET /api/leds`      | `{"leds":[0]}` (real LED GPIO state)            |
| `POST /api/leds`     | body `{"leds":[0]}` -> applies, `{"leds":[...]}` |
| `GET /api/adc`       | `{"vrefint_mv":3296.6,"temp_c":21.5,"ts":...}`  |
| `GET /api/info`      | `{"arch":"riscv","lan_ip":"192.168.5.100",...}` |
| `GET /public/*`      | 404 (no files embedded - flash budget)          |

The console (USART1, PA9, WCH-Link CDC @ 115200) prints the boot banner, the
static IP and one `HTTP <method> <path>` line per request.

## Implementation notes

* `HTTP/http_server.c` is the request router (`/`, `/api/*`, `/public/*`),
  modeled on the f746 lwIP `http_server.c` but on the WCHNET socket API.
* Accepted sockets need `WCHNET_ModifyRecvBuf()` in the `SINT_STAT_CONNECT`
  handler, otherwise `SINT_STAT_RECV` never fires (WCH example convention).
* Large bodies are sent in chunks: `WCHNET_SocketSend` accepts less than
  requested when its send buffer fills, so `send_all()` pumps
  `WCHNET_MainTask()` and retries.
* Static bodies (the gzip page) are served straight from flash - the stack
  copies them (`CFG0_TCP_SEND_COPY = 1`).
* Float `printf` needs plain newlib-nano (`-u _printf_float`); do **not**
  link WCH's `-lprintf` (its printf drops `%f` output).
* Uses the project-local 256K FLASH / 64K RAM linker script (`Ld/Link.ld`)
  and the board-root `NetLib/` (wchnet + 10M PHY drivers).

## Build & flash

```bash
make           # build build/eth_http_server.elf
make web       # re-pack the site: python ../../e_server/build_web.py
make hex       # build build/eth_http_server.hex
make flash     # program via WCH-Link + OpenOCD
make size
make clean
```

## Verified

2026-09-29, host 192.168.5.250 -> board 192.168.5.100:

```
GET  /           -> 200, 4679 bytes (gzip page)
GET  /api/leds   -> {"leds":[0]}
POST /api/leds   -> {"leds":[1]} (PA0 on), {"leds":[0]} (PA0 off)
GET  /api/adc    -> {"vrefint_mv":3296.6,"temp_c":21.5,"ts":9163}
GET  /api/info   -> {"arch":"riscv","lan_ip":"192.168.5.100",...}
GET  /favicon.ico -> 204; unknown path -> 404
```
