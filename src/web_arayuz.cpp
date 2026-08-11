#include "web_arayuz.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

#include "web_sayfa.h"

// --- ERISIM NOKTASI (AP) AYARLARI ---
// Aracta WiFi agi bulunmadigi icin ESP32 kendi agini kurar; telefon/laptop
// dogrudan bu aga baglanir. Varsayilan adres: http://192.168.4.1
// Bu degerler yalnizca ilk acilista (NVS bosken) kullanilir; sonrasinda
// web arayuzunden degistirilebilir.
static const char *VARSAYILAN_AP_ADI = "Telpost-Monitor";
static const char *VARSAYILAN_AP_SIFRE = "telpost1234"; // WPA2 icin en az 8 karakter

// WPA2 sifre uzunluk sinirlari. Bu araligin disi kabul edilmez; bos birakilirsa
// ag sifresiz yayin yapar.
#define SIFRE_MIN 8
#define SIFRE_MAX 63

// Captive portal: telefon aga baglanir baglanmaz panelin acilmasi icin tum
// alan adlari cihazin IP'sine cozulur.
static DNSServer dnsSunucu;
#define DNS_PORTU 53

// Yonlendirmelerde kullanilan panel adresi. AP kurulduktan sonra gercek IP
// ile doldurulur; istek isleyicileri (capture'siz lambda) buradan okur.
static String portalAdresi = "http://192.168.4.1/";

// --- GECMIS VERI TAMPONU ---
// Grafik icin son olcumler RAM'de dairesel tamponda tutulur. 120 ornek,
// varsayilan 2 sn araliginda yaklasik 4 dakikalik gecmise denk gelir.
#define GECMIS_KAPASITE 120

struct Olcum {
  float sicaklik;
  float basinc;
  float su;
  float yag;
};

static Olcum gecmis[GECMIS_KAPASITE];
static uint16_t gecmisSayisi = 0; // Tamponda kac gecerli kayit var
static uint16_t gecmisBas = 0;    // En eski kaydin indeksi

static AsyncWebServer sunucu(80);
static AsyncWebSocket websoket("/ws");
static Preferences kalici;

static Ayarlar ayarlar = {
    .olcumAraligiMs = 2000,
    .suAlarmEsigi = 95.0F,
    .yagAlarmEsigi = 110.0F,
    .sessizMod = false,
    // Varsayilan yerlesim: sol ust sicaklik, sag ust su, sol alt basinc, sag alt yag
    .ceyrek = {OLCUM_SICAKLIK, OLCUM_SU, OLCUM_BASINC, OLCUM_YAG},
    .cizgiRengi = 0xFFFF, // beyaz
    // Etiketler ayarlariYukle() icinde varsayilanlarla doldurulur
    .etiketler = {{0}, {0}, {0}, {0}},
    // ST77XX_WHITE
    .etiketRenkleri = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    // ST77XX_YELLOW, ST77XX_GREEN, ST77XX_CYAN, ST77XX_ORANGE
    .degerRenkleri = {0xFFE0, 0x07E0, 0x07FF, 0xFC00},
    .apAdi = {0}, // ayarlariYukle() varsayilanlarla doldurur
    .apSifresi = {0},
};

// Sirasi OLCUM_* ile ayni olmalidir.
static const char *VARSAYILAN_ETIKETLER[OLCUM_SAYISI] = {
    "TEMPERATURE C", "PRESSURE hPa", "WATER C", "OIL C"};

// Yerlesim/renk degisince main.cpp'nin ekrani yeniden cizmesi icin bayrak
static bool yerlesimDegisti = false;

// WiFi kimlik bilgileri degisince cihaz yeniden baslatilir. Yanit tarayiciya
// ulassin diye hemen degil, kisa bir gecikmeyle. 0 = planlanmadi.
static uint32_t yenidenBaslatmaZamani = 0;

const Ayarlar &webAyarlar() { return ayarlar; }

bool webYerlesimDegistiMi() {
  if (!yerlesimDegisti) return false;
  yerlesimDegisti = false;
  return true;
}

