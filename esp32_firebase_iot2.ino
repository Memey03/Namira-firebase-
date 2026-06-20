/*
 * =====================================================
 *  IoT Firebase - ESP32 + DHT11 + 4 Relay
 *  Firebase Realtime Database (Auth: Database Secret)
 * 
 *  Pin Configuration:
 *  - DHT11  : GPIO 4
 *  - Relay1 : GPIO 23
 *  - Relay2 : GPIO 19
 *  - Relay3 : GPIO 18
 *  - Relay4 : GPIO 5
 * =====================================================
 * 
 *  Library yang dibutuhkan (Install via Library Manager):
 *  1. Firebase ESP Client  -> by Mobizt
 *  2. DHT sensor library   -> by Adafruit
 *  3. Adafruit Unified Sensor -> by Adafruit
 * =====================================================
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// =====================================================
//  KONFIGURASI - SESUAIKAN BAGIAN INI
// =====================================================

// WiFi
#define WIFI_SSID       "hotspot@unama"
#define WIFI_PASSWORD   ""

// Firebase
#define DATABASE_URL    "https://namira-firebase-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SECRET "eAvP2GCBpC57rNNcowrgEQBVIVS48vakzry439oU"

// Pin DHT11
#define DHT_PIN         4
#define DHT_TYPE        DHT11

// Pin Relay (active LOW: relay nyala saat pin LOW)
#define RELAY1_PIN      23
#define RELAY2_PIN      19
#define RELAY3_PIN      18
#define RELAY4_PIN      5

// Interval kirim data sensor (millisecond)
#define SEND_INTERVAL   3000  // 3 detik

// =====================================================
//  INISIALISASI OBJEK
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================================================
//  VARIABEL GLOBAL
// =====================================================

unsigned long lastSendTime = 0;

// Status relay lokal (untuk deteksi perubahan)
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

// Mode animasi
int animasiMode = 0;
unsigned long lastAnimasiTime = 0;
int animasiStep = 0;

// =====================================================
//  SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 IoT Firebase (Secret Auth) ===");

  // --- Inisialisasi Pin Relay ---
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Matikan semua relay saat boot
  setRelay(1, false);
  setRelay(2, false);
  setRelay(3, false);
  setRelay(4, false);

  // --- Inisialisasi DHT11 ---
  dht.begin();

  // --- Koneksi WiFi ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --- Konfigurasi Firebase dengan Database Secret ---
  // Tidak perlu signUp/signIn - langsung pakai secret sebagai token
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  // Mulai Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Tambah timeout untuk jaringan lambat
  config.timeout.serverResponse = 10 * 1000;  // 10 detik

  Serial.println("Firebase siap!");
  Serial.println("=========================================\n");
}

// =====================================================
//  LOOP UTAMA
// =====================================================

void loop() {
  if (Firebase.ready()) {

    // 1. Kirim data sensor setiap SEND_INTERVAL
    kirimDataSensor();

    // 2. Baca status relay dari Firebase
    bacaRelay();

    // 3. Baca mode animasi dari Firebase
    bacaAnimasi();

    // 4. Jalankan animasi jika mode aktif
    jalankanAnimasi();
  }
}

// =====================================================
//  FUNGSI: SET RELAY
//  Active LOW: true(ON) = pin LOW, false(OFF) = pin HIGH
// =====================================================

void setRelay(int nomor, bool status) {
  int pin;
  switch (nomor) {
    case 1: pin = RELAY1_PIN; relay1State = status; break;
    case 2: pin = RELAY2_PIN; relay2State = status; break;
    case 3: pin = RELAY3_PIN; relay3State = status; break;
    case 4: pin = RELAY4_PIN; relay4State = status; break;
    default: return;
  }
  digitalWrite(pin, status ? LOW : HIGH);
  Serial.printf("Relay %d -> %s\n", nomor, status ? "ON" : "OFF");
}

// =====================================================
//  FUNGSI: KIRIM DATA SENSOR KE FIREBASE
// =====================================================

void kirimDataSensor() {
  unsigned long now = millis();
  if (now - lastSendTime < SEND_INTERVAL) return;
  lastSendTime = now;

  float suhu = dht.readTemperature();
  float kelembaban = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembaban)) {
    Serial.println("ERROR: Gagal membaca DHT11!");
    return;
  }

  Serial.printf("Sensor -> Suhu: %.1f C | Kelembaban: %.1f%%\n", suhu, kelembaban);

  // Kirim suhu
  if (Firebase.RTDB.setFloat(&fbdo, "/sensor/suhu", suhu)) {
    Serial.println("  Suhu terkirim");
  } else {
    Serial.printf("  Gagal kirim suhu: %s\n", fbdo.errorReason().c_str());
  }

  // Kirim kelembaban
  if (Firebase.RTDB.setFloat(&fbdo, "/sensor/kelembaban", kelembaban)) {
    Serial.println("  Kelembaban terkirim");
  } else {
    Serial.printf("  Gagal kirim kelembaban: %s\n", fbdo.errorReason().c_str());
  }
}

// =====================================================
//  FUNGSI: BACA STATUS RELAY DARI FIREBASE
// =====================================================

void bacaRelay() {
  bool statusBaru;

  if (Firebase.RTDB.getBool(&fbdo, "/relay/relay1")) {
    statusBaru = fbdo.boolData();
    if (statusBaru != relay1State) setRelay(1, statusBaru);
  }

  if (Firebase.RTDB.getBool(&fbdo, "/relay/relay2")) {
    statusBaru = fbdo.boolData();
    if (statusBaru != relay2State) setRelay(2, statusBaru);
  }

  if (Firebase.RTDB.getBool(&fbdo, "/relay/relay3")) {
    statusBaru = fbdo.boolData();
    if (statusBaru != relay3State) setRelay(3, statusBaru);
  }

  if (Firebase.RTDB.getBool(&fbdo, "/relay/relay4")) {
    statusBaru = fbdo.boolData();
    if (statusBaru != relay4State) setRelay(4, statusBaru);
  }
}

// =====================================================
//  FUNGSI: BACA MODE ANIMASI DARI FIREBASE
// =====================================================

void bacaAnimasi() {
  if (Firebase.RTDB.getInt(&fbdo, "/animasi/mode")) {
    int modeBaru = fbdo.intData();
    if (modeBaru != animasiMode) {
      animasiMode = modeBaru;
      animasiStep = 0;
      Serial.printf("Mode animasi -> %d\n", animasiMode);

      // Reset semua relay saat ganti mode ke manual
      if (animasiMode == 0) {
        setRelay(1, false); setRelay(2, false);
        setRelay(3, false); setRelay(4, false);
        Firebase.RTDB.setBool(&fbdo, "/relay/relay1", false);
        Firebase.RTDB.setBool(&fbdo, "/relay/relay2", false);
        Firebase.RTDB.setBool(&fbdo, "/relay/relay3", false);
        Firebase.RTDB.setBool(&fbdo, "/relay/relay4", false);
      }
    }
  }
}

// =====================================================
//  FUNGSI: JALANKAN ANIMASI
//  Variasi 1 (mode=1): Kedip bergantian relay 1&3 vs 2&4
//  Variasi 2 (mode=2): Running light relay 1->2->3->4->ulang
// =====================================================

void jalankanAnimasi() {
  if (animasiMode == 0) return;

  unsigned long now = millis();

  // --- VARIASI 1: Kedip Bergantian (interval 500ms) ---
  if (animasiMode == 1) {
    if (now - lastAnimasiTime < 500) return;
    lastAnimasiTime = now;

    if (animasiStep % 2 == 0) {
      setRelay(1, true);  setRelay(2, false);
      setRelay(3, true);  setRelay(4, false);
    } else {
      setRelay(1, false); setRelay(2, true);
      setRelay(3, false); setRelay(4, true);
    }
    animasiStep++;
  }

  // --- VARIASI 2: Running Light (interval 300ms) ---
  else if (animasiMode == 2) {
    if (now - lastAnimasiTime < 300) return;
    lastAnimasiTime = now;

    // Matikan semua dulu
    setRelay(1, false); setRelay(2, false);
    setRelay(3, false); setRelay(4, false);

    // Nyalakan satu relay berurutan
    int relayAktif = (animasiStep % 4) + 1;
    setRelay(relayAktif, true);
    animasiStep++;
  }
}
