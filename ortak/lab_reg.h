/*
 * İleri Gömülü Sistemler - Laboratuvar 2 ortak başlık dosyası (ESP32-S3)
 * GPIO ve IO_MUX registerları için adresler, bit alanları ve
 * yardımcı fonksiyonlar. Yalnızca GPIO0-31 için geçerlidir.
 */
#pragma once
#include <stdint.h>

#define REG(addr)                (*(volatile uint32_t *)(addr))

/* ---- Taban adresler ----------------------------------------------- */
#define DR_GPIO_BASE             0x60004000u
#define DR_IO_MUX_BASE           0x60009000u

/* ---- GPIO registerları -------------------------------------------- */
#define GPIO_OUT_REG             (DR_GPIO_BASE + 0x0004u)
#define GPIO_OUT_W1TS_REG        (DR_GPIO_BASE + 0x0008u)
#define GPIO_OUT_W1TC_REG        (DR_GPIO_BASE + 0x000Cu)
#define GPIO_ENABLE_REG          (DR_GPIO_BASE + 0x0020u)
#define GPIO_ENABLE_W1TS_REG     (DR_GPIO_BASE + 0x0024u)
#define GPIO_ENABLE_W1TC_REG     (DR_GPIO_BASE + 0x0028u)
#define GPIO_IN_REG              (DR_GPIO_BASE + 0x003Cu)
#define GPIO_STATUS_REG          (DR_GPIO_BASE + 0x0044u)
#define GPIO_STATUS_W1TC_REG     (DR_GPIO_BASE + 0x004Cu)
#define GPIO_PCPU_INT_REG        (DR_GPIO_BASE + 0x005Cu)
#define GPIO_PIN_REG(n)          (DR_GPIO_BASE + 0x0074u + 4u * (n))
#define GPIO_FUNC_OUT_SEL_REG(n) (DR_GPIO_BASE + 0x0554u + 4u * (n))

/* ---- IO_MUX registerı --------------------------------------------- */
#define IO_MUX_GPIO_REG(n)       (DR_IO_MUX_BASE + 0x0004u + 4u * (n))

/* ---- IO_MUX_GPIOn alanları ---------------------------------------- */
#define FUN_PD                   (1u << 7)
#define FUN_PU                   (1u << 8)
#define FUN_IE                   (1u << 9)
#define FUN_DRV_S                10
#define FUN_DRV_M                (0x3u << FUN_DRV_S)
#define MCU_SEL_S                12
#define MCU_SEL_M                (0x7u << MCU_SEL_S)
#define PIN_FUNC_GPIO            1u

/* ---- GPIO_PINn alanları ------------------------------------------- */
#define PIN_PAD_DRIVER           (1u << 2)
#define PIN_INT_TYPE_S           7
#define PIN_INT_TYPE_M           (0x7u << PIN_INT_TYPE_S)
#define PIN_INT_ENA_S            13
#define PIN_INT_ENA_M            (0x1Fu << PIN_INT_ENA_S)
#define PIN_INT_ENA_CPU          (1u << PIN_INT_ENA_S)

/* GPIO_PINn.INT_TYPE değerleri */
#define KESME_YOK                0u
#define KESME_YUKSELEN           1u
#define KESME_DUSEN              2u
#define KESME_HER_KENAR          3u
#define KESME_DUSUK_DUZEY        4u
#define KESME_YUKSEK_DUZEY       5u

/* ---- GPIO_FUNCn_OUT_SEL_CFG alanları ------------------------------ */
#define SIG_GPIO_OUT_IDX         256u
#define FUNC_OEN_SEL             (1u << 10)

/* Pini itme-çekme (push-pull) çıkış yapar; Lab 1'deki beş adım. */
static inline void gpio_cikis_ayarla(uint32_t pin)
{
    uint32_t mask = 1u << pin;
    REG(GPIO_ENABLE_W1TC_REG) = mask;
    REG(GPIO_OUT_W1TC_REG)    = mask;

    uint32_t mux = REG(IO_MUX_GPIO_REG(pin));
    mux &= ~(MCU_SEL_M | FUN_DRV_M | FUN_IE | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | (2u << FUN_DRV_S) | FUN_IE;
    REG(IO_MUX_GPIO_REG(pin)) = mux;

    uint32_t r = REG(GPIO_PIN_REG(pin));
    r &= ~(PIN_PAD_DRIVER | PIN_INT_TYPE_M | PIN_INT_ENA_M);
    REG(GPIO_PIN_REG(pin)) = r;

    REG(GPIO_FUNC_OUT_SEL_REG(pin)) = SIG_GPIO_OUT_IDX | FUNC_OEN_SEL;
    REG(GPIO_ENABLE_W1TS_REG) = mask;
}

/* Pini giriş yapar; pullup sıfırdan farklıysa dahili pull-up açılır. */
static inline void gpio_giris_ayarla(uint32_t pin, uint32_t pullup)
{
    REG(GPIO_ENABLE_W1TC_REG) = 1u << pin;
    REG(GPIO_FUNC_OUT_SEL_REG(pin)) = SIG_GPIO_OUT_IDX | FUNC_OEN_SEL;

    uint32_t mux = REG(IO_MUX_GPIO_REG(pin));
    mux &= ~(MCU_SEL_M | FUN_IE | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;
    if (pullup) {
        mux |= FUN_PU;
    }
    REG(IO_MUX_GPIO_REG(pin)) = mux;

    uint32_t r = REG(GPIO_PIN_REG(pin));
    r &= ~(PIN_PAD_DRIVER | PIN_INT_TYPE_M | PIN_INT_ENA_M);
    REG(GPIO_PIN_REG(pin)) = r;
}

/* Kesme türünü ayarlar; tur == KESME_YOK ise kesmeyi kapatır. */
static inline void gpio_kesme_ayarla(uint32_t pin, uint32_t tur)
{
    uint32_t r = REG(GPIO_PIN_REG(pin));
    r &= ~(PIN_INT_TYPE_M | PIN_INT_ENA_M);
    if (tur != KESME_YOK) {
        r |= (tur << PIN_INT_TYPE_S) | PIN_INT_ENA_CPU;
    }
    REG(GPIO_PIN_REG(pin)) = r;
}

static inline void gpio_yaz(uint32_t pin, uint32_t duzey)
{
    if (duzey) {
        REG(GPIO_OUT_W1TS_REG) = 1u << pin;
    } else {
        REG(GPIO_OUT_W1TC_REG) = 1u << pin;
    }
}

static inline uint32_t gpio_oku(uint32_t pin)
{
    return (REG(GPIO_IN_REG) >> pin) & 1u;
}
