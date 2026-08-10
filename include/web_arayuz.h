#pragma once

#include <Arduino.h>

// Web arayuzunun main.cpp'ye actigi arayuz.
// Sunucu asenkron calisir: istekleri kendi olay dongusunde isler, bu yuzden
// loop() icinde ayrica cagrilmasi gereken bir islev yoktur.

// Olcum turleri. Ekrandaki ceyreklere bu indeksler atanir.
enum : uint8_t {
  OLCUM_SICAKLIK = 0,
  OLCUM_BASINC,
  OLCUM_SU,
  OLCUM_YAG,
  OLCUM_SAYISI
};

// Ekran ceyrekleri (yerlesim sirasi).
enum : uint8_t {
  CEYREK_SOL_UST = 0,
  CEYREK_SAG_UST,
  CEYREK_SOL_ALT,
  CEYREK_SAG_ALT,
  CEYREK_SAYISI
};

// Ekran etiketi icin azami karakter sayisi.
// Sutun genisligi ~112 px, size 1 fontta karakter 6 px: 18 karakter sigar.
#define ETIKET_UZUNLUK 18

// Tarayicidan degistirilebilen, guc kesilse de korunan ayarlar.
struct Ayarlar {
  uint32_t olcumAraligiMs; // Sensor okuma araligi
  float suAlarmEsigi;      // Bu degerin ustunde su sicakligi alarm sayilir
  float yagAlarmEsigi;     // Bu degerin ustunde yag sicakligi alarm sayilir

  // Sessiz mod: alarm durumunda ekranda unlem cikar ama buzzer otmez.
  bool sessizMod;

  // Hangi ceyrekte hangi olcum gosterilecek. Indeks: CEYREK_*, deger: OLCUM_*
  uint8_t ceyrek[CEYREK_SAYISI];

  // Ekrandaki ayirici cizgilerin rengi (RGB565)
  uint16_t cizgiRengi;

  // Her olcum icin ekranda gorunen etiket metni ve renkler (RGB565).
  // Indeks: OLCUM_*. Etiket ve renkler olcume bagli oldugu icin olcum baska
  // bir ceyrege tasindiginda onunla birlikte tasinir.
  char etiketler[OLCUM_SAYISI][ETIKET_UZUNLUK + 1];
  uint16_t etiketRenkleri[OLCUM_SAYISI];
  uint16_t degerRenkleri[OLCUM_SAYISI];

  // WiFi erisim noktasi kimlik bilgileri.
  // apSifresi bos ise ag sifresiz (acik) yayin yapar.
  char apAdi[33];     // SSID en fazla 32 karakter
  char apSifresi[64]; // WPA2 sifresi 8-63 karakter, veya bos
};

// Kalici ayarlari NVS'ten yukler, WiFi erisim noktasini ve web sunucusunu baslatir.
void webArayuzunuBaslat();

// loop() icinden her turda cagrilmalidir. Captive portal DNS isteklerini
// isler ve WiFi ayari degistiginde planlanan yeniden baslatmayi yurutur.
// HTTP/WebSocket trafigi asenkron isledigi icin burada beklemez.
void webDongu();

// Yeni bir olcum setini web istemcilerine gonderir ve gecmis tamponuna ekler.
// DS18B20 okunamadiginda ilgili degere NAN gecilmelidir.
void webOlcumBildir(float sicaklik, float basinc, float su, float yag);

// main.cpp'nin olcum araligini ve yerlesimi okuyabilmesi icin guncel ayarlar.
const Ayarlar &webAyarlar();

// Yerlesim veya cizgi rengi degistiyse true doner ve bayragi sifirlar.
// main.cpp bunu gorunce ekran iskeletini yeniden cizer.
bool webYerlesimDegistiMi();

// Bir olcumun alarm esigini asip asmadigini soyler.
// Esigi tanimli olmayan olcumler (sicaklik, basinc) her zaman false doner.
bool webAlarmVar(uint8_t olcum, float deger);
