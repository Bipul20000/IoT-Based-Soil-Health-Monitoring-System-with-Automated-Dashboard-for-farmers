
#include <Adafruit_AHT10.h>
#include <Wire.h>
#include <SPI.h>
#include <MQUnifiedsensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Define constants
#define placa "Arduino MEGA"
#define Voltage_Resolution 5
#define ADC_Bit_Resolution 10 

// MQ131 (Ozone Sensor)
#define MQ131_PIN A1
#define MQ131_TYPE "MQ-131"
#define RatioMQ131CleanAir 15

// MQ135 (NH4 Sensor)
#define MQ135_PIN A0
#define MQ135_TYPE "MQ-135"
#define RatioMQ135CleanAir 3.6 // RS / R0 = 3.6 ppm

// Define sensor pins
#define MQ7_PIN A5
#define UV_PIN A3
#define GasSensorPin A2
#define SensorPinPh A4
#define soil_moisture_sensor_pin A7

// Dallas Temperature sensor pins
#define ONE_WIRE_BUS 4 // Pin for Dallas Temperature sensor

// Sensor objects
MQUnifiedsensor MQ131(placa, Voltage_Resolution, ADC_Bit_Resolution, MQ131_PIN, MQ131_TYPE);
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, MQ135_PIN, MQ135_TYPE);

Adafruit_AHT10 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;

// Dallas temperature sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasSensors(&oneWire);

// Variables for pH sensor
int buf[10];
int avgValue = 0;

void setup() {
    Serial.begin(9600);

    // Initialize MQ131 sensor (Ozone)
    MQ131.setRegressionMethod(1); //_PPM = a * ratio ^ b
    MQ131.setA(23.943); 
    MQ131.setB(-1.11); 
    MQ131.init();
    calibrateSensor(MQ131, RatioMQ131CleanAir);

    // Initialize MQ135 sensor (NH4)
    MQ135.setRegressionMethod(1); //_PPM = a * ratio ^ b
    MQ135.setA(102.2); 
    MQ135.setB(-2.473); 
    MQ135.init();
    calibrateSensor(MQ135, RatioMQ135CleanAir);

    // Initialize AHT10 sensor (Temperature and Humidity)
    if (!aht.begin()) {
        Serial.println("Failed to find AHT10 chip");
        while (1) delay(10);
    }
    aht_temp = aht.getTemperatureSensor();
    aht_humidity = aht.getHumiditySensor();

    // Initialize Dallas temperature sensor
    dallasSensors.begin();

    pinMode(13, OUTPUT);
}

void loop() {
    // Read sensor values
    int mq6Value = readGasSensor(GasSensorPin);
    sensors_event_t humidity, temp;
    readTemperatureHumidity(humidity, temp);
    float uvVoltage = readUVSensor(UV_PIN);
    int mq7Value = readMQ7(MQ7_PIN);
    float phValue = readPHSensor(SensorPinPh);
    float ozoneValue = readOzoneSensor();
    float soilMoisture = readSoilMoisture(soil_moisture_sensor_pin);
    float nh4Value = readMQ135Sensor();
    float dallasTempC = readDallasTemperature(); // Reading Dallas Temperature sensor

    // Display values
    Serial.print("\t\tMQ 6 Output: ");
    Serial.println(mq6Value);

    Serial.print("\t\tTemperature: ");
    Serial.print(temp.temperature);
    Serial.println(" deg C");

    Serial.print("\t\tHumidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println(" % rH");

    Serial.println("\t\tUV Sensor Output:");
    Serial.print("\t\tsensor voltage = ");
    Serial.println(uvVoltage);

    Serial.print("\t\tMQ7 Value: ");
    Serial.println(mq7Value);

    Serial.print("\t\tpH: ");
    Serial.println(phValue, 2);

    Serial.println("\t\tOzone Sensor:");
    MQ131.serialDebug();  // Ozone sensor debug info

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture);
    Serial.println("%");

    Serial.println("\t\tNH4 Sensor:");
    MQ135.serialDebug();  // NH4 sensor debug info

    Serial.print("Dallas Temperature (Celsius): ");
    Serial.println(dallasTempC);

    // Data logging output
    Serial.print("Saving:(");
    Serial.print(mq6Value); Serial.print(",");
    Serial.print(temp.temperature); Serial.print(",");
    Serial.print(humidity.relative_humidity); Serial.print(",");
    Serial.print(uvVoltage); Serial.print(",");
    Serial.print(mq7Value); Serial.print(",");
    Serial.print(phValue); Serial.print(",");
    Serial.print(ozoneValue); Serial.print(",");
    Serial.print(soilMoisture); Serial.print(",");
    Serial.print(dallasTempC);
    Serial.println(")");

    delay(1000);
}

// Function to calibrate MQ sensors
void calibrateSensor(MQUnifiedsensor &sensor, float cleanAirRatio) {
    Serial.print("Calibrating sensor, please wait.");
    float calcR0 = 0;
    for (int i = 1; i <= 10; i++) {
        sensor.update();
        calcR0 += sensor.calibrate(cleanAirRatio);
        Serial.print(".");
    }
    sensor.setR0(calcR0 / 10);
    Serial.println(" done!");

    if (isinf(calcR0)) {
        Serial.println("Warning: Connection issue (Open circuit). Please check wiring.");
        while (1);
    } else if (calcR0 == 0) {
        Serial.println("Warning: Connection issue (Short circuit). Please check wiring.");
        while (1);
    }
    sensor.serialDebug(true);
}

// Function to read Gas Sensor MQ6
int readGasSensor(int pin) {
    return analogRead(pin);
}

// Function to read Temperature and Humidity (AHT10)
void readTemperatureHumidity(sensors_event_t &humidity, sensors_event_t &temp) {
    aht_humidity->getEvent(&humidity);
    aht_temp->getEvent(&temp);
}

// Function to read UV Sensor
float readUVSensor(int pin) {
    float sensorValue = analogRead(pin);
    return sensorValue / 1024 * 5.0;
}

// Function to read MQ7 Sensor
int readMQ7(int pin) {
    return analogRead(pin);
}

// Function to read pH Sensor
float readPHSensor(int pin) {
    for (int i = 0; i < 10; i++) {
        buf[i] = analogRead(pin);
        delay(10);
    }

    int minValue = buf[0];
    int maxValue = buf[0];
    int sum = 0;

    for (int i = 0; i < 10; i++) {
        if (buf[i] < minValue) minValue = buf[i];
        if (buf[i] > maxValue) maxValue = buf[i];
        sum += buf[i];
    }

    sum -= (minValue + maxValue); // Exclude min and max
    float phValue = (float)sum / 8 * 5.0 / 1024 / 6; // Adjust scaling
    return 3.5 * phValue; // Calibration
}

// Function to read Ozone Sensor (MQ131)
float readOzoneSensor() {
    MQ131.update();
    MQ131.readSensorR0Rs();
    return MQ131.readSensor(); // Change this if necessary
}

// Function to read Soil Moisture Sensor
float readSoilMoisture(int pin) {
    int sensorValue = analogRead(pin);
    return (100 - ((sensorValue / 1023.0) * 100));
}

// Function to read MQ135 Sensor (NH4)
float readMQ135Sensor() {
    MQ135.update();
    return MQ135.readSensor(); // Change this if necessary
}

// Function to read Dallas Temperature Sensor
float readDallasTemperature() {
    dallasSensors.requestTemperatures(); // Request temperature from all devices
    return dallasSensors.getTempCByIndex(0); // Assuming only one device
}