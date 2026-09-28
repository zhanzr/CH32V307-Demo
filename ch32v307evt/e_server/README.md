# e_server - embedded web demo for the CH32V307 EVT board (frontend + reference C backend)

A minimal single-page web app for the **CH32V307VCT6** (CH32V307 EVT), LED
tab wired to the board's **single LED on PA0**, sensor tab to the **ADC1
internal channels** (Vrefint + die temperature - the CH32V307 has no VBAT
channel, so there are two plots), plus a host-side C backend that mirrors the
API the board's embedded server implements (`bare/eth_http_server`).
The web assets are bundled into C arrays by a build script (inline CSS/JS,
gzip the page) so they can be served straight from flash on the MCU.

This is a port of the `f7-demo/nucleo-f746/e_server` original; the frontend
was adapted (1 LED, 2 sensor plots, no board photo - flash budget) and the
`public/` folder is intentionally empty.

## Layout

```
e_server/
  web/             raw frontend sources
    index.html     single page, three tabs
    style.css
    app.js
  public/          (empty - see note below)
  build_web.py     bundle: inline CSS/JS -> gzip page -> C arrays (--out to reuse)
  web_assets.h     generated C arrays (page, no images)
  server.c         reference C backend (host-side)
  Makefile / build.sh
```

`e_server/` is the **standalone source of truth for the site**. The embedded
project packs it with:

```
python build_web.py --out ../bare/eth_http_server/User/web_assets.h
```

## Frontend (three tabs, no external libraries)

1. **LED control** - one checkbox for the PA0 LED. Each click POSTs the new
   state to the backend (no page reload) and the checkbox re-syncs to the
   server's reply. Rapid clicks are coalesced.
2. **ADC values** - two canvas plots (VREFINT-derived VDD estimate / die
   temperature). A dropdown picks the sample interval: 1 s (default), 2 s,
   4 s; the choice is persisted in `localStorage`. Samples are polled only
   while the tab is open; on timeout/error the previous value is kept and the
   series continues.
3. **Board info** - architecture, LAN IP, public IP, geo location and weather
   (each falls back to "N/A" if unavailable - on the board the last three are
   always null), a manual *Refresh* button.

## API (shared contract with the embedded server)

| Route                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `GET /`              | the page: gzipped, CSS/JS inlined (`Content-Encoding: gzip`) |
| `GET /api/leds`      | `{"leds":[0]}`                                  |
| `POST /api/leds`     | body `{"leds":[0]}` -> applied, `{"leds":[...]}` |
| `GET /api/adc`       | `{"vrefint_mv":3297.9,"temp_c":43.6,"ts":...}`  |
| `GET /api/info`      | `{"arch","lan_ip","public_ip","geo","weather","ts"}` (nulls = unavailable) |

## Build & run (host reference)

Needs Python 3 and a host C compiler (gcc/clang). Windows/MinGW adds
`-lws2_32` automatically.

```bash
python build_web.py      # or: make web_assets.h
make                     # or: bash build.sh
./e_server 8080          # or: make run
```

Then open `http://localhost:8080/`.

## Bundling (build_web.py)

```text
[ web/index.html ] --+--> inline <style>+<script> --> index.html
[ web/style.css  ] --+
[ web/app.js     ] --+

index.html -> gzip -> index_html_gz[]   (served with Content-Encoding: gzip)
public/*   -> raw byte arrays + lookup table (none on this board)
```

* The page is served gzipped (browser decompresses via `Content-Encoding: gzip`).
* Regenerate after any change to `web/`; commit `web_assets.h` alongside the
  sources.
* Other projects reuse the packer:
  `python build_web.py --out ../some/project/User/web_assets.h`.
