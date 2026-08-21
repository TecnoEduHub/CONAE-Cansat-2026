

/* 
  Transmisor TX Oficial para Heltec WiFi LoRa 32 V3 + BMP280 
*/

#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>  // Librería para BMP280

char Version[] = "v3.0 BMP280 - Heltec V3";

#define HELTEC_NSS 8
#define HELTEC_DIO1 14
#define HELTEC_RST 12
#define HELTEC_BUSY 13
#define LED 35
#define VEXT_PIN 36

#define I2C_SDA 41
#define I2C_SCL 42

SX1262 radio = new Module(HELTEC_NSS, HELTEC_DIO1, HELTEC_RST, HELTEC_BUSY);
Adafruit_BMP280 bmp;  // Objeto BMP280

#define SERIAL_BAUDRATE 115200
#define INTERVAL_TIME_TX 1000

#define LORA_FREQUENCY 915.0
#define LORA_BANDWIDTH 125.0
#define LORA_SPREAD_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0xF3
#define LORA_POWER 17

double baseline;
double T, P, A;
unsigned int pktNumber = 0;

void readTempPressure();

void setup() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);  // Alimenta I2C / Pantalla
  delay(100);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  Serial.println("\n--- Inicializando Transmisor (BMP280) ---");

  // Inicializar LoRa

  int
    state =
      radio.begin(LORA_FREQUENCY,
                  LORA_BANDWIDTH,
                  LORA_SPREAD_FACTOR,
                  LORA_CODING_RATE, LORA_SYNC_WORD, LORA_POWER);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("¡LoRa RadioLib OK!");
  } else {
    Serial.print("Error LoRa, código: ");
    Serial.println(state);
    while (true)
      ;
  }

  // Inicializar I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  bool bmpIsInit = false;

  do {
    delay(1000);
    // Probamos primero con dirección 0x76 (típica del BMP280) y luego 0x77
    if (bmp.begin(0x76) || bmp.begin(0x77)) {
      Serial.println("¡BMP280 encontrado e iniciado OK!");
      readTempPressure();
      baseline = P;
      Serial.print("Presion Base: ");
      Serial.print(baseline * 0.01);
      Serial.println(" hpa");
      bmpIsInit = true;
      digitalWrite(LED, LOW);
    } else {
      Serial.println("Buscando BMP280 en GPIO 41/42...");
    }
  } while (!bmpIsInit);
}

void loop() {
  readTempPressure();

  pktNumber++;
  if (pktNumber >= 65500) pktNumber = 0;

  String packetData = String(pktNumber) + "," + String(baseline * 0.01, 2) + "," + String(P * 0.01, 2) + "," + String(A, 2) + "," + String(T, 1);

  Serial.print("TX - Version ");
  Serial.println(Version);
  Serial.print("Packet Number: ");
  Serial.println(pktNumber);
  Serial.print("Enviando: ");
  Serial.println(packetData);

  digitalWrite(LED, HIGH);

  int state = radio.transmit(packetData);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Paquete enviado con éxito.");
  } else {
    Serial.print("Error al transmitir: ");
    Serial.println(state);
  }

  digitalWrite(LED, LOW);
  Serial.println("=====================================================================");

  delay(INTERVAL_TIME_TX);
}

void readTempPressure() {
  T = bmp.readTemperature();
  P = bmp.readPressure();
  A = bmp.readAltitude(1013.25);  // Presión promedio a nivel del mar
}
