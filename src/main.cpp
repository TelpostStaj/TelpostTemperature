#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- TFT EKRAN AYARLARI ---
// Panel: 2.25" IPS bar LCD, ST7789P3 sürücü, 76x284 piksel, SPI arayüz.
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_SCLK 18
#define TFT_MOSI 23

#define TFT_WIDTH  76
#define TFT_HEIGHT 284

// Kutuphanenin varsayilani 32 MHz; breadboard ve jumper kablolarla bu hiz
// sinyal bozulmasina yol acar. 4 MHz prototip kablolamada guvenli sinir.
#define TFT_SPI_HIZI 4000000UL

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// --- SENSÖR AYARLARI ---
Adafruit_BMP280 bmp; // BMP280 nesnesini oluşturuyoruz

// --- DS18B20 (SU / YAĞ SICAKLIK SENSÖRLERİ) ---
// 1-Wire protokolü: iki sensör de ayni veri hattini paylasir (D27),
// hatta 4.7k pull-up direnc (DQ - 3.3V arasi) gerekir. Sensorler ROM
// adresleriyle ayirt edilir; index'e (0,1) guvenmek bus sirasi degisirse
// su/yag karismasina yol acabilir.
#define ONEWIRE_PIN 27
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature ds18b20(&oneWire);

// Adres kesif prosedurüyle (asagidaki yorum satirina bakin) bulunan
// gercek ROM adresleri:
DeviceAddress SU_SENSOR_ADRESI  = { 0x28, 0xB2, 0x9A, 0xC8, 0x00, 0x00, 0x00, 0x58 };
DeviceAddress YAG_SENSOR_ADRESI = { 0x28, 0xA5, 0x9F, 0xC8, 0x00, 0x00, 0x00, 0x54 };

/* ============================================================
   DS18B20 ADRES KEŞİF KODU (REFERANS - AKTİF DEĞİL)
   ------------------------------------------------------------
   Bir sensör değişirse veya yeni sensör eklenirse adresini bulmak
   için bu bloğu geçici olarak aktif edin:

   1) Aşağıdaki fonksiyonları ve setup() içindeki çağrı satırını
      yorumdan çıkarın.
   2) Önce SADECE ilgili sensörü D27 hattına bağlayıp yükleyin.
      Seri portta "Taranan adres: { 0x.., ... }" şeklinde tek adres
      çıkar, onu ilgili DeviceAddress dizisine yapıştırın.
   3) İşiniz bitince bu bloğu tekrar yorum satırına alın.

static bool adresBos(const DeviceAddress adres) {
  for (uint8_t i = 0; i < 8; i++) {
    if (adres[i] != 0x00) return false;
  }
  return true;
}

static void adresYazdir(const DeviceAddress adres) {
  Serial.print(F("{ "));
  for (uint8_t i = 0; i < 8; i++) {
    Serial.print(F("0x"));
    if (adres[i] < 0x10) Serial.print('0');
    Serial.print(adres[i], HEX);
    if (i < 7) Serial.print(F(", "));
  }
  Serial.println(F(" }"));
}

static void adresKesifModu() {
  ds18b20.begin();
  int cihazSayisi = ds18b20.getDeviceCount();

  Serial.println(F("=== DS18B20 ADRES KESIF MODU ==="));
  Serial.print(F("Bus'ta bulunan cihaz sayisi: "));
  Serial.println(cihazSayisi);

  if (cihazSayisi == 0) {
    Serial.println(F("Hic sensor bulunamadi. DQ/pull-up/VCC baglantilarini kontrol edin."));
  }

  for (int i = 0; i < cihazSayisi; i++) {
    DeviceAddress adres;
    if (ds18b20.getAddress(adres, i)) {
      Serial.print(F("Taranan adres: "));
      adresYazdir(adres);
    }
  }

  while (1) {
    delay(5000); // Kesif modunda normal olcume gecilmez, kilitli kalir.
  }
}

   // setup() içinde çağrılacak satır:
   // if (adresBos(SU_SENSOR_ADRESI) || adresBos(YAG_SENSOR_ADRESI)) {
   //   adresKesifModu();
   // }
   ============================================================ */

// --- EKRAN YERLEŞİMİ (YATAY / LANDSCAPE, 2x2 IZGARA) ---
// setRotation(1) ile panel 284 px genis, 76 px yuksek hale gelir.
// 4 deger sigdirmak icin ekran 2 satir x 2 sutuna bolunur:
//   Ust satir : SICAKLIK (BMP280) | BASINC (BMP280)
//   Alt satir : SU (DS18B20)      | YAG (DS18B20)
#define EKRAN_GENISLIK 284
#define EKRAN_YUKSEKLIK 76

