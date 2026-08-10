# TelpostTemperature

ESP32 tabanlı, ortam sıcaklığı/basıncı ile su ve yağ sıcaklıklarını ölçüp ST7789 TFT ekranda gösteren gömülü sistem projesi. Cihaz ayrıca kendi WiFi ağını kurar ve tarayıcıdan erişilen bir izleme/ayar paneli sunar. PlatformIO ile geliştirilmektedir.

## Donanım

| Bileşen | Açıklama |
|---|---|
| Geliştirme kartı | ESP32 DevKit V1 (`esp32dev`) |
| Ortam sıcaklık/basınç sensörü | BMP280 (I2C) |
| Su sıcaklık sensörü | DS18B20 (1-Wire) |
| Yağ sıcaklık sensörü | DS18B20 (1-Wire) |
| Ekran | 2.25" IPS bar LCD, ST7789P3 sürücü, 76x284 px (SPI) |
| Alarm | Buzzer (aktif veya pasif) |

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

**Buzzer**

| Buzzer pini | ESP32 pini |
|---|---|
| + (sinyal) | D32 |
| - (GND) | GND |

Hem aktif hem pasif buzzer desteklenir: LEDC ile 2700 Hz ton üretilir, aktif buzzer da bu sinyalle öter.

Tüm pinlerin özeti için [Pin Kullanım Özeti](#pin-kullanım-özeti) bölümüne bakın.

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
- `esp32async/ESPAsyncWebServer`
- `esp32async/AsyncTCP`

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
├── src/
│   ├── main.cpp           # Sensör okuma, ekran çizimi, buzzer
│   └── web_arayuz.cpp     # WiFi AP, HTTP/WebSocket sunucu, ayar kalıcılığı
├── include/
│   ├── web_arayuz.h       # Web modülünün main.cpp'ye açtığı arayüz
│   ├── web_sayfa.h        # Tarayıcıya sunulan tek sayfalık arayüz (HTML/CSS/JS)
│   ├── logo_acilis.h      # Üretilen açılış logosu bitmap'i
│   └── logo_merkez.h      # Üretilen merkez logosu bitmap'i
├── lib/                   # Projeye özel kütüphaneler
├── test/                  # PlatformIO test dosyaları
├── tools/                 # Yardımcı script'ler (logo dönüştürücü)
└── platformio.ini         # Ortam, board ve bağımlılık tanımları
```

Web arayüzü ayrı modülde tutulur; `main.cpp` sensör ve ekran işine odaklı kalır.

## Çalışma Mantığı

1. Ekran başlatılır, 2.5 saniye açılış logosu gösterilir.
2. Kalıcı ayarlar NVS'ten yüklenir, WiFi erişim noktası ve web sunucusu başlatılır. Bu adım ekran iskeletinden **önce** gelir: kayıtlı yerleşim, renk ve etiketler iskelet çizilirken kullanılır.
3. Sensörler başlatılır. **Hiçbir sensör eksikliği çalışmayı durdurmaz** — eksik değerler `--` olarak görünür, web arayüzü ve diğer sensörler çalışmaya devam eder. BMP280 bulunamazsa her ölçüm turunda yeniden bağlanma denenir; sonradan takılırsa kendiliğinden devreye girer.
4. Ekran yatay (landscape) modda kullanılır: `setRotation(1)` ile 284x76 çözünürlük.
5. Ana döngüde `millis()` tabanlı aralıklarla dört değer okunur: ortam sıcaklığı (°C), basınç (hPa), su sıcaklığı (°C), yağ sıcaklığı (°C). Aralık web arayüzünden değiştirilebilir.
6. Değerler seri porta, TFT ekrana ve WebSocket üzerinden web istemcilerine yazılır. Ekranda yalnızca değişen değerler yeniden çizilir (titreme önlenir).
7. Bir ölçüm alarm eşiğini aşarsa değerin yanında kırmızı ünlem çıkar ve buzzer kesintili öter (sessiz mod kapalıysa).

Ana döngü hiçbir noktada `delay()` ile bloklanmaz; ölçüm zamanlaması, buzzer bip deseni ve captive portal DNS işleme `millis()` ile yürütülür.

### Ekran Yerleşimi

Ekran dört çeyreğe bölünmüştür, logo ayırıcı çizgilerin kesiştiği merkeze oturur:

```
┌──────────────────────┬──────────────────────┐
│ TEMPERATURE C        │ WATER C              │
│ 25.7      (sarı)     │ 96.4 !    (cyan)     │
├──────────────────[logo]──────────────────────┤
│ PRESSURE hPa         │ OIL C                │
│ 1006.4    (yeşil)    │ 38.1    (turuncu)    │
└──────────────────────┴──────────────────────┘
```

Etiket metinleri, etiket renkleri, değer renkleri, çeyrek yerleşimi ve ayırıcı çizgi rengi web arayüzünden değiştirilebilir; hepsi NVS'e kalıcı kaydedilir. Etiketler ekrana sığması için 18 karakterle sınırlıdır.

Bu ayarlar **ölçüme** bağlıdır, çeyreğe değil: bir ölçüm başka çeyreğe taşındığında etiketi, renkleri ve alarm göstergesi onunla birlikte taşınır.

Okunamayan bir sensör için hücrede `--` gösterilir. Son geçerli değer bilinçli olarak ekranda bırakılmaz: kopmuş bir sensörün eski değerini göstermeye devam etmek yanıltıcı olur.

## Web Arayüzü

Cihaz kendi WiFi ağını kurar (AP modu). Araçta bağlanılacak bir router bulunmadığı için Station modu yerine bu seçilmiştir.

| | |
|---|---|
| Ağ adı | `Telpost-Monitor` (varsayılan) |
| Şifre | `telpost1234` (varsayılan) |
| Adres | `http://192.168.4.1` |

**Bağlanma:** Telefonun WiFi ayarlarından ağa katılın. "Bu ağın internet bağlantısı yok" uyarısı çıkarsa bağlı kalmayı seçin, aksi halde telefon mobil veriye döner.

**Captive portal:** Ağa katıldığınızda panel kendiliğinden açılır. Bilinen internet denetim adresleri (Android `/generate_204`, iOS `/hotspot-detect.html`, Windows `/ncsi.txt`, Firefox `/canonical.html`) ve tanınmayan tüm istekler panele yönlendirilir; DNS sunucusu da her alan adını cihaza çözer. Davranış cihaza göre değişir: iOS genelde otomatik açar, Android bazen yalnızca bildirim gösterir. Açılan pencere tam tarayıcı olmadığından (iOS'ta Captive Network Assistant), canlı akış takılırsa paneli normal tarayıcıda açın.

### Panel Bölümleri

- **Durum çubuğu** — bağlantı durumu ve sensör rozetleri.
- **Ölçüm kartları** — dört değer, TFT ekranla aynı isim ve renklerle. Eşik aşılınca kart kırmızı çerçeveye döner.
- **History** — son ölçümlerin grafiği. Zaman penceresi (30/60/120 örnek) seçilebilir.
- **Settings** — ölçüm aralığı, su/yağ alarm eşikleri, sessiz mod.
- **Display Layout** — hangi ölçümün hangi çeyrekte görüneceği ve ayırıcı çizgi rengi.
- **Labels & Colors** — her ölçüm için etiket metni, etiket rengi ve değer rengi.
- **Network** — WiFi ağ adı ve şifresi.

Tüm ayarlar NVS'e yazılır ve güç kesilse de korunur.

### Sensör Rozetleri

Durum çubuğundaki rozetler yeşil (veri geliyor), kırmızı (okunamıyor) veya gri (bağlantı yok) olur.

**Ekran ve buzzer rozetleri "bağlı mı" bilgisini göstermez.** Bu iki bileşen tespit edilemez: ekran 4 telli SPI ile bağlıdır ve modülde MISO pini yoktur (`GND VCC SCL SDA RES DC CS BL`), buzzer da salt çıkıştır. İkisine de yazılır ama geri bildirim alınamaz. Bu yüzden rozetler yapılandırma durumunu gösterir: ekran için "başlatıldı", buzzer için "etkin / sessiz modda".

### Alarm ve Sessiz Mod

Su veya yağ sıcaklığı eşiği aştığında:

- TFT ekranda değerin sağında kırmızı ünlem (`!`) çıkar
- Web panelinde ilgili kart kırmızı çerçeveye döner ve "Alarm" rozeti görünür
- Buzzer 150 ms bip / 850 ms sessizlik döngüsüyle öter

Sürekli ses yerine kesintili bip tercih edilmiştir: hem daha az rahatsız edici hem de daha dikkat çekicidir.

**Sessiz mod** (Settings bölümü) buzzer'ı susturur; ekrandaki ünlem ve paneldeki alarm göstergesi çalışmaya devam eder.

### Grafik Ölçeklemesi

Her seri kendi min/max aralığında normalize edilir, böylece farklı birimdeki değerler (hPa ve °C) aynı grafikte okunabilir kalır.

Her serinin **asgari bir ölçeği** vardır (sıcaklıklar 2 °C, basınç 5 hPa). Bu olmadan sabit duran bir değer, sensör gürültüsü kadar dar bir aralıkta tüm grafik yüksekliğine yayılır ve ciddi bir dalgalanma varmış gibi görünür. Açıklamada her serinin adının yanında o an çizilen alt-üst sınır gösterilir.

### WiFi Ayarlarını Değiştirme

Network bölümünden ağ adı ve şifre değiştirilebilir, ya da şifresiz (açık) ağ seçilebilir. Kaydedilince cihaz yeniden başlar ve yeni bilgilerle tekrar bağlanmanız gerekir.

Kilitlenmeye karşı alınan önlemler:

- SSID 1-32 karakter, şifre 8-63 karakter olarak doğrulanır (WPA2 zorunluluğu)
- Şifre alanı boş bırakılırsa mevcut şifre korunur; şifresiz ağa geçmek için onay kutusu açıkça işaretlenmelidir
- Mevcut şifre panele geri gönderilmez, yalnızca "ayarlı mı" bilgisi gider
- **Kurtarma:** Açılışta ağ adı ve şifre seri porta yazılır. Ağa girilemezse USB ile bağlanıp okunabilir.

## Logo Dönüştürme

ESP32'de dosya sistemi kullanılmadığı için logo görselleri derleme zamanında RGB565 C dizisine çevrilip flash belleğe gömülür. Dönüştürmeyi `tools/logo_donustur.py` yapar (Pillow gerektirir: `python -m pip install Pillow`).

```bash
# Açılış ekranı: 284x76 kutusuna sığdırılır (en/boy oranı korunur)
python tools/logo_donustur.py bmwacilis.jpg include/logo_acilis.h logoAcilis=284x76

# Merkez logo: 34x34 kare
python tools/logo_donustur.py bmwlogo.png include/logo_merkez.h logoMerkez=34
```

Script'in yaptıkları:

- **Boşluk kırpma**: Logo genelde büyük bir tuval içinde ortalanmış gelir. Bu boşluk kırpılmazsa logo hedef kutuda küçük kalır. Saydamlık varsa alpha kanalından, yoksa siyah zeminden gerçek sınırlar bulunur.
- **Oran koruma**: Görsel hedef kutuya sığacak şekilde ölçeklenir, ezilmez. `ad=34` kare kutu, `ad=284x76` dikdörtgen kutu anlamına gelir.
- **Maske üretimi**: Saydam pikseller için 1 bit/piksel maske dizisi üretilir. `drawRGBBitmap()`'in maskeli sürümüyle çizildiğinde logo, arka planın üzerine kare blok bırakmadan oturur.

## Geliştirme Notları

Bu panelle çalışırken karşılaşılan ve çözülen noktalar:

- **SPI hızı**: Adafruit kütüphanesinin varsayılanı 32 MHz'dir. Breadboard ve jumper kablolarla bu hız sinyal bozulmasına yol açar; proje 4 MHz kullanır (`TFT_SPI_HIZI`).
- **Init sırası**: Kütüphanenin `init()` fonksiyonu panel init komutlarını 32 MHz'de gönderir ve `setSPISpeed()` yalnızca init'ten sonra etki eder. Bu nedenle hız düşürüldükten sonra çekirdek ST7789 init komutları `panelInitEldenGonder()` ile elle yeniden gönderilir.
- **GRAM offset**: 76x284 standart dışı bir çözünürlüktür. Kütüphane bu boyut için offsetleri (`colstart=82`, `rowstart=18`) otomatik hesaplar, manuel ayar gerekmez.
- **Renk çevirme (INVON/INVOFF)**: Bu panelde renk çevirme **kapalı** olmalıdır. Init sırasında `INVON` (0x21) gönderildiğinde tüm renkler ters çıkar: siyah arka plan beyaza, beyaz yazılar siyaha, sarı maviye döner. Kod `INVOFF` (0x20) gönderir. Farklı bir panelle çalışılırsa bu komutun ters çevrilmesi gerekebilir.
- **Ekran yönü**: Görüntü baş aşağı çıkarsa `setRotation(1)` yerine `setRotation(3)` kullanılabilir.
- **GPIO2 (DC)**: ESP32'de strapping pindir. Yükleme sorunu yaşanırsa DC pini D15'e taşınabilir.
- **DS18B20 adres keşfi**: Sensör değiştirilirse veya yeni sensör eklenirse, `src/main.cpp` içindeki "DS18B20 ADRES KEŞİF KODU" yorum bloğu geçici olarak aktif edilerek yeni ROM adresi seri porttan okunabilir. Adres alındıktan sonra blok tekrar yorum satırına alınmalıdır.
- **Protokol karışıklığı**: TFT modülünün header'ında `SCL`/`SDA` yazması yanıltıcıdır — bunlar I2C değil, SPI'nin `SCK`/`MOSI` hatlarıdır. ST7789P3 çipi I2C'yi donanımsal olarak desteklemez, bu nedenle BMP280 ile aynı hatta bağlanamaz.
- **Eksik sensörde durmama**: Başlangıçta BMP280 bulunamadığında kod `while(1)` ile kilitleniyordu. Bu, `loop()` hiç çalışmadığı için captive portal DNS sunucusunun da işlememesine yol açıyordu — yani sensörsüz testte web paneli açılıyor ama otomatik yönlendirme çalışmıyordu. Artık hiçbir sensör eksikliği çalışmayı durdurmaz.
- **NVS okuma hataları**: `Preferences::getString()` ve `begin(..., true)` (salt okunur), anahtar/alan mevcut değilken varsayılan verilse bile seri porta `NOT_FOUND` hatası yazar. İlk açılışta bu gürültüyü önlemek için alan yazılabilir modda açılır ve string okumadan önce `isKey()` ile kontrol edilir.
- **Ayar yükleme sırası**: `webArayuzunuBaslat()` ekran iskeletinden önce çağrılmalıdır; ayarlar NVS'ten orada yüklenir. Aksi halde kayıtlı yerleşim, renk ve etiketler açılışta uygulanmaz.
- **Ekran/buzzer tespiti**: Bu iki bileşenin bağlı olup olmadığı yazılımdan anlaşılamaz. TFT 4 telli SPI ile bağlıdır (MISO yok), buzzer salt çıkıştır; her ikisi de geri bildirim vermez.

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
| D32 | Buzzer |

Toplam 9 GPIO kullanılmaktadır. İleride pin kazanımı gerekirse: aynı protokoldeki yeni cihazlar mevcut hatları paylaşabilir (I2C için farklı adres, SPI için ek CS pini, 1-Wire için ek pin gerekmez). TFT `RES` pini de yazılımsal reset kullanılarak serbest bırakılabilir.