bool webAlarmVar(uint8_t olcum, float deger) {
  if (isnan(deger)) return false;
  if (olcum == OLCUM_SU) return deger >= ayarlar.suAlarmEsigi;
  if (olcum == OLCUM_YAG) return deger >= ayarlar.yagAlarmEsigi;
  return false; // Sicaklik ve basinc icin esik tanimli degil
}

// NAN degerleri JSON'da null olarak yazar; tarayici tarafi bunu "--" gosterir.
static void sayiEkle(String &json, const char *ad, float deger, bool sonuncu) {
  json += '"';
  json += ad;
  json += "\":";
  if (isnan(deger)) {
    json += "null";
  } else {
    json += String(deger, 1);
  }
  if (!sonuncu) json += ',';
}

static void ayarlariYukle() {
  // Yazilabilir acilir: ilk acilista NVS alani henuz yoktur ve salt okunur
  // acmak "nvs_open failed: NOT_FOUND" hatasi verir. Yazilabilir modda alan
  // olusturuldugu icin bu hata olusmaz; kayit yoksa varsayilanlar kalir.
  kalici.begin("telpost", false);
  ayarlar.olcumAraligiMs = kalici.getUInt("aralik", ayarlar.olcumAraligiMs);
  ayarlar.suAlarmEsigi = kalici.getFloat("suAlarm", ayarlar.suAlarmEsigi);
  ayarlar.yagAlarmEsigi = kalici.getFloat("yagAlarm", ayarlar.yagAlarmEsigi);
  ayarlar.cizgiRengi = kalici.getUShort("cizgi", ayarlar.cizgiRengi);
  ayarlar.sessizMod = kalici.getBool("sessiz", ayarlar.sessizMod);

  // Dort ceyrek atamasi tek bir 32 bitlik degerde paketlenir (ceyrek basina
  // 1 bayt). Kaydin varligi isKey() ile sorulur: paketin 0 olmasi gecerli bir
  // yerlesimdir (dort ceyrekte de sicaklik), "kayit yok" anlamina gelmez.
  if (kalici.isKey("ceyrek")) {
    uint32_t paket = kalici.getUInt("ceyrek");
    bool gecerli = true;
    uint8_t okunan[CEYREK_SAYISI];
    for (uint8_t i = 0; i < CEYREK_SAYISI; i++) {
      okunan[i] = (paket >> (i * 8)) & 0xFF;
      if (okunan[i] >= OLCUM_SAYISI) gecerli = false;
    }
    // Bozuk kayit varsayilani ezmesin
    if (gecerli) memcpy(ayarlar.ceyrek, okunan, sizeof(okunan));
  }

  // Olcum basina etiket metni ve renkler.
  // NVS anahtarlari 15 karakteri asamaz, bu yuzden kisa tutulur.
  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    String etiketAnahtari = "etk" + String(i);
    String etiket = kalici.isKey(etiketAnahtari.c_str())
                        ? kalici.getString(etiketAnahtari.c_str())
                        : String(VARSAYILAN_ETIKETLER[i]);
    if (etiket.length() == 0) etiket = VARSAYILAN_ETIKETLER[i];
    strlcpy(ayarlar.etiketler[i], etiket.c_str(), sizeof(ayarlar.etiketler[i]));

    ayarlar.etiketRenkleri[i] =
        kalici.getUShort(("eRnk" + String(i)).c_str(), ayarlar.etiketRenkleri[i]);
    ayarlar.degerRenkleri[i] =
        kalici.getUShort(("dRnk" + String(i)).c_str(), ayarlar.degerRenkleri[i]);
  }

  // WiFi kimlik bilgileri. Kayit yoksa varsayilanlar kullanilir.
  // isKey() ile onceden bakilir: getString() olmayan anahtarda varsayilan
  // verilse bile seri porta NOT_FOUND hatasi yazar.
  String ad = kalici.isKey("apAdi") ? kalici.getString("apAdi")
                                    : String(VARSAYILAN_AP_ADI);
  String sifre = kalici.isKey("apSifre") ? kalici.getString("apSifre")
                                         : String(VARSAYILAN_AP_SIFRE);
  if (ad.length() == 0 || ad.length() > 32) ad = VARSAYILAN_AP_ADI;
  strlcpy(ayarlar.apAdi, ad.c_str(), sizeof(ayarlar.apAdi));
  strlcpy(ayarlar.apSifresi, sifre.c_str(), sizeof(ayarlar.apSifresi));

  kalici.end();
}

