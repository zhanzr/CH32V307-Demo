/*
  nv3030b_md183_240x284_cst816d main for the CH32V307 EVT board
  (CH32V307VCT6 @ 144 MHz). TK018F3716 module: NV3030B 240x284 panel
  over its wrapped-command SPI protocol on soft (bit-banged) SPI
  (mode 3, ~1.8 MHz), plus CST816D capacitive touch over
  bit-banged I2C, with the touch state printed on the serial console.

  Demo phases per pass (live FPS counter throughout): big-font banner,
  TEST_STAND vendor screens (timed solid fills), info pages (normal +
  inverted), HSV gradient sweep, LED test. Ported from the tc212-kit
  project of the same name (same module, same demo loop).

  Wiring: SCK=PB13, MOSI=PB15, CS=PB12 (all GPIO, soft SPI). MISO
  (PB14) is NOT connected - the driver is write-only by design. The
  module has no DC/reset/backlight pin in use (wrapped-command SPI
  protocol; backlight is powered from the module supply).
  Touch: CST816D I2C on SCL=PB10 SDA=PB11 (the board's spare I2C2
  pins, driven bit-banged).
*/

#include <string.h>
#include <stdio.h>
#include "debug.h"
#include "lcd.h"
#include "interface.h"
#include "touch.h"
#include "lcd_font_1608.h"
#include "asset_test1.h"

#define SCREEN_W   LCD_Width     /* 240 (row buffer sizing) */
#define FPS_BAND   20            /* bottom rows reserved for the FPS text   */
#define BACK_COLOR LCD_BLACK
#define LED_HALF   1000          /* LED test dwell (ms)                    */

/* Info page layout. The big 8x16 font needs a 24 px line pitch (16 px
 * glyph + spacing), which limits the page to 11 lines + the FPS band on
 * a 284 px panel.
 *
 * The panel has rounded corners, so text flush against row 0 or the left
 * edge gets cropped. INFO_TOP starts the block one glyph line down, and
 * INFO_X indents far enough to clear the corner radius. */
#define INFO_DY    24            /* info page line pitch (8x16 font)       */
#define INFO_X     22            /* left indent: clears the corner radius  */
#define INFO_TOP   18            /* first text row (skip row 0)            */
#define INFO_LINES 11            /* usable lines before the FPS band       */

/* Cold-boot panel bring-up.
 *
 * How long a cold-booted panel needs before it accepts commands varies run
 * to run and cannot be measured (the module is write-only by design).
 * Rather than sleeping or counting retries, this fills the screen with a
 * tiled asset: the drawing is the wait, and it doubles as the "the panel
 * came up" test - the moment the artwork appears the panel is live. See
 * panel_bringup_draw().
 *
 * Each cycle is one whole-screen fill plus one LCD_Reinit, so the loop
 * interleaves drawing with init attempts - an init that lands shows up as
 * the next fill actually appearing. */
#define ASSET_TILE_GAP      4    /* gap between tiles, px (keeps a border)  */

/* Draw/re-init cycles. Trades boot time for init attempts (~0.5 s per
 * cycle, mostly the datasheet delays inside LCD_Reinit). A final fill is
 * always done after the last Reinit. */
#define LCD_BRINGUP_PASSES  2u

/* ---- on-board LED (PA0, the board's single user LED) ---- */
static void leds_all(uint8_t on)
{
    if (on != 0u)
    {
        GPIO_WriteBit (GPIOA, GPIO_Pin_0, Bit_SET);
    }
    else
    {
        GPIO_WriteBit (GPIOA, GPIO_Pin_0, Bit_RESET);
    }
}

static void led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOA, &GPIO_InitStructure);
    GPIO_WriteBit (GPIOA, GPIO_Pin_0, Bit_RESET);
}

/* Milliseconds from the SysTick tick counter (debug.c). */
static uint32_t ms_now(void)
{
    return Get_SysTick_MS();
}

static void delay_ms(uint32_t ms)
{
    Delay_MS (ms);
}

/* Milliseconds since boot - prefixes every phase line so a cold-boot
 * capture shows exactly when the panel starts responding. */
static uint32_t uptime_ms(void)
{
    static uint32_t s_t0;
    uint32_t t = ms_now();

    if (s_t0 == 0U)
    {
        s_t0 = t;
    }
    return t - s_t0;
}

/* Die temperature in hundredths of a degree C (ADC1 internal sensor,
 * same measurement as the adc_temp_internal project), sampled on
 * demand. */
