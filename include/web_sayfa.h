#pragma once

#include <Arduino.h>

// Tarayiciya sunulan tek sayfalik arayuz.
// Aracta internet baglantisi olmayacagi icin hicbir harici kaynak (CDN, font,
// grafik kutuphanesi) kullanilmaz; CSS, JS ve grafik cizimi sayfa icindedir.
// Grafik, ek kutuphane gerektirmemesi icin dogrudan canvas'a cizilir.
const char WEB_SAYFA[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Telpost Monitor</title>
<style>
  :root {
    --zemin: #0f1115;
    --kart: #1a1d24;
    --cizgi: #2a2f3a;
    --metin: #e6e9ef;
    --soluk: #9aa3b2;
    --sari: #ffd23f;
    --yesil: #3ddc84;
    --mavi: #4dc3ff;
    --turuncu: #ff9f45;
    --kirmizi: #ff5c5c;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 16px;
    background: var(--zemin); color: var(--metin);
    font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  }
  h1 { font-size: 18px; margin: 0 0 4px; }
  .durum { font-size: 13px; color: var(--soluk); margin-bottom: 8px; }
  .durum.kopuk { color: var(--kirmizi); }
  /* Sensor durumu: nokta yesilse veri geliyor, kirmizi ise okunamiyor */
  .sensorler { display: flex; gap: 12px; flex-wrap: wrap; margin-bottom: 16px; }
  .sensor { font-size: 12px; color: var(--soluk); }
  .sensor::before {
    content: ""; display: inline-block; width: 7px; height: 7px;
    border-radius: 50%; margin-right: 5px; vertical-align: middle;
    background: var(--cizgi);
  }
  .sensor.acik::before { background: var(--yesil); }
  .sensor.kapali::before { background: var(--kirmizi); }
  .sensor.kapali { color: var(--kirmizi); }
  .izgara {
    display: grid; grid-template-columns: repeat(2, 1fr);
    gap: 10px; margin-bottom: 18px;
  }
  .kutu {
    background: var(--kart); border: 1px solid var(--cizgi);
    border-radius: 10px; padding: 12px;
  }
  .kutu.alarm { border-color: var(--kirmizi); }
  .etiket { font-size: 12px; color: var(--soluk); text-transform: uppercase; letter-spacing: .5px; }
  .deger { font-size: 30px; font-weight: 600; margin-top: 4px; font-variant-numeric: tabular-nums; }
  .birim { font-size: 14px; color: var(--soluk); margin-left: 4px; font-weight: 400; }
  .s-sicaklik .deger { color: var(--sari); }
  .s-basinc  .deger { color: var(--yesil); }
  .s-su      .deger { color: var(--mavi); }
  .s-yag     .deger { color: var(--turuncu); }
  .rozet {
    display: none; margin-top: 6px; font-size: 11px; font-weight: 600;
    color: var(--kirmizi); text-transform: uppercase; letter-spacing: .5px;
  }
  .kutu.alarm .rozet { display: block; }
  h2 { font-size: 14px; color: var(--soluk); margin: 0 0 8px; font-weight: 600; }
  canvas { width: 100%; height: 180px; display: block; }
  .aciklama { display: flex; gap: 14px; flex-wrap: wrap; font-size: 12px; margin-top: 8px; color: var(--soluk); }
  /* Seri rengi ozellestirilebilir; JS --seri-renk degerini gunceller. */
  .aciklama span::before {
    content: ""; display: inline-block; width: 10px; height: 3px;
    border-radius: 2px; margin-right: 5px; vertical-align: middle;
    background: var(--seri-renk);
  }
  .a-sicaklik { --seri-renk: var(--sari); }
  .a-basinc   { --seri-renk: var(--yesil); }
  .a-su       { --seri-renk: var(--mavi); }
  .a-yag      { --seri-renk: var(--turuncu); }
  form { display: grid; gap: 12px; }
  label { display: block; font-size: 13px; color: var(--soluk); margin-bottom: 4px; }
  input {
    width: 100%; padding: 9px 10px; font-size: 15px;
    background: var(--zemin); color: var(--metin);
    border: 1px solid var(--cizgi); border-radius: 7px;
  }
  button {
    padding: 11px; font-size: 15px; font-weight: 600;
    background: var(--mavi); color: #08131b;
    border: 0; border-radius: 7px; cursor: pointer;
  }
  button:disabled { opacity: .6; cursor: default; }
  .bilgi { font-size: 13px; color: var(--yesil); min-height: 18px; }
  .ipucu { font-size: 12px; color: var(--soluk); margin: 0 0 12px; line-height: 1.45; }
  .yerlesim { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
  select {
    width: 100%; padding: 9px 10px; font-size: 15px;
    background: var(--zemin); color: var(--metin);
    border: 1px solid var(--cizgi); border-radius: 7px;
  }
  input[type="color"] { height: 42px; padding: 4px; cursor: pointer; }
  .uyari { color: var(--turuncu); }
  .onay { display: flex; align-items: center; gap: 8px; }
  .onay input { width: auto; }
  .onay label { margin: 0; }
  .bilgi.hata { color: var(--kirmizi); }
  .gorunum-satir {
    display: grid; grid-template-columns: 1fr auto auto;
    gap: 8px; align-items: end; margin-bottom: 12px;
  }
  .gorunum-satir .renk { width: 52px; }
  .gorunum-satir label { white-space: nowrap; }
  .grafik-baslik { display: flex; justify-content: space-between; align-items: center; gap: 10px; }
  .grafik-baslik h2 { margin: 0; }
  .grafik-baslik select { width: auto; padding: 5px 8px; font-size: 13px; }
  .aciklama span { white-space: nowrap; }
  .aralik { color: var(--soluk); opacity: .75; }
</style>
</head>
<body>
  <div class="durum" id="durum">Connecting...</div>
  <div class="sensorler">
    <span class="sensor" id="s-bmp">BMP280</span>
    <span class="sensor" id="s-su">Water</span>
    <span class="sensor" id="s-yag">Oil</span>
    <span class="sensor" id="s-ekran" title="Write-only SPI: cannot be detected, shows initialized state">Display</span>
    <span class="sensor" id="s-buzzer" title="Output only: cannot be detected, shows enabled state">Buzzer</span>
  </div>

  <div class="izgara">
    <div class="kutu s-sicaklik" id="k-sicaklik">
      <div class="etiket">Temperature</div>
      <div class="deger"><span id="v-sicaklik">--</span><span class="birim">&deg;C</span></div>
    </div>
    <div class="kutu s-basinc" id="k-basinc">
      <div class="etiket">Pressure</div>
      <div class="deger"><span id="v-basinc">--</span><span class="birim">hPa</span></div>
    </div>
    <div class="kutu s-su" id="k-su">
      <div class="etiket">Water</div>
      <div class="deger"><span id="v-su">--</span><span class="birim">&deg;C</span></div>
      <div class="rozet">Alarm</div>
    </div>
    <div class="kutu s-yag" id="k-yag">
      <div class="etiket">Oil</div>
      <div class="deger"><span id="v-yag">--</span><span class="birim">&deg;C</span></div>
      <div class="rozet">Alarm</div>
    </div>
  </div>

  <div class="kutu" style="margin-bottom:18px">
    <div class="grafik-baslik">
      <h2>History</h2>
      <select id="pencere" title="How many recent samples to show">
        <option value="30">Last 30</option>
        <option value="60">Last 60</option>
        <option value="120" selected>Last 120</option>
      </select>
    </div>
    <canvas id="grafik"></canvas>
    <div class="aciklama">
      <span class="a-sicaklik">Temperature</span>
      <span class="a-basinc">Pressure</span>
      <span class="a-su">Water</span>
      <span class="a-yag">Oil</span>
    </div>
    <p class="ipucu" style="margin:10px 0 0">
      Each series is scaled to its own range, so lines share the height. The
      span next to a name shows the minimum and maximum currently plotted.
    </p>
  </div>

  <div class="kutu">
    <h2>Settings</h2>
    <form id="form">
      <div>
        <label for="aralik">Measurement interval (ms)</label>
        <input type="number" id="aralik" min="500" max="60000" step="100" required>
      </div>
      <div>
        <label for="suAlarm">Water alarm threshold (&deg;C)</label>
        <input type="number" id="suAlarm" min="-40" max="200" step="0.5" required>
      </div>
      <div>
        <label for="yagAlarm">Oil alarm threshold (&deg;C)</label>
        <input type="number" id="yagAlarm" min="-40" max="200" step="0.5" required>
      </div>
      <div class="onay">
        <input type="checkbox" id="sessizMod">
        <label for="sessizMod">Silent mode (warning icon only, no buzzer)</label>
      </div>
      <button type="submit" id="kaydet">Save</button>
      <div class="bilgi" id="bilgi"></div>
    </form>
  </div>

  <div class="kutu" style="margin-top:18px">
    <h2>Display Layout</h2>
    <p class="ipucu">
      Choose which reading appears in each quadrant. Picking a reading that is
      already placed swaps the two quadrants.
    </p>
    <form id="yerlesimForm">
      <div class="yerlesim">
        <div>
          <label for="ceyrek0">Top left</label>
          <select id="ceyrek0"></select>
        </div>
        <div>
          <label for="ceyrek1">Top right</label>
          <select id="ceyrek1"></select>
        </div>
        <div>
          <label for="ceyrek2">Bottom left</label>
          <select id="ceyrek2"></select>
        </div>
        <div>
          <label for="ceyrek3">Bottom right</label>
          <select id="ceyrek3"></select>
        </div>
      </div>
      <div>
        <label for="cizgi">Divider line color</label>
        <input type="color" id="cizgi">
      </div>
      <button type="submit" id="yerlesimKaydet">Apply to Display</button>
      <div class="bilgi" id="yerlesimBilgi"></div>
    </form>
  </div>

  <div class="kutu" style="margin-top:18px">
    <h2>Labels &amp; Colors</h2>
    <p class="ipucu">
      Rename each reading and pick its label and value colors. Names are limited
      to 18 characters so they fit the display. Leave a name blank to restore
      its default.
    </p>
    <form id="gorunumForm">
      <div id="gorunumSatirlari"></div>
      <button type="submit" id="gorunumKaydet">Apply to Display</button>
      <div class="bilgi" id="gorunumBilgi"></div>
    </form>
  </div>

  <div class="kutu" style="margin-top:18px">
    <h2>Network</h2>
    <p class="ipucu uyari">
      Saving restarts the device. You will be disconnected and must rejoin using
      the new name and password. If you get locked out, connect over USB: the
      current credentials are printed to the serial monitor at startup.
    </p>
    <form id="agForm">
      <div>
        <label for="apAdi">Network name (SSID)</label>
        <input type="text" id="apAdi" maxlength="32" required>
      </div>
      <div class="onay">
        <input type="checkbox" id="apAcikAg">
        <label for="apAcikAg">Open network (no password)</label>
      </div>
      <div id="sifreAlani">
        <label for="apSifresi">Password</label>
        <input type="password" id="apSifresi" maxlength="63"
               placeholder="Leave blank to keep current password">
      </div>
      <button type="submit" id="agKaydet">Save &amp; Restart</button>
      <div class="bilgi" id="agBilgi"></div>
    </form>
  </div>

<script>
const $ = (id) => document.getElementById(id);
const gecmis = { sicaklik: [], basinc: [], su: [], yag: [] };
let ayarlar = { suAlarmEsigi: 95, yagAlarmEsigi: 110 };
// Grafik seri renkleri; ayarlar yuklenince cihazdaki deger renkleriyle degisir
let seriRenkleri = ["#ffd23f", "#3ddc84", "#4dc3ff", "#ff9f45"];

function yaz(alan, deger) {
  $("v-" + alan).textContent = (deger === null) ? "--" : deger.toFixed(1);
}

function alarmGuncelle(alan, deger, esik) {
  $("k-" + alan).classList.toggle("alarm", deger !== null && deger >= esik);
}

// Sensor okunamadiginda cihaz null gonderir; durum bundan cikarilir.
function sensorGuncelle(kimlik, calisiyor) {
  const oge = $(kimlik);
  oge.classList.toggle("acik", calisiyor);
  oge.classList.toggle("kapali", !calisiyor);
}

function olcumIsle(d) {
  yaz("sicaklik", d.sicaklik); yaz("basinc", d.basinc);
  yaz("su", d.su);             yaz("yag", d.yag);
  alarmGuncelle("su", d.su, ayarlar.suAlarmEsigi);
  alarmGuncelle("yag", d.yag, ayarlar.yagAlarmEsigi);

  // BMP280 hem sicaklik hem basinc verir; ikisi de gelmiyorsa sorun var
  sensorGuncelle("s-bmp", d.sicaklik !== null || d.basinc !== null);
  sensorGuncelle("s-su", d.su !== null);
  sensorGuncelle("s-yag", d.yag !== null);
}

// Baglanti koptugunda sensor durumu bilinemez: noktalar notr griye doner
function sensorleriBilinmezYap() {
  ["s-bmp", "s-su", "s-yag"].forEach((kimlik) => {
    $(kimlik).classList.remove("acik", "kapali");
  });
}

// --- Grafik: harici kutuphane olmadan canvas'a cizim ---
const tuval = $("grafik");
const ctx = tuval.getContext("2d");

function tuvalOlcekle() {
  const oran = window.devicePixelRatio || 1;
  tuval.width = tuval.clientWidth * oran;
  tuval.height = tuval.clientHeight * oran;
  ctx.setTransform(oran, 0, 0, oran, 0, 0);
  ciz();
}

// Her serinin en az bu kadarlik bir olcegi olur. Bu olmadan sabit duran bir
// deger, sensor gurultusu kadar (0.1 C) bir aralikta tum grafik yuksekligine
// yayilir ve ciddi bir dalgalanma varmis gibi gorunur.
const ASGARI_ARALIK = [2, 5, 2, 2]; // sicaklik C, basinc hPa, su C, yag C

// Her seri kendi min/max araliginda normalize edilir; boylece birimleri
// farkli olan basinc (hPa) ve sicakliklar (C) ayni grafikte okunabilir kalir.
// Cizilen aralik dondurulur ki acikamada gosterilebilsin.
function seriCiz(dizi, renk, g, y, asgariAralik) {
  const gecerli = dizi.filter((v) => v !== null);
  if (gecerli.length === 0) return null;

  let alt = Math.min(...gecerli), ust = Math.max(...gecerli);

  // Olcegi asgari araliga tamamla, veriyi ortada tut
  const yayilim = ust - alt;
  if (yayilim < asgariAralik) {
    const orta = (alt + ust) / 2;
    alt = orta - asgariAralik / 2;
    ust = orta + asgariAralik / 2;
  }

  if (gecerli.length < 2) return { alt, ust };

  ctx.beginPath();
  ctx.strokeStyle = renk;
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";

  let basladi = false;
  dizi.forEach((v, i) => {
    if (v === null) { basladi = false; return; }
    const x = (i / Math.max(1, dizi.length - 1)) * g;
    const py = y - 6 - ((v - alt) / (ust - alt)) * (y - 12);
    if (basladi) ctx.lineTo(x, py); else { ctx.moveTo(x, py); basladi = true; }
  });
  ctx.stroke();

  return { alt, ust };
}

function ciz() {
  const g = tuval.clientWidth, y = tuval.clientHeight;
  ctx.clearRect(0, 0, g, y);

  ctx.strokeStyle = "#2a2f3a";
  ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    const py = (y / 4) * i;
    ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(g, py); ctx.stroke();
  }

  // Yalnizca secilen pencere kadar son ornek cizilir
  const pencere = parseInt($("pencere").value, 10);
  const sonlar = (dizi) => dizi.slice(-pencere);

  ALAN_ADLARI.forEach((alan, i) => {
    const aralik = seriCiz(sonlar(gecmis[alan]), seriRenkleri[i], g, y,
                           ASGARI_ARALIK[i]);
    araligiGoster(alan, aralik);
  });
}

// Acikamada serinin adinin yanina o an cizilen alt-ust siniri yazar.
function araligiGoster(alan, aralik) {
  const oge = document.querySelector(".a-" + alan);
  if (!oge) return;

  const ad = oge.dataset.ad || oge.textContent;
  oge.dataset.ad = ad;

  oge.innerHTML = aralik
    ? ad + ' <span class="aralik">' + aralik.alt.toFixed(1) + " - " +
      aralik.ust.toFixed(1) + "</span>"
    : ad;
}

// --- WebSocket: olcumler sayfa yenilenmeden akar ---
let ws;
function baglan() {
  ws = new WebSocket("ws://" + location.host + "/ws");

  ws.onopen = () => {
    $("durum").textContent = "Connected";
    $("durum").classList.remove("kopuk");
  };

  ws.onclose = () => {
    $("durum").textContent = "Connection lost, retrying...";
    $("durum").classList.add("kopuk");
    sensorleriBilinmezYap();
    setTimeout(baglan, 2000);
  };

  ws.onmessage = (olay) => {
    const d = JSON.parse(olay.data);
    if (d.tip === "olcum") {
      olcumIsle(d);
      ["sicaklik", "basinc", "su", "yag"].forEach((a) => {
        gecmis[a].push(d[a]);
        if (gecmis[a].length > 120) gecmis[a].shift();
      });
      ciz();
    } else if (d.tip === "gecmis") {
      ["sicaklik", "basinc", "su", "yag"].forEach((a) => { gecmis[a] = d[a]; });
      ciz();
    }
  };
}

// --- Yerlesim ve renk ---
const OLCUM_ADLARI = ["Temperature", "Pressure", "Water", "Oil"];
const CEYREK_KIMLIKLERI = ["ceyrek0", "ceyrek1", "ceyrek2", "ceyrek3"];

// Cihaz renkleri RGB565 (16 bit) tutar; renk secici ise #RRGGBB bekler.
function rgb565ten(deger) {
  const r = ((deger >> 11) & 0x1F) * 255 / 31;
  const g = ((deger >> 5) & 0x3F) * 255 / 63;
  const b = (deger & 0x1F) * 255 / 31;
  const ikilik = (v) => Math.round(v).toString(16).padStart(2, "0");
  return "#" + ikilik(r) + ikilik(g) + ikilik(b);
}

function rgb565e(renk) {
  const r = parseInt(renk.slice(1, 3), 16);
  const g = parseInt(renk.slice(3, 5), 16);
  const b = parseInt(renk.slice(5, 7), 16);
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

function secenekleriDoldur() {
  CEYREK_KIMLIKLERI.forEach((kimlik) => {
    const secim = $(kimlik);
    secim.innerHTML = "";
    OLCUM_ADLARI.forEach((ad, i) => {
      const secenek = document.createElement("option");
      secenek.value = i;
      secenek.textContent = ad;
      secim.appendChild(secenek);
    });

    // Ayni olcum iki cevrekte gorunmesin: secilen olcum baska cevrekteyse
    // o cevrek, bu cevregin eski degerini alir (yer degistirme).
    secim.addEventListener("change", () => {
      const yeni = secim.value;
      CEYREK_KIMLIKLERI.forEach((digerKimlik) => {
        if (digerKimlik === kimlik) return;
        const diger = $(digerKimlik);
        if (diger.value === yeni) diger.value = secim.dataset.oncekiDeger;
      });
      CEYREK_KIMLIKLERI.forEach((k) => { $(k).dataset.oncekiDeger = $(k).value; });
    });
  });
}

function yerlesimiGoster(y) {
  CEYREK_KIMLIKLERI.forEach((kimlik, i) => {
    $(kimlik).value = y.ceyrek[i];
    $(kimlik).dataset.oncekiDeger = y.ceyrek[i];
  });
  $("cizgi").value = rgb565ten(y.cizgiRengi);
}

// --- Ayarlar ---
async function ayarlariYukle() {
  const y = await (await fetch("/api/ayarlar")).json();
  ayarlar = y;
  $("aralik").value = y.olcumAraligiMs;
  $("suAlarm").value = y.suAlarmEsigi;
  $("yagAlarm").value = y.yagAlarmEsigi;
  $("sessizMod").checked = y.sessizMod;

  // Ekran ve buzzer geri bildirim vermeyen ciktilar; rozetler "bagli mi"
  // degil, yapilandirilmis durumu gosterir.
  sensorGuncelle("s-ekran", true);
  sensorGuncelle("s-buzzer", !y.sessizMod);
  $("s-buzzer").textContent = y.sessizMod ? "Buzzer (silent)" : "Buzzer";

  yerlesimiGoster(y);
  gorunumuUygula(y);
  agiGoster(y);
}

$("yerlesimForm").addEventListener("submit", async (olay) => {
  olay.preventDefault();
  $("yerlesimKaydet").disabled = true;
  $("yerlesimBilgi").textContent = "";

  const govde = new URLSearchParams({ cizgiRengi: rgb565e($("cizgi").value) });
  CEYREK_KIMLIKLERI.forEach((kimlik, i) => govde.append("ceyrek" + i, $(kimlik).value));

  try {
    const cevap = await fetch("/api/ayarlar", { method: "POST", body: govde });
    if (!cevap.ok) throw new Error();
    ayarlar = await cevap.json();
    yerlesimiGoster(ayarlar);
    $("yerlesimBilgi").textContent = "Applied to display.";
  } catch {
    $("yerlesimBilgi").textContent = "Apply failed.";
  } finally {
    $("yerlesimKaydet").disabled = false;
  }
});

$("form").addEventListener("submit", async (olay) => {
  olay.preventDefault();
  $("kaydet").disabled = true;
  $("bilgi").textContent = "";

  // sessizModGonderildi: isaretsiz onay kutusu tarayici tarafindan hic
  // gonderilmedigi icin, cihazin "kapatildi" ile "gonderilmedi" ayrimini
  // yapabilmesi gerekiyor.
  const govde = new URLSearchParams({
    olcumAraligiMs: $("aralik").value,
    suAlarmEsigi: $("suAlarm").value,
    yagAlarmEsigi: $("yagAlarm").value,
    sessizModGonderildi: "1",
    sessizMod: $("sessizMod").checked ? "1" : "0",
  });

  try {
    const cevap = await fetch("/api/ayarlar", { method: "POST", body: govde });
    if (!cevap.ok) throw new Error();
    ayarlar = await cevap.json();
    $("bilgi").textContent = "Saved.";
  } catch {
    $("bilgi").textContent = "Save failed.";
  } finally {
    $("kaydet").disabled = false;
  }
});

// --- Etiket ve renkler ---
// Olcum sirasi cihazdaki OLCUM_* ile ayni: sicaklik, basinc, su, yag
const ALAN_ADLARI = ["sicaklik", "basinc", "su", "yag"];

function gorunumSatirlariniOlustur() {
  const kap = $("gorunumSatirlari");
  kap.innerHTML = "";

  ALAN_ADLARI.forEach((alan, i) => {
    const satir = document.createElement("div");
    satir.className = "gorunum-satir";
    satir.innerHTML =
      '<div>' +
        '<label for="etiket' + i + '">Name</label>' +
        '<input type="text" id="etiket' + i + '" maxlength="18">' +
      '</div>' +
      '<div>' +
        '<label for="etiketRengi' + i + '">Label</label>' +
        '<input type="color" class="renk" id="etiketRengi' + i + '">' +
      '</div>' +
      '<div>' +
        '<label for="degerRengi' + i + '">Value</label>' +
        '<input type="color" class="renk" id="degerRengi' + i + '">' +
      '</div>';
    kap.appendChild(satir);
  });
}

// Cihazdaki ozellestirmeleri web kartlarina da uygular: ekran ve panel
// ayni gorunumu paylasir.
function gorunumuUygula(y) {
  ALAN_ADLARI.forEach((alan, i) => {
    const ad = y.etiketler[i];
    const etiketRenk = rgb565ten(y.etiketRenkleri[i]);
    const degerRenk = rgb565ten(y.degerRenkleri[i]);

    $("etiket" + i).value = ad;
    $("etiketRengi" + i).value = etiketRenk;
    $("degerRengi" + i).value = degerRenk;

    const kutu = $("k-" + alan);
    const baslik = kutu.querySelector(".etiket");
    baslik.textContent = ad;
    baslik.style.color = etiketRenk;
    kutu.querySelector(".deger").style.color = degerRenk;

    // Grafik acikamasi ve yerlesim menuleri de ayni adlari kullansin.
    // dataset.ad, araligiGoster() icin temel ad olarak saklanir.
    const aciklama = document.querySelector(".a-" + alan);
    aciklama.dataset.ad = ad;
    aciklama.textContent = ad;
    aciklama.style.setProperty("--seri-renk", degerRenk);

    OLCUM_ADLARI[i] = ad;
  });

  CEYREK_KIMLIKLERI.forEach((kimlik) => {
    const secim = $(kimlik);
    Array.from(secim.options).forEach((secenek, i) => {
      secenek.textContent = OLCUM_ADLARI[i];
    });
  });

  // Sensor rozetleri de ozellestirilmis isimleri gostersin
  $("s-su").textContent = y.etiketler[2];
  $("s-yag").textContent = y.etiketler[3];

  // Grafik cizgileri de ayarlanan deger renklerini kullansin
  seriRenkleri = y.degerRenkleri.map(rgb565ten);
  ciz();
}

$("gorunumForm").addEventListener("submit", async (olay) => {
  olay.preventDefault();
  $("gorunumKaydet").disabled = true;
  $("gorunumBilgi").textContent = "";

  const govde = new URLSearchParams();
  ALAN_ADLARI.forEach((alan, i) => {
    govde.append("etiket" + i, $("etiket" + i).value);
    govde.append("etiketRengi" + i, rgb565e($("etiketRengi" + i).value));
    govde.append("degerRengi" + i, rgb565e($("degerRengi" + i).value));
  });

  try {
    const cevap = await fetch("/api/ayarlar", { method: "POST", body: govde });
    if (!cevap.ok) throw new Error();
    ayarlar = await cevap.json();
    gorunumuUygula(ayarlar);
    $("gorunumBilgi").textContent = "Applied to display.";
  } catch {
    $("gorunumBilgi").textContent = "Apply failed.";
  } finally {
    $("gorunumKaydet").disabled = false;
  }
});

// --- Ag ayarlari ---
// Acik ag secilince sifre alani gizlenir; kapatilinca geri gelir.
$("apAcikAg").addEventListener("change", () => {
  $("sifreAlani").style.display = $("apAcikAg").checked ? "none" : "";
});

function agiGoster(y) {
  $("apAdi").value = y.apAdi;
  $("apAcikAg").checked = !y.apSifreliMi;
  $("sifreAlani").style.display = y.apSifreliMi ? "" : "none";
  $("apSifresi").value = "";
}

$("agForm").addEventListener("submit", async (olay) => {
  olay.preventDefault();
  $("agKaydet").disabled = true;
  $("agBilgi").classList.remove("hata");
  $("agBilgi").textContent = "";

  const govde = new URLSearchParams({
    apAdi: $("apAdi").value,
    apAcikAg: $("apAcikAg").checked ? "1" : "0",
    apSifresi: $("apSifresi").value,
  });

  try {
    const cevap = await fetch("/api/wifi", { method: "POST", body: govde });
    const sonuc = await cevap.json();

    if (!cevap.ok) {
      $("agBilgi").classList.add("hata");
      $("agBilgi").textContent = sonuc.hata || "Could not save.";
      return;
    }

    $("agBilgi").textContent = sonuc.yenidenBaslatiliyor
      ? "Saved. Restarting - reconnect using the new settings."
      : "No changes.";
  } catch {
    $("agBilgi").classList.add("hata");
    $("agBilgi").textContent = "Could not save.";
  } finally {
    $("agKaydet").disabled = false;
  }
});

window.addEventListener("resize", tuvalOlcekle);
$("pencere").addEventListener("change", ciz);
tuvalOlcekle();
secenekleriDoldur();
gorunumSatirlariniOlustur();
ayarlariYukle();
baglan();
</script>
</body>
</html>
)HTML";