static void ayarlariKaydet() {
  kalici.begin("telpost", false); // yazilabilir
  kalici.putUInt("aralik", ayarlar.olcumAraligiMs);
  kalici.putFloat("suAlarm", ayarlar.suAlarmEsigi);
  kalici.putFloat("yagAlarm", ayarlar.yagAlarmEsigi);
  kalici.putUShort("cizgi", ayarlar.cizgiRengi);
  kalici.putBool("sessiz", ayarlar.sessizMod);

  uint32_t paket = 0;
  for (uint8_t i = 0; i < CEYREK_SAYISI; i++) {
    paket |= ((uint32_t)ayarlar.ceyrek[i]) << (i * 8);
  }
  kalici.putUInt("ceyrek", paket);

  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    kalici.putString(("etk" + String(i)).c_str(), ayarlar.etiketler[i]);
    kalici.putUShort(("eRnk" + String(i)).c_str(), ayarlar.etiketRenkleri[i]);
    kalici.putUShort(("dRnk" + String(i)).c_str(), ayarlar.degerRenkleri[i]);
  }

  kalici.putString("apAdi", ayarlar.apAdi);
  kalici.putString("apSifre", ayarlar.apSifresi);

  kalici.end();
}

// SSID kullanici tarafindan girildigi icin JSON'u bozabilecek karakterler kacirilir.
static String jsonMetin(const char *metin) {
  String cikti = "\"";
  for (const char *p = metin; *p; p++) {
    if (*p == '"' || *p == '\\') {
      cikti += '\\';
      cikti += *p;
    } else if ((uint8_t)*p < 0x20) {
      continue; // kontrol karakterlerini at
    } else {
      cikti += *p;
    }
  }
  cikti += '"';
  return cikti;
}

static String ayarlarJson() {
  String json = "{";
  json += "\"olcumAraligiMs\":" + String(ayarlar.olcumAraligiMs) + ',';
  json += "\"suAlarmEsigi\":" + String(ayarlar.suAlarmEsigi, 1) + ',';
  json += "\"yagAlarmEsigi\":" + String(ayarlar.yagAlarmEsigi, 1) + ',';
  json += "\"cizgiRengi\":" + String(ayarlar.cizgiRengi) + ',';
  json += "\"sessizMod\":";
  json += ayarlar.sessizMod ? "true" : "false";
  json += ',';
  json += "\"apAdi\":" + jsonMetin(ayarlar.apAdi) + ',';
  // Sifrenin kendisi gonderilmez; arayuz yalnizca ayarli olup olmadigini bilir.
  json += "\"apSifreliMi\":";
  json += (strlen(ayarlar.apSifresi) > 0) ? "true" : "false";
  json += ',';
  json += "\"ceyrek\":[";
  for (uint8_t i = 0; i < CEYREK_SAYISI; i++) {
    if (i) json += ',';
    json += String(ayarlar.ceyrek[i]);
  }
  json += "],";

  json += "\"etiketler\":[";
  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    if (i) json += ',';
    json += jsonMetin(ayarlar.etiketler[i]);
  }
  json += "],";

  json += "\"etiketRenkleri\":[";
  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    if (i) json += ',';
    json += String(ayarlar.etiketRenkleri[i]);
  }
  json += "],";

  json += "\"degerRenkleri\":[";
  for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
    if (i) json += ',';
    json += String(ayarlar.degerRenkleri[i]);
  }
  json += "]}";

  return json;
}