static s16  s_cal;
static u8   s_adc_ready;

static u16 adc_conv_val(s16 val)
{
    if (((val + s_cal) < 0) || (val == 0)) { return 0; }
    if (((s_cal + val) > 4095) || (val == 4095)) { return 4095; }
    return (u16)(val + s_cal);
}

static s32 adc_celsius_x100(void)
{
    u16  vref_raw, ts_raw;
    float vdd_mv, vsense_mv;
    u16  refer_volt, refer_temper;
    u8   t;
    u32  sum_v = 0, sum_t = 0;

    if (s_adc_ready == 0U)
    {
        ADC_InitTypeDef ADC_InitStructure = {0};

        RCC_APB2PeriphClockCmd (RCC_APB2Periph_ADC1, ENABLE);
        RCC_ADCCLKConfig (RCC_PCLK2_Div8);
        ADC_DeInit (ADC1);
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
        ADC_Init (ADC1, &ADC_InitStructure);
        ADC_Cmd (ADC1, ENABLE);
        ADC_BufferCmd (ADC1, DISABLE);
        ADC_ResetCalibration (ADC1);
        while (ADC_GetResetCalibrationStatus (ADC1));
        ADC_StartCalibration (ADC1);
        while (ADC_GetCalibrationStatus (ADC1));
        s_cal = Get_CalibrationValue (ADC1);
        ADC_BufferCmd (ADC1, ENABLE);
        ADC_TempSensorVrefintCmd (ENABLE);
        s_adc_ready = 1U;
    }

    for (t = 0; t < 8u; t++)
    {
        ADC_RegularChannelConfig (ADC1, ADC_Channel_Vrefint, 1, ADC_SampleTime_239Cycles5);
        ADC_SoftwareStartConvCmd (ADC1, ENABLE);
        while (!ADC_GetFlagStatus (ADC1, ADC_FLAG_EOC));
        sum_v += ADC_GetConversionValue (ADC1);

        ADC_RegularChannelConfig (ADC1, ADC_Channel_TempSensor, 1, ADC_SampleTime_239Cycles5);
        ADC_SoftwareStartConvCmd (ADC1, ENABLE);
        while (!ADC_GetFlagStatus (ADC1, ADC_FLAG_EOC));
        sum_t += ADC_GetConversionValue (ADC1);

        Delay_MS (2);
    }
    vref_raw = adc_conv_val ((s16)(sum_v / 8u));
    ts_raw   = adc_conv_val ((s16)(sum_t / 8u));

    /* Vrefint is 1.2 V typical: the raw reading gives the actual VDD. */
    vdd_mv    = 1200.0f * 4096.0f / vref_raw;
    vsense_mv = ts_raw * vdd_mv / 4096.0f;

    /* Same conversion as the periph lib TempSensor_Volt_To_Temper():
     * factory calibration pair at 0x1FFFF720, 4.3 mV/degC slope. */
    refer_volt   = (u16)(*(u32 *)0x1FFFF720 & 0x0000FFFF);
    refer_temper = (u16)((*(u32 *)0x1FFFF720 >> 16) & 0x0000FFFF);

    return (s32)((refer_temper - (vsense_mv - refer_volt) * 10.0f / 43.0f) * 100.0f);
}

/* --------------------------------------------------------------------- */
/* Touch printout: polls the CST816D and prints state/X/Y on the serial
 * port (on touch-down, and on release).                                 */
static uint8_t s_touch_down;

static void touch_task(void)
{
    uint8_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t x, y;

    Touch_Read (buf, 8);

    if (buf[3] == 0x80U && buf[4] > 1U)
    {
        x = buf[4];
        y = (uint16_t)(((buf[5] & 0x0FU) << 8) | buf[6]);

        if (s_touch_down == 0U)
        {
            printf ("[TOUCH] down X=%u Y=%u (284-Y=%u)\r\n",
                    (unsigned)x, (unsigned)y, (unsigned)(284U - y));
            s_touch_down = 1U;
        }
    }
    else if (s_touch_down != 0U)
    {
        printf ("[TOUCH] release\r\n");
        s_touch_down = 0U;
    }
}

/* Runtime window geometry (follows LCD_SetWindow). */
static uint16_t anim_h(void)
{
    return (uint16_t)(LCD_H() - FPS_BAND);
}

