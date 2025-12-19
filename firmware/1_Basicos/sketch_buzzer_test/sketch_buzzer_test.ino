const int PIN_ALARM_LED = 4;

void setup() {
  pinMode(PIN_ALARM_LED, OUTPUT);
}

void loop() {
  digitalWrite(PIN_ALARM_LED, HIGH);
  delay(500);
  digitalWrite(PIN_ALARM_LED, LOW);
  delay(500);
}