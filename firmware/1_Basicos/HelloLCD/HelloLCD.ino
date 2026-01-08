#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN 18
#define SCL_PIN 19

// IMPORTANTE: aquí van dirección I2C, columnas, FILAS
LiquidCrystal_I2C lcd(0x3F, 16, 2);  // 16x2 típico

void setup() {
  // Inicializar I2C en los pines 18 (SDA), 19 (SCL)
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializar LCD
  lcd.init();          // o lcd.begin(16, 2); según la versión de la lib
  lcd.backlight();     // encender luz de fondo

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MiniHVAC ESP32");
  lcd.setCursor(0, 1);
  lcd.print("Hello LCD :)");
}

void loop() {
  // No hace falta nada en loop para esta prueba
}