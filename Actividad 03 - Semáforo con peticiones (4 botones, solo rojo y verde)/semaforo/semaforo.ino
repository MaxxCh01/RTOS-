#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

const int ROJO_1 = 19;
const int VERDE_1 = 18;
const int ROJO_2 = 21;
const int VERDE_2 = 5;
const int BTN_1 = 25;
const int BTN_2 = 26;
const int SENSOR_1 = 23;
const int SENSOR_2 = 22;
static SemaphoreHandle_t semaforoAcceso;

volatile int turno = 0;           // 0 = libre, 1 = semáforo 1, 2 = semáforo 2
volatile int turnoPendiente = 0;  // 0 = ninguno, 1 = espera 1, 2 = espera 2

void tareaB1(void *parameter);
void tareaB2(void *parameter);
void tareaSemaforo1(void *parameter);
void tareaSemaforo2(void *parameter);

void setup() {
  Serial.begin(115200);
  pinMode(ROJO_1, OUTPUT);
  pinMode(VERDE_1, OUTPUT);
  pinMode(ROJO_2, OUTPUT);
  pinMode(VERDE_2, OUTPUT);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);
  digitalWrite(ROJO_1, HIGH);
  digitalWrite(VERDE_1, LOW);
  digitalWrite(ROJO_2, HIGH);
  digitalWrite(VERDE_2, LOW);

  semaforoAcceso = xSemaphoreCreateBinary();
  xSemaphoreGive(semaforoAcceso);  

  // Tareas
  xTaskCreatePinnedToCore(tareaB1, "Boton1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaB2, "Boton2", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo1, "Semaforo1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo2, "Semaforo2", 2048, NULL, 1, NULL, app_cpu);
}

void loop() {}

void tareaB1(void *parameter) {
  bool lastState = HIGH;
  while (1) {
    bool currentState = digitalRead(BTN_1);
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS); 
      if (digitalRead(BTN_1) == LOW) {
        if (turno == 0) { 
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {
            turno = 1;
            Serial.println("Semáforo 1 ACTIVADO");
          }
        } else if (turno != 1) {  
          turnoPendiente = 1;
          Serial.println("Semáforo 1 EN ESPERA");
        }
      }
    }
    lastState = currentState;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void tareaB2(void *parameter) {
  bool lastState = HIGH;
  while (1) {
    bool currentState = digitalRead(BTN_2);
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if (digitalRead(BTN_2) == LOW) {
        if (turno == 0) {
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {
            turno = 2;
            Serial.println("Semáforo 2 ACTIVADO");
          }
        } else if (turno != 2) {
          turnoPendiente = 2;
          Serial.println("Semáforo 2 EN ESPERA");
        }
      }
    }
    lastState = currentState;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void tareaSemaforo1(void *parameter) {
  while (1) {
    if (turno == 1) {

      digitalWrite(ROJO_1, LOW);
      digitalWrite(VERDE_1, HIGH);
      digitalWrite(ROJO_2, HIGH);
      digitalWrite(VERDE_2, LOW);
      Serial.println("Semáforo 1 EN VERDE");

      while (digitalRead(SENSOR_1) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      Serial.println("Sensor 1 detectó salida → Liberando control");
      digitalWrite(VERDE_1, LOW);
      digitalWrite(ROJO_1, HIGH);
      if (turnoPendiente == 2) {
        turnoPendiente = 0;
        turno = 2;
        Serial.println("Cambio automático a Semáforo 2");
      } else {
        turno = 0;
        xSemaphoreGive(semaforoAcceso);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void tareaSemaforo2(void *parameter) {
  while (1) {
    if (turno == 2) {
      digitalWrite(ROJO_2, LOW);
      digitalWrite(VERDE_2, HIGH);
      digitalWrite(ROJO_1, HIGH);
      digitalWrite(VERDE_1, LOW);
      Serial.println("Semáforo 2 EN VERDE");
      while (digitalRead(SENSOR_2) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      Serial.println("Sensor 2 detectó salida → Liberando control");
      digitalWrite(VERDE_2, LOW);
      digitalWrite(ROJO_2, HIGH);
      if (turnoPendiente == 1) {
        turnoPendiente = 0;
        turno = 1;
        Serial.println("Cambio automático a Semáforo 1");
      } else {
        turno = 0;
        xSemaphoreGive(semaforoAcceso);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}