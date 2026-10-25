#!/usr/bin/env python3
"""
İleri Gömülü Sistemler - Laboratuvar 2, Görev 4
lab2_zamanlayici monitör kayıtlarından periyot histogramlarını çizer ve
rapor tablosu için özet değerleri yazar.

Kullanım:
    python3 hist_ciz.py kayit_wifi_kapali.txt kayit_wifi_acik.txt
    python3 hist_ciz.py kayit_wifi_kapali.txt kayit_wifi_acik.txt -o histogram.png

Okunan satırlar (program her 2 saniyede bir basar):
    I (...) lab2: GPTimer   n= 4000 ort= 500.000 ss=  0.050 min= 499.8 max= 500.2 (us)
    hist,GPTimer,500,3998
    hist,esp_timer,<400,1
    hist,esp_timer,>600,2
"""
import argparse
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

KAYNAKLAR = ("GPTimer", "esp_timer")
HIST_ALT, HIST_UST = 400, 600          # main.c: HIST_ALT_US, HIST_ALT_US + HIST_KUTU - 1
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
HIST = re.compile(r"hist,(GPTimer|esp_timer),([<>]?)(\d+),(\d+)")
OZET = re.compile(
    r"(GPTimer|esp_timer)\s+n=\s*(\d+)\s+ort=\s*([-\d.]+)\s+ss=\s*([-\d.]+)"
    r"\s+min=\s*([-\d.]+)\s+max=\s*([-\d.]+)")

# Kategorik renkler (sabit sıra): 1. kayıt mavi, 2. kayıt turuncu
RENKLER = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]


def etiket(yol: Path) -> str:
    ad = yol.stem.lower()
    if "kapali" in ad:
        return "Wi-Fi kapalı"
    if "acik" in ad:
        return "Wi-Fi açık"
    return yol.stem


def oku(yol: Path):
    hist = {k: defaultdict(int) for k in KAYNAKLAR}
    alt = {k: 0 for k in KAYNAKLAR}
    ust = {k: 0 for k in KAYNAKLAR}
    ozet = {k: [] for k in KAYNAKLAR}
    with open(yol, encoding="utf-8", errors="replace") as f:
        for satir in f:
            satir = ANSI.sub("", satir)
            m = HIST.search(satir)
            if m:
                kaynak, isaret, deger, sayi = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
                if isaret == "<":
                    alt[kaynak] += sayi
                elif isaret == ">":
                    ust[kaynak] += sayi
                else:
                    hist[kaynak][deger] += sayi
                continue
            m = OZET.search(satir)
            if m:
                kaynak = m.group(1)
                n, ort, ss, mn, mx = (int(m.group(2)), *map(float, m.group(3, 4, 5, 6)))
                ozet[kaynak].append((n, ort, ss, mn, mx))
    return hist, alt, ust, ozet


def birlestir(pencereler):
    """2 saniyelik pencerelerin özetlerini tek ortalama ve standart sapmada birleştirir."""
    N = sum(p[0] for p in pencereler)
    if N == 0:
        return None
    ort = sum(p[0] * p[1] for p in pencereler) / N
    ikinci_moment = sum(p[0] * (p[2] ** 2 + p[1] ** 2) for p in pencereler) / N
    ss = math.sqrt(max(ikinci_moment - ort ** 2, 0.0))
    return {
        "pencere": len(pencereler), "n": N, "ort": ort, "ss": ss,
        "min": min(p[3] for p in pencereler), "max": max(p[4] for p in pencereler),
    }


def tolerans_orani(h, alt, ust, merkez=500, tol=5):
    """Kutu değeri floor(us) olduğundan [merkez-tol, merkez+tol) aralığına düşen örneklerin oranı."""
    toplam = sum(h.values()) + alt + ust
    if toplam == 0:
        return float("nan")
    icinde = sum(s for d, s in h.items() if merkez - tol <= d < merkez + tol)
    return 100.0 * icinde / toplam


