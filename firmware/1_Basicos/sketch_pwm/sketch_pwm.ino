#include <Arduino.h>

// Usamos el LED integrado de la placa
const int PIN_PWM = 5;

// Configuración PWM
const int PWM_CHANNEL    = 0;     // canal 0
const int PWM_FREQUENCY  = 5000;  // 5 kHz
const int PWM_RESOLUTION = 8;     // 8 bits: 0..255

void setup() {
  // Configuramos el canal PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  // Asociamos el canal al pin del LED
  ledcAttachPin(PIN_PWM, PWM_CHANNEL);
}

void loop() {
  // Duty bajo
  ledcWrite(PWM_CHANNEL, 51);   // 51 ~ 20 % de 255
  delay(2000);

  // Duty medio
  ledcWrite(PWM_CHANNEL, 128);  // ~50 %
  delay(2000);

  // Duty alto
  ledcWrite(PWM_CHANNEL, 230);  // ~90 %
  delay(2000);

  // Opcional: apagar completamente
  ledcWrite(PWM_CHANNEL, 0);    // 0 %
  delay(2000);
}