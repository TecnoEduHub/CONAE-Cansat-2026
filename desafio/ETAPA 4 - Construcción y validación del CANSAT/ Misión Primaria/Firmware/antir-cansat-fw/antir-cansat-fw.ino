/*  Transmisor TX - E.E.S.T. N.5 "Amancio Williams"
    CONAE Cansat Secundario Edicion 2026

    Transmisor TX para Heltec WiFi LoRa 32 V3 (RadioLib SX1262)
    VERSION DEPURACION: dispara GPIO46 (optoacoplador -> GPIO12 ESP32-CAM)
    al superar ALTURA_UMBRAL metros sobre el punto de arranque.
    + GPS NEO-6M en GPIO39 (RX, recibe TX del GPS) / GPIO38 (TX, va al RX del GPS)
*/

#include <RadioLib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <TinyGPSPlus.h>

char Version[] = "v3.4 FOTOS SIEMPRE ON + GPS - Heltec V3";

// ---------------- DEPURACION ----------------
// 1 = ignora la altura y cada TEST_PERIODO_MS alterna GPIO46
//     (sirve para probar el optoacoplador y la ESP32-CAM sin subir nada)
// 0 = funcionamiento normal (dispara por altura)
#define MODO_TEST_PULSO 0

// 1 = GPIO46 en HIGH desde que se enciende el Heltec (la camara saca fotos siempre)
// 0 = dispara por altura
#define FOTOS_DESDE_ENCENDIDO 1
#define TEST_PERIODO_MS 15000UL

#define PIN_ALTURA 46
#define ALTURA_UMBRAL 2.0f    // metros sobre el arranque (en vuelo real: 100.0)
#define LECTURAS_CONFIRMAR 3  // lecturas seguidas sobre el umbral para disparar
bool alturaSuperada = false;  // latch: una vez true, no vuelve a false
int lecturasSobreUmbral = 0;
unsigned long tTest = 0;

const byte pinPrueba = 47;

#define HELTEC_NSS 8
#define HELTEC_DIO1 14
#define HELTEC_RST 12
#define HELTEC_BUSY 13
#define LED 35
#define VEXT_PIN 36

#define I2C_SDA 41
#define I2C_SCL 42

// ---- GPS ----
#define GPS_RX_PIN 39  // recibe el TX del GPS
#define GPS_TX_PIN 38  // va al RX del GPS
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
double gpsLat = 0, gpsLon = 0, gpsAlt = 0, gpsVel = 0;
int gpsSats = 0;
int gpsFix = 0;

// 1 = imprime las sentencias NMEA crudas que llegan del GPS (muy verboso)
#define GPS_DUMP_NMEA 0

// Campos extra para diagnostico (el NEO-6M usa el prefijo GP)
TinyGPSCustom satsEnVista(gps, "GPGSV", 3);  // satelites que "ve" la antena
TinyGPSCustom calidadFix(gps, "GPGGA", 6);   // 0 = sin fix, 1 = GPS, 2 = DGPS
TinyGPSCustom estadoRMC(gps, "GPRMC", 2);    // A = valido, V = invalido

uint32_t gpsCharsPrev = 0;
int gpsFixPrev = 0;
unsigned long tArranque = 0;
unsigned long tPrimerFix = 0;

SX1262 radio = new Module(HELTEC_NSS, HELTEC_DIO1, HELTEC_RST, HELTEC_BUSY);
Adafruit_BMP280 bmp;

#define SERIAL_BAUDRATE 115200
#define INTERVAL_TIME_TX 1000

#define LORA_FREQUENCY 915.0
#define LORA_BANDWIDTH 125.0
#define LORA_SPREAD_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0xF3
#define LORA_POWER 17

// ---- MPU6050 por I2C directo (sin libreria) ----
#define MPU_REG_CONFIG 0x1A
#define MPU_REG_GYRO_CFG 0x1B
#define MPU_REG_ACCEL_CFG 0x1C
#define MPU_REG_DATA 0x3B
#define MPU_REG_PWR_MGMT 0x6B
#define MPU_REG_WHO_AM_I 0x75

// Rango +-8 g -> 4096 LSB/g ; rango +-500 dps -> 65.5 LSB/dps
#define ACCEL_LSB_PER_G 4096.0f
#define GYRO_LSB_PER_DPS 65.5f
#define G_TO_MS2 9.80665f
#define DEG_TO_RAD_F 0.0174533f

uint8_t mpuAddr = 0;
float gyroOffX = 0, gyroOffY = 0, gyroOffZ = 0;  // offsets en rad/s

double baseline;  // presion base en Pa
double T, P, A;   // A = altura relativa al arranque (m)
unsigned int pktNumber = 0;
sensors_event_t mpu_a, mpu_g, mpu_temp;

