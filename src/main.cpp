#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "logo_acilis.h"
#include "logo_merkez.h"
#include "web_arayuz.h"

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
// --- BUZZER ---
// Aktif buzzer (icinde osilator olan) da pasif buzzer da calisir: pasif
// buzzer icin LEDC ile ton uretilir, aktif buzzer bu sinyalde de oter.
#define BUZZER_PIN 32
#define BUZZER_LEDC_KANALI 0
#define BUZZER_FREKANS 2700 // Hz - kucuk buzzerlarin en gur oldugu bolge
#define BUZZER_BIP_MS 150   // Bir bip suresi
#define BUZZER_ARA_MS 850   // Bipler arasi sessizlik

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
      Seri portta "Scanned address: { 0x.., ... }" şeklinde tek adres
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

  Serial.println(F("=== DS18B20 ADDRESS DISCOVERY MODE ==="));
  Serial.print(F("Devices found on bus: "));
  Serial.println(cihazSayisi);

  if (cihazSayisi == 0) {
    Serial.println(F("No sensors found. Check DQ / pull-up / VCC wiring."));
  }

  for (int i = 0; i < cihazSayisi; i++) {
    DeviceAddress adres;
    if (ds18b20.getAddress(adres, i)) {
      Serial.print(F("Scanned address: "));
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

// --- LOGO ---
// Amblem gercek gorselden uretilmis RGB565 bitmap olarak gomulur.
// include/logo.h dosyasi tools/logo_donustur.py tarafindan olusturulur:
//   python tools/logo_donustur.py bmwlogo.png include/logo.h logoBuyuk=64 logoMerkez=34
// Acilis icin logoBuyuk (64x64), merkez icin logoMerkez (34x34) kullanilir.

// --- EKRAN YERLEŞİMİ (YATAY / LANDSCAPE, 2x2 IZGARA) ---
// setRotation(1) ile panel 284 px genis, 76 px yuksek hale gelir.
// 4 deger sigdirmak icin ekran 2 satir x 2 sutuna bolunur:
//   Ust satir : SICAKLIK (BMP280) | BASINC (BMP280)
//   Alt satir : SU (DS18B20)      | YAG (DS18B20)
#define EKRAN_GENISLIK 284
#define EKRAN_YUKSEKLIK 76

// Ayirici cizgilerin kesistigi merkez; logo buraya oturur.
#define MERKEZ_X 142
#define MERKEZ_Y 38
#define MERKEZ_LOGO_YARICAP 17

// GFX fontunda karakter genisligi: size 1 -> 6 px, size 2 -> 12 px
// Sutunlar merkezdeki logoya carpmayacak sekilde disari kaydirildi.
#define SOL_SUTUN_X  8
#define SAG_SUTUN_X  166

#define UST_ETIKET_Y   6
#define UST_DEGER_Y    16
#define ALT_ETIKET_Y   44
#define ALT_DEGER_Y    54

#define DEGER_GENISLIK 100
#define DEGER_YUKSEKLIK 16

// --- ÖLÇÜM ZAMANLAMASI ---
// delay() yerine millis() tabanlı kontrol: loop bloklanmaz, böylece asenkron
// web sunucusu istekleri gecikmeden isleyebilir.
// Aralik web arayuzunden degistirilebilir; guncel deger webAyarlar() ile alinir.
static unsigned long sonOlcumZamani = 0;

// --- ÖLÇÜM TANIMLARI ---
// Etiket metni ve renkler artık ayarlardan gelir (web arayüzünden
// değiştirilebilir). Bunlar ölçüme bağlıdır, çeyreğe değil: bir ölçüm hangi
// çeyreğe taşınırsa etiketi, renkleri ve alarm göstergesi onunla birlikte taşınır.

// Çeyrek konumları: CEYREK_SOL_UST, CEYREK_SAG_UST, CEYREK_SOL_ALT, CEYREK_SAG_ALT
static const int16_t CEYREK_X[CEYREK_SAYISI] = {
    SOL_SUTUN_X, SAG_SUTUN_X, SOL_SUTUN_X, SAG_SUTUN_X};

static const int16_t CEYREK_ETIKET_Y[CEYREK_SAYISI] = {
    UST_ETIKET_Y, UST_ETIKET_Y, ALT_ETIKET_Y, ALT_ETIKET_Y};

static const int16_t CEYREK_DEGER_Y[CEYREK_SAYISI] = {
    UST_DEGER_Y, UST_DEGER_Y, ALT_DEGER_Y, ALT_DEGER_Y};

// --- BUZZER DURUMU ---
// Bip deseni millis() ile yurutulur; delay() kullanilmaz ki olcum ve web
// trafigi aksamasin.
static bool buzzerOtuyor = false;
static unsigned long buzzerDegisimZamani = 0;
// Son olcumde herhangi bir esik asildi mi. loop() her turda buzzer'i buna
// gore surer, olcum araligini beklemeden.
static bool alarmAktif = false;

// BMP280 bağlı ve yapılandırılmış mı. Bağlı değilse loop() periyodik olarak
// yeniden dener, böylece sensör çalışırken takılırsa kendiliğinden devreye girer.
static bool bmpHazir = false;

// Ekranda gereksiz yeniden çizimi (titremeyi) önlemek için son yazılan durum.
// Ölçüm indeksine göre tutulur, böylece yerleşim değişse de tutarlı kalır.
static float sonDegerler[OLCUM_SAYISI] = {NAN, NAN, NAN, NAN};
static bool sonAlarmlar[OLCUM_SAYISI] = {false, false, false, false};
// İlgili hücreye henüz bir şey çizilmemişse false. NAN ile "hiç çizilmedi"
// durumunu ayırt etmek için gerekli.
static bool sonCizildi[OLCUM_SAYISI] = {false, false, false, false};

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

  // Bu panelde renk cevirme KAPALI olmali. INVON gonderildiginde arka plan
  // beyaza, beyaz yazilar siyaha doner (tum renkler ters cikar).
  tft.sendCommand(0x20); // INVOFF
  delay(10);

  tft.sendCommand(0x13); // NORON - normal goruntuleme modu
  delay(10);

  tft.sendCommand(0x29); // DISPON - ekrani ac
  delay(120);
}

// Bitmap logoyu verilen merkez noktasina ortalayarak cizer.
// Maske sayesinde dairenin disindaki saydam pikseller cizilmez; logo
// arka planin uzerine kare bir blok birakmadan oturur.
static void logoCiz(int16_t cx, int16_t cy, const uint16_t *bitmap,
                    const uint8_t *maske, int16_t genislik, int16_t yukseklik) {
  tft.drawRGBBitmap(cx - genislik / 2, cy - yukseklik / 2,
                    bitmap, maske, genislik, yukseklik);
}

// Cihaz acilisinda birkac saniye gosterilen logo ekrani.
static void acilisEkraniniGoster() {
  tft.fillScreen(ST77XX_BLACK);

  // M logosu yatay oranli (yaklasik 6:1), bu yuzden ekran genisligini tam
  // kaplayacak sekilde olceklenir ve dikeyde ortalanir.
  logoCiz(EKRAN_GENISLIK / 2, EKRAN_YUKSEKLIK / 2,
          logoAcilis, logoAcilisMaske,
          LOGOACILIS_GENISLIK, LOGOACILIS_YUKSEKLIK);

  delay(2500);
}

// Değişmeyen başlık, etiket ve birimleri çizer.
// Yerleşim veya çizgi rengi değiştiğinde web arayüzünden tetiklenerek
// yeniden çağrılır.
static void ekranIskeletiniCiz() {
  const Ayarlar &ayar = webAyarlar();

  tft.fillScreen(ST77XX_BLACK);

  // Izgarayi dort ceyrege bolen ayirici cizgiler (rengi ayarlanabilir)
  tft.drawFastVLine(MERKEZ_X, 2, EKRAN_YUKSEKLIK - 4, ayar.cizgiRengi);
  tft.drawFastHLine(2, MERKEZ_Y, EKRAN_GENISLIK - 4, ayar.cizgiRengi);

  // Her ceyrege atanmis olcumun etiketini kendi renginde yaz
  tft.setTextSize(1);
  for (uint8_t c = 0; c < CEYREK_SAYISI; c++) {
    uint8_t olcum = ayar.ceyrek[c];
    tft.setTextColor(ayar.etiketRenkleri[olcum]);
    tft.setCursor(CEYREK_X[c], CEYREK_ETIKET_Y[c]);
    tft.print(ayar.etiketler[olcum]);
  }

  // Logo en son cizilir: cizgilerin kesisimini kapatarak merkeze oturur.
  logoCiz(MERKEZ_X, MERKEZ_Y, logoMerkez, logoMerkezMaske,
          LOGOMERKEZ_GENISLIK, LOGOMERKEZ_YUKSEKLIK);
}

// Buzzer sesini acar/kapatir.
static void buzzerSesi(bool acik) {
  ledcWriteTone(BUZZER_LEDC_KANALI, acik ? BUZZER_FREKANS : 0);
  buzzerOtuyor = acik;
}

// Alarm durumuna gore buzzer'i surer. loop() icinden her turda cagrilir.
// Sessiz modda ses uretilmez; ekrandaki unlem isareti bundan bagimsizdir.
static void buzzeriGuncelle(bool alarmVar) {
  bool calmali = alarmVar && !webAyarlar().sessizMod;

  if (!calmali) {
    if (buzzerOtuyor) buzzerSesi(false);
    return;
  }

  // Kesintili bip: surekli ses hem rahatsiz edici hem de dikkat cekmiyor
  unsigned long simdi = millis();
  unsigned long sure = buzzerOtuyor ? BUZZER_BIP_MS : BUZZER_ARA_MS;
  if (simdi - buzzerDegisimZamani >= sure) {
    buzzerDegisimZamani = simdi;
    buzzerSesi(!buzzerOtuyor);
  }
}

// BMP280'i başlatır ve örnekleme ayarlarını uygular.
// Hem setup()'ta hem de sensör sonradan takıldığında loop()'tan çağrılır.
static bool bmp280Baslat() {
  if (!bmp.begin(0x76)) {
    return false;
  }

  // Sensörün örnekleme kalitesini ayarlıyoruz (Standart kullanım için)
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Çalışma modu. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Sıcaklık aşırı örnekleme */
                  Adafruit_BMP280::SAMPLING_X16,    /* Basınç aşırı örnekleme */
                  Adafruit_BMP280::FILTER_X16,      /* Gürültü filtresi */
                  Adafruit_BMP280::STANDBY_MS_500); /* Ölçümler arası bekleme süresi */
  return true;
}

