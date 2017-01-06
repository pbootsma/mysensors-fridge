#include <SPI.h>
#include <MySensor.h>
#include <Wire.h>

unsigned long SLEEP_TIME = 300000;

#define CHILD_ID_TEMP 0
#define CHILD_ID_HUM 1
#define CHILD_ID_VOLT 2

MySensor gw;

MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgVolt(CHILD_ID_VOLT, V_VOLTAGE);

int RAW_VOLT_PIN = A0;
int SI7021_power_PIN = 7;

float currentTemperature;
float currentHumidity;

void setup()
{
	Serial.println("Sketch setup");

	pinMode(13, OUTPUT);
	digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
	delay(1000);              // wait for a second
	digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
	delay(1000);

	// use the 1.1 V internal reference
	analogReference(INTERNAL);

	// Power up Si7021
	pinMode(SI7021_power_PIN, OUTPUT);
	digitalWrite(SI7021_power_PIN, HIGH);

	gw.begin();

	// Send the Sketch Version Information to the Gateway
	gw.sendSketchInfo("Koelkast", "1.2");

	// Register all sensors to gw (they will be created as child devices)
	gw.present(CHILD_ID_HUM, S_HUM);
	gw.present(CHILD_ID_TEMP, S_TEMP);
	gw.present(CHILD_ID_VOLT, S_MULTIMETER);
	
	// Start i2c bus
	Wire.begin();
	
	Serial.println("Sketch setup done");
}

void loop()
{
	currentHumidity = 0 ;
	currentTemperature = 0 ;

	//Power up I2C SDA SCL
	digitalWrite(SDA, HIGH);
	digitalWrite(SCL, HIGH);

	//Power up 7021 and wait for stable state
	digitalWrite(SI7021_power_PIN, HIGH);
	delay(250); // was 100

	// READ DATA
	readSi7021Data();

	//Power down 7021
	digitalWrite(SI7021_power_PIN, LOW);

	//Power down I2C SDA SCL
	digitalWrite(SDA, LOW);
	digitalWrite(SCL, LOW);

	// get the battery Voltage
	int voltValue = analogRead(RAW_VOLT_PIN);
	float volt = voltValue/1023.0 * 1.1 * (987000+335000)/335000.0;
	gw.send(msgVolt.set(volt, 2));
	
	int batteryPcnt = 100*(volt - 3.0)/1.0; // 4 volt is full, 3 volt is empty
	if (batteryPcnt > 100) batteryPcnt = 100;
	gw.sendBatteryLevel(batteryPcnt);

	if (currentTemperature != 0 && currentHumidity != 0) {
		// Send temperature through RF
		gw.send(msgTemp.set(currentTemperature, 1));
	
		// Send Humidity through RF
		gw.send(msgHum.set(currentHumidity, 1));
	}

	// Sleep : cunsumption drop to around 20uA
	gw.sleep(SLEEP_TIME); //sleep a bit
}

void readSi7021Data()
{
	const int SI7021_ADDR = 0x40;
	const int CMD_READ_HUMIDITY = 0xE5;
	const int CMD_READ_TEMP = 0xE0; // Read temp from the humidity measurement, avoids a measurement

	// Led On
	digitalWrite(13, HIGH);

	// Initiating humidity and temperature measurement
	Wire.beginTransmission(SI7021_ADDR);
	Wire.write(CMD_READ_HUMIDITY);
	Wire.endTransmission();

	// Read and calculate humidity data
	Wire.requestFrom(SI7021_ADDR, 2);
	if(Wire.available() >= 2) {
		long rhCode = (Wire.read() << 8 | Wire.read()) & 0x0000FFFF;
		currentHumidity = (125 * rhCode) / 65536.0 - 6;
	}
	
	// Initiating temperature measurement
	Wire.beginTransmission(SI7021_ADDR);
	Wire.write(CMD_READ_TEMP);
	Wire.endTransmission();

	// Read and calculate temperature
	Wire.requestFrom(SI7021_ADDR, 2);
	if (Wire.available() >= 2) {
		long tempCode = (Wire.read() << 8 | Wire.read()) & 0x0000FFFF;
		currentTemperature = (175.72 * tempCode) / 65536 - 46.85;
	}

	// Led Off
	digitalWrite(13, LOW);
}

