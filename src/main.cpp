#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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

// --- EKRAN YERLEŞİMİ (YATAY / LANDSCAPE) ---
// setRotation(1) ile panel 284 px genis, 76 px yuksek hale gelir.
// Yatayda genislik bol oldugu icin sicaklik ve basinc iki sutuna yerlestirilir.
#define EKRAN_GENISLIK 284
#define EKRAN_YUKSEKLIK 76

// GFX fontunda karakter genisligi: size 1 -> 6 px, size 2 -> 12 px, size 3 -> 18 px
#define SOL_SUTUN_X  6
#define SAG_SUTUN_X  150
#define ETIKET_Y     24

// Sicaklik: size 3 ("25.7" = 4 karakter x 18 px = 72 px)
#define SICAKLIK_DEGER_Y 38
#define SICAKLIK_DEGER_W 78
#define SICAKLIK_DEGER_H 24

// Basinc: basamak sayisi fazla oldugu icin size 2 ("1006.4" = 6 x 12 = 72 px)
#define BASINC_DEGER_Y 42
#define BASINC_DEGER_W 78
#define BASINC_DEGER_H 16

// --- ÖLÇÜM ZAMANLAMASI ---
// delay() yerine millis() tabanlı kontrol: loop bloklanmaz, ileride
// eklenecek görevler (WiFi, buton vb.) gecikmeden çalışabilir.
static const unsigned long OLCUM_ARALIGI_MS = 2000;
static unsigned long sonOlcumZamani = 0;

// Ekranda gereksiz yeniden çizimi (titremeyi) önlemek için son yazılan değerler
static float sonSicaklik = NAN;
static float sonBasinc = NAN;

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
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(SOL_SUTUN_X, 4);
  tft.print(F("TELPOST :)"));

  tft.drawFastHLine(4, 16, EKRAN_GENISLIK - 8, ST77XX_WHITE);

  // Iki sutunu ayiran dikey cizgi
  tft.drawFastVLine(SAG_SUTUN_X - 10, 22, EKRAN_YUKSEKLIK - 28, ST77XX_WHITE);

  tft.setTextColor(ST77XX_WHITE);

  // Sol sutun: sicaklik
  tft.setCursor(SOL_SUTUN_X, ETIKET_Y);
  tft.print(F("SICAKLIK"));
  tft.setCursor(SOL_SUTUN_X + SICAKLIK_DEGER_W + 4, SICAKLIK_DEGER_Y + 8);
  tft.print(F("C"));

  // Sag sutun: basinc
  tft.setCursor(SAG_SUTUN_X, ETIKET_Y);
  tft.print(F("BASINC"));
  tft.setCursor(SAG_SUTUN_X + BASINC_DEGER_W + 4, BASINC_DEGER_Y + 4);
  tft.print(F("hPa"));
}

// Bir ölçüm değerini, eski değerin üzerini temizleyerek yazar.
static void degerYaz(int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t yaziBoyutu, float deger, uint16_t renk) {
  tft.fillRect(x, y, w, h, ST77XX_BLACK);

  tft.setTextSize(yaziBoyutu);
  tft.setTextColor(renk);
  tft.setCursor(x, y);
  tft.print(deger, 1);
  tft.setTextSize(1);
}

void setup() {
  // Seri haberleşmeyi başlat (Hata ayıklama için)
  Serial.begin(115200);
  Serial.println(F("BMP280 ve ST7789 Testi Basliyor..."));

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
    tft.setCursor(SOL_SUTUN_X, SICAKLIK_DEGER_Y);
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
}

void loop() {
  if (millis() - sonOlcumZamani < OLCUM_ARALIGI_MS) {
    return;
  }
  sonOlcumZamani = millis();

  // 1. Sensörden verileri okuma
  float sicaklik = bmp.readTemperature();
  // Basınç Pascal olarak döner, hPa (milibar) cinsine çevirmek için 100'e bölüyoruz
  float basinc = bmp.readPressure() / 100.0F;

  // 2. Verileri Seri Monitörde (Bilgisayarda) gösterme
  Serial.print(F("Sicaklik: "));
  Serial.print(sicaklik);
  Serial.println(F(" *C"));

  Serial.print(F("Basinc: "));
  Serial.print(basinc);
  Serial.println(F(" hPa"));

  // 3. Verileri TFT ekranda gösterme (sadece degisen degerler yeniden cizilir)
  if (isnan(sonSicaklik) || fabsf(sicaklik - sonSicaklik) >= 0.05F) {
    degerYaz(SOL_SUTUN_X, SICAKLIK_DEGER_Y, SICAKLIK_DEGER_W, SICAKLIK_DEGER_H,
             3, sicaklik, ST77XX_YELLOW);
    sonSicaklik = sicaklik;
  }

  if (isnan(sonBasinc) || fabsf(basinc - sonBasinc) >= 0.05F) {
    degerYaz(SAG_SUTUN_X, BASINC_DEGER_Y, BASINC_DEGER_W, BASINC_DEGER_H,
             2, basinc, ST77XX_GREEN);
    sonBasinc = basinc;
  }
}
