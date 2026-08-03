# TelpostTemperature

ESP32 tabanlı, BMP280 sensörü ile sıcaklık ve basınç verisi okuyup ST7789 TFT ekranda gösteren gömülü sistem projesi. PlatformIO ile geliştirilmektedir.

## Donanım

| Bileşen | Açıklama |
|---|---|
| Geliştirme kartı | ESP32 DevKit V1 (`esp32dev`) |
| Sıcaklık/Basınç sensörü | BMP280 (I2C) |
| Ekran | 2.25" IPS bar LCD, ST7789P3 sürücü, 76x284 px (SPI) |

Ekran modülü etiketi: `FP-225TSFP09A`, PCB üzerinde `Driver IC: ST7789P3`, `Resolution: 76x284`.

### Bağlantı Şeması

**Ekran (SPI)** — modül header'ı: `GND VCC SCL SDA RES DC CS BL`

| Ekran pini | ESP32 pini | İşlev |
|---|---|---|
| GND | GND | Ortak toprak |
| VCC | 3.3V | Besleme (5V bağlamayın) |
| SCL | D18 | SPI clock (SCK) |
| SDA | D23 | SPI veri (MOSI) |
| RES | D4 | Reset |
| DC | D2 | Data/Command seçici |
| CS | D5 | Chip select |
| BL | 3.3V | Arka ışık |

**BMP280 (I2C)**

| BMP280 pini | ESP32 pini |
|---|---|
| VIN | 3.3V |
| GND | GND |
| SCL | D22 |
| SDA | D21 |

I2C adresi: `0x76` (piyasadaki çoğu ucuz modül bu adresi kullanır; orijinal Adafruit kartlar `0x77` kullanabilir).

## Yazılım Gereksinimleri

- [PlatformIO](https://platformio.org/) (VS Code eklentisi veya CLI)
- Framework: Arduino

## Kütüphane Bağımlılıkları

`platformio.ini` içinde tanımlı ve derleme sırasında PlatformIO tarafından otomatik indirilir:

- `adafruit/Adafruit BMP280 Library`
- `adafruit/Adafruit GFX Library`
- `adafruit/Adafruit ST7735 and ST7789 Library`

## Derleme ve Yükleme

```bash
# Bağımlılıkları indir ve derle
pio run

# ESP32'ye yükle
pio run --target upload

# Seri port çıktısını izle (115200 baud)
pio device monitor
```

> Not: Seri monitör açıkken yükleme yapılamaz (port meşgul hatası). Yüklemeden önce monitörü kapatın.

## Proje Yapısı

```
├── src/             # Uygulama kaynak kodu (main.cpp)
├── include/         # Proje geneli header dosyaları
├── lib/             # Projeye özel kütüphaneler
├── test/            # PlatformIO test dosyaları
└── platformio.ini   # Ortam, board ve bağımlılık tanımları
```

## Çalışma Mantığı

1. Başlangıçta ST7789 ekran SPI üzerinden, BMP280 sensörü I2C üzerinden başlatılır. Sensör bulunamazsa seri porta ve ekrana hata mesajı yazılır.
2. Ekran yatay (landscape) modda kullanılır: `setRotation(1)` ile 284x76 çözünürlük.
3. Ana döngüde `millis()` tabanlı 2 saniyelik aralıklarla BMP280'den sıcaklık (°C) ve basınç (hPa) okunur.
4. Değerler hem seri porta hem TFT ekrana yazılır. Ekranda yalnızca değişen değerler yeniden çizilir (titreme önlenir).

## Geliştirme Notları

Bu panelle çalışırken karşılaşılan ve çözülen noktalar:

- **SPI hızı**: Adafruit kütüphanesinin varsayılanı 32 MHz'dir. Breadboard ve jumper kablolarla bu hız sinyal bozulmasına yol açar; proje 4 MHz kullanır (`TFT_SPI_HIZI`).
- **Init sırası**: Kütüphanenin `init()` fonksiyonu panel init komutlarını 32 MHz'de gönderir ve `setSPISpeed()` yalnızca init'ten sonra etki eder. Bu nedenle hız düşürüldükten sonra çekirdek ST7789 init komutları `panelInitEldenGonder()` ile elle yeniden gönderilir.
- **GRAM offset**: 76x284 standart dışı bir çözünürlüktür. Kütüphane bu boyut için offsetleri (`colstart=82`, `rowstart=18`) otomatik hesaplar, manuel ayar gerekmez.
- **Ekran yönü**: Görüntü baş aşağı çıkarsa `setRotation(1)` yerine `setRotation(3)` kullanılabilir.
- **GPIO2 (DC)**: ESP32'de strapping pindir. Yükleme sorunu yaşanırsa DC pini D15'e taşınabilir.