// Yeni baglanan istemciye tampondaki tum gecmisi tek seferde gonderir,
// boylece grafik bos baslamak yerine dolu gelir.
static String gecmisJson() {
  String json = "{\"tip\":\"gecmis\",";

  const char *adlar[4] = {"sicaklik", "basinc", "su", "yag"};
  for (uint8_t alan = 0; alan < 4; alan++) {
    json += '"';
    json += adlar[alan];
    json += "\":[";

    for (uint16_t i = 0; i < gecmisSayisi; i++) {
      const Olcum &o = gecmis[(gecmisBas + i) % GECMIS_KAPASITE];
      float deger = (alan == 0) ? o.sicaklik
                  : (alan == 1) ? o.basinc
                  : (alan == 2) ? o.su
                                : o.yag;
      if (i) json += ',';
      json += isnan(deger) ? "null" : String(deger, 1);
    }

    json += ']';
    if (alan < 3) json += ',';
  }

  json += '}';
  return json;
}

static void gecmiseEkle(const Olcum &olcum) {
  if (gecmisSayisi < GECMIS_KAPASITE) {
    gecmis[(gecmisBas + gecmisSayisi) % GECMIS_KAPASITE] = olcum;
    gecmisSayisi++;
  } else {
    // Tampon dolu: en eskinin uzerine yazip basi ilerlet
    gecmis[gecmisBas] = olcum;
    gecmisBas = (gecmisBas + 1) % GECMIS_KAPASITE;
  }
}

static void websoketOlayi(AsyncWebSocket *, AsyncWebSocketClient *istemci,
                          AwsEventType tip, void *, uint8_t *, size_t) {
  if (tip == WS_EVT_CONNECT) {
    istemci->text(gecmisJson());
  }
}

