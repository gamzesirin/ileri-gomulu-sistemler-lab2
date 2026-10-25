/*
 * İleri Gömülü Sistemler - Laboratuvar 2, Görev 3 (ESP32-S3)
 * BOOT düğmesi (GPIO0) kesmesinin register düzeyinde kurulması,
 * buton sıçramasının ölçülmesi ve yazılımsal sıçrama filtresi.
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_intr_alloc.h"
#include "soc/interrupts.h"
#include "esp_cpu.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "lab_reg.h"

#define BUTON_PIN          0u
#define LED_PIN            4u
#define CPU_MHZ            CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#define SESSIZLIK_MS       20
#define KUYRUK_BOYU        64
#define EN_FAZLA_LISTE     12
#define INTR_MAP_GPIO_REG  0x600C2040u  /* INTERRUPT_CORE0_GPIO_INTERRUPT_PRO_MAP_REG */

typedef struct {
    uint32_t zaman;   /* kenar anındaki CPU çevrim sayacı */
    uint32_t duzey;   /* kenardan hemen sonra okunan pin düzeyi */
} kenar_t;

static const char *TAG = "lab2";
static QueueHandle_t kuyruk;
static volatile uint32_t kayip_kenar;

static void IRAM_ATTR gpio_isr(void *arg)
{
    uint32_t durum = REG(GPIO_PCPU_INT_REG);
    REG(GPIO_STATUS_W1TC_REG) = durum;

    if (durum & (1u << BUTON_PIN)) {
        kenar_t k = {
            .zaman = esp_cpu_get_cycle_count(),
            .duzey = gpio_oku(BUTON_PIN),
        };
        BaseType_t uyandir = pdFALSE;
        if (xQueueSendFromISR(kuyruk, &k, &uyandir) != pdTRUE) {
            kayip_kenar++;
        }
        if (uyandir) {
            portYIELD_FROM_ISR();
        }
    }
}

static void buton_gorevi(void *arg)
{
    kenar_t k;
    uint32_t kararli = gpio_oku(BUTON_PIN);
    uint32_t basis = 0, toplam_kenar = 0;
    char liste[EN_FAZLA_LISTE * 8 + 8];

    while (1) {
        /* 1. Yeni bir kenar dizisinin ilk kenarını bekle */
        xQueueReceive(kuyruk, &k, portMAX_DELAY);
        uint32_t ilk = k.zaman, son = k.zaman, n = 1;
        int yazilan = snprintf(liste, sizeof(liste), "0");

        /* 2. SESSIZLIK_MS boyunca kenar gelmeyene kadar topla */
        while (xQueueReceive(kuyruk, &k, pdMS_TO_TICKS(SESSIZLIK_MS)) == pdTRUE) {
            son = k.zaman;
            n++;
            if (n <= EN_FAZLA_LISTE) {
                yazilan += snprintf(liste + yazilan, sizeof(liste) - yazilan,
                                    " %" PRIu32, (son - ilk) / CPU_MHZ);
            }
        }
        toplam_kenar += n;

        /* 3. Sinyal duruldu: kararlı düzeyi oku ve olayı belirle */
        uint32_t yeni = gpio_oku(BUTON_PIN);
        const char *olay = "gürültü";
        if (yeni != kararli) {
            kararli = yeni;
            if (kararli == 0) {
                olay = "BASMA";
                basis++;
                gpio_yaz(LED_PIN, basis & 1u);
            } else {
                olay = "BIRAKMA";
            }
        }
        ESP_LOGI(TAG, "%-8s %2" PRIu32 " kenar, %6" PRIu32 " us | basış=%" PRIu32
                 " kenar=%" PRIu32 " kayıp=%" PRIu32,
                 olay, n, (son - ilk) / CPU_MHZ, basis, toplam_kenar, kayip_kenar);
        ESP_LOGI(TAG, "         kenar zamanları (us): %s%s", liste,
                 n > EN_FAZLA_LISTE ? " ..." : "");
    }
}

void app_main(void)
{
    gpio_cikis_ayarla(LED_PIN);
    gpio_giris_ayarla(BUTON_PIN, 1);

    kuyruk = xQueueCreate(KUYRUK_BOYU, sizeof(kenar_t));

    intr_handle_t kesme;
    ESP_ERROR_CHECK(esp_intr_alloc(ETS_GPIO_INTR_SOURCE,
                                   ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1,
                                   gpio_isr, NULL, &kesme));

    REG(GPIO_STATUS_W1TC_REG) = 1u << BUTON_PIN;
    gpio_kesme_ayarla(BUTON_PIN, KESME_HER_KENAR);

    ESP_LOGI(TAG, "IO_MUX_GPIO0 = 0x%08" PRIx32 ", GPIO_PIN0 = 0x%08" PRIx32,
             REG(IO_MUX_GPIO_REG(BUTON_PIN)), REG(GPIO_PIN_REG(BUTON_PIN)));
    ESP_LOGI(TAG, "Kesme: CPU%d, hat %d, matris eşlemesi = %" PRIu32,
             esp_intr_get_cpu(kesme), esp_intr_get_intno(kesme),
             REG(INTR_MAP_GPIO_REG) & 0x1Fu);

    xTaskCreate(buton_gorevi, "buton", 4096, NULL, 5, NULL);
}
