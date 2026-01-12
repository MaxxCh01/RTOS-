#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FirebaseESP32.h>

/*================= WIFI / FIREBASE =================*/
#define WIFI_SSID     "MEGACABLE-2.4G-E7AB"
#define WIFI_PASSWORD "DfSAfsmm49"
#define FIREBASE_HOST "https://maxchqto-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "CPDP7ryL2W3e3KhSGQyyhAwbKc5oJ0KxPGrZPi35"

/*================= PINES =================*/
#define PIN_HEATER 23
#define PIN_FAN    16
#define PIN_TACHO  4
#define PIN_TEMP   34   // LM35
#define PIN_HUM    35   // AM1011A

// BOTONES (Usa pines que no interfieran con el arranque)
#define BTN_UP     12
#define BTN_DOWN   14

#define FAN_CHANNEL 0

/*================= OLED =================*/
Adafruit_SSD1306 display(128, 32, &Wire, -1);

/*================= ESTRUCTURAS PID =================*/
struct PID_t {
  float sp;
  float kp, ki, kd;
  float integral;
  float lastError;
  float out;
};

// Iniciamos con Setpoint en 0 (OFF)
PID_t heater = {0.0, 30.0, 0.1, 100.0, 0, 0, 0};

/*================= VARIABLES COMPARTIDAS =================*/
float sharedTemp = 0;
float sharedHum  = 0;
float fanPWM     = 0;
int   rpmFan     = 0;
volatile int pulses = 0;

SemaphoreHandle_t dataMutex;

/*================= FIREBASE =================*/
FirebaseData fbData;
FirebaseConfig fbConfig;
FirebaseAuth fbAuth;

/*================= ISR =================*/
void IRAM_ATTR isrTacho() {
  pulses++;
}

/*================= ALGORITMO PID =================*/
float computePID(PID_t &p, float input) {
  static unsigned long lastTime = millis();
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  if (dt <= 0) dt = 0.01;
  lastTime = now;

  float error = p.sp - input;
  p.integral += error * dt;
  p.integral = constrain(p.integral, -50, 50);

  float derivative = (error - p.lastError) / dt;
  p.lastError = error;

  p.out = p.kp * error + p.ki * p.integral + p.kd * derivative;
  p.out = constrain(p.out, 0, 255);

  return p.out;
}

/*================= CONTROL DIFUSO (VENTILADOR) =================*/
float controlFanDifuso(float h) {
  float h_Baja = 0, h_Media = 0, h_Alta = 0;

  // Fuzzificación
  if (h <= 40) h_Baja = 1.0;
  else if (h > 40 && h < 50) h_Baja = (50.0 - h) / 10.0;

  if (h > 40 && h <= 55) h_Media = (h - 40.0) / 15.0;
  else if (h > 55 && h < 70) h_Media = (70.0 - h) / 15.0;

  if (h >= 80) h_Alta = 1.0;
  else if (h > 60 && h < 80) h_Alta = (h - 60.0) / 20.0;

  // Defuzzificación (Centroide simplificado)
  float sumaPesos = (h_Baja * 0.0) + (h_Media * 130.0) + (h_Alta * 255.0);
  float sumaMembresia = h_Baja + h_Media + h_Alta;

  if (sumaMembresia == 0) return 0; 
  return sumaPesos / sumaMembresia;
}

/*================= TAREA BOTONES =================*/
void TaskButtons(void *pv) {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  while (1) {
    // Botón SUBIR
    if (digitalRead(BTN_UP) == LOW) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      heater.sp += 1.0;
      if (heater.sp > 50.0) heater.sp = 50.0;
      xSemaphoreGive(dataMutex);
      vTaskDelay(pdMS_TO_TICKS(200));
    }

    // Botón BAJAR
    if (digitalRead(BTN_DOWN) == LOW) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      heater.sp -= 1.0;
      if (heater.sp < 0.0) heater.sp = 0.0;
      xSemaphoreGive(dataMutex);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/*================= TAREA SENSORES =================*/
void TaskSensors(void *pv) {
  pinMode(PIN_TACHO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TACHO), isrTacho, FALLING);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);

  while (1) {
    float sumT = 0, sumH = 0;
    for (int i = 0; i < 15; i++) {
      sumT += analogRead(PIN_TEMP);
      sumH += analogRead(PIN_HUM);
      vTaskDelay(2);
    }
    float vT = (sumT / 15.0) * 3.3 / 4095.0;
    float tempC = vT * 100.0;
    float vH = (sumH / 15.0) * 3.3 / 4095.0;
    float humRH = constrain(vH / 0.03, 0, 100);

    static unsigned long lastRPM = 0;
    if (millis() - lastRPM >= 1000) {
      rpmFan = (pulses * 60) / 2;
      pulses = 0;
      lastRPM = millis();
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    sharedTemp = tempC;
    sharedHum  = humRH;
    xSemaphoreGive(dataMutex);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/*================= TAREA PID (CALEFACTOR) =================*/
void TaskPID(void *pv) {
  pinMode(PIN_HEATER, OUTPUT);
  unsigned long windowStart = millis();

  while (1) {
    float t, spActual;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    t = sharedTemp;
    spActual = heater.sp;
    xSemaphoreGive(dataMutex);

    if (spActual <= 0.5) {
      // MODO OFF
      digitalWrite(PIN_HEATER, LOW);
      heater.out = 0;
    } else {
      // MODO CONTROL
      float out = computePID(heater, t);
      if (millis() - windowStart > 1000) windowStart = millis();
      digitalWrite(PIN_HEATER, (millis() - windowStart < (out / 255.0) * 1000));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/*================= TAREA FAN (DIFUSO) =================*/
void TaskFan(void *pv) {
  ledcAttach(PIN_FAN, 25000, 8);
  while (1) {
    float h;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    h = sharedHum;
    xSemaphoreGive(dataMutex);

    fanPWM = controlFanDifuso(h);
    ledcWrite(FAN_CHANNEL, (int)fanPWM);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/*================= TAREA UI / OLED =================*/
void TaskUI(void *pv) {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  while (1) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    
    float t, h, spDisp;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    t = sharedTemp;
    h = sharedHum;
    spDisp = heater.sp;
    xSemaphoreGive(dataMutex);

    display.printf("T:%.1f C  H:%.1f%%\n", t, h);
    
    if (spDisp > 0) {
      display.printf("SETPOINT: %.0f C\n", spDisp);
    } else {
      display.printf("SETPOINT: OFF\n");
    }
    
    display.printf("FAN:%d%%  RPM:%d", (int)(fanPWM/2.55), rpmFan);

    display.display();
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

/*================= TAREA FIREBASE =================*/
void TaskFirebase(void *pv) {
  fbConfig.database_url = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  while (1) {
    if (Firebase.ready()) {
      FirebaseJson json;
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      json.set("temp", sharedTemp);
      json.set("hum", sharedHum);
      json.set("sp", heater.sp);
      json.set("fan", fanPWM);
      json.set("RPM", rpmFan);
      xSemaphoreGive(dataMutex);

      Firebase.updateNode(fbData, "/secadora/estado", json);
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

/*================= SETUP =================*/
void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  dataMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(TaskSensors, "SENS", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(TaskPID,     "PID",  4096, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(TaskFan,     "FAN",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskButtons, "BTNS", 2048, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(TaskUI,      "UI",   4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskFirebase,"FB",   10240,NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelete(NULL);
}