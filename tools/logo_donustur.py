#!/usr/bin/env python3
"""
Logo gorselini ST7789 ekranda kullanilabilecek RGB565 C dizisine cevirir.

ESP32'de dosya sistemi kullanmadigimiz icin gorsel, derleme zamaninda
flash bellege gomulur. Uretilen header dosyasi Adafruit_GFX'in
drawRGBBitmap() fonksiyonuyla dogrudan kullanilabilir.

Kullanim:
    python tools/logo_donustur.py <girdi_gorsel> <cikti_header> <ad=olcu> [ad=olcu ...]

Olcu bicimleri:
    ad=34         -> 34x34 kutusuna sigdirir
    ad=284x76     -> 284x76 kutusuna sigdirir

Her iki bicimde de en/boy orani korunur; gorsel verilen kutuya sigacak
sekilde olceklenir, ezilmez.

Ornekler:
    python tools/logo_donustur.py bmwacilis.jpg include/logo_acilis.h logoAcilis=284x76
    python tools/logo_donustur.py bmwlogo.png include/logo_merkez.h logoMerkez=34

Notlar:
  - Gorselin cevresindeki bos alan otomatik kirpilir: saydamlik varsa alpha
    kanalindan, yoksa siyah zeminden. Boylece logo hedef kutuyu tam kullanir.
  - Saydam pikseller icin ayrica maske dizisi uretilir; bu sayede logo
    arka planin uzerine kare blok birakmadan cizilir.
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit(
        "HATA: Pillow kurulu degil.\n"
        "Kurmak icin: python -m pip install Pillow"
    )

# Siyah kabul edilen parlaklik esigi (JPG sikistirma gurultusune tolerans)
SIYAH_ESIGI = 24


def rgb565(r: int, g: int, b: int) -> int:
    """24 bit RGB degerini 16 bit RGB565 formatina sikistirir."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def icerik_kirp(kaynak: Image.Image) -> Image.Image:
    """Logonun cevresindeki bos alani kirpar.

    Logo gorselleri genelde buyuk bir tuval icinde ortalanmis gelir. Bu bosluk
    kirpilmazsa olcekleme sirasinda logo hedef kutuda kucuk kalir. Saydamlik
    varsa alpha kanali, yoksa siyah zemin sinir olarak kullanilir.
    """
    gorsel = kaynak.convert("RGBA")
    alpha = gorsel.getchannel("A")

    if alpha.getextrema()[0] < 255:
        sinirlar = alpha.getbbox()
    else:
        parlaklik = gorsel.convert("L")
        sinirlar = parlaklik.point(lambda p: 255 if p > SIYAH_ESIGI else 0).getbbox()

    return gorsel.crop(sinirlar) if sinirlar else gorsel


def kutuya_sigdir(gorsel: Image.Image, hedef_g: int, hedef_y: int) -> Image.Image:
    """En/boy oranini bozmadan verilen kutuya sigacak sekilde olcekler."""
    g, y = gorsel.size
    oran = min(hedef_g / g, hedef_y / y)
    return gorsel.resize(
        (max(1, round(g * oran)), max(1, round(y * oran))),
        Image.LANCZOS,
    )


def maske_uret(alpha: Image.Image, genislik: int, yukseklik: int) -> list:
    """Alpha kanalindan Adafruit_GFX maskesi uretir.

    Bicim: piksel basina 1 bit, MSB once, her satir tam bayta tamamlanir.
    Bit 1 ise piksel cizilir, 0 ise atlanir (arka plan gorunur kalir).
    """
    satir_bayt = (genislik + 7) // 8
    baytlar = []

    for y in range(yukseklik):
        for bayt_no in range(satir_bayt):
            bayt = 0
            for bit in range(8):
                x = bayt_no * 8 + bit
                bayt <<= 1
                if x < genislik and alpha.getpixel((x, y)) >= 128:
                    bayt |= 1
            baytlar.append(bayt)

    return baytlar


def c_dizisi(degerler: list, bicim: str, sutun: int) -> str:
    """Sayi listesini girintili C dizisi govdesine cevirir."""
    satirlar = []
    for i in range(0, len(degerler), sutun):
        parca = ", ".join(format(d, bicim) for d in degerler[i:i + sutun])
        satirlar.append("    " + parca + ",")
    return "\n".join(satirlar)


