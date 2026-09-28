/**
 * @file    http_server.c
 * @brief   e_server-style HTTP server on the WCH net stack (wchnet).
 *
 * Adapted from f7-demo/nucleo-f746/bare/eth_http_server/src/http_server.c
 * (lwIP raw API) to the WCHNET socket API. Serves the single-page site from
 * User/web_assets.h (packed by ../../e_server/build_web.py) and the JSON API:
 *
 *   GET  /                 page, Content-Encoding: gzip
 *   GET  /api/leds         {"leds":[0]}          (real LED GPIO state, PA0)
 *   POST /api/leds         body {"leds":[0]}     -> applies to the LED
 *   GET  /api/adc          {"vrefint_mv":..,"temp_c":..,"ts":..}
 *   GET  /api/info         {"arch":"riscv","lan_ip":...,
 *                           "public_ip":null,"geo":null,"weather":null}
 *   GET  /public/<name>    raw bytes from the embedded_files[] table
 *                          (none embedded on this board - flash budget)
 *
 * The public IP / geo / weather fields are reported as null (the page shows
 * "N/A") because the board has no HTTP/TLS client.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wchnet.h"
#include "http_server.h"
#include "web_assets.h"

/* --------------------------------------------------------------------------
 * On-board LED: single LED on PA0 (push-pull, high active), the same wiring
 * as the blink project on this board.
 * ------------------------------------------------------------------------ */
static void led_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA, ENABLE);
    gpio.GPIO_Pin   = GPIO_Pin_0;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOA, &gpio);
    GPIO_WriteBit (GPIOA, GPIO_Pin_0, Bit_RESET);
}

static int led_get(void)
{
    return GPIO_ReadOutputDataBit (GPIOA, GPIO_Pin_0) ? 1 : 0;
}

static void led_set(int on)
{
    GPIO_WriteBit (GPIOA, GPIO_Pin_0, on ? Bit_SET : Bit_RESET);
}

/* --------------------------------------------------------------------------
 * ADC1 internal channels (CH16 temperature sensor / CH17 Vrefint), the same
 * measurement as bare/adc_temp_internal. There is no VBAT channel on the
 * CH32V307, so the API reports the VDD estimate from Vrefint plus the die
 * temperature.
 * ------------------------------------------------------------------------ */
#define ADC_TS_CAL_ADDR   ((u32)0x1FFFF720)   /* factory calibration: lo = V@30C, hi = 30C temp */

static s16 Calibrattion_Val;
static u8  adc_ready;

static void adc_init(void)
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
    Calibrattion_Val = Get_CalibrationValue (ADC1);
    ADC_BufferCmd (ADC1, ENABLE);

    ADC_TempSensorVrefintCmd (ENABLE);
    adc_ready = 1;
}

static u16 adc_avg(u8 ch, u8 times)
{
    u32 sum = 0;
    u8 t;
    u16 val;

    for (t = 0; t < times; t++)
    {
        ADC_RegularChannelConfig (ADC1, ch, 1, ADC_SampleTime_239Cycles5);
        ADC_SoftwareStartConvCmd (ADC1, ENABLE);
        while (!ADC_GetFlagStatus (ADC1, ADC_FLAG_EOC));
        sum += ADC_GetConversionValue (ADC1);
        Delay_MS (2);
    }

    val = sum / times;
    if ((val + Calibrattion_Val) < 0 || val == 0) return 0;
    if ((Calibrattion_Val + val) > 4095 || val == 4095) return 4095;
    return val + Calibrattion_Val;
}

static void adc_measure(float *vdd_mv, float *temp_c)
{
    u16 vref_raw, ts_raw;
    u16 refer_volt, refer_temper;
    float vsense_mv;

    if (!adc_ready)
    {
        adc_init();
    }

    /* Vrefint is 1.2 V typical: the raw reading gives the actual VDD. */
    vref_raw = adc_avg (ADC_Channel_Vrefint, 8);
    *vdd_mv = 1200.0f * 4096.0f / vref_raw;

    ts_raw = adc_avg (ADC_Channel_TempSensor, 8);
    vsense_mv = ts_raw * *vdd_mv / 4096.0f;

    /* Same conversion as the periph lib TempSensor_Volt_To_Temper(): factory
     * calibration pair at 0x1FFFF720, 4.3 mV/degC slope. */
    refer_volt   = (u16)(*(u32 *)ADC_TS_CAL_ADDR & 0x0000FFFF);
    refer_temper = (u16)((*(u32 *)ADC_TS_CAL_ADDR >> 16) & 0x0000FFFF);
    *temp_c = (float)refer_temper - (vsense_mv - refer_volt) * 10.0f / 43.0f;
}

/* --------------------------------------------------------------------------
 * Response plumbing over the WCHNET socket API.
 * ------------------------------------------------------------------------ */
#define SEND_CHUNK   1024

/* Send the whole buffer; WCHNET_SocketSend may accept less than requested
 * (its send buffer fills up), so pump the stack and retry until done. */
static void send_all(u8 socketid, const u8 *buf, u32 len)
{
    u32 off = 0;

    while (off < len)
    {
        u32 n = len - off;
        u8 s = WCHNET_SocketSend (socketid, (u8 *)buf + off, &n);
        if (s == WCHNET_ERR_SUCCESS && n > 0)
        {
            off += n;
            continue;
        }
        if (s != WCHNET_ERR_SUCCESS)
        {
            return;                     /* socket went away */
        }
        WCHNET_MainTask ();             /* drain the send buffer */
        Delay_Us (200);
    }
}