void webArayuzunuBaslat() {
  ayarlariYukle();

  WiFi.mode(WIFI_AP);
  // Sifre bos ise nullptr gecilir: WiFi kutuphanesi bunu sifresiz ag olarak kurar.
  bool acikAg = strlen(ayarlar.apSifresi) == 0;
  WiFi.softAP(ayarlar.apAdi, acikAg ? nullptr : ayarlar.apSifresi);

  // Kimlik bilgileri seri porta yazilir: web arayuzunden yanlis bir sifre
  // ayarlanip aga girilemedigi durumda USB uzerinden kurtarma yolu budur.
  Serial.print(F("Access point ready. Network: "));
  Serial.println(ayarlar.apAdi);
  Serial.print(F("Password: "));
  Serial.println(acikAg ? "(open network)" : ayarlar.apSifresi);
  portalAdresi = "http://" + WiFi.softAPIP().toString() + "/";
  Serial.print(F("Address: "));
  Serial.println(portalAdresi);

  // Tum alan adlarini cihaza cozerek captive portal davranisini saglar
  dnsSunucu.setErrorReplyCode(DNSReplyCode::NoError);
  dnsSunucu.start(DNS_PORTU, "*", WiFi.softAPIP());

  websoket.onEvent(websoketOlayi);
  sunucu.addHandler(&websoket);

  sunucu.on("/", HTTP_GET, [](AsyncWebServerRequest *istek) {
    istek->send(200, "text/html; charset=utf-8", WEB_SAYFA);
  });

  sunucu.on("/api/ayarlar", HTTP_GET, [](AsyncWebServerRequest *istek) {
    istek->send(200, "application/json", ayarlarJson());
  });

  sunucu.on("/api/ayarlar", HTTP_POST, [](AsyncWebServerRequest *istek) {
    if (istek->hasParam("olcumAraligiMs", true)) {
      uint32_t deger = istek->getParam("olcumAraligiMs", true)->value().toInt();
      // Cok kisa aralik sensorleri ve ekrani gereksiz yorar, cok uzun aralik
      // arayuzu olu gosterir; makul sinirlar icinde kisitlanir.
      ayarlar.olcumAraligiMs = constrain(deger, 500UL, 60000UL);
    }
    if (istek->hasParam("suAlarmEsigi", true)) {
      ayarlar.suAlarmEsigi = istek->getParam("suAlarmEsigi", true)->value().toFloat();
    }
    if (istek->hasParam("yagAlarmEsigi", true)) {
      ayarlar.yagAlarmEsigi = istek->getParam("yagAlarmEsigi", true)->value().toFloat();
    }

    // Onay kutusu isaretsizken tarayici alani hic gondermez; bu yuzden
    // varligi degil, formun sessizMod alani gonderip gondermedigi kontrol edilir.
    if (istek->hasParam("sessizModGonderildi", true)) {
      ayarlar.sessizMod = istek->hasParam("sessizMod", true) &&
                          istek->getParam("sessizMod", true)->value() == "1";
    }

    if (istek->hasParam("cizgiRengi", true)) {
      uint32_t renk = istek->getParam("cizgiRengi", true)->value().toInt();
      if (renk != ayarlar.cizgiRengi) {
        ayarlar.cizgiRengi = (uint16_t)renk;
        yerlesimDegisti = true;
      }
    }

    // Ceyrek atamalari: ceyrek0..ceyrek3, her biri bir OLCUM_* indeksi
    for (uint8_t i = 0; i < CEYREK_SAYISI; i++) {
      String ad = "ceyrek" + String(i);
      if (!istek->hasParam(ad, true)) continue;

      uint32_t deger = istek->getParam(ad, true)->value().toInt();
      if (deger < OLCUM_SAYISI && ayarlar.ceyrek[i] != deger) {
        ayarlar.ceyrek[i] = (uint8_t)deger;
        yerlesimDegisti = true;
      }
    }

    // Olcum basina etiket metni ve renkler
    for (uint8_t i = 0; i < OLCUM_SAYISI; i++) {
      String etiketAdi = "etiket" + String(i);
      if (istek->hasParam(etiketAdi, true)) {
        String metin = istek->getParam(etiketAdi, true)->value();
        metin.trim();
        // Bos birakilirsa varsayilana doner; boylece etiket kaybolmaz
        if (metin.length() == 0) metin = VARSAYILAN_ETIKETLER[i];
        if (metin != ayarlar.etiketler[i]) {
          strlcpy(ayarlar.etiketler[i], metin.c_str(), sizeof(ayarlar.etiketler[i]));
          yerlesimDegisti = true;
        }
      }

      String eRenkAdi = "etiketRengi" + String(i);
      if (istek->hasParam(eRenkAdi, true)) {
        uint16_t renk = (uint16_t)istek->getParam(eRenkAdi, true)->value().toInt();
        if (renk != ayarlar.etiketRenkleri[i]) {
          ayarlar.etiketRenkleri[i] = renk;
          yerlesimDegisti = true;
        }
      }

      String dRenkAdi = "degerRengi" + String(i);
      if (istek->hasParam(dRenkAdi, true)) {
        uint16_t renk = (uint16_t)istek->getParam(dRenkAdi, true)->value().toInt();
        if (renk != ayarlar.degerRenkleri[i]) {
          ayarlar.degerRenkleri[i] = renk;
          yerlesimDegisti = true;
        }
      }
    }

    ayarlariKaydet();
    istek->send(200, "application/json", ayarlarJson());
  });

  // WiFi kimlik bilgileri ayri bir uc noktada tutulur: kaydedildiginde cihaz
  // yeniden baslar, bu yuzden diger ayarlarla ayni istege karistirilmaz.
  sunucu.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *istek) {
    if (!istek->hasParam("apAdi", true)) {
      istek->send(400, "application/json", "{\"hata\":\"Network name is required\"}");
      return;
    }

    String ad = istek->getParam("apAdi", true)->value();
    ad.trim();
    if (ad.length() == 0 || ad.length() > 32) {
      istek->send(400, "application/json",
                  "{\"hata\":\"Network name must be 1-32 characters\"}");
      return;
    }

    bool acikAg = istek->hasParam("apAcikAg", true) &&
                  istek->getParam("apAcikAg", true)->value() == "1";

    String sifre = istek->hasParam("apSifresi", true)
                       ? istek->getParam("apSifresi", true)->value()
                       : String();

    if (acikAg) {
      sifre = ""; // Sifresiz ag istendi
    } else if (sifre.length() == 0) {
      // Bos birakildi: mevcut sifre korunur. Ancak mevcut sifre de yoksa
      // kullanici sifreli ag isteyip sifre vermemis demektir.
      if (strlen(ayarlar.apSifresi) == 0) {
        istek->send(400, "application/json",
                    "{\"hata\":\"Enter a password or choose an open network\"}");
        return;
      }
      sifre = ayarlar.apSifresi;
    } else if (sifre.length() < SIFRE_MIN || sifre.length() > SIFRE_MAX) {
      istek->send(400, "application/json",
                  "{\"hata\":\"Password must be 8-63 characters\"}");
      return;
    }

    bool degisti = (ad != ayarlar.apAdi) || (sifre != ayarlar.apSifresi);
    if (!degisti) {
      istek->send(200, "application/json", "{\"yenidenBaslatiliyor\":false}");
      return;
    }

    strlcpy(ayarlar.apAdi, ad.c_str(), sizeof(ayarlar.apAdi));
    strlcpy(ayarlar.apSifresi, sifre.c_str(), sizeof(ayarlar.apSifresi));
    ayarlariKaydet();

    istek->send(200, "application/json", "{\"yenidenBaslatiliyor\":true}");

    // Yanit tarayiciya ulassin diye kisa gecikme; yeniden baslatmayi webDongu() yapar.
    yenidenBaslatmaZamani = millis() + 1200;
  });

  // --- CAPTIVE PORTAL ---
  // Isletim sistemleri aga baglaninca bilinen bir adrese istek atip beklenen
  // yaniti alamazlarsa "aga giris yap" ekranini acar. Bu adreslerin her birine
  // panele yonlendirme donulur. Yalnizca onNotFound yeterli olmayabiliyor,
  // cunku bazi surumler yanit govdesini ve durum kodunu de denetliyor.
  auto portalaYonlendir = [](AsyncWebServerRequest *istek) {
    istek->redirect(portalAdresi);
  };

  sunucu.on("/generate_204", HTTP_GET, portalaYonlendir);          // Android
  sunucu.on("/gen_204", HTTP_GET, portalaYonlendir);               // Android
  sunucu.on("/hotspot-detect.html", HTTP_GET, portalaYonlendir);   // iOS / macOS
  sunucu.on("/library/test/success.html", HTTP_GET, portalaYonlendir);
  sunucu.on("/connecttest.txt", HTTP_GET, portalaYonlendir);       // Windows
  sunucu.on("/ncsi.txt", HTTP_GET, portalaYonlendir);              // Windows
  sunucu.on("/redirect", HTTP_GET, portalaYonlendir);              // Windows
  sunucu.on("/canonical.html", HTTP_GET, portalaYonlendir);        // Firefox
  sunucu.on("/success.txt", HTTP_GET, portalaYonlendir);           // Firefox

  sunucu.onNotFound(portalaYonlendir);

  sunucu.begin();
}

void webDongu() {
  dnsSunucu.processNextRequest();

  if (yenidenBaslatmaZamani != 0 && millis() >= yenidenBaslatmaZamani) {
    Serial.println(F("WiFi settings changed, restarting..."));
    Serial.flush();
    ESP.restart();
  }
}

void webOlcumBildir(float sicaklik, float basinc, float su, float yag) {
  Olcum olcum = {sicaklik, basinc, su, yag};
  gecmiseEkle(olcum);

  // Kopmus istemcilerin tamponlari birikmesin diye duzenli temizlik
  websoket.cleanupClients();

  if (websoket.count() == 0) {
    return; // Dinleyen yoksa JSON uretme zahmetine girme
  }

  String json = "{\"tip\":\"olcum\",";
  sayiEkle(json, "sicaklik", sicaklik, false);
  sayiEkle(json, "basinc", basinc, false);
  sayiEkle(json, "su", su, false);
  sayiEkle(json, "yag", yag, true);
  json += '}';

  websoket.textAll(json);
}