/* --------------------------------------------------------------------- */
/* FPS counter.                                                          */
static volatile uint32_t g_frames;
static uint32_t         g_last_frames;
static uint32_t         g_fps_last_tick;
static uint32_t         g_fps_color = LCD_WHITE;   /* FPS glyph color     */

static void fps_frame(void)
{
    g_frames++;
}

static void fps_update(void)
{
    uint32_t now = ms_now();
    if (now - g_fps_last_tick >= 1000)
    {
        uint32_t fps = g_frames - g_last_frames;
        g_last_frames = g_frames;
        g_fps_last_tick = now;

        char buf[8];
        buf[0] = 'F'; buf[1] = 'P'; buf[2] = 'S'; buf[3] = ':';
        buf[4] = (char)('0' + (fps / 100) % 10);
        buf[5] = (char)('0' + (fps / 10) % 10);
        buf[6] = (char)('0' + fps % 10);
        buf[7] = '\0';
        LCD_SetColor (g_fps_color);
        LCD_ShowTransparent (1);              /* no opaque box */
        LCD_DisplayString (1, (uint16_t)(anim_h() + 4), buf);
        LCD_ShowTransparent (0);
    }
}

static void paint_fps_band(void)
{
    LCD_SetColor (BACK_COLOR);
    LCD_SetBackColor (BACK_COLOR);
    LCD_FillRect (0, anim_h(), LCD_W(), FPS_BAND);
}

static void delay_with_fps(uint32_t ms)
{
    uint32_t start = ms_now();
    do
    {
        fps_update();
        touch_task();                        /* touch printout during waits */
        delay_ms (50);
    } while (ms_now() - start < ms);
}

/* --------------------------------------------------------------------- */
/* Animated gradient: hue sweeps the full color wheel over `ms`.         */
static uint32_t hsv_to_rgb(int h, int s, int v)
{
    int region = (h / 600) % 6;
    int fpart  = h % 600;
    int p = v * (255 - s) / 255;
    int q = v * (255 - (s * fpart) / 600) / 255;
    int t = v * (255 - (s * (600 - fpart)) / 600) / 255;
    int r, g, b;
    switch (region)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default:r = v; g = p; b = q; break;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_gradient(int hue_a, int hue_b, uint16_t *row)
{
    int y;
    for (y = 0; y < (int)anim_h(); y++)
    {
        int frac = y * 1000 / (int)anim_h();
        int hue  = hue_a + (hue_b - hue_a) * frac / 1000;
        uint32_t c = hsv_to_rgb (hue, 255, 255);
        uint16_t rgb565 = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) |
                                     ((c >> 3) & 0x001F));
        int x;
        for (x = 0; x < (int)LCD_W(); x++)
        {
            row[x] = rgb565;
        }
        LCD_CopyBuffer (0, (uint16_t)y, LCD_W(), 1, row);
    }
}

static void gradient_demo(uint32_t ms)
{
    static uint16_t row[SCREEN_W];
    LCD_SetBackColor (BACK_COLOR);
    paint_fps_band();

    uint32_t start = ms_now();
    uint32_t t = 0;
    do
    {
        int hue_a = (int)(t * 3600 / ms);
        int hue_b = hue_a + 1800;
        if (hue_b >= 3600) { hue_b -= 3600; }
        draw_gradient (hue_a, hue_b, row);
        fps_frame();
        fps_update();
        t = ms_now() - start;
    } while (t < ms);
}

/* --------------------------------------------------------------------- */
/* LED test (the on-board user LED PA0).                                 */
static void led_test(void)
{
    printf ("[LCD] LED ON\r\n");
    leds_all (1u);
    delay_with_fps (LED_HALF);
    printf ("[LCD] LED OFF\r\n");
    leds_all (0u);
    delay_with_fps (LED_HALF);
}

/* --------------------------------------------------------------------- */
/* Vendor TEST_STAND screens. The five solid-color fills are timed (ms). */
static uint32_t g_solid_ms[5];
const char *const g_solid_name[5] =
{
    "RED", "GREEN", "BLUE", "WHITE", "BLACK"
};

/* kHz -> "36 MHz" / "1.8 MHz" text (shared by console + info page). */
static const char *mhz_text(unsigned long khz)
{
    static char t[16];
    if (khz % 1000UL == 0UL)
    {
        snprintf (t, sizeof t, "%lu MHz", khz / 1000UL);
    }
    else
    {
        snprintf (t, sizeof t, "%lu.%lu MHz",
                  khz / 1000UL, (khz % 1000UL) / 100UL);
    }
    return t;
}

