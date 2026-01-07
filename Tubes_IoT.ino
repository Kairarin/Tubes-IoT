#define BLYNK_TEMPLATE_ID "TMPL6OvjFuKR0"
#define BLYNK_TEMPLATE_NAME "TubesIoT"
#define BLYNK_AUTH_TOKEN "Y3fyKrFKMTBpOrtxCHMYynU0O6cpY_jv"

#include <WiFi.h>
#include <DHT.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>

// ==== Pin ====
#define DHTPIN   4
#define DHTTYPE  DHT11
#define MQ_PIN   36     // ADC1 (sensor MQ-135)
#define GREEN_LED_PIN  5    // LED Hijau
#define RED_LED_PIN    18   // LED Merah (tambahan)
#define BUZZER_PIN     15   // Buzzer

// ==== WiFi ====
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "shaina35";

// ==== Google Apps Script Web App ID ====
String GOOGLE_SCRIPT_ID = "AKfycby22J5RuieeoPZI2E-d-2fuoDPsayzE-UnlAxQyw2i6CmNJ-31sngqjRwETR1KgQ8DR-g";

// ===== MQ sensitivity tuning =====
const int MQ_AVG_SAMPLES = 30;
float MQ_GAIN = 6.0;
int mqBaseline = 0;

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // alamat I2C biasanya 0x27, sesuaikan jika beda

// ==== Threshold asap ====
const float SMOKE_THRESHOLD = 1500;
const float HYSTERESIS = 50;

bool smokeAlert = false;

// Fungsi-fungsi readMQRawAvg, getGasIndex, getSmokeStatus, sendToSheet
// (sama seperti kode asli Anda, saya tidak ubah)

int readMQRawAvg(int n = MQ_AVG_SAMPLES) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    sum += analogRead(MQ_PIN);
    delay(5);
  }
  return (int)(sum / n);
}

float getGasIndex() {
  int raw = readMQRawAvg();
  int delta = raw - mqBaseline;
  if (delta < 0) delta = 0;
  float gasIndex = 350.0 + (delta * MQ_GAIN);
  if (gasIndex > 5000) gasIndex = 5000;

  Serial.print("MQ raw="); Serial.print(raw);
  Serial.print(" baseline="); Serial.print(mqBaseline);
  Serial.print(" delta="); Serial.print(delta);
  Serial.print(" gasIndex="); Serial.println(gasIndex);

  if (raw >= 4090) {
    Serial.println("WARNING: MQ ADC mentok (4095). Cek wiring!");
  }
  return gasIndex;
}

String getSmokeStatus(float gasVal) {
  if (!smokeAlert && gasVal >= SMOKE_THRESHOLD) {
    smokeAlert = true;
  } else if (smokeAlert && gasVal <= (SMOKE_THRESHOLD - HYSTERESIS)) {
    smokeAlert = false;
  }
  return smokeAlert ? "BAHAYA_ASAP" : "AMAN";
}

void sendToSheet(float t, float h, float gas, const String& status) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID +
               "/exec?suhu=" + String(t, 1) +
               "&lembab=" + String(h, 0) +
               "&gas=" + String(gas, 0) +
               "&status=" + status;
  http.begin(url);
  int code = http.GET();
  http.end();
  Serial.println(code == 200 || code == 302 ? "Sheet OK!" : "Sheet gagal: " + String(code));
}

void sendData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float gas = getGasIndex();

  if (isnan(t) || isnan(h)) {
    Serial.println("DHT GAGAL!");
    return;
  }

  String status = getSmokeStatus(gas);

  Serial.printf("Suhu: %.1fC | Lembab: %.0f%% | GasIndex: %.0f | Status: %s\n",
                t, h, gas, status.c_str());

  // Blynk
  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);
  Blynk.virtualWrite(V2, gas);
  Blynk.virtualWrite(V3, status);

  sendToSheet(t, h, gas, status);

  // Kontrol LED & Buzzer
  if (smokeAlert) {
    digitalWrite(GREEN_LED_PIN, LOW);   // Hijau mati
    digitalWrite(RED_LED_PIN, HIGH);    // Merah nyala
    digitalWrite(BUZZER_PIN, HIGH);     // Buzzer bunyi
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);  // Hijau nyala
    digitalWrite(RED_LED_PIN, LOW);     // Merah mati
    digitalWrite(BUZZER_PIN, LOW);      // Buzzer mati
  }

  // Tampilan LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S: ");
  lcd.print(t, 1);
  lcd.print("C  H: ");
  lcd.print(h, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("PPM: ");
  lcd.print(gas, 0);
  lcd.print(" ");
  lcd.print(status);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== FIRE/SMOKE IOT START ===");

  analogReadResolution(12);
  analogSetPinAttenuation(MQ_PIN, ADC_11db);

  Serial.println("MQ-135 warming up 30 detik...");
  delay(30000);

  mqBaseline = readMQRawAvg(50);
  Serial.print("MQ baseline: "); Serial.println(mqBaseline);

  dht.begin();

  // Inisialisasi pin baru
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  // WiFi & Blynk (sama seperti asli)
  Serial.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    Blynk.config(BLYNK_AUTH_TOKEN);
    if (Blynk.connect(5000)) {
      Serial.println("Blynk OK!");
    } else {
      Serial.println("Blynk FAIL");
    }
  } else {
    Serial.println("\nWiFi FAIL");
  }

  timer.setInterval(5000L, sendData);
  sendData();
}

void loop() {
  Blynk.run();
  timer.run();
}