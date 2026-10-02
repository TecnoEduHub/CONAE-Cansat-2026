/*  Firmware ES32-CAM A.N.T.I.R.
    E.E.S.T. N.°5 "Amancio Williams"
    Septiembre de 2026

    Toma fotos cada 5 segundos si hay un estado LOW en el GPIO12
    
    ESP32-CAM AI-Thinker Pinout Guide: GPIOs Usage Explained
    https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
*/

#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"

// ESP32-CAM AI-Thinker
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// GPIO que habilita las fotos
#define TRIGGER_GPIO 12

unsigned long ultimaFoto = 0;
unsigned long numeroFoto = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIGGER_GPIO, INPUT_PULLUP);

  // Configuración de cámara
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Error cámara: 0x%x\n", err);
    return;
  }

  // Inicializar SD
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Error inicializando SD");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No hay tarjeta SD");
    return;
  }

  Serial.println("Cámara y SD inicializadas.");

  // Buscar el último número de foto existente en la SD para no sobrescribir
  numeroFoto = obtenerUltimoNumeroFoto();
  Serial.printf("Próxima foto será: foto_%05lu.jpg\n", numeroFoto + 1);
}

void loop() {

  // GPIO en LOW = tomar fotos
  if (digitalRead(TRIGGER_GPIO) == LOW) {

    if (millis() - ultimaFoto >= 5000) {
      ultimaFoto = millis();
      tomarFoto();
    }

  }
  delay(10);
}

void tomarFoto() {

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Error capturando imagen");
    return;
  }

  numeroFoto++;

  char nombreArchivo[32];
  sprintf(nombreArchivo, "/foto_%05lu.jpg", numeroFoto);

  Serial.print("Guardando: ");
  Serial.println(nombreArchivo);

  File archivo = SD_MMC.open(nombreArchivo, FILE_WRITE);

  if (!archivo) {
    Serial.println("No se pudo abrir el archivo");
    esp_camera_fb_return(fb);
    return;
  }

  archivo.write(fb->buf, fb->len);
  archivo.close();

  Serial.printf("Foto guardada (%u bytes)\n", fb->len);

  esp_camera_fb_return(fb);
}

// Función auxiliar para leer la SD y encontrar el número más alto guardado
unsigned long obtenerUltimoNumeroFoto() {
  File root = SD_MMC.open("/");
  if (!root) {
    return 0;
  }
  if (!root.isDirectory()) {
    return 0;
  }

  unsigned long maxNum = 0;
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String fileName = String(file.name());
      if (fileName.startsWith("foto_") && fileName.endsWith(".jpg")) {
        // Extraer la parte numérica del nombre (ej: "foto_00012.jpg" -> "00012")
        String numStr = fileName.substring(5, fileName.indexOf('.'));
        unsigned long num = numStr.toInt();
        if (num > maxNum) {
          maxNum = num;
        }
      }
    }
    file = root.openNextFile();
  }
  return maxNum;
}