static void TEST_STAND(void)
{
    const uint32_t solid_color[5] = { C565_RED, C565_GREEN, C565_BLUE, C565_WHITE, C565_BLACK };
    int i;

    DispFrame();
    StopDelay (Delay_Time);

    DispGrayHor16();
    StopDelay (Delay_Time);

    DispBand();
    StopDelay (Delay_Time);

    for (i = 0; i < 5; i++)
    {
        uint32_t t0 = ms_now();
        DispColor (solid_color[i]);
        g_solid_ms[i] = ms_now() - t0;
        StopDelay (Delay_Time);
    }

    printf ("[LCD] solid fills (ms): RED=%lu GREEN=%lu BLUE=%lu "
            "WHITE=%lu BLACK=%lu\r\n",
            (unsigned long)g_solid_ms[0], (unsigned long)g_solid_ms[1],
            (unsigned long)g_solid_ms[2], (unsigned long)g_solid_ms[3],
            (unsigned long)g_solid_ms[4]);
}

/* --------------------------------------------------------------------- */
/* Info page: compiler, build date, clock rates, die temperature and
 * the IO map, in the big 8x16 font (same as the banner page). `invert`
 * swaps fg/bg (white background page).
 *
 * Layout notes:
 *  - The panel has rounded corners, so the block is indented by INFO_X and
 *    starts at INFO_TOP instead of row 0; a line touching row 0 or the
 *    left edge would have its glyphs clipped.
 *  - A 24 px pitch is used instead of the 16 px glyph height so the bigger
 *    font does not run into itself. That caps the page at 11 lines plus
 *    the FPS band, so the content is trimmed to fit; the full detail stays
 *    on the serial console. */
