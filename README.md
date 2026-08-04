# TelpostTemperature

ESP32 tabanlı, ortam sıcaklığı/basıncı ile su ve yağ sıcaklıklarını ölçüp ST7789 TFT ekranda gösteren gömülü sistem projesi. PlatformIO ile geliştirilmektedir.

## Donanım

| Bileşen | Açıklama |
|---|---|
| Geliştirme kartı | ESP32 DevKit V1 (`esp32dev`) |
| Ortam sıcaklık/basınç sensörü | BMP280 (I2C) |
| Su sıcaklık sensörü | DS18B20 (1-Wire) |
| Yağ sıcaklık sensörü | DS18B20 (1-Wire) |
| Ekran | 2.25" IPS bar LCD, ST7789P3 sürücü, 76x284 px (SPI) |

Sistemde üç farklı haberleşme protokolü birlikte kullanılır: ekran SPI, BMP280 I2C, DS18B20'ler 1-Wire. Bu protokoller birbirine dönüştürülemez; her biri kendi hattını kullanır.

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

**DS18B20 x2 (1-Wire)**

Her iki sensör de aynı veri hattını paylaşır; ek pin gerekmez.

| DS18B20 pini | ESP32 pini |
|---|---|
| VDD | 3.3V |
| GND | GND |
| DQ (veri) | D27 |

> **Pull-up direnç zorunludur**: DQ hattı ile 3.3V arasına **4.7kΩ** direnç bağlanmalıdır. Tüm hat için tek direnç yeterlidir; eksikse hiçbir sensör bulunamaz.

Sensörler 64-bit ROM adresleriyle ayırt edilir (`SU_SENSOR_ADRESI`, `YAG_SENSOR_ADRESI`). Index tabanlı erişim (`getTempCByIndex`) tercih edilmemiştir: bus'taki cihaz sırası değişirse su ve yağ okumaları karışabilir, bu da yanlış veri anlamına gelir.

## Yazılım Gereksinimleri

- [PlatformIO](https://platformio.org/) (VS Code eklentisi veya CLI)
- Framework: Arduino

## Kütüphane Bağımlılıkları

`platformio.ini` içinde tanımlı ve derleme sırasında PlatformIO tarafından otomatik indirilir:

- `adafruit/Adafruit BMP280 Library`
- `adafruit/Adafruit GFX Library`
- `adafruit/Adafruit ST7735 and ST7789 Library`
- `paulstoffregen/OneWire`
- `milesburton/DallasTemperature`

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

1. Başlangıçta ST7789 ekran SPI üzerinden, BMP280 I2C üzerinden, DS18B20'ler 1-Wire üzerinden başlatılır. BMP280 bulunamazsa seri porta ve ekrana hata yazılıp çalışma durdurulur; DS18B20'ler bulunamazsa yalnızca uyarı verilir, sistem çalışmaya devam eder.
2. Ekran yatay (landscape) modda kullanılır: `setRotation(1)` ile 284x76 çözünürlük.
3. Ana döngüde `millis()` tabanlı 2 saniyelik aralıklarla dört değer okunur: ortam sıcaklığı (°C), basınç (hPa), su sıcaklığı (°C), yağ sıcaklığı (°C).
4. Değerler hem seri porta hem TFT ekrana yazılır. Ekranda yalnızca değişen değerler yeniden çizilir (titreme önlenir).

### Ekran Yerleşimi

Ekran 2x2 ızgaraya bölünmüştür, dört değer aynı anda görünür:

```
┌─────────────────────────────────────┐
│ TELPOST :)                          │
├──────────────────┬──────────────────┤
│ SICAKLIK C       │ BASINC hPa       │
│ 25.7   (sarı)    │ 1006.4  (yeşil)  │
├──────────────────┼──────────────────┤
│ SU C             │ YAG C            │
│ 24.3   (cyan)    │ 38.1  (turuncu)  │
└──────────────────┴──────────────────┘
```

Bağlantısı kopan bir DS18B20 için ilgili hücre güncellenmez, son geçerli değer ekranda kalır.

## Geliştirme Notları

Bu panelle çalışırken karşılaşılan ve çözülen noktalar:

- **SPI hızı**: Adafruit kütüphanesinin varsayılanı 32 MHz'dir. Breadboard ve jumper kablolarla bu hız sinyal bozulmasına yol açar; proje 4 MHz kullanır (`TFT_SPI_HIZI`).
- **Init sırası**: Kütüphanenin `init()` fonksiyonu panel init komutlarını 32 MHz'de gönderir ve `setSPISpeed()` yalnızca init'ten sonra etki eder. Bu nedenle hız düşürüldükten sonra çekirdek ST7789 init komutları `panelInitEldenGonder()` ile elle yeniden gönderilir.
- **GRAM offset**: 76x284 standart dışı bir çözünürlüktür. Kütüphane bu boyut için offsetleri (`colstart=82`, `rowstart=18`) otomatik hesaplar, manuel ayar gerekmez.
- **Ekran yönü**: Görüntü baş aşağı çıkarsa `setRotation(1)` yerine `setRotation(3)` kullanılabilir.
- **GPIO2 (DC)**: ESP32'de strapping pindir. Yükleme sorunu yaşanırsa DC pini D15'e taşınabilir.
- **DS18B20 adres keşfi**: Sensör değiştirilirse veya yeni sensör eklenirse, `src/main.cpp` içindeki "DS18B20 ADRES KEŞİF KODU" yorum bloğu geçici olarak aktif edilerek yeni ROM adresi seri porttan okunabilir. Adres alındıktan sonra blok tekrar yorum satırına alınmalıdır.
- **Protokol karışıklığı**: TFT modülünün header'ında `SCL`/`SDA` yazması yanıltıcıdır — bunlar I2C değil, SPI'nin `SCK`/`MOSI` hatlarıdır. ST7789P3 çipi I2C'yi donanımsal olarak desteklemez, bu nedenle BMP280 ile aynı hatta bağlanamaz.

## Pin Kullanım Özeti

| GPIO | Kullanım |
|---|---|
| D2 | TFT DC |
| D4 | TFT RES |
| D5 | TFT CS |
| D18 | TFT SCK |
| D21 | I2C SDA (BMP280) |
| D22 | I2C SCL (BMP280) |
| D23 | TFT MOSI |
| D27 | 1-Wire DQ (her iki DS18B20) |

Toplam 8 GPIO kullanılmaktadır. İleride pin kazanımı gerekirse: aynı protokoldeki yeni cihazlar mevcut hatları paylaşabilir (I2C için farklı adres, SPI için ek CS pini, 1-Wire için ek pin gerekmez).
