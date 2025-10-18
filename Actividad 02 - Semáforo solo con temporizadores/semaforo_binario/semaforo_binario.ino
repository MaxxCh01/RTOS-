/*
  Actividad 02 - Semáforo solo con temporizadores

  Programa que simula el funcionamiento de un semáforo utilizando únicamente temporizadores 
  de FreeRTOS. Cada color (rojo, verde y amarillo) se controla mediante un temporizador que 
  define su tiempo de encendido. El LED amarillo parpadea tres veces antes de volver al rojo, 
  creando un ciclo continuo sin usar tareas, solo con temporización.
*/

#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define LED_ROJO     25
#define LED_AMARILLO 26
#define LED_VERDE    27

TimerHandle_t timerRojo;
TimerHandle_t timerVerde;
TimerHandle_t timerAmarillo;

volatile int blinkCount = 0;
void startRojo(TimerHandle_t xTimer);
void startVerde(TimerHandle_t xTimer);
void startAmarillo(TimerHandle_t xTimer);

void setup() {
  Serial.begin(115200);

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);

  timerRojo     = xTimerCreate("Rojo",     pdMS_TO_TICKS(2500), pdFALSE, (void*)0, startRojo);
  timerVerde    = xTimerCreate("Verde",    pdMS_TO_TICKS(2500), pdFALSE, (void*)0, startVerde);
  timerAmarillo = xTimerCreate("Amarillo", pdMS_TO_TICKS(500),  pdFALSE, (void*)0, startAmarillo);


  xTimerStart(timerRojo, 0);
}

void loop() {

}




void startRojo(TimerHandle_t xTimer) {
  digitalWrite(LED_ROJO, HIGH);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARILLO, LOW);

  Serial.println("LED ROJO encendido");


  xTimerStart(timerVerde, 0);
}

void startVerde(TimerHandle_t xTimer) {
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AMARILLO, LOW);

  Serial.println("LED VERDE encendido");

  blinkCount = 0;
  xTimerStart(timerAmarillo, 0);
}

void startAmarillo(TimerHandle_t xTimer) {
  static bool estado = false;

  if (!estado) {
    digitalWrite(LED_AMARILLO, HIGH);
    Serial.println("LED AMARILLO ON");
    estado = true;
  } else {
    digitalWrite(LED_AMARILLO, LOW);
    Serial.println("LED AMARILLO OFF");
    estado = false;
    blinkCount++;
  }

  if (blinkCount < 3) {

    xTimerStart(timerAmarillo, 0);
  } else {

    xTimerStart(timerRojo, 0);
  }
}