static void info_demo(uint32_t ms, uint8_t invert)
{
    char buf[40];
    char comp[24];
    unsigned long mhz = (unsigned long)(SystemCoreClock / 1000000UL);
    uint32_t fg = invert ? LCD_BLACK : LCD_WHITE;
    uint32_t bg = invert ? LCD_WHITE : LCD_BLACK;
    int i, y;

#if defined(__GNUC__)
    snprintf (comp, sizeof comp, "GCC %d.%d.%d",
              __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    snprintf (comp, sizeof comp, "unknown");
#endif

    printf ("[LCD] info%s: compiler=%s build=%s %s\r\n",
            invert ? " (inverted)" : "", comp, __DATE__, __TIME__);
    printf ("[LCD] info: freq=%lu MHz  spi=%s  touch=%lu kHz\r\n",
            mhz, mhz_text (LCD_HwSpiKHz()), Touch_GetHz() / 1000UL);
    printf ("[LCD] info: die=%d.%02d C  solids(R,G,B,W,K)=%lu,%lu,%lu,%lu,%lu ms\r\n",
            (int)(adc_celsius_x100() / 100), (int)(adc_celsius_x100() % 100),
            (unsigned long)g_solid_ms[0], (unsigned long)g_solid_ms[1],
            (unsigned long)g_solid_ms[2], (unsigned long)g_solid_ms[3],
            (unsigned long)g_solid_ms[4]);

    /* Big font for the whole page (matches the banner page). */
    LCD_SetAsciiFont (&ASCII_Font16);
    LCD_SetColor (fg);
    LCD_SetBackColor (bg);
    LCD_Clear();

    /* FPS band in the page background color, then restore fg/bg. */
    LCD_SetColor (bg);
    LCD_SetBackColor (bg);
    LCD_FillRect (0, anim_h(), LCD_W(), FPS_BAND);
    LCD_SetColor (fg);
    LCD_SetBackColor (bg);
    g_fps_color = fg;                     /* FPS glyph matches the page */

    /* 8 px per glyph in the 8x16 font. */
    y = INFO_TOP;

    snprintf (buf, sizeof buf, "%s", comp);
    LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf (buf, sizeof buf, "CPU %lu MHz", mhz);
    LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf (buf, sizeof buf, "SPI");
    LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);
    snprintf (buf, sizeof buf, "%s", mhz_text (LCD_HwSpiKHz()));
    LCD_DisplayString ((uint16_t)(INFO_X + 5 * 8), (uint16_t)y, buf);  y += INFO_DY;

    snprintf (buf, sizeof buf, "I2C");
    LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);
    snprintf (buf, sizeof buf, "%lu kHz", (unsigned long)(Touch_GetHz() / 1000UL));
    LCD_DisplayString ((uint16_t)(INFO_X + 5 * 8), (uint16_t)y, buf);  y += INFO_DY;

    /* Die temperature, as reported by the adc_temp_internal project. */
    {
        s32 t100 = adc_celsius_x100();
        snprintf (buf, sizeof buf, "DIE");
        LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);
        snprintf (buf, sizeof buf, "%d.%02d C", (int)(t100 / 100), (int)(t100 % 100));
        LCD_DisplayString ((uint16_t)(INFO_X + 5 * 8), (uint16_t)y, buf);  y += INFO_DY;
    }

    /* Fill durations, measured earlier this pass so they always reflect the
     * current SPI rate. The names come from g_solid_name, so adding a
     * colour to the list cannot leave this page mislabelled. The 8x16
     * font allows 29 chars per line at the 240 px width. */
    for (i = 0; i < 5; i += 2)
    {
        if (i + 1 < 5)
        {
            snprintf (buf, sizeof buf, "%s %lu  %s %lu",
                      g_solid_name[i], (unsigned long)g_solid_ms[i],
                      g_solid_name[i + 1], (unsigned long)g_solid_ms[i + 1]);
        }
        else
        {
            snprintf (buf, sizeof buf, "%s %lu",
                      g_solid_name[i], (unsigned long)g_solid_ms[i]);
        }
        LCD_DisplayString ((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;
    }

    g_fps_color = LCD_WHITE;              /* restore default FPS glyph color */

    {
        uint32_t start = ms_now();
        do
        {
            fps_update();
            touch_task();                 /* touch printout during waits */
            delay_ms (50);
        } while (ms_now() - start < ms);
    }

    LCD_SetAsciiFont (&ASCII_Font12);     /* back to the normal font      */
}

/* --------------------------------------------------------------------- */
/* Big-font banner page (8x16 font), lines centered on the panel width.  */
static void banner_page(const char *l1, const char *l2,
                        uint32_t fg, uint32_t bg, uint32_t ms)
{
    g_fps_color = fg;

    LCD_SetAsciiFont (&ASCII_Font16);     /* bigger than the normal 6x12  */
    LCD_SetColor (fg);
    LCD_SetBackColor (bg);
    LCD_Clear();
    LCD_SetColor (bg);
    LCD_SetBackColor (bg);
    LCD_FillRect (0, anim_h(), LCD_W(), FPS_BAND);
    LCD_SetColor (fg);
    LCD_SetBackColor (bg);

    /* 8 px/glyph: center each line dynamically. */
    LCD_DisplayString ((uint16_t)((LCD_W() - (int)strlen(l1) * 8) / 2), 40,
                       (char *)l1);
    LCD_DisplayString ((uint16_t)((LCD_W() - (int)strlen(l2) * 8) / 2), 64,
                       (char *)l2);

    delay_ms (ms);

    LCD_SetAsciiFont (&ASCII_Font12);     /* back to the normal font      */
    g_fps_color = LCD_WHITE;
}

/* --------------------------------------------------------------------- */
/* Full test-pattern set.                                                */
/* --------------------------------------------------------------------- */
/* Checkerboard stress pattern - the sensitive test the solid-fill screens
 * are missing.
 *
 * Solid fills never change their data, so any bit error is hidden, while a
 * checkerboard alternates every single pixel and exposes one wrong bit as
 * a broken cell.
 *
 * Single pixels are drawn with LCD_CopyBuffer so the pattern also
 * exercises the address-window setup (a lost WriteComm shows up as a
 * shifted row). Any tearing, bit error or framing slip is immediately
 * obvious. */
static void stress_pattern(void)
{
    static uint16_t row[SCREEN_W];
    int y, x;

    LCD_SetColor (LCD_WHITE);
    LCD_SetBackColor (BACK_COLOR);
    paint_fps_band();

    for (y = 0; y < (int)anim_h(); y++)
    {
        for (x = 0; x < (int)LCD_W(); x++)
        {
            /* 1-pixel checkerboard: adjacent pixels are opposite, so every
             * data bit toggles on every pixel. */
            row[x] = ((x ^ y) & 1) ? C565_WHITE : C565_BLACK;
        }
        LCD_CopyBuffer (0, (uint16_t)y, LCD_W(), 1, row);
    }
}

/* =====================================================================
   Asset fill - the cold-boot wait, made productive.
   ===================================================================== */

/* Tile asset_test1 over the row range [y0, y1).
 *
 * As many complete tiles as fit are drawn, centred, with a gap between
 * them and a margin all round - nothing is drawn hard against an edge, so
 * the rounded corners never clip a tile and no partial tile is emitted.
 * This is the real image (RGB565, alpha composited over black when it was
 * converted), not a synthesised shape.
 *
 * 64x64 tiles at 240 px wide give 3 columns; the drawing is therefore
 * short (tens of ms). The wait is dominated by the LCD_Reinit() calls in
 * panel_bringup_draw(), which each carry the datasheet's own delays. */
static void asset_fill_range(uint16_t y0, uint16_t y1)
{
    const int tw  = ASSET_TEST1_W;
    const int th  = ASSET_TEST1_H;
    const int gap = ASSET_TILE_GAP;
    int span_h = (int)y1 - (int)y0;
    int cols, rows, used_w, used_h, x_off, y_off, r, c;

    if ((span_h < th) || ((int)LCD_W() < tw))
    {
        return;                          /* no room for even one tile */
    }

    cols = ((int)LCD_W() - gap) / (tw + gap);
    rows = (span_h   - gap) / (th + gap);
    if ((cols <= 0) || (rows <= 0))
    {
        return;
    }

    used_w = cols * tw + (cols - 1) * gap;
    used_h = rows * th + (rows - 1) * gap;
    x_off  = ((int)LCD_W() - used_w) / 2;
    y_off  = (int)y0 + (span_h - used_h) / 2;

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            LCD_CopyBuffer ((uint16_t)(x_off + c * (tw + gap)),
                            (uint16_t)(y_off + r * (th + gap)),
                            (uint16_t)tw, (uint16_t)th, asset_test1);
        }
    }
}

