/*  ESTACIÓN TERRENA - E.E.S.T. N.°5 "Amancio Williams"
    CONAE Cansat Secundario Edición 2026    
    
    Receptor RX Oficial para Heltec WiFi LoRa 32 V3 
    Usa RadioLib para el chip SX1262 
*/

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "logo_eest5.h"

char Version[] = "v3.0 RX - Heltec V3";

// Configuraciones generales
#define DEBUG_MODE 1
#define TEST_MODE 0
#define SERIAL_BAUDRATE 115200

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// Definición de pines para Heltec V3 (SX1262)
#define HELTEC_NSS 8
#define HELTEC_DIO1 14
#define HELTEC_RST 12
#define HELTEC_BUSY 13
#define LED 35
#define VEXT_PIN 36

// Instancia del módulo LoRa SX1262
SX1262 radio = new Module(HELTEC_NSS, HELTEC_DIO1, HELTEC_RST, HELTEC_BUSY);

// Configuraciones del módulo LoRa
#define LORA_FREQUENCY 915.0  // MHz
#define LORA_BANDWIDTH 125.0  // kHz
#define LORA_SPREAD_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0xF3

// Instancia de la pantalla OLED
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

void setup() {
  // Activar alimentación externa (Vext)
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  // Initialising the display
  display.init();
  screenLogo();
  delay(2000);
  screenInit();

  // Inicialización de RadioLib
  DEBUG_PRINTLN("\n--- Inicializando Receptor LoRa SX1262 (Heltec V3) ---");
  delay(1500);
  int state = radio.begin(LORA_FREQUENCY,
                          LORA_BANDWIDTH,
                          LORA_SPREAD_FACTOR,
                          LORA_CODING_RATE, LORA_SYNC_WORD);

  if (state == RADIOLIB_ERR_NONE) {
    screenLoRaOK();
    DEBUG_PRINTLN("¡LoRa RX Inicializado Correctamente!");
    digitalWrite(LED, LOW);
  } else {
    screenLoRaError();
    DEBUG_PRINT("Error al iniciar LoRa, código: ");
    DEBUG_PRINTLN(state);
    while (true) {
      digitalWrite(LED, !digitalRead(LED));
      delay(200);
    }
  }
}


void loop() {

#if TEST_MODE
  // Solo pruebas del Dashboard (SerialStudio)
  // Packet Number (1), Presion Base (2), Presion Absoluta (3), Altura (4), Temperatura (5),
  // Aceleracion X (6), Aceleracion Y (7), Aceleracion  Z (8), Velocidad Angular X (9),
  // Velocidad Angular Y (10), Velocidad Angular Z (11), RSSI (12), SNR (13)
  Serial.println("1,1013.2,987.4,215.3,24.5,10,11,12,20,21,22,30,31");
  delay(1000);
#endif

  String strData;
  int state = radio.receive(strData);

  if (state == RADIOLIB_ERR_NONE) {
    digitalWrite(LED, HIGH);

    DEBUG_PRINTLN("RX - Version ");
    DEBUG_PRINTLN(Version);

    DEBUG_PRINT("Telemetria RAW Recibida: ");
    Serial.println(strData);

    // Parseo de los datos separados por comas
    int indicador1 = strData.indexOf(',');
    String pktNumber = strData.substring(0, indicador1);
    DEBUG_PRINT("Packet Number: ");
    DEBUG_PRINTLN(pktNumber);

    int indicador2 = strData.indexOf(',', indicador1 + 1);
    String presionBase = strData.substring(indicador1 + 1, indicador2);
    DEBUG_PRINT("Presion Base: ");
    DEBUG_PRINTLN(presionBase);

    int indicador3 = strData.indexOf(',', indicador2 + 1);
    String presionAbsoluta = strData.substring(indicador2 + 1, indicador3);
    DEBUG_PRINT("Presion Absoluta: ");
    DEBUG_PRINTLN(presionAbsoluta);

    int indicador4 = strData.indexOf(',', indicador3 + 1);
    String altura = strData.substring(indicador3 + 1, indicador4);
    DEBUG_PRINT("Altura: ");
    DEBUG_PRINTLN(altura);

    int indicador5 = strData.indexOf(',', indicador4 + 1);
    String temperatura = strData.substring(indicador4 + 1, indicador5);
    DEBUG_PRINT("Temperatura: ");
    DEBUG_PRINTLN(temperatura);

    // MPU6050 data
    int indicador6 = strData.indexOf(',', indicador5 + 1);
    String Ax = strData.substring(indicador5 + 1, indicador6);
    DEBUG_PRINT("Aceleración X: ");
    DEBUG_PRINTLN(Ax);

    int indicador7 = strData.indexOf(',', indicador6 + 1);
    String Ay = strData.substring(indicador6 + 1, indicador7);
    DEBUG_PRINT("Aceleración Y: ");
    DEBUG_PRINTLN(Ay);

    int indicador8 = strData.indexOf(',', indicador7 + 1);
    String Az = strData.substring(indicador7 + 1, indicador8);
    DEBUG_PRINT("Aceleración Z: ");
    DEBUG_PRINTLN(Az);

    int indicador9 = strData.indexOf(',', indicador8 + 1);
    String Gx = strData.substring(indicador8 + 1, indicador9);
    DEBUG_PRINT("Velocidad Angular X: ");
    DEBUG_PRINTLN(Gx);

    int indicador10 = strData.indexOf(',', indicador9 + 1);
    String Gy = strData.substring(indicador9 + 1, indicador10);
    DEBUG_PRINT("Velocidad Angular Y: ");
    DEBUG_PRINTLN(Gy);

    int indicador11 = strData.indexOf(',', indicador10 + 1);
    String Gz = strData.substring(indicador10 + 1, indicador11);
    DEBUG_PRINT("Velocidad Angular Z: ");
    DEBUG_PRINTLN(Gz);

    // RSSI y SNR del paquete recibido
    DEBUG_PRINT("RSSI - Nivel de señal [dBm]: ");
    DEBUG_PRINTLN(radio.getRSSI());

    DEBUG_PRINT("SNR - Relación Señal/Ruido [dB]: ");
    DEBUG_PRINTLN(radio.getSNR());

    DEBUG_PRINT("=====================================================================");
    digitalWrite(LED, LOW);
  }
}

// Pantallas OLED
void screenLogo() {
  display.clear();
  display.drawXbm(34, 5, 48, 48, logo_eest5_48x48);
  display.display();
}

void screenInit() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(10, 10, "E.E.S.T. N.°5");
  display.setFont(ArialMT_Plain_10);
  display.drawString(20, 35, "Heltec LoRa V3");
  display.drawString(20, 50, "Init Receptor LoRa...");
  display.display();
}

void screenLoRaOK() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "LoRa RX:");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 25, "¡Inicializado");
  display.drawString(0, 40, "Correctamente!");
  display.display();
}

void screenLoRaError() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "LoRa RX:");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 25, "Error al iniciar");
  //display.drawString(0, 40, "");
  display.display();
}