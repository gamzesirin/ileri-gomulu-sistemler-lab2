/*
 * İleri Gömülü Sistemler - Laboratuvar 2, Görev 4 (ESP32-S3)
 * GPTimer (GPIO4) ve esp_timer (GPIO5) ile 1 kHz kare dalga,
 * çağrı aralığı istatistikleri ve isteğe bağlı Wi-Fi yükü.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "lab_reg.h"

#define WIFI_YUKU          0     /* 1: arka planda sürekli Wi-Fi taraması */

#if WIFI_YUKU
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#endif

#define PIN_GPTIMER        4u
#define PIN_ESPTIMER       5u
#define ARALIK_US          500u    /* yarım periyot: 500 us -> 1 kHz */
#define CPU_MHZ            CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#define HIST_ALT_US        400u
#define HIST_KUTU          201u    /* 400..600 us, 1 us genişlik */

/* GPTimer sürücüsünün ayarladığı TIMG0 Timer0 registerları */
#define TIMG0_BASE           0x6001F000u
#define TIMG0_T0CONFIG_REG   (TIMG0_BASE + 0x00u)
#define TIMG0_T0ALARMLO_REG  (TIMG0_BASE + 0x10u)
#define TIMG0_T0LOADLO_REG   (TIMG0_BASE + 0x18u)
#define TIMG0_INT_ENA_REG    (TIMG0_BASE + 0x70u)
#define INTR_MAP_TG0_T0_REG  0x600C20C8u   /* INTERRUPT_CORE0_TG_T0_INT_MAP_REG */

typedef struct {
    const char *ad;
    uint32_t pin;
    uint32_t duzey;
    uint32_t onceki;
    uint32_t n, min, max;
    uint64_t toplam, kare_toplam;
    uint32_t hist[HIST_KUTU + 2];
} kanal_t;

static kanal_t kanal_gpt = { .ad = "GPTimer",   .pin = PIN_GPTIMER,  .min = UINT32_MAX };
static kanal_t kanal_est = { .ad = "esp_timer", .pin = PIN_ESPTIMER, .min = UINT32_MAX };
static portMUX_TYPE kilit = portMUX_INITIALIZER_UNLOCKED;
static const char *TAG = "lab2";

static inline void IRAM_ATTR kanal_kaydet(kanal_t *k, uint32_t simdi)
{
    gpio_yaz(k->pin, k->duzey);
    k->duzey ^= 1u;
    if (k->onceki != 0) {
        uint32_t d = simdi - k->onceki;
        k->n++;
        k->toplam += d;
        k->kare_toplam += (uint64_t)d * d;
        if (d < k->min) k->min = d;
        if (d > k->max) k->max = d;
        uint32_t us = d / CPU_MHZ;
        uint32_t i = (us < HIST_ALT_US) ? 0 : (us - HIST_ALT_US + 1);
        if (i > HIST_KUTU + 1) i = HIST_KUTU + 1;
        k->hist[i]++;
    }
    k->onceki = simdi;
}

static bool IRAM_ATTR gpt_alarm(gptimer_handle_t t,
                                const gptimer_alarm_event_data_t *e, void *ctx)
{
    uint32_t simdi = esp_cpu_get_cycle_count();
    portENTER_CRITICAL_ISR(&kilit);
    kanal_kaydet(&kanal_gpt, simdi);
    portEXIT_CRITICAL_ISR(&kilit);
    return false;
}

static void esp_timer_cb(void *arg)
{
    uint32_t simdi = esp_cpu_get_cycle_count();
    portENTER_CRITICAL(&kilit);
    kanal_kaydet(&kanal_est, simdi);
    portEXIT_CRITICAL(&kilit);
}

