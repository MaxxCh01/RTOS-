/*
  Actividad 04 - Examen 01 Simón dice

  Programa que implementa el juego "Simón dice" en un ESP32 utilizando FreeRTOS. 
  El sistema genera secuencias aleatorias de teclas mostradas en una pantalla OLED, 
  que el usuario debe repetir usando un teclado matricial. Se emplean semáforos y 
  mutex para sincronizar las tareas de generación de secuencia y captura de entrada, 
  controlando turnos, validación y reinicio del juego en caso de error.
*/

#include <Arduino.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {32, 33, 15, 2};
byte colPins[COLS] = {4, 16, 17, 5}; 

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

SemaphoreHandle_t sem_inputReady;   
SemaphoreHandle_t sem_sequenceNext; 
SemaphoreHandle_t mutex_sequence;  

std::vector<char> sequence; 
size_t userIndex = 0;

const size_t MAX_SEQUENCE_LENGTH = ROWS * COLS; 
const uint32_t DISPLAY_DELAY_MS = 700;        
const uint32_t BETWEEN_DISPLAYS_MS = 200;    
const uint32_t PER_KEY_TIMEOUT_MS = 5000;     

TaskHandle_t TareaSequenceHandle = NULL;
TaskHandle_t TareaInputHandle = NULL;



char randomKeyFromPad() {
  uint32_t r = esp_random();
  size_t idx = r % (ROWS * COLS);
  size_t row = idx / COLS;
  size_t col = idx % COLS;
  return keys[row][col];
}

// ------------------------------------------------------
// Muestra texto centrado en la OLED
void showCenteredText(const String &line1, const String &line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;

  // Línea 1 centrada
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  int x_center1 = (SCREEN_WIDTH - w) / 2;
  int y_center1 = 8; // línea superior
  display.setCursor(x_center1, y_center1);
  display.print(line1);

  // Línea 2 centrada
  if (line2 != "") {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int x_center2 = (SCREEN_WIDTH - w) / 2;
    int y_center2 = 20; // línea inferior
    display.setCursor(x_center2, y_center2);
    display.print(line2);
  }

  display.display();
}

// ------------------------------------------------------

void displaySequenceOnOLED(const std::vector<char> &seq) {
  for (size_t i = 0; i < seq.size(); ++i) {
    String s = String(seq[i]);
    showCenteredText("Simon Dice", s);
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY_MS));
    showCenteredText("");
    vTaskDelay(pdMS_TO_TICKS(BETWEEN_DISPLAYS_MS));
  }
}

// ------------------------------------------------------

void resetGame() {
  if (xSemaphoreTake(mutex_sequence, pdMS_TO_TICKS(50)) == pdTRUE) {
    sequence.clear();
    userIndex = 0;
    xSemaphoreGive(mutex_sequence);
  }
}

// ------------------------------------------------------

void TareaSequence(void *parameter) {
  xSemaphoreGive(sem_sequenceNext);

  for (;;) {
    if (xSemaphoreTake(sem_sequenceNext, portMAX_DELAY) == pdTRUE) {
      if (xSemaphoreTake(mutex_sequence, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (sequence.size() >= MAX_SEQUENCE_LENGTH) {
          showCenteredText("Completado");
          vTaskDelay(pdMS_TO_TICKS(1500));
          sequence.clear();
        }
        char nuevo = randomKeyFromPad();
        sequence.push_back(nuevo);
        userIndex = 0;
        xSemaphoreGive(mutex_sequence);
        displaySequenceOnOLED(sequence);
        showCenteredText("Tu turno", "Presiona...");
        xSemaphoreGive(sem_inputReady);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ------------------------------------------------------

void TareaInput(void *parameter) {
  for (;;) {
    if (xSemaphoreTake(sem_inputReady, portMAX_DELAY) == pdTRUE) {
      bool failed = false;
      size_t expecting = 0;
      if (xSemaphoreTake(mutex_sequence, pdMS_TO_TICKS(100)) == pdTRUE) {
        expecting = sequence.size();
        xSemaphoreGive(mutex_sequence);
      }

      userIndex = 0;
      TickType_t startTick = xTaskGetTickCount();
      const TickType_t TIMEOUT_TICKS = pdMS_TO_TICKS(PER_KEY_TIMEOUT_MS);

      while (userIndex < expecting && !failed) {
        char key = keypad.getKey();
        if (key) {
          showCenteredText("Presionado:", String(key));
          if (xSemaphoreTake(mutex_sequence, pdMS_TO_TICKS(50)) == pdTRUE) {
            char esperado = sequence[userIndex];
            xSemaphoreGive(mutex_sequence);

            if (key == esperado) {
              userIndex++;
              showCenteredText("Correcto", String(userIndex) + "/" + String(expecting));
              vTaskDelay(pdMS_TO_TICKS(350));
            } else {
              failed = true;
              showCenteredText("Incorrecto", String("Esperado: ") + esperado);
              vTaskDelay(pdMS_TO_TICKS(1000));
            }
          }
          startTick = xTaskGetTickCount();
        }
      }

      if (failed) {
        resetGame();
        showCenteredText("Intenta otra vez", "Preparando...");
        vTaskDelay(pdMS_TO_TICKS(800));
        xSemaphoreGive(sem_sequenceNext);
      } else {
        showCenteredText("Siguiente", "ronda...");
        vTaskDelay(pdMS_TO_TICKS(700));
        xSemaphoreGive(sem_sequenceNext);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ------------------------------------------------------

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("No se detectó la pantalla OLED"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

  showCenteredText("Simon Dice ESP32", "Iniciando...");
  delay(800);

  sem_inputReady = xSemaphoreCreateBinary();
  sem_sequenceNext = xSemaphoreCreateBinary();
  mutex_sequence = xSemaphoreCreateMutex();

  uint32_t s = esp_random() ^ (uint32_t)micros();
  randomSeed(s);

  xTaskCreatePinnedToCore(TareaSequence, "TareaSequence", 4096, NULL, 1, &TareaSequenceHandle, app_cpu);
  xTaskCreatePinnedToCore(TareaInput, "TareaInput", 4096, NULL, 2, &TareaInputHandle, app_cpu);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
