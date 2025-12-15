#include "DHTesp.h"
DHTesp dht;       //Define the DHT object
int dhtPin = 21;  //Define the dht pin
void setup() {
  dht.setup(dhtPin, DHTesp::DHT11);  //Initialize the dht pin and dht object
  Serial.begin(115200);              //Set the baud rate to 115200
}
void loop() {
flag:
  TempAndHumidity newValues = dht.getTempAndHumidity();  //Get the Temperature and humidity
  if (dht.getStatus() != 0) {                            //Judge if the correct value is read
    goto flag;                                           //If there is an error, go back to the flag and re-read the data
  }
  Serial.println("Temperature:" + String(newValues.temperature) + " Humidity:" + String(newValues.humidity));
  delay(2000);
}
