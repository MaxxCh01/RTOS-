/*
  Actividad 01 - Secuencia de mezclado

  Programa que simula un sistema de mezclado con FreeRTOS. Usa tareas para controlar 
  el llenado y vaciado de una cabina según el peso medido por una báscula. Los botones 
  aumentan el peso, la compuerta se abre al llegar a 100 kg y luego la cabina se drena, 
  mostrando cada etapa con LEDs indicadores.
*/

//configuracion de nucleos para utilizar
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define BTN1 12
#define BTN2 14
#define COMPUERTA 25
#define SENSOR2 33
#define DRENAJE 23

#define REVUELVE_LED 26   
#define SENSOR1_LED 27    

volatile int peso = 0;     
volatile int cabina = 0;   

void TaskBotones(void *pvParameters) {
  for (;;) {
    if (digitalRead(BTN1) == LOW) {
      peso += 50;
      Serial.printf("Botón1 presionado. Peso = %d kg\n", peso);
      vTaskDelay(300 / portTICK_PERIOD_MS); 
    }
    if (digitalRead(BTN2) == LOW) {
      peso += 50;
      Serial.printf("Botón2 presionado. Peso = %d kg\n", peso);
      vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void TaskBascula(void *pvParameters) {
  for (;;) {
    if (peso >= 100) {
      Serial.println("Peso alcanzó 100 kg → Abriendo compuerta...");
      digitalWrite(COMPUERTA, HIGH);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      digitalWrite(COMPUERTA, LOW);
      Serial.println("Compuerta cerrada. Cabina llenándose...");

      cabina = 200; 
      peso = 0;     
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void TaskCabina(void *pvParameters) {
  for (;;) {
    if (cabina >= 200) {
      Serial.println("Sensor2 activado → Drenando cabina...");
      digitalWrite(REVUELVE_LED, HIGH);  // Enciende el LED de revolver
      digitalWrite(DRENAJE, HIGH);
      vTaskDelay(3000 / portTICK_PERIOD_MS);
      digitalWrite(DRENAJE, LOW);
      digitalWrite(REVUELVE_LED, LOW);   // Apaga el LED después de drenar
      cabina = 0;
      Serial.println("Cabina drenada.");
    }

    // Si la cabina está vacía, prende el sensor1_LED
    if (cabina == 0) {
      digitalWrite(SENSOR1_LED, HIGH);
    } else {
      digitalWrite(SENSOR1_LED, LOW);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN1, INPUT);
  pinMode(BTN2, INPUT);
  pinMode(COMPUERTA, OUTPUT);
  pinMode(SENSOR2, INPUT);
  pinMode(DRENAJE, OUTPUT);
  pinMode(REVUELVE_LED, OUTPUT);  
  pinMode(SENSOR1_LED, OUTPUT);  

  xTaskCreate(TaskBotones, "Botones", 2048, NULL, 1, NULL);
  xTaskCreate(TaskBascula, "Bascula", 2048, NULL, 2, NULL);
  xTaskCreate(TaskCabina, "Cabina", 2048, NULL, 2, NULL);
}

void loop() {
  // No se usa en FreeRTOS
}