static void reply_raw(u8 socketid, const char *hdr, u32 hdr_len,
                      const u8 *body, u32 body_len)
{
    send_all (socketid, (const u8 *)hdr, hdr_len);
    if (body_len)
    {
        send_all (socketid, body, body_len);
    }
}

/* JSON replies are built into this buffer (header + body together). */
static char json_buf[512];

static void reply_json(u8 socketid, const char *body)
{
    int n = snprintf (json_buf, sizeof (json_buf),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json; charset=utf-8\r\n"
                      "Content-Length: %u\r\n"
                      "Cache-Control: no-store\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "%s",
                      (unsigned)strlen (body), body);
    reply_raw (socketid, json_buf, (u32)n, NULL, 0);
}

static void reply_error(u8 socketid, int code, const char *status)
{
    int n = snprintf (json_buf, sizeof (json_buf),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 0\r\n"
                      "Cache-Control: no-store\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      code, status);
    reply_raw (socketid, json_buf, (u32)n, NULL, 0);
}

/* Static bodies (page/images) are const arrays in flash; the stack copies
 * them (CFG0_TCP_SEND_COPY = 1), so no bounce buffer is needed. */
static void reply_static(u8 socketid, const char *ctype, const char *encoding,
                         const u8 *data, u32 len)
{
    int n = snprintf (json_buf, sizeof (json_buf),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "%s"
                      "Content-Length: %lu\r\n"
                      "Cache-Control: no-store\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      ctype,
                      encoding ? encoding : "",
                      (unsigned long)len);
    reply_raw (socketid, json_buf, (u32)n, data, len);
}

/* --------------------------------------------------------------------------
 * API handlers.
 * ------------------------------------------------------------------------ */
static void api_leds(u8 socketid, const char *method, const char *body)
{
    if (strcmp (method, "POST") == 0 && body)
    {
        const char *b = strchr (body, '[');
        if (b)
        {
            int a[3] = { 0, 0, 0 };
            int got = sscanf (b + 1, "%d , %d , %d", &a[0], &a[1], &a[2]);
            /* Only index 0 exists on this board; extra values are ignored. */
            if (got > 0)
            {
                led_set (a[0]);
            }
        }
    }

    char resp[32];
    snprintf (resp, sizeof (resp), "{\"leds\":[%d]}", led_get());
    reply_json (socketid, resp);
}

static void api_adc(u8 socketid)
{
    float vdd_mv, temp_c;
    char resp[128];

    adc_measure (&vdd_mv, &temp_c);
    snprintf (resp, sizeof (resp),
              "{\"vrefint_mv\":%.1f,\"temp_c\":%.1f,\"ts\":%lu}",
              vdd_mv, temp_c, (unsigned long)Get_SysTick_MS());
    reply_json (socketid, resp);
}

static void api_info(u8 socketid)
{
    extern u8 IPAddr[4];
    char resp[192];

    snprintf (resp, sizeof (resp),
              "{\"arch\":\"riscv\",\"lan_ip\":\"%u.%u.%u.%u\","
              "\"public_ip\":null,\"geo\":null,\"weather\":null,\"ts\":%lu}",
              IPAddr[0], IPAddr[1], IPAddr[2], IPAddr[3],
              (unsigned long)Get_SysTick_MS());
    reply_json (socketid, resp);
}

static void serve_public(u8 socketid, const char *path)
{
#if defined(embedded_files_count) && embedded_files_count > 0
    for (unsigned i = 0; i < embedded_files_count; i++)
    {
        if (strcmp (path, embedded_files[i].path) == 0)
        {
            reply_static (socketid, embedded_files[i].ctype, NULL,
                          embedded_files[i].data, embedded_files[i].len);
            return;
        }
    }
#endif
    reply_error (socketid, 404, "Not Found");
}

/* --------------------------------------------------------------------------
 * Routing.
 * ------------------------------------------------------------------------ */
#define HTTP_REQ_MAX   1500   /* request buffer clamp (below RECE_BUF_LEN) */

void http_server_serve(u8 socketid, u8 *buf, u32 len)
{
    char method[8] = {0}, path[256] = {0};
    char *body;

    if (len > HTTP_REQ_MAX)
    {
        len = HTTP_REQ_MAX;
    }
    buf[len] = 0;

    if (sscanf ((char *)buf, "%7s %255s", method, path) != 2)
    {
        reply_error (socketid, 400, "Bad Request");
        return;
    }
    {
        char *q = strchr (path, '?');
        if (q) *q = 0;
    }
    body = strstr ((char *)buf, "\r\n\r\n");
    body = body ? body + 4 : NULL;

    printf ("HTTP %s %s\r\n", method, path);

    if (strcmp (path, "/") == 0 || strcmp (path, "/index.html") == 0)
    {
        reply_static (socketid, "text/html; charset=utf-8",
                      "Content-Encoding: gzip\r\n",
                      index_html_gz, index_html_gz_len);
    }
    else if (strcmp (path, "/api/leds") == 0)
    {
        api_leds (socketid, method, body);
    }
    else if (strcmp (path, "/api/adc") == 0)
    {
        api_adc (socketid);
    }
    else if (strcmp (path, "/api/info") == 0)
    {
        api_info (socketid);
    }
    else if (strcmp (path, "/favicon.ico") == 0)
    {
        reply_error (socketid, 204, "No Content");
    }
    else if (strncmp (path, "/public/", 8) == 0)
    {
        serve_public (socketid, path);
    }
    else
    {
        reply_error (socketid, 404, "Not Found");
    }
}

void http_server_init(void)
{
    led_init();
    led_set (0);
}
