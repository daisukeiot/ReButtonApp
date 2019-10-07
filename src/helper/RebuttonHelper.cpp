#include <Arduino.h>
#include "../Common.h"
#include "RebuttonHelper.h"
#include <ReButton.h>
#include "../Grove_BME280/Seeed_BME280.h"

static BME280 bme280;
static bool BME280_Initialized = false;

double ReadBatteryVoltage()
{
	return ReButton::ReadPowerSupplyVoltage();
}

char* GetSwVersion()
{
    return (char *)CONFIG_FIRMWARE_VERSION;
}

float GetTemperature()
{
	float temperature = 0;

	if (BME280_Initialized == false)
	{
		if (bme280.init())
		{
			BME280_Initialized = true;
		} else {
			Serial.println("BME280 Init Failed");
		}
	}

	if (BME280_Initialized == true)
	{
		temperature = bme280.getTemperature();
	}

	return temperature;
}

float GetHumidity()
{
	float humidity = 0;

	if (BME280_Initialized == false)
	{
		if (bme280.init())
		{
			BME280_Initialized = true;
		} else {
			Serial.println("BME280 Init Failed");
		}
	}

	if (BME280_Initialized == true)
	{
		humidity = bme280.getHumidity();
	}

	return humidity;
}