/* Bring the panel up by drawing.
 *
 * The drawing replaces the old sleep-and-retry wait: the time is spent
 * sending real pixels, and the artwork appearing is itself the "the
 * panel is up" signal. The LCD_Reinit() calls are what actually spend
 * most of the wall-clock time, and each one is another chance for a
 * slow-starting panel to catch an init - which is why the structure
 * interleaves draws and reinits: draw -> Reinit -> draw -> Reinit ->
 * ... -> final draw.
 *
 * The final draw is deliberate: it happens after the last Reinit, so the
 * image that stays on screen was sent with the most recent init in
 * effect. Every tile is whole and inset, so nothing is clipped by the
 * rounded corners. */
static void panel_bringup_draw(void)
{
    uint32_t pass;

    LCD_Clear();

    for (pass = 0; pass < LCD_BRINGUP_PASSES; pass++)
    {
        printf ("[%lums] bring-up %lu/%lu: draw\r\n",
                (unsigned long)uptime_ms(),
                (unsigned long)(pass + 1), (unsigned long)LCD_BRINGUP_PASSES);
        asset_fill_range (INFO_TOP, anim_h());

        printf ("[%lums] bring-up: Reinit\r\n", (unsigned long)uptime_ms());
        LCD_Reinit();
    }

    /* Last draw with the most recent init in effect. */
    asset_fill_range (INFO_TOP, anim_h());
    printf ("[%lums] bring-up: done\r\n", (unsigned long)uptime_ms());
}

static void run_patterns(void)
{
    /* Sensitive pattern FIRST, right after LCD_Init(): this is what
     * actually proves the panel initialised and the SPI rate is usable.
     * Clean checkerboard = the init landed; malformed = it did not (or
     * the link is marginal). Running it first also shortens the wait
     * before the first meaningful thing appears on a cold boot. */
    printf ("[%lums] phase: STRESS (checkerboard @ %s)\r\n",
            (unsigned long)uptime_ms(), mhz_text (LCD_HwSpiKHz()));
    stress_pattern();
    delay_with_fps (3000);

    printf ("[%lums] phase: TEST_STAND\r\n", (unsigned long)uptime_ms());
    memset (g_solid_ms, 0, sizeof g_solid_ms);   /* current method only */
    TEST_STAND();

    printf ("[%lums] phase: info\r\n", (unsigned long)uptime_ms());
    info_demo (5000, 0);

    printf ("[%lums] phase: info (inverted colors)\r\n", (unsigned long)uptime_ms());
    info_demo (5000, 1);

    printf ("[%lums] phase: gradient\r\n", (unsigned long)uptime_ms());
    gradient_demo (4000);

    printf ("[%lums] phase: LED test\r\n", (unsigned long)uptime_ms());
    led_test();
}