def main():
    ap = argparse.ArgumentParser(description="Lab 2 Görev 4 histogram çizimi")
    ap.add_argument("kayitlar", nargs="+", type=Path, help="monitör kayıt dosyaları")
    ap.add_argument("-o", "--cikti", type=Path, default=Path("histogram.png"))
    ap.add_argument("--dogrusal", action="store_true", help="y eksenini logaritmik değil doğrusal çiz")
    args = ap.parse_args()

    veriler = []
    for yol in args.kayitlar:
        if not yol.exists():
            sys.exit(f"Dosya bulunamadı: {yol}")
        hist, alt, ust, ozet = oku(yol)
        if not any(hist[k] or alt[k] or ust[k] for k in KAYNAKLAR):
            sys.exit(f"{yol}: 'hist,' satırı bulunamadı. Kaydın en az bir 2 saniyelik rapor içerdiğini kontrol edin.")
        veriler.append((etiket(yol), hist, alt, ust, ozet))

    # ---- Rapor tablosu için özet ----
    print("Rapor tablosu (konsol özetlerinden birleştirilmiş, birim us)")
    print(f"{'Kayıt':<14}{'Kaynak':<11}{'pencere':>8}{'n':>8}{'ort':>10}{'ss':>9}"
          f"{'min':>10}{'max':>10}{'±5us %':>9}{'<400':>6}{'>600':>6}")
    for ad, hist, alt, ust, ozet in veriler:
        for k in KAYNAKLAR:
            o = birlestir(ozet[k])
            oran = tolerans_orani(hist[k], alt[k], ust[k])
            if o:
                print(f"{ad:<14}{k:<11}{o['pencere']:>8}{o['n']:>8}{o['ort']:>10.3f}{o['ss']:>9.3f}"
                      f"{o['min']:>10.3f}{o['max']:>10.3f}{oran:>9.2f}{alt[k]:>6}{ust[k]:>6}")
            else:
                print(f"{ad:<14}{k:<11}{'özet satırı yok':>34}{oran:>27.2f}{alt[k]:>6}{ust[k]:>6}")
    print("\n'±5us %': 495-504 us kutularına düşen örneklerin yüzdesi (rapor sorusu 7 için).")

    # ---- Çizim ----
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
    satir_sayisi = len(veriler)
    fig, eksenler = plt.subplots(satir_sayisi, len(KAYNAKLAR), figsize=(10, 2.8 * satir_sayisi + 0.8),
                                 sharex=True, sharey=True, squeeze=False)
    tum_kutular = [d for _, h, *_ in veriler for k in KAYNAKLAR for d in h[k]]
    x_alt = max(HIST_ALT, min(tum_kutular) - 3) if tum_kutular else HIST_ALT
    x_ust = min(HIST_UST, max(tum_kutular) + 4) if tum_kutular else HIST_UST
    kenarlar = list(range(x_alt, x_ust + 1))
    taban = 0.7 if not args.dogrusal else 0

    for r, (ad, hist, alt, ust, ozet) in enumerate(veriler):
        for c, kaynak in enumerate(KAYNAKLAR):
            ax = eksenler[r][c]
            h = hist[kaynak]
            degerler = [h.get(x, 0) for x in kenarlar[:-1]]
            renk = RENKLER[c]
            ax.axvspan(495, 505, color="#888888", alpha=0.12, linewidth=0, zorder=0)
            ax.stairs([max(v, taban) for v in degerler], kenarlar, baseline=taban,
                      fill=True, color=renk, alpha=0.35, zorder=2)
            ax.stairs([max(v, taban) for v in degerler], kenarlar, baseline=taban,
                      color=renk, linewidth=1.5, zorder=3)
            o = birlestir(ozet[kaynak])
            satirlar = []
            if o:
                satirlar.append(f"ss = {o['ss']:.3f} us")
                satirlar.append(f"min / max = {o['min']:.1f} / {o['max']:.1f} us")
            disari = alt[kaynak] + ust[kaynak]
            satirlar.append(f"400-600 us dışı: {disari}")
            ax.text(0.98, 0.95, "\n".join(satirlar), transform=ax.transAxes, ha="right", va="top",
                    fontsize=9, color="#333333")
            if r == 0:
                ax.set_title(kaynak, loc="left", fontsize=11, fontweight="bold")
            if c == 0:
                ax.set_ylabel(f"{ad}\nörnek sayısı")
            if not args.dogrusal:
                ax.set_yscale("log")
            ax.grid(axis="y", color="#dddddd", linewidth=0.6, zorder=1)
    for ax in eksenler[-1]:
        ax.set_xlabel("ardışık çağrılar arası süre (us); gri bant 500 ± 5 us")
    fig.suptitle("Lab 2 Görev 4: 500 us periyotlu zamanlayıcı çağrılarının dağılımı",
                 x=0.01, ha="left", fontsize=12)
    fig.tight_layout()
    fig.savefig(args.cikti, dpi=150)
    print(f"\nGrafik kaydedildi: {args.cikti}")


if __name__ == "__main__":
    main()