// Bir ölçüm değerini, eski değerin üzerini temizleyerek yazar.
// Alarm durumunda değerin yanına kırmızı ünlem işareti eklenir.
static void degerYaz(int16_t x, int16_t y, float deger, uint16_t renk, bool alarm) {
  tft.fillRect(x, y, DEGER_GENISLIK, DEGER_YUKSEKLIK, ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(renk);
  tft.setCursor(x, y);
  if (isnan(deger)) {
    tft.print(F("--"));
  } else {
    tft.print(deger, 1);
  }

  if (alarm) {
    // Deger alaninin sag ucunda, en genis deger ("1006.4" = 72 px) ile
    // cakismayacak konumda
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(x + 80, y);
    tft.print('!');
  }

  tft.setTextSize(1);
}

void setup() {
  // Seri haberleşmeyi başlat (Hata ayıklama için)
  Serial.begin(115200);
  Serial.println(F("Starting BMP280, DS18B20 and ST7789..."));

  // Buzzer cikisi. LEDC ile ton uretilir; frekans buzzeriGuncelle() belirler.
  ledcSetup(BUZZER_LEDC_KANALI, BUZZER_FREKANS, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_KANALI);
  buzzerSesi(false);

  // 1. TFT EKRANI BAŞLATMA
  // ST7789 SPI ile calisir; init() panel cozunurlugune gore GRAM offsetlerini
  // (76x284 icin colstart=82, rowstart=18) kendisi hesaplar.
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setSPISpeed(TFT_SPI_HIZI);
  panelInitEldenGonder();
  // Yatay (landscape) yerlesim: 284x76. Goruntu bas asagi cikarsa 3 yapin.
  tft.setRotation(1);
  acilisEkraniniGoster();

  // 2. WIFI ERISIM NOKTASI VE WEB ARAYUZU
  // Ekran iskeletinden ONCE baslatilir: kayitli yerlesim ve cizgi rengi
  // ayarlari NVS'ten burada yuklenir, iskelet bunlara gore cizilir.
  // Asenkron sunucu kendi olay dongusunde calisir; loop() icinde ek cagri
  // gerektirmez, bu yuzden olcum zamanlamasini etkilemez.
  webArayuzunuBaslat();

  // 3. EKRAN ISKELETI (kayitli ayarlarla)
  ekranIskeletiniCiz();

  // 4. BMP280 SENSÖRÜNÜ BAŞLATMA
  // Piyasadaki ucuz BMP280 modüllerinin adresi genellikle 0x76'dır.
  // Orijinal Adafruit sensörleri 0x77 kullanır. Bu yüzden 0x76 adresini veriyoruz.
  // Sensör yoksa çalışma durdurulmaz: web arayüzü ve diğer sensörler çalışmaya
  // devam eder, eksik değerler ekranda "--" olarak görünür. Sensör sonradan
  // takılırsa loop() içindeki yeniden deneme onu devreye alır.
  bmpHazir = bmp280Baslat();
  if (!bmpHazir) {
    Serial.println(F("BMP280 not found! Check the I2C address and wiring."));
  }

  // 5. DS18B20 SICAKLIK SENSÖRLERİNİ BAŞLATMA
  ds18b20.begin();
  if (!ds18b20.isConnected(SU_SENSOR_ADRESI)) {
    Serial.println(F("WARNING: Water sensor (DS18B20) not found at its address."));
  }
  if (!ds18b20.isConnected(YAG_SENSOR_ADRESI)) {
    Serial.println(F("WARNING: Oil sensor (DS18B20) not found at its address."));
  }
}

void loop() {
  // Captive portal DNS istekleri ve bekleyen yeniden baslatma islenir.
  // Olcum araligindan bagimsiz olarak her turda calismalidir.
  webDongu();

  // Bip deseni olcum araligindan cok daha kisa oldugu icin buzzer da her
  // turda guncellenir. Alarm durumu son olcumden gelir.
  buzzeriGuncelle(alarmAktif);

  if (millis() - sonOlcumZamani < webAyarlar().olcumAraligiMs) {
    return;
  }
  sonOlcumZamani = millis();

  // 1. BMP280'den sicaklik/basinc okuma
  // Sensor bagli degilse her olcum turunda yeniden baglanmayi dene
  if (!bmpHazir) {
    bmpHazir = bmp280Baslat();
    if (bmpHazir) {
      Serial.println(F("BMP280 connected."));
    }
  }

  float sicaklik = NAN;
  float basinc = NAN;
  if (bmpHazir) {
    sicaklik = bmp.readTemperature();
    // Basınç Pascal olarak döner, hPa (milibar) cinsine çevirmek için 100'e bölüyoruz
    basinc = bmp.readPressure() / 100.0F;
  }

  // 2. DS18B20'lerden su/yag sicakliklarini okuma (1-Wire, ayni D27 hatti)
  ds18b20.requestTemperatures();
  float suSicakligi = ds18b20.getTempC(SU_SENSOR_ADRESI);
  float yagSicakligi = ds18b20.getTempC(YAG_SENSOR_ADRESI);

  // 3. Verileri Seri Monitörde gösterme
  Serial.print(F("Temperature: "));
  if (isnan(sicaklik)) {
    Serial.println(F("READ FAILED"));
  } else {
    Serial.print(sicaklik);
    Serial.println(F(" *C"));
  }

  Serial.print(F("Pressure: "));
  if (isnan(basinc)) {
    Serial.println(F("READ FAILED"));
  } else {
    Serial.print(basinc);
    Serial.println(F(" hPa"));
  }

  Serial.print(F("Water temp: "));
  if (suSicakligi == DEVICE_DISCONNECTED_C) {
    Serial.println(F("READ FAILED"));
  } else {
    Serial.print(suSicakligi);
    Serial.println(F(" *C"));
  }

  Serial.print(F("Oil temp: "));
  if (yagSicakligi == DEVICE_DISCONNECTED_C) {
    Serial.println(F("READ FAILED"));
  } else {
    Serial.print(yagSicakligi);
    Serial.println(F(" *C"));
  }

  // 4. Olcumleri web arayuzune bildir (WebSocket ile canli akis + gecmis kaydi)
  // Okunamayan DS18B20 degerleri NAN olarak gecilir; arayuz bunlari "--" gosterir.
  webOlcumBildir(sicaklik, basinc,
                 (suSicakligi == DEVICE_DISCONNECTED_C) ? NAN : suSicakligi,
                 (yagSicakligi == DEVICE_DISCONNECTED_C) ? NAN : yagSicakligi);

  // 5. Verileri TFT ekranda gösterme
  // Web arayuzunden yerlesim veya cizgi rengi degistiyse once iskeleti yeniden
  // ciz; onbellegi temizleyerek tum degerlerin yeni konumlarina yazilmasini sagla.
  bool tumunuYenidenCiz = webYerlesimDegistiMi();
  if (tumunuYenidenCiz) {
    ekranIskeletiniCiz();
    for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
      sonCizildi[i] = false;
    }
  }

  float degerler[OLCUM_SAYISI] = {
      sicaklik,
      basinc,
      (suSicakligi == DEVICE_DISCONNECTED_C) ? NAN : suSicakligi,
      (yagSicakligi == DEVICE_DISCONNECTED_C) ? NAN : yagSicakligi,
  };

  const Ayarlar &ayar = webAyarlar();

  // Herhangi bir olcum esigi asiyorsa buzzer calisir
  alarmAktif = false;
  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    if (webAlarmVar(i, degerler[i])) alarmAktif = true;
  }

  for (uint8_t c = 0; c < CEYREK_SAYISI; c++) {
    uint8_t olcum = ayar.ceyrek[c];
    float deger = degerler[olcum];
    bool alarm = webAlarmVar(olcum, deger);

    // Okunamayan sensor icin "--" cizilir; eski deger ekranda birakilmaz,
    // cunku kopmus bir sensorun son degerini gostermek yaniltici olur.
    bool ayni = sonCizildi[olcum] &&
                (isnan(deger) == isnan(sonDegerler[olcum])) &&
                (isnan(deger) || fabsf(deger - sonDegerler[olcum]) < 0.05F);
    if (ayni && alarm == sonAlarmlar[olcum]) continue;

    degerYaz(CEYREK_X[c], CEYREK_DEGER_Y[c], deger, ayar.degerRenkleri[olcum], alarm);
    sonDegerler[olcum] = deger;
    sonAlarmlar[olcum] = alarm;
    sonCizildi[olcum] = true;
  }
}