def diziye_cevir(kaynak: Image.Image, ad: str, hedef_g: int, hedef_y: int):
    """Gorseli olcekleyip C dizisi metnine cevirir; (metin, bayt) dondurur."""
    gorsel = kutuya_sigdir(icerik_kirp(kaynak), hedef_g, hedef_y)
    genislik, yukseklik = gorsel.size

    # Maske icin alpha kanalini sakla, sonra rengi siyah zemine yedir.
    alpha = gorsel.getchannel("A")
    zemin = Image.new("RGBA", gorsel.size, (0, 0, 0, 255))
    renkli = Image.alpha_composite(zemin, gorsel).convert("RGB")

    # tobytes() ham RGB dizisi verir: piksel basina 3 bayt
    ham = renkli.tobytes()
    degerler = [
        rgb565(ham[i], ham[i + 1], ham[i + 2])
        for i in range(0, len(ham), 3)
    ]

    maske = maske_uret(alpha, genislik, yukseklik)
    toplam = len(degerler) * 2 + len(maske)

    metin = (
        f"// {ad}: {genislik}x{yukseklik} piksel\n"
        f"//   renk verisi {len(degerler) * 2} bayt, maske {len(maske)} bayt\n"
        f"#define {ad.upper()}_GENISLIK {genislik}\n"
        f"#define {ad.upper()}_YUKSEKLIK {yukseklik}\n"
        f"const uint16_t {ad}[{len(degerler)}] PROGMEM = {{\n"
        + c_dizisi(degerler, "#06X", 12)
        + "\n};\n\n"
        f"// Saydam pikselleri atlayan maske; logo koseleri arka plani kapatmaz.\n"
        f"const uint8_t {ad}Maske[{len(maske)}] PROGMEM = {{\n"
        + c_dizisi(maske, "#04X", 16)
        + "\n};\n"
    )

    return metin, toplam


def olcu_coz(metin: str) -> tuple:
    """'34' veya '284x76' bicimindeki olcuyu (genislik, yukseklik) cevirir."""
    if "x" in metin.lower():
        g, _, y = metin.lower().partition("x")
        if not (g.isdigit() and y.isdigit()):
            return None
        return int(g), int(y)
    if metin.isdigit():
        return int(metin), int(metin)
    return None


def main() -> None:
    if len(sys.argv) < 4:
        sys.exit(__doc__)

    girdi = Path(sys.argv[1])
    cikti = Path(sys.argv[2])
    istekler = sys.argv[3:]

    if not girdi.is_file():
        sys.exit(f"HATA: Gorsel bulunamadi: {girdi}")

    kaynak = Image.open(girdi)

    bloklar = []
    toplam_bayt = 0
    for istek in istekler:
        if "=" not in istek:
            sys.exit(f"HATA: '{istek}' gecersiz. Beklenen bicim: ad=olcu")
        ad, _, olcu_metni = istek.partition("=")
        olcu = olcu_coz(olcu_metni)
        if olcu is None:
            sys.exit(f"HATA: '{istek}' icindeki olcu 34 veya 284x76 biciminde olmali.")

        metin, bayt = diziye_cevir(kaynak, ad, olcu[0], olcu[1])
        bloklar.append(metin)
        toplam_bayt += bayt

    icerik = (
        "// Bu dosya tools/logo_donustur.py tarafindan uretilmistir.\n"
        f"// Kaynak gorsel: {girdi.name}\n"
        "// Elle duzenlemeyin; degisiklik icin script'i yeniden calistirin.\n\n"
        "#pragma once\n\n"
        "#include <Arduino.h>\n\n"
        + "\n".join(bloklar)
    )

    cikti.parent.mkdir(parents=True, exist_ok=True)
    cikti.write_text(icerik, encoding="utf-8")

    print(f"Olusturuldu: {cikti}")
    print(f"Toplam flash kullanimi: {toplam_bayt} bayt ({toplam_bayt / 1024:.1f} KB)")


if __name__ == "__main__":
    main()
