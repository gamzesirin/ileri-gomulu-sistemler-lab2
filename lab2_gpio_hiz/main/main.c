/*
 * İleri Gömülü Sistemler - Laboratuvar 2, Görev 1 ve 2 (ESP32-S3)
 * Görev 1: register ile yapılan ayarın gpio_config() ile karşılaştırılması
 * Görev 2: üç erişim yöntemiyle en yüksek pin değiştirme hızı
 */
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "lab_reg.h"

#define CIKIS_PIN        4u
#define CIKIS_MASK       (1u << CIKIS_PIN)
#define PERIYOT_SAYISI   10000u
#define CPU_MHZ          CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ

static const char *TAG = "lab2";
static portMUX_TYPE kilit = portMUX_INITIALIZER_UNLOCKED;

static void reg_dump(const char *baslik)
{
    ESP_LOGI(TAG, "--- %s ---", baslik);
    ESP_LOGI(TAG, "IO_MUX_GPIO4  = 0x%08" PRIx32, REG(IO_MUX_GPIO_REG(CIKIS_PIN)));
    ESP_LOGI(TAG, "GPIO_PIN4     = 0x%08" PRIx32, REG(GPIO_PIN_REG(CIKIS_PIN)));
    ESP_LOGI(TAG, "FUNC4_OUT_SEL = 0x%08" PRIx32, REG(GPIO_FUNC_OUT_SEL_REG(CIKIS_PIN)));
    ESP_LOGI(TAG, "ENABLE bit 4  = %" PRIu32, (REG(GPIO_ENABLE_REG) >> CIKIS_PIN) & 1u);
}

/* Yöntem 1: ESP-IDF sürücü API'si */
static void __attribute__((noinline)) degistir_surucu(uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        gpio_set_level(CIKIS_PIN, 1);
        gpio_set_level(CIKIS_PIN, 0);
    }
}

/* Yöntem 2: soc/gpio_struct.h yapı eşlemesi */
static void __attribute__((noinline)) degistir_yapi(uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        GPIO.out_w1ts = CIKIS_MASK;
        GPIO.out_w1tc = CIKIS_MASK;
    }
}

/* Yöntem 3: adres makrosu ile ham register erişimi */
static void __attribute__((noinline)) degistir_register(uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        REG(GPIO_OUT_W1TS_REG) = CIKIS_MASK;
        REG(GPIO_OUT_W1TC_REG) = CIKIS_MASK;
    }
}

static void olc(const char *ad, void (*fonk)(uint32_t))
{
    taskENTER_CRITICAL(&kilit);
    uint32_t c0 = esp_cpu_get_cycle_count();
    fonk(PERIYOT_SAYISI);
    uint32_t c1 = esp_cpu_get_cycle_count();
    taskEXIT_CRITICAL(&kilit);

    uint32_t toplam = c1 - c0;
    uint32_t cevrim = toplam / PERIYOT_SAYISI;
    uint64_t f_hz = (uint64_t)CPU_MHZ * 1000000u * PERIYOT_SAYISI / toplam;
    ESP_LOGI(TAG, "%-9s: %4" PRIu32 " çevrim/periyot, f = %7" PRIu64 " Hz",
             ad, cevrim, f_hz);
}

void app_main(void)
{
    /* Görev 1: önce registerlarla, sonra gpio_config() ile ayar */
    gpio_cikis_ayarla(CIKIS_PIN);
    reg_dump("register ile ayar");

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CIKIS_PIN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    reg_dump("gpio_config() ile ayar");

    /* Görev 2: üç yöntem sırayla, aralarında bekleme */
    ESP_LOGI(TAG, "CPU saati: %d MHz", CPU_MHZ);
    while (1) {
        olc("surucu", degistir_surucu);
        vTaskDelay(pdMS_TO_TICKS(200));
        olc("yapi", degistir_yapi);
        vTaskDelay(pdMS_TO_TICKS(200));
        olc("register", degistir_register);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