void setup() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);  // Alimenta I2C / Pantalla
  pinMode(pinPrueba, OUTPUT);
  digitalWrite(pinPrueba, HIGH);
  delay(100);

  pinMode(LED, OUTPUT);
  pinMode(PIN_ALTURA, OUTPUT);
  digitalWrite(PIN_ALTURA, FOTOS_DESDE_ENCENDIDO ? HIGH : LOW);
  digitalWrite(LED, HIGH);

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  Serial.println("\n--- Inicializando Transmisor (BMP280 + MPU6050 + GPS) ---");
  Serial.println(Version);
  Serial.printf("GPIO%d inicial = %d\n", PIN_ALTURA, digitalRead(PIN_ALTURA));

  // Inicializar LoRa
  int state = radio.begin(LORA_FREQUENCY,
                          LORA_BANDWIDTH,
                          LORA_SPREAD_FACTOR,
                          LORA_CODING_RATE, LORA_SYNC_WORD, LORA_POWER);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa RadioLib OK!");
  } else {
    Serial.print("Error LoRa, codigo: ");
    Serial.println(state);
    while (true)
      ;
  }

  // Inicializar I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  // Inicializar GPS
  gpsSerial.setRxBufferSize(1024);  // tiene que ir ANTES del begin
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  tArranque = millis();
  Serial.println("GPS iniciado en GPIO39 (RX) / GPIO38 (TX)");

  bool bmpIsInit = false;

  do {
    delay(1000);
    if (bmp.begin(0x76) || bmp.begin(0x77)) {
      Serial.println("BMP280 encontrado e iniciado OK!");
      // Promedio de 10 lecturas para una presion base estable
      double suma = 0;
      for (int i = 0; i < 10; i++) {
        suma += bmp.readPressure();
        delay(100);
      }
      baseline = suma / 10.0;
      readTempPressure();
      Serial.print("Presion Base: ");
      Serial.print(baseline * 0.01);
      Serial.println(" hpa");
      bmpIsInit = true;
      digitalWrite(LED, LOW);
    } else {
      Serial.println("Buscando BMP280 en GPIO 41/42...");
    }
  } while (!bmpIsInit);

  mpuInit();
  tTest = millis();
}

void loop() {
  readTempPressure();
  mpuRead();
  mpuPrint();

  gpsUpdate();
  actualizarDisparo();

  pktNumber++;
  if (pktNumber >= 65500) pktNumber = 0;

  // Packet Number (1), Presion Base (2), Presion Absoluta (3), Altura (4)
  // Temperatura (5), Aceleracion X (6), Y (7), Z (8),
  // Velocidad Angular X (9), Y (10), Z (11)
  // Latitud (14), Longitud (15), Altura GPS (16), Velocidad km/h (17),
  // Satelites (18), Fix valido 1/0 (19)
  String packetData = String(pktNumber) + "," + String(baseline * 0.01, 2) + "," + String(P * 0.01, 2) + "," + String(A, 2)
                      + "," + String(T, 1) + "," + String(mpu_a.acceleration.x) + "," + String(mpu_a.acceleration.y)
                      + "," + String(mpu_a.acceleration.z) + "," + String(mpu_g.gyro.x) + "," + String(mpu_g.gyro.y)
                      + "," + String(mpu_g.gyro.z)
                      + "," + String(gpsLat, 6) + "," + String(gpsLon, 6) + "," + String(gpsAlt, 1)
                      + "," + String(gpsVel, 1) + "," + String(gpsSats) + "," + String(gpsFix);

  Serial.print("TX - Version ");
  Serial.println(Version);
  Serial.print("Packet Number: ");
  Serial.println(pktNumber);
  Serial.print("Enviando: ");
  Serial.println(packetData);

  digitalWrite(LED, HIGH);

  int state = radio.transmit(packetData);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Paquete enviado con exito.");
  } else {
    Serial.print("Error al transmitir: ");
    Serial.println(state);
  }
  digitalWrite(LED, LOW);
  Serial.println("=====================================================================");
  esperarGPS(INTERVAL_TIME_TX);  // reemplaza al delay(): sigue leyendo el GPS
}

// ---------- GPS ----------

void gpsFeed() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
#if GPS_DUMP_NMEA
    Serial.write(c);
#endif
    gps.encode(c);
  }
}

// Reemplaza al delay(): espera ms pero sigue leyendo el GPS
void esperarGPS(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) {
    gpsFeed();
    delay(1);
  }
}

