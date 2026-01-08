#include <Wire.h>

#define SDA_PIN 18
#define SCL_PIN 19

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Iniciando escaner I2C...");
  Wire.begin(SDA_PIN, SCL_PIN);
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Escaneando...");

  for (address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado en 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }

  if (nDevices == 0) {
    Serial.println("No se encontraron dispositivos I2C\n");
  } else {
    Serial.println("Escaneo terminado.\n");
  }

  delay(3000);
}