// GFX fontunda karakter genisligi: size 1 -> 6 px, size 2 -> 12 px
#define SOL_SUTUN_X  6
#define SAG_SUTUN_X  146
#define SUTUN_AYIRICI_X 142

#define UST_ETIKET_Y   14
#define UST_DEGER_Y    24
#define ALT_ETIKET_Y   46
#define ALT_DEGER_Y    56
#define SATIR_AYIRICI_Y 43

#define DEGER_GENISLIK 78
#define DEGER_YUKSEKLIK 16

// --- ÖLÇÜM ZAMANLAMASI ---
// delay() yerine millis() tabanlı kontrol: loop bloklanmaz, ileride
// eklenecek görevler (WiFi, buton vb.) gecikmeden çalışabilir.
static const unsigned long OLCUM_ARALIGI_MS = 2000;
static unsigned long sonOlcumZamani = 0;

// Ekranda gereksiz yeniden çizimi (titremeyi) önlemek için son yazılan değerler
static float sonSicaklik = NAN;
static float sonBasinc = NAN;
static float sonSu = NAN;
static float sonYag = NAN;

// ST7789 cekirdek init dizisini elle gonderir.
// Gerekce: kutuphanenin init() fonksiyonu bu komutlari 32 MHz'de gonderir ve
// setSPISpeed() ancak init'ten SONRA etki eder. Bu yuzden hiz dusuruldukten
// sonra komutlari tekrar gonderiyoruz ki panel guvenli hizda uyansin.
static void panelInitEldenGonder() {
  uint8_t arg;

  tft.sendCommand(0x01); // SWRESET - yazilimsal reset
  delay(150);

  tft.sendCommand(0x11); // SLPOUT - uyku modundan cik
  delay(120);

  arg = 0x55;                     // 16 bit/piksel (RGB565)
  tft.sendCommand(0x3A, &arg, 1); // COLMOD
  delay(10);

  arg = 0x00;                     // Varsayilan tarama yonu
  tft.sendCommand(0x36, &arg, 1); // MADCTL
  delay(10);

  tft.sendCommand(0x21); // INVON - IPS panellerde renk cevirme genelde gerekli
  delay(10);

  tft.sendCommand(0x13); // NORON - normal goruntuleme modu
  delay(10);

  tft.sendCommand(0x29); // DISPON - ekrani ac
  delay(120);
}

// Değişmeyen başlık, etiket ve birimleri bir kez çizer.
static void ekranIskeletiniCiz() {
  tft.fillScreen(ST77XX_BLACK);

  // Ust baslik seridi
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(SOL_SUTUN_X, 2);
  tft.print(F("TELPOST :)"));

  tft.drawFastHLine(0, 11, EKRAN_GENISLIK, ST77XX_WHITE);

  // Izgara ayirici cizgiler
  tft.drawFastVLine(SUTUN_AYIRICI_X, 13, EKRAN_YUKSEKLIK - 15, ST77XX_WHITE);
  tft.drawFastHLine(0, SATIR_AYIRICI_Y, EKRAN_GENISLIK, ST77XX_WHITE);

  tft.setTextColor(ST77XX_WHITE);

  // Ust sol: sicaklik
  tft.setCursor(SOL_SUTUN_X, UST_ETIKET_Y);
  tft.print(F("SICAKLIK C"));

  // Ust sag: basinc
  tft.setCursor(SAG_SUTUN_X, UST_ETIKET_Y);
  tft.print(F("BASINC hPa"));

  // Alt sol: su
  tft.setCursor(SOL_SUTUN_X, ALT_ETIKET_Y);
  tft.print(F("SU C"));

  // Alt sag: yag
  tft.setCursor(SAG_SUTUN_X, ALT_ETIKET_Y);
  tft.print(F("YAG C"));
}

