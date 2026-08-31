#include <TinyGPSPlus.h>

/*
   Ejemplo para ESP32 con GPS NEO-6M
   Usa UART2 hardware en lugar de SoftwareSerial
*/

// Configuración de pines para UART2
static const int RXPin = 16;           // GPIO16 para RX (conectar al TX del GPS)
static const int TXPin = 17;           // GPIO17 para TX (conectar al RX del GPS)
static const uint32_t GPSBaud = 9600;  // NEO-6M normalmente usa 9600 baudios

// El objeto TinyGPSPlus
TinyGPSPlus gps;

// La conexión serial al GPS usando UART2 hardware
HardwareSerial ss(2);  // UART2

void setup() {
  Serial.begin(115200);
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  Serial.println(F("DeviceExample.ino"));
  Serial.println(F("Una demostración simple de TinyGPSPlus con un módulo GPS"));
  Serial.print(F("Probando librería TinyGPSPlus v. "));
  Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();
}

void loop() {
  // Esta función muestra información cada vez que se codifica una nueva oración correctamente
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No se detectó GPS: verifica las conexiones."));
    while (true)
      ;
  }
}

void displayInfo() {
  Serial.print(F("Ubicación: "));
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  } else {
    Serial.print(F("INVÁLIDO"));
  }

  Serial.print(F("  Fecha/Hora: "));
  if (gps.date.isValid()) {
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  } else {
    Serial.print(F("INVÁLIDO"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid()) {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  } else {
    Serial.print(F("INVÁLIDO"));
  }

  Serial.println();
}