static void kanal_yazdir(kanal_t *k)
{
    static kanal_t kopya;
    portENTER_CRITICAL(&kilit);
    kopya = *k;
    k->n = 0;
    k->toplam = 0;
    k->kare_toplam = 0;
    k->min = UINT32_MAX;
    k->max = 0;
    memset(k->hist, 0, sizeof(k->hist));
    portEXIT_CRITICAL(&kilit);

    if (kopya.n == 0) {
        ESP_LOGW(TAG, "%s: örnek yok", kopya.ad);
        return;
    }
    double ort = (double)kopya.toplam / kopya.n;
    double var = (double)kopya.kare_toplam / kopya.n - ort * ort;
    double ss  = (var > 0) ? sqrt(var) : 0.0;
    ESP_LOGI(TAG, "%-9s n=%5" PRIu32 " ort=%8.3f ss=%7.3f min=%8.3f max=%8.3f (us)",
             kopya.ad, kopya.n, ort / CPU_MHZ, ss / CPU_MHZ,
             (double)kopya.min / CPU_MHZ, (double)kopya.max / CPU_MHZ);
    for (uint32_t i = 0; i < HIST_KUTU + 2; i++) {
        if (kopya.hist[i] == 0) continue;
        if (i == 0) {
            printf("hist,%s,<%u,%" PRIu32 "\n", kopya.ad, HIST_ALT_US, kopya.hist[i]);
        } else if (i == HIST_KUTU + 1) {
            printf("hist,%s,>%u,%" PRIu32 "\n", kopya.ad,
                   HIST_ALT_US + HIST_KUTU - 1, kopya.hist[i]);
        } else {
            printf("hist,%s,%" PRIu32 ",%" PRIu32 "\n", kopya.ad,
                   HIST_ALT_US + i - 1, kopya.hist[i]);
        }
    }
}

static void timg_dump(void)
{
    uint32_t c = REG(TIMG0_T0CONFIG_REG);
    ESP_LOGI(TAG, "TIMG0_T0CONFIG  = 0x%08" PRIx32, c);
    ESP_LOGI(TAG, "  EN=%" PRIu32 " INCREASE=%" PRIu32 " AUTORELOAD=%" PRIu32
             " DIVIDER=%" PRIu32 " ALARM_EN=%" PRIu32 " USE_XTAL=%" PRIu32,
             (c >> 31) & 1u, (c >> 30) & 1u, (c >> 29) & 1u,
             (c >> 13) & 0xFFFFu, (c >> 10) & 1u, (c >> 9) & 1u);
    ESP_LOGI(TAG, "TIMG0_T0ALARMLO = %" PRIu32, REG(TIMG0_T0ALARMLO_REG));
    ESP_LOGI(TAG, "TIMG0_T0LOADLO  = %" PRIu32, REG(TIMG0_T0LOADLO_REG));
    ESP_LOGI(TAG, "TIMG0_INT_ENA   = 0x%08" PRIx32, REG(TIMG0_INT_ENA_REG));
    ESP_LOGI(TAG, "TG0_T0 eşlemesi = %" PRIu32, REG(INTR_MAP_TG0_T0_REG) & 0x1Fu);
}

#if WIFI_YUKU
static void wifi_yuk_gorevi(void *arg)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    while (1) {
        esp_wifi_scan_start(NULL, true);
        esp_wifi_clear_ap_list();
    }
}
#endif

void app_main(void)
{
    gpio_cikis_ayarla(PIN_GPTIMER);
    gpio_cikis_ayarla(PIN_ESPTIMER);

    /* GPTimer: 1 MHz sayım, 500 sayımda alarm, otomatik yeniden yükleme */
    gptimer_handle_t gpt;
    gptimer_config_t gcfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&gcfg, &gpt));
    gptimer_event_callbacks_t cbs = { .on_alarm = gpt_alarm };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gpt, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gpt));
    gptimer_alarm_config_t alarm = {
        .alarm_count  = ARALIK_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gpt, &alarm));
    ESP_ERROR_CHECK(gptimer_start(gpt));

    /* esp_timer: 500 us periyotlu yazılım zamanlayıcısı */
    esp_timer_handle_t est;
    esp_timer_create_args_t ecfg = {
        .callback        = esp_timer_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "kare_dalga",
    };
    ESP_ERROR_CHECK(esp_timer_create(&ecfg, &est));
    ESP_ERROR_CHECK(esp_timer_start_periodic(est, ARALIK_US));

    timg_dump();
#if WIFI_YUKU
    xTaskCreatePinnedToCore(wifi_yuk_gorevi, "wifi_yuk", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "Wi-Fi yükü açık");
#endif

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        kanal_yazdir(&kanal_gpt);
        kanal_yazdir(&kanal_est);
    }
}
