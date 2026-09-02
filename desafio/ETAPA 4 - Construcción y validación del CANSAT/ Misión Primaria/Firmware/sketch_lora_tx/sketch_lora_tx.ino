/*  Transmisor TX - E.E.S.T. N.°5 "Amancio Williams"
    CONAE Cansat Secundario Edición 2026    
    
    Transmisor TX Oficial para Heltec WiFi LoRa 32 V3 
    Usa RadioLib para el chip SX1262 
*/

#include <RadioLib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

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
Adafruit_BMP280 bmp;   // Objeto BMP280
Adafruit_MPU6050 mpu;  // Objeto MPU6050

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
sensors_event_t mpu_a, mpu_g, mpu_temp;

void setup() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);  // Alimenta I2C / Pantalla
  delay(100);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  // Inicializar LoRa
  Serial.println("\n--- Inicializando Transmisor (BMP280) ---");
  int state = radio.begin(LORA_FREQUENCY,
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

  // Inicializar MPU6050
  mpuInit();
}

void loop() {
  readTempPressure();
  mpuRead();
  mpuPrint();

  pktNumber++;
  if (pktNumber >= 65500) pktNumber = 0;

  // Packet Number (1), Presion Base (2), Presion Absoluta (3), Altura (4)
  // Temperatura (5), Aceleracion X (6), Aceleracion Y (7),
  // Aceleracion  Z (8), Velocidad Angular X (9), Velocidad Angular Y (10)
  // Velocidad Angular Z (11)
  String packetData = String(pktNumber) + "," + String(baseline * 0.01, 2) + "," + String(P * 0.01, 2) + "," + String(A, 2)
                      + "," + String(T, 1) + "," + String(mpu_a.acceleration.x) + "," + String(mpu_a.acceleration.y)
                      + "," + String(mpu_a.acceleration.z) + "," + String(mpu_g.gyro.x) + "," + String(mpu_g.gyro.y)
                      + "," + String(mpu_g.gyro.z);

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

void mpuInit() {
  Serial.println("Adafruit MPU6050 Init!");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("No se encuentra el chip MPU6050");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 encontrado!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Rango del Acelerometro: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("+-2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("+-4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("+-8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("+-16G");
      break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Rango del Giroscopio: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:
      Serial.println("+- 250 deg/s");
      break;
    case MPU6050_RANGE_500_DEG:
      Serial.println("+- 500 deg/s");
      break;
    case MPU6050_RANGE_1000_DEG:
      Serial.println("+- 1000 deg/s");
      break;
    case MPU6050_RANGE_2000_DEG:
      Serial.println("+- 2000 deg/s");
      break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Ancho de banda del Filtro: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260 Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184 Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94 Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44 Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21 Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10 Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5 Hz");
      break;
  }
  Serial.println("");
}

void mpuRead() {
  mpu.getEvent(&mpu_a, &mpu_g, &mpu_temp);
}

void mpuPrint() {
  Serial.print("Acceleration X: ");
  Serial.print(mpu_a.acceleration.x);
  Serial.print(", Y: ");
  Serial.print(mpu_a.acceleration.y);
  Serial.print(", Z: ");
  Serial.print(mpu_a.acceleration.z);
  Serial.println(" m/s^2");

  Serial.print("Rotation X: ");
  Serial.print(mpu_g.gyro.x);
  Serial.print(", Y: ");
  Serial.print(mpu_g.gyro.y);
  Serial.print(", Z: ");
  Serial.print(mpu_g.gyro.z);
  Serial.println(" rad/s");

  Serial.print("Temperature: ");
  Serial.print(mpu_temp.temperature);
  Serial.println(" degC");

  Serial.println("");
}