/* --------------------------------------------------------------------- */
/* One complete bring-up + demo pass on one transport.
 *
 * Both the LCD and the touch bus are switched together, the panel is
 * initialised from scratch, and the full pattern set runs. Doing the
 * whole sequence per transport is the point: the same panel is driven
 * end to end by soft (bit-banged) SPI/I2C and by the SPI2/I2C2
 * peripherals, one after the other. */
static void bus_pass(uint8_t hw, const char *name)
{
    static uint8_t st_buf[8];
    uint8_t ack;
    uint8_t i;

    printf ("\r\n[%lums] ======== %s pass ========\r\n",
            (unsigned long)uptime_ms(), name);

    /* Switch transport on both buses, then bring each one up. */
    LCD_SelectBus (hw);
    Touch_SelectBus (hw);

    LCD_UseHwBus();
    Touch_Init();
    Touch_Scan();

    /* Bring-up diagnostic: does the CST816D answer on this bus at all? */
    ack = Touch_SelfTest();
    printf ("[TOUCH] self-test: %s\r\n",
            (ack != 0U) ? "ACK (chip present)" : "NO ACK (check wiring/addr)");

    Touch_Read (st_buf, 8);
    printf ("[TOUCH] id regs:");
    for (i = 0; i < 8U; i++)
    {
        printf (" %02X", st_buf[i]);
    }
    printf ("\r\n");

    LCD_BusDump();        /* bus state, for soft-vs-hardware comparison */

    LCD_Init();
    LCD_SetAsciiFont (&ASCII_Font12);
    paint_fps_band();

    /* Panel bring-up by drawing.
     *
     * The wait is the drawing itself (no idle delay), progress is
     * visible, and the icon appearing IS the "panel is up" signal - it is
     * also the test, because the icon is drawn with the same path as
     * everything else. Each draw half is preceded by LCD_Reinit(), so
     * both init paths get a chance.
     *
     * A border is left undrawn around the icon field, and the range stays
     * inside INFO_TOP..anim_h(), to keep clear of the rounded corners and
     * the FPS band. */
    panel_bringup_draw();

    printf ("[%lums] phase: banner (Reinit)\r\n", (unsigned long)uptime_ms());
    LCD_Reinit();         /* re-frame the panel */
    banner_page ("NV3030B", name, LCD_BLACK, LCD_CYAN, 3000);

    printf ("[%lums] running patterns on %s @ %s\r\n",
            (unsigned long)uptime_ms(), name, mhz_text (LCD_HwSpiKHz()));
    run_patterns();
}

void lcd_demo_main(void);

int main(void)
{
    SystemCoreClockUpdate();
    SysTick_Init();
    USART_Printf_Init (115200);

    lcd_demo_main();

    while (1)
    {
        ;
    }
}

void lcd_demo_main(void)
{
    printf ("\r\n==== ch32v307evt (CH32V307) nv3030b_md183_240x284_cst816d @ %lu MHz ====\r\n",
            (unsigned long)(SystemCoreClock / 1000000UL));
    printf ("NV3030B 1.83\" 240x284 (wrapped-command SPI, MADCTL 0x08):\r\n");
    printf ("SCK=PB13 MOSI=PB15 CS=PB12; no DC/MISO/RST/BL pin\r\n");
    printf ("TOUCH: CST816D I2C on SCL=PB10 SDA=PB11\r\n");
    printf ("Alternating SOFT (bit-banged) and HARDWARE (SPI2/I2C2) passes.\r\n");

    printf ("[BOOT] ChipID=%08X sysclk=%lu MHz\r\n",
            (unsigned)DBGMCU_GetCHIPID(),
            (unsigned long)(SystemCoreClock / 1000000UL));

    led_init();

    /* Alternate the two transports forever: soft (bit-banged) first, then
     * the SPI2/I2C2 peripherals, each doing a full bring-up + pattern
     * set. The panel is re-initialised on every pass, so each transport
     * is exercised from a clean state rather than inheriting the other's
     * setup. */
    while (1)
    {
        bus_pass (LCD_BUS_SOFT, "soft SPI");
        bus_pass (LCD_BUS_HW,   "HW SPI2");
    }
}
