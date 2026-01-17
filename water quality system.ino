// === Water Quality Monitoring + ThingSpeak Upload ===
// Sensors: TDS, Turbidity, DS18B20 Temperature

#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- WIFI SETTINGS ----------------
const char* ssid = "OnePlus Nord CE 3 Lite 5G";  // your hotspot name
const char* password = "11111111";               // your hotspot password

// --------------- THINGSPEAK SETTINGS -------------
unsigned long myChannelNumber = 3195111;  // <-- PUT YOUR CHANNEL ID (NUMBER)
const char* myWriteAPIKey = "EKL4589JOAKVQ7ZP";

WiFiClient client;

// ---------------- PIN CONFIGURATION -----------------
const byte ANALOG_PIN = A0;
const byte TDS_POWER_PIN = D5;
const byte TURB_POWER_PIN = D6;
const byte ONE_WIRE_BUS = D2;

// ---------------- DS18B20 SETUP --------------------
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
float waterTempC = 25.0;

// ---------------- TDS SETTINGS ---------------------
const float AREF = 3.3;
const float TDS_CALIBRATION = 1.0;
const float OFFSET = 0.14;
float tdsValue = 0;

// ---------------- TURBIDITY SETTINGS ---------------
const int clearWaterAnalogValue = 500;
const int cloudyWaterAnalogValue = 300;
const int dirtyWaterAnalogValue = 100;

// ---------------- SETUP ----------------------------
void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println();
  Serial.println("Booting...");

  pinMode(TDS_POWER_PIN, OUTPUT);
  pinMode(TURB_POWER_PIN, OUTPUT);
  digitalWrite(TDS_POWER_PIN, LOW);
  digitalWrite(TURB_POWER_PIN, LOW);

  tempSensor.begin();

  // WIFI CONNECT
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize ThingSpeak
  ThingSpeak.begin(client);
}

// ---------------- MAIN LOOP ------------------------
void loop() {
  readTemperature();
  readTds();
  int turbValue = readTurbidity();

  sendToThingSpeak(waterTempC, tdsValue, turbValue);

  Serial.println("Waiting 15 seconds...\n");
  delay(15000);  // >=15 seconds for ThingSpeak free
}

// ---------------- READ TEMPERATURE -------------------
void readTemperature() {
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C) {
    waterTempC = tempC;
  }

  Serial.print("Temperature: ");
  Serial.print(waterTempC);
  Serial.println(" °C");
}

// ---------------- READ TDS SENSOR --------------------
void readTds() {
  const int samples = 10;
  float avg = 0;

  digitalWrite(TURB_POWER_PIN, LOW);
  digitalWrite(TDS_POWER_PIN, HIGH);
  delay(1500);

  for (int i = 0; i < samples; i++) {
    avg += analogRead(ANALOG_PIN);
    delay(50);
  }
  digitalWrite(TDS_POWER_PIN, LOW);

  avg /= samples;
  float voltage = avg * AREF / 1024.0;

  float compCoeff = 1.0 + 0.02 * (waterTempC - 25.0);
  float compVolt = voltage / compCoeff;

  compVolt = (compVolt * TDS_CALIBRATION) - OFFSET;
  if (compVolt < 0) compVolt = 0;

  tdsValue = (133.42 * pow(compVolt, 3)
              - 255.86 * compVolt * compVolt
              + 857.39 * compVolt)
             * 0.5;

  Serial.print("TDS: ");
  Serial.print(tdsValue, 0);
  Serial.print(" ppm");
  if (tdsValue <= 150) Serial.println(" : Ideal for drinking.");
  else if (tdsValue <= 300) Serial.println(" -> Good for drinking.");
  else if (tdsValue <= 500) Serial.println(" -> Acceptable & safe for drinking.");
  else if (tdsValue <= 1000) Serial.println(" -> Poor for drinking.");
  else Serial.println(" -> Unfit for drinking.");
}

// ---------------- READ TURBIDITY SENSOR ---------------
int readTurbidity() {
  digitalWrite(TDS_POWER_PIN, LOW);
  digitalWrite(TURB_POWER_PIN, HIGH);
  delay(300);

  int value = analogRead(ANALOG_PIN);
  digitalWrite(TURB_POWER_PIN, LOW);

  Serial.print("Turbidity: ");
  Serial.print(value);
  if (value >= clearWaterAnalogValue)
    Serial.println(" -> Water Status: Clear");
  else if (value >= cloudyWaterAnalogValue)
    Serial.println(" -> Water Status: Cloudy");
  else
    Serial.println(" -> Water Status: Dirty");

  return value;
}

// ---------------- SEND DATA TO THINGSPEAK --------------
void sendToThingSpeak(float temp, float tds, int turb) {
  // Set fields
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, tds);
  ThingSpeak.setField(3, turb);

  Serial.print("Sending to ThingSpeak -> ");
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print("  TDS: ");
  Serial.print(tds);
  Serial.print("  Turbidity: ");
  Serial.println(turb);

  int httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (httpCode == 200) {
    Serial.println("✅ Data successfully sent to ThingSpeak!");
  } else {
    Serial.print("❌ Problem sending data. HTTP error code: ");
    Serial.println(httpCode);
  }
}