void gpsUpdate() {
  gpsFeed();

  gpsFix = (gps.location.isValid() && gps.location.age() < 3000) ? 1 : 0;
  if (gps.location.isValid()) {  // si se pierde el fix, quedan los ultimos valores
    gpsLat = gps.location.lat();
    gpsLon = gps.location.lng();
  }
  if (gps.altitude.isValid()) gpsAlt = gps.altitude.meters();
  if (gps.speed.isValid()) gpsVel = gps.speed.kmph();
  gpsSats = gps.satellites.isValid() ? gps.satellites.value() : 0;

  // ---- avisos de cambio de estado ----
  if (gpsFix && !gpsFixPrev) {
    if (tPrimerFix == 0) tPrimerFix = millis();
    Serial.println("");
    Serial.println(">>> GPS: FIX OBTENIDO! <<<");
    Serial.printf(">>> Tiempo hasta el primer fix: %lu s\n", (millis() - tArranque) / 1000);
  }
  if (!gpsFix && gpsFixPrev) {
    Serial.println("");
    Serial.println(">>> GPS: SE PERDIO EL FIX <<<");
  }
  gpsFixPrev = gpsFix;

  gpsDiagnostico();
}

void gpsDiagnostico() {
  uint32_t chars = gps.charsProcessed();
  uint32_t ok = gps.passedChecksum();
  uint32_t mal = gps.failedChecksum();
  uint32_t conFix = gps.sentencesWithFix();
  int enVista = atoi(satsEnVista.value());
  int calidad = atoi(calidadFix.value());
  const char *rmc = estadoRMC.value();

  Serial.println("---------------- DIAGNOSTICO GPS ----------------");
  Serial.printf("Tiempo desde arranque: %lu s\n", (millis() - tArranque) / 1000);
  Serial.printf("Caracteres recibidos : %lu (+%lu en este ciclo)\n", chars, chars - gpsCharsPrev);
  Serial.printf("Sentencias OK / error: %lu / %lu\n", ok, mal);
  Serial.printf("Sentencias con fix   : %lu\n", conFix);
  Serial.printf("Satelites en vista   : %d (los que la antena detecta)\n", enVista);
  Serial.printf("Satelites usados     : %d\n", gpsSats);
  Serial.printf("Calidad fix (GGA)    : %d  (0 = sin fix, 1 = GPS, 2 = DGPS)\n", calidad);
  Serial.printf("Estado RMC           : %s  (A = valido, V = sin fix)\n", rmc[0] ? rmc : "-");
  if (gps.hdop.isValid()) Serial.printf("HDOP                 : %.2f\n", gps.hdop.hdop());
  if (gps.date.isValid() && gps.time.isValid()) {
    Serial.printf("Fecha/hora UTC       : %04d-%02d-%02d %02d:%02d:%02d\n",
                  gps.date.year(), gps.date.month(), gps.date.day(),
                  gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    Serial.println("Fecha/hora UTC       : sin datos todavia");
  }
  Serial.printf("Posicion             : lat=%.6f lon=%.6f alt=%.1f m vel=%.1f km/h\n",
                gpsLat, gpsLon, gpsAlt, gpsVel);

  // ---- veredicto ----
  Serial.print("ESTADO: ");
  if (chars == 0) {
    Serial.println("SIN DATOS. No llega nada del GPS -> revisar que TX del GPS este en GPIO39, GND comun y VCC.");
  } else if (chars == gpsCharsPrev) {
    Serial.println("Dejaron de llegar datos -> cable suelto o GPS sin alimentacion.");
  } else if (ok == 0) {
    Serial.println("Llegan caracteres pero ninguna sentencia valida -> baudrate incorrecto o ruido (probar 9600).");
  } else if (gpsFix) {
    Serial.printf("GPS ENGANCHADO OK (%d satelites usados)\n", gpsSats);
  } else if (enVista == 0) {
    Serial.println("Cableado OK, pero NO VE NINGUN SATELITE -> sacar afuera, antena ceramica mirando al cielo, conectada al modulo.");
  } else {
    Serial.printf("Cableado OK, ve %d satelites pero todavia sin fix -> esperar (en frio puede tardar varios minutos).\n", enVista);
  }
  Serial.println("-------------------------------------------------");

  gpsCharsPrev = chars;
}

// ---------- Disparo de la camara ----------

void actualizarDisparo() {
#if FOTOS_DESDE_ENCENDIDO
  Serial.printf("Fotos siempre ON | GPIO%d = %d | Altura rel: %.2f m\n", PIN_ALTURA, digitalRead(PIN_ALTURA), A);
  return;
#endif
#if MODO_TEST_PULSO
  // Alterna GPIO46 cada TEST_PERIODO_MS: HIGH = camara saca fotos, LOW = no
  if (millis() - tTest >= TEST_PERIODO_MS) {
    tTest = millis();
    digitalWrite(PIN_ALTURA, !digitalRead(PIN_ALTURA));
  }
  Serial.printf("[TEST] GPIO%d = %d\n", PIN_ALTURA, digitalRead(PIN_ALTURA));
#else
  if (!alturaSuperada) {
    if (A > ALTURA_UMBRAL) {
      lecturasSobreUmbral++;
    } else {
      lecturasSobreUmbral = 0;
    }
    if (lecturasSobreUmbral >= LECTURAS_CONFIRMAR) {
      alturaSuperada = true;
      digitalWrite(PIN_ALTURA, HIGH);  // queda en 1 para siempre
      Serial.println(">>> Altura superada: GPIO46 en HIGH, la camara empieza a sacar fotos <<<");
    }
  }
  Serial.printf("Altura rel: %.2f m (umbral %.1f) | confirmaciones: %d/%d | GPIO%d = %d\n",
                A, ALTURA_UMBRAL, lecturasSobreUmbral, LECTURAS_CONFIRMAR,
                PIN_ALTURA, digitalRead(PIN_ALTURA));
#endif
}

void readTempPressure() {
  T = bmp.readTemperature();
  P = bmp.readPressure();
  // Altura relativa al punto de arranque (readAltitude espera hPa)
  A = bmp.readAltitude(baseline / 100.0);
}

// ---------- MPU6050 directo ----------

bool mpuReadReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

void mpuWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// Lee los 14 bytes crudos. Devuelve false si falla.
bool mpuReadRaw(int16_t &ax, int16_t &ay, int16_t &az, int16_t &tp,
                int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(MPU_REG_DATA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, (uint8_t)14) != 14) return false;

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  tp = (Wire.read() << 8) | Wire.read();
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void mpuInit() {
  Serial.println("MPU6050 Init (lectura directa)...");

  uint8_t id = 0;
  uint8_t addrs[2] = { 0x68, 0x69 };

  while (mpuAddr == 0) {
    for (int i = 0; i < 2; i++) {
      if (mpuReadReg(addrs[i], MPU_REG_WHO_AM_I, id)) {
        mpuAddr = addrs[i];
        break;
      }
    }
    if (mpuAddr == 0) {
      Serial.println("No se encuentra el MPU6050 en 0x68/0x69, reintentando...");
      delay(1000);
    }
  }

  Serial.print("MPU encontrado en 0x");
  Serial.print(mpuAddr, HEX);
  Serial.print(" - WHO_AM_I = 0x");
  Serial.println(id, HEX);

  mpuWriteReg(mpuAddr, MPU_REG_PWR_MGMT, 0x01);
  delay(100);

  mpuWriteReg(mpuAddr, MPU_REG_CONFIG, 0x04);     // filtro pasa-bajos ~21 Hz
  mpuWriteReg(mpuAddr, MPU_REG_GYRO_CFG, 0x08);   // +-500 deg/s
  mpuWriteReg(mpuAddr, MPU_REG_ACCEL_CFG, 0x10);  // +-8 g
  delay(100);

  Serial.println("Rango del Acelerometro: +-8G");
  Serial.println("Rango del Giroscopio: +- 500 deg/s");
  Serial.println("Ancho de banda del Filtro: 21 Hz");

  Serial.println("Calibrando giroscopio, no mover el modulo...");
  double sx = 0, sy = 0, sz = 0;
  int n = 0;
  for (int i = 0; i < 200; i++) {
    int16_t ax, ay, az, tp, gx, gy, gz;
    if (mpuReadRaw(ax, ay, az, tp, gx, gy, gz)) {
      sx += gx;
      sy += gy;
      sz += gz;
      n++;
    }
    delay(5);
    gpsFeed();  // mantiene vaciado el buffer del GPS durante la calibracion
  }
  if (n > 0) {
    gyroOffX = (sx / n) / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
    gyroOffY = (sy / n) / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
    gyroOffZ = (sz / n) / GYRO_LSB_PER_DPS * DEG_TO_RAD_F;
  }
  Serial.println("Calibracion lista.");
  Serial.println("");
}

void mpuRead() {
  int16_t ax, ay, az, tp, gx, gy, gz;
  if (!mpuReadRaw(ax, ay, az, tp, gx, gy, gz)) {
    Serial.println("Error leyendo el MPU6050 (se mantienen los ultimos valores)");
    return;
  }

  mpu_a.acceleration.x = ax / ACCEL_LSB_PER_G * G_TO_MS2;
  mpu_a.acceleration.y = ay / ACCEL_LSB_PER_G * G_TO_MS2;
  mpu_a.acceleration.z = az / ACCEL_LSB_PER_G * G_TO_MS2;

  mpu_g.gyro.x = gx / GYRO_LSB_PER_DPS * DEG_TO_RAD_F - gyroOffX;
  mpu_g.gyro.y = gy / GYRO_LSB_PER_DPS * DEG_TO_RAD_F - gyroOffY;
  mpu_g.gyro.z = gz / GYRO_LSB_PER_DPS * DEG_TO_RAD_F - gyroOffZ;

  mpu_temp.temperature = tp / 340.0f + 36.53f;
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