// Bir ölçüm değerini, eski değerin üzerini temizleyerek yazar.
static void degerYaz(int16_t x, int16_t y, float deger, uint16_t renk) {
  tft.fillRect(x, y, DEGER_GENISLIK, DEGER_YUKSEKLIK, ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(renk);
  tft.setCursor(x, y);
  tft.print(deger, 1);
  tft.setTextSize(1);
}

void setup() {
  // Seri haberleşmeyi başlat (Hata ayıklama için)
  Serial.begin(115200);
  Serial.println(F("BMP280, DS18B20 ve ST7789 Basliyor..."));

  // 1. TFT EKRANI BAŞLATMA
  // ST7789 SPI ile calisir; init() panel cozunurlugune gore GRAM offsetlerini
  // (76x284 icin colstart=82, rowstart=18) kendisi hesaplar.
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setSPISpeed(TFT_SPI_HIZI);
  panelInitEldenGonder();
  // Yatay (landscape) yerlesim: 284x76. Goruntu bas asagi cikarsa 3 yapin.
  tft.setRotation(1);
  ekranIskeletiniCiz();

  // 2. BMP280 SENSÖRÜNÜ BAŞLATMA
  // Piyasadaki ucuz BMP280 modüllerinin adresi genellikle 0x76'dır.
  // Orijinal Adafruit sensörleri 0x77 kullanır. Bu yüzden 0x76 adresini veriyoruz.
  if (!bmp.begin(0x76)) {
    Serial.println(F("BMP280 sensoru bulunamadi! Adresi veya kablolari kontrol et."));
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.setCursor(SOL_SUTUN_X, UST_DEGER_Y);
    tft.print(F("SENSOR YOK"));
    tft.setTextSize(1);
    while (1); // Sensör yoksa kodu kilitle
  }

  // Sensörün örnekleme kalitesini ayarlıyoruz (Standart kullanım için)
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Çalışma modu. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Sıcaklık aşırı örnekleme */
                  Adafruit_BMP280::SAMPLING_X16,    /* Basınç aşırı örnekleme */
                  Adafruit_BMP280::FILTER_X16,      /* Gürültü filtresi */
                  Adafruit_BMP280::STANDBY_MS_500); /* Ölçümler arası bekleme süresi */

  // 3. DS18B20 SICAKLIK SENSÖRLERİNİ BAŞLATMA
  ds18b20.begin();
  if (!ds18b20.isConnected(SU_SENSOR_ADRESI)) {
    Serial.println(F("UYARI: Su sensoru (DS18B20) tanimli adreste bulunamadi."));
  }
  if (!ds18b20.isConnected(YAG_SENSOR_ADRESI)) {
    Serial.println(F("UYARI: Yag sensoru (DS18B20) tanimli adreste bulunamadi."));
  }
}

void loop() {
  if (millis() - sonOlcumZamani < OLCUM_ARALIGI_MS) {
    return;
  }
  sonOlcumZamani = millis();

  // 1. BMP280'den sicaklik/basinc okuma
  float sicaklik = bmp.readTemperature();
  // Basınç Pascal olarak döner, hPa (milibar) cinsine çevirmek için 100'e bölüyoruz
  float basinc = bmp.readPressure() / 100.0F;

  // 2. DS18B20'lerden su/yag sicakliklarini okuma (1-Wire, ayni D27 hatti)
  ds18b20.requestTemperatures();
  float suSicakligi = ds18b20.getTempC(SU_SENSOR_ADRESI);
  float yagSicakligi = ds18b20.getTempC(YAG_SENSOR_ADRESI);

  // 3. Verileri Seri Monitörde gösterme
  Serial.print(F("Sicaklik: "));
  Serial.print(sicaklik);
  Serial.println(F(" *C"));

  Serial.print(F("Basinc: "));
  Serial.print(basinc);
  Serial.println(F(" hPa"));

  Serial.print(F("Su Sicakligi: "));
  if (suSicakligi == DEVICE_DISCONNECTED_C) {
    Serial.println(F("OKUNAMADI"));
  } else {
    Serial.print(suSicakligi);
    Serial.println(F(" *C"));
  }

  Serial.print(F("Yag Sicakligi: "));
  if (yagSicakligi == DEVICE_DISCONNECTED_C) {
    Serial.println(F("OKUNAMADI"));
  } else {
    Serial.print(yagSicakligi);
    Serial.println(F(" *C"));
  }

  // 4. Verileri TFT ekranda gösterme (sadece degisen degerler yeniden cizilir)
  if (isnan(sonSicaklik) || fabsf(sicaklik - sonSicaklik) >= 0.05F) {
    degerYaz(SOL_SUTUN_X, UST_DEGER_Y, sicaklik, ST77XX_YELLOW);
    sonSicaklik = sicaklik;
  }

  if (isnan(sonBasinc) || fabsf(basinc - sonBasinc) >= 0.05F) {
    degerYaz(SAG_SUTUN_X, UST_DEGER_Y, basinc, ST77XX_GREEN);
    sonBasinc = basinc;
  }

  if (suSicakligi != DEVICE_DISCONNECTED_C &&
      (isnan(sonSu) || fabsf(suSicakligi - sonSu) >= 0.05F)) {
    degerYaz(SOL_SUTUN_X, ALT_DEGER_Y, suSicakligi, ST77XX_CYAN);
    sonSu = suSicakligi;
  }

  if (yagSicakligi != DEVICE_DISCONNECTED_C &&
      (isnan(sonYag) || fabsf(yagSicakligi - sonYag) >= 0.05F)) {
    degerYaz(SAG_SUTUN_X, ALT_DEGER_Y, yagSicakligi, ST77XX_ORANGE);
    sonYag = yagSicakligi;
  }
}
