# Laboratuvar 2 – GPIO, Kesme ve Zamanlayıcılar

İleri Gömülü Sistemler dersi (Dr. Murat Sürücü) için hazırlanan Laboratuvar 2 çalışması.

- **Hazırlayan:** Gamze Şirin
- **Öğrenci numarası:** 2510140022
- **Donanım:** ESP32-S3-N16R8 geliştirme kartı (CPU 240 MHz)
- **Yazılım:** ESP-IDF v6.1, Windows

**Rapor:** [lab2/Lab2_Rapor.pdf](lab2/Lab2_Rapor.pdf)

## Depo içeriği

```
lab2/
├── ortak/              lab_reg.h – register adresleri ve yardımcı fonksiyonlar
├── lab2_gpio_hiz/      Görev 1–2: register ile ayar, gpio_config() ve erişim hızı
├── lab2_buton/         Görev 3: BOOT düğmesi kesmesi ve sıçrama ölçümü
├── lab2_zamanlayici/   Görev 4: GPTimer ve esp_timer karşılaştırması
├── hist_ciz.py         Görev 4 kayıtlarından histogram çizer
├── olcum/              Monitör kayıtları, objdump çıktısı, histogram
└── Lab2_Rapor.pdf
```

## Ölçüm kayıtları

| Görev | Dosya | Açıklama |
|---|---|---|
| 1–2 | `olcum/g1_output.txt` | Açılış register değerleri (`GPIO_MODE_OUTPUT`) ve hız ölçümleri |
| 1 | `olcum/g1_input_output.txt` | Aynı ölçüm, `GPIO_MODE_INPUT_OUTPUT` ile |
| 2 | `olcum/g2_disasm.txt` | Üç erişim fonksiyonu ve `gpio_set_level` için objdump |
| 3 | `olcum/g3_s20.txt` | `SESSIZLIK_MS = 20`: BOOT ile 20 basış, kabloyla 10 temas |
| 3 | `olcum/g3_s1.txt`, `olcum/g3_s50.txt` | `SESSIZLIK_MS = 1` ve `50` denemeleri |
| 4 | `olcum/kayit_wifi_kapali.txt`, `olcum/kayit_wifi_acik.txt` | Zamanlayıcı kayıtları |
| 4 | `olcum/histogram.png` | `python hist_ciz.py kayit_wifi_kapali.txt kayit_wifi_acik.txt` |

Kayıtlar ESP-IDF monitörünün dosyaya kaydetme özelliğiyle (Ctrl+T, Ctrl+L) alındı. Lojik analizör kullanılmadı. Süreler işlemcinin çevrim sayacı ve `esp_timer` ile ölçüldü.

## Derleme ve yükleme

```
cd lab2/lab2_buton
idf.py set-target esp32s3
idf.py -p COMx flash monitor
```

Diğer projeler için klasör adını değiştirmek yeterli.

---

Not: Yapay zekâdan yararlanılmıştır.
