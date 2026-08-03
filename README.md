# TelpostTemperature

ESP32 tabanlı, BMP280 sensörü ile sıcaklık ve basınç verisi okuyup SSD1306 OLED ekranda gösteren gömülü sistem projesi. PlatformIO ile geliştirilmektedir.

## Donanım

| Bileşen | Açıklama |
|---|---|
| Geliştirme kartı | ESP32 (`esp32dev`) |
| Sıcaklık/Basınç sensörü | BMP280 (I2C) |
| Ekran | SSD1306 OLED, 128x64 px (I2C) |

### I2C Bağlantıları

ESP32'nin varsayılan donanımsal I2C pinleri kullanılır:

| Sinyal | ESP32 Pin |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3V |
| GND | GND |

Her iki modül (BMP280 ve SSD1306) aynı I2C hattına paralel bağlanır.

### I2C Adresleri

| Modül | Adres | Not |
|---|---|---|
| BMP280 | `0x76` | Piyasadaki çoğu ucuz modül bu adresi kullanır (orijinal Adafruit kartlar `0x77` kullanabilir) |
| SSD1306 | `0x3C` | OLED ekranlarda standart adres |

## Yazılım Gereksinimleri

- [PlatformIO](https://platformio.org/) (VS Code eklentisi veya CLI)
- Framework: Arduino

## Kütüphane Bağımlılıkları

`platformio.ini` içinde tanımlı ve derleme sırasında PlatformIO tarafından otomatik indirilir:

- `adafruit/Adafruit BMP280 Library`
- `adafruit/Adafruit SSD1306`
- `adafruit/Adafruit GFX Library`

## Derleme ve Yükleme

```bash
# Bağımlılıkları indir ve derle
pio run

# ESP32'ye yükle
pio run --target upload

# Seri port çıktısını izle (115200 baud)
pio device monitor
```

## Proje Yapısı

```
├── src/            # Uygulama kaynak kodu (main.cpp)
├── include/         # Proje geneli header dosyaları
├── lib/             # Projeye özel kütüphaneler
├── test/            # PlatformIO test dosyaları
└── platformio.ini   # Ortam, board ve bağımlılık tanımları
```

## Çalışma Mantığı

1. Başlangıçta OLED ekran ve BMP280 sensörü I2C üzerinden başlatılır. Başlatma başarısız olursa hata mesajı seri porta yazılır ve cihaz o adımda durur.
2. Ana döngüde (`loop`) BMP280'den sıcaklık (°C) ve basınç (hPa) verisi okunur.
3. Okunan değerler hem seri port üzerinden hem de OLED ekranda gösterilir.
4. Ölçümler arasında 2 saniye bekleme uygulanır.
