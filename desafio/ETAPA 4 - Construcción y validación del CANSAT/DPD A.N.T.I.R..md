# Planteo de diseño del equipo A.N.T.I.R.

## Misión primaria

Además de cumplir con la medición obligatoria de presión y temperatura decidieron ampliar considerablemente la telemetría incorporando:

* BMP280 (presión y temperatura)
* MPU6050 (aceleración y velocidad angular)
* GPS NEO-8M (latitud, longitud y altitud)

Todos esos datos serán almacenados en MicroSD y transmitidos en tiempo real mediante LoRa SX1278.

## Misión secundaria

Consiste en utilizar una **ESP32-CAM** apuntando hacia el suelo para fotografiar la costa de Mar del Plata durante el descenso. El objetivo no es solamente obtener imágenes sino realizar un registro ambiental orientado a:

* detectar residuos en playas;
* documentar la evolución de la contaminación;
* generar evidencia útil para futuras políticas ambientales;
* colaborar eventualmente con organizaciones locales. 

## Arquitectura electrónica

Eligieron como computadora principal un ESP32 DevKit con los siguientes pPeriféricos:

* BMP280
* MPU6050
* GPS NEO-8M
* ESP32-CAM
* LoRa SX1278
* MicroSD
* batería LiPo 3,7 V
* regulador MP1584. 

## Estructura

Plantean:

* carcasa impresa en 3D (PETG),
* protección mediante goma EVA,
* paracaídas superior,
* componentes montados internamente. 

## Presupuesto de masa

Estimaron **211 gramos**, por lo que quedarían aproximadamente **39 gramos por debajo del límite reglamentario de 250 g**, dejando un margen razonable. 

## Operación prevista

El funcionamiento propuesto es:

1. Leer sensores.
2. Obtener posición GPS.
3. Capturar fotografías.
4. Guardar todo en MicroSD.
5. Transmitir telemetría por LoRa en tiempo real. 

## Plan de pruebas

Prevén validar individualmente:

* sensores;
* cámara;
* GPS;
* LoRa;
* batería;
* regulador;
* paracaídas.

Luego realizarán:

* integración completa;
* prueba de caída;
* prueba de vibraciones;
* verificación de dimensiones;
* simulación de vuelo con telemetría.