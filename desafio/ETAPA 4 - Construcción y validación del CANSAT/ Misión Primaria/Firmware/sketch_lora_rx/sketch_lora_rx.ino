
/********* 
  Receptor RX Oficial para Heltec WiFi LoRa 32 V3 
  Usa RadioLib para el chip SX1262 
*********/

#include <RadioLib.h>
#include <SPI.h>

char Version[] = "v3.0 RX - Heltec V3";

// Definición de pines para Heltec V3 (SX1262)
#define HELTEC_NSS 8
#define HELTEC_DIO1 14
#define HELTEC_RST 12
#define HELTEC_BUSY 13
#define LED 35
#define VEXT_PIN 36

// Instancia del módulo LoRa SX1262
SX1262 radio = new Module(HELTEC_NSS, HELTEC_DIO1, HELTEC_RST, HELTEC_BUSY);

#define SERIAL_BAUDRATE 115200

// Configuraciones del módulo LoRa
#define LORA_FREQUENCY 915.0  // MHz
#define LORA_BANDWIDTH 125.0  // kHz
#define LORA_SPREAD_FACTOR 7
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0xF3

void setup() {
  // Activar alimentación externa (Vext)
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  Serial.println("\n--- Inicializando Receptor LoRa SX1262 (Heltec V3) ---");

  // Inicialización de RadioLib

  int
    state =
      radio.begin(LORA_FREQUENCY,
                  LORA_BANDWIDTH,
                  LORA_SPREAD_FACTOR,
                  LORA_CODING_RATE, LORA_SYNC_WORD);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("¡LoRa RX Inicializado Correctamente!");
    digitalWrite(LED, LOW);
  } else {
    Serial.print("Error al iniciar LoRa, código: ");
    Serial.println(state);
    while (true) {
      digitalWrite(LED, !digitalRead(LED));
      delay(200);
    }
  }
}


void loop() {
  String strData;
  int state = radio.receive(strData);

  if (state == RADIOLIB_ERR_NONE) {
    digitalWrite(LED, HIGH);

    Serial.print("RX - Version ");
    Serial.println(Version);

    Serial.print("Telemetria RAW Recibida: ");
    Serial.println(strData);

    // Parseo de los datos separados por comas
    int indicador1 = strData.indexOf(',');
    String pktNumber = strData.substring(0, indicador1);
    Serial.print("Packet Number: ");
    Serial.println(pktNumber);

    int indicador2 = strData.indexOf(',', indicador1 + 1);
    String presionBase = strData.substring(indicador1 + 1, indicador2);
    Serial.print("Presion Base: ");
    Serial.println(presionBase);

    int indicador3 = strData.indexOf(',', indicador2 + 1);
    String presionAbsoluta = strData.substring(indicador2 + 1, indicador3);
    Serial.print("Presion Absoluta: ");
    Serial.println(presionAbsoluta);

    int indicador4 = strData.indexOf(',', indicador3 + 1);
    String altura = strData.substring(indicador3 + 1, indicador4);
    Serial.print("Altura: ");
    Serial.println(altura);

    int indicador5 = strData.indexOf(',', indicador4 + 1);
    String temperatura = strData.substring(indicador4 + 1, indicador5);
    Serial.print("Temperatura: ");
    Serial.println(temperatura);

    // RSSI y SNR del paquete recibido
    Serial.print("Nivel de señal [RSSI]: ");
    Serial.print(radio.getRSSI());
    Serial.println(" dBm");

    Serial.print("Relación Señal/Ruido [SNR]: ");
    Serial.print(radio.getSNR());
    Serial.println(" dB");

    Serial.println("=====================================================================");
    digitalWrite(LED, LOW);
  }
}
