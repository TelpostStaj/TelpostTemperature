#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED EKRAN AYARLARI ---
#define SCREEN_WIDTH 128 // OLED ekranın piksel olarak genişliği
#define SCREEN_HEIGHT 64 // OLED ekranın piksel olarak yüksekliği
// ESP32 donanımsal I2C kullandığı için reset pinine gerek yok, -1 atıyoruz.
#define OLED_RESET -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- SENSÖR AYARLARI ---
Adafruit_BMP280 bmp; // BMP280 nesnesini oluşturuyoruz

void setup() {
  // Seri haberleşmeyi başlat (Hata ayıklama için)
  Serial.begin(115200);
  Serial.println(F("BMP280 ve OLED Testi Basliyor..."));

  // 1. OLED EKRANI BAŞLATMA
  // SSD1306_SWITCHCAPVCC: Ekranın kendi içindeki voltaj pompasını çalıştırır
  // 0x3C: OLED ekranların piyasadaki standart I2C adresidir. 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED ekran bulunamadi! Baglantilari kontrol et."));
    while(1); // Ekran yoksa kodu burada kilitle (Sonsuz döngü)
  }
  
  // Ekranı temizle, yazı boyutunu ve rengini ayarla
  display.clearDisplay();
  display.setTextSize(1);      // Normal boyutta metin
  display.setTextColor(SSD1306_WHITE); // Arka plan siyah, yazı beyaz
  
  display.setCursor(0,0);      // Yazmaya sol üst köşeden (x=0, y=0) başla
  display.println(F("OLED Hazir!"));
  display.display();           // Bellekteki yaziyi ekrana basar
  delay(1000);                 // Kullanıcının okuması için 1 saniye bekle

  // 2. BMP280 SENSÖRÜNÜ BAŞLATMA
  // Piyasadaki ucuz BMP280 modüllerinin adresi genellikle 0x76'dır. 
  // Orijinal Adafruit sensörleri 0x77 kullanır. Bu yüzden 0x76 adresini veriyoruz.
  if (!bmp.begin(0x76)) {
    Serial.println(F("BMP280 sensoru bulunamadi! Adresi veya kablolari kontrol et."));
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
  // 1. Sensörden verileri okuma
  float sicaklik = bmp.readTemperature();
  // Basınç Pascal olarak döner, hPa (milibar) cinsine çevirmek için 100'e bölüyoruz
  float basinc = bmp.readPressure() / 100.0F; 

  // 2. Verileri Seri Monitörde (Bilgisayarda) gösterme
  Serial.print(F("Sicaklik: "));
  Serial.print(sicaklik);
  Serial.println(F(" *C"));

  // 3. Verileri OLED Ekranda gösterme
  display.clearDisplay(); // Önceki turdan kalan yazıları temizle

  // Sıcaklık Yazdırma
  display.setTextSize(2); // Sıcaklığı büyük yazalım
  display.setCursor(0, 10);
  display.print(F("Sic: "));
  display.print(sicaklik);
  display.setTextSize(1); // Derece sembolü ve C için boyutu küçült
  display.print(F(" C"));

  // Basınç Yazdırma
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print(F("Basinc: "));
  display.print(basinc);
  display.print(F(" hPa"));

  // Tampon belleğe (RAM) yazdıklarımızı OLED panele gönder!
  display.display(); 

  // Her iki saniyede bir ölçüm yap
  delay(2000); 
}
