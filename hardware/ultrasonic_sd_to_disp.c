#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int trigPin = 11;
const int echoPin = 12;
const int chipSelect = 8;

File myFile;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  display.begin(SSD1306_I2C, 0x3C);

  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Initialization failed!");
    while (1);
  }
  Serial.println("Initialization done.");

  myFile = SD.open("distances.txt", FILE_WRITE);
  if (myFile) {
    myFile.println("Distance Measurements:");
    myFile.close();
  } else {
    Serial.println("Error opening distances.txt");
  }
}

void loop() {
  float echoTime;
  float calculatedDistance;

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  echoTime = pulseIn(echoPin, HIGH);
  calculatedDistance = echoTime / 148.0;

  Serial.print("Distance: ");
  Serial.print(calculatedDistance);
  Serial.println(" in");

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Distance: ");
  display.print(calculatedDistance);
  display.print(" in");
  display.display();

  delay(50);

  logDistance(calculatedDistance);
}

void logDistance(float distance) {
  myFile = SD.open("distances.txt", FILE_WRITE);
  if (myFile) {
    myFile.print("Distance: ");
    myFile.print(distance);
    myFile.println(" in");
    myFile.close();
  } else {
    Serial.println("Error opening distances.txt");
  }
}
