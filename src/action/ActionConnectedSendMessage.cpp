#include <Arduino.h>
#include "../Common.h"
#include "ActionConnectedSendMessage.h"

#include <azure_c_shared_utility/platform.h>

#include <ReButton.h>
#include "../input/Input.h"
#include "../azureiot/ReButtonClient2.h"
#include "../helper/ReButtonWiFi.h"
#include "../helper/SasToken.h"
#include "../gencode/pnp_device.h"
#include "../gencode/ReButton_impl.h"

// #include <DevkitDPSClient.h>
#define IIADEMO

#define KEEP_ALIVE			(60)
#define LOG_TRACE			(false)

#define POLLING_INTERVAL	(100)

#define LOOP_WAIT_TIME		(10)	// [msec.]

static const DISPLAY_COLOR_TYPE COLOR_DISCONNECTED = { 255, 255, 255 };
static const DISPLAY_COLOR_TYPE COLOR_CONNECTED    = { 255, 255, 255 };

bool ActionConnectedSendMessage()
{
    Serial.println("ActionConnectedSendMessage() : Enter");
	DisplayStartActionDisconnected({ 255, 0, 255 });

	////////////////////
	// Set display
#ifndef IIADEMO
	DisplayStartActionDisconnected(COLOR_DISCONNECTED);
#endif
	bool isConnected = false;

	if (!ReButtonWiFiConnect())
	{
		Serial.println("Failed to connect to WiFi");
		return false;
	}

	platform_init();

  	////////////////////
  	// Connect Azure

	////////////////////
	// Make sure we are connected
	AutoShutdownSuspend();

	ReButtonClient2 client;

	if (strlen(Config.IoTHubConnectionString) >= 1)
	{
		Serial.printf("ActionConnectedSendMessage() : Connecting to IoT Hub.\n");
		if (!client.ConnectIoTHub(Config.IoTHubConnectionString)) return false;
	}
	else if (strlen(Config.IoTHubConnectionString) <= 0 && strlen(Config.ScopeId) >= 1 && strlen(Config.DeviceId) >= 1 && strlen(Config.SasKey) >= 1)
	{

#ifdef IIADEMO
		DisplayStartFinish({ 0, 255, 0 });
		Serial.println("Wait for Button Click");
		int i = 0;
		while (ReButton::IsJumperShort())
		{
			if (ReButton::IsButtonPressed())
			{
				DisplayStartActionDisconnected(COLOR_DISCONNECTED);
				break;
			} else {
				i++;
			}
			delay(POLLING_INTERVAL * 5);  // 500ms

			if (i == 6)
			{
				DisplayStartFinish({ 0, 0, 0 });
			}
		}

#endif
		Serial.println("ActionConnectedSendMessage() : Connecting to DPS/IoT Central.");

		if (!client.ConnectIoTHubWithDPS(GLOBAL_DEVICE_ENDPOINT, Config.ScopeId, Config.DeviceId, Config.SasKey)) 
		{
			Serial.println("ActionConnectedSendMessage() : Connecting to DPS/IoT Central failed.");
			return false;
		}
		else
		{
			Serial.println("ActionConnectedSendMessage() : Connected to IoT Hub.");
			isConnected = true;
		}
	}
	else
	{
		return false;
	}


	////////////////////
    // Do work
	while (ReButton::IsJumperShort() || ReButton_Property_Pending_Flag != 0)
	{
		if (isConnected)
		{
			DisplayStartActionDisconnected(COLOR_DISCONNECTED);
		}

		if (ReButton::IsButtonPressed())
		{
			InputBegin();
			for (;;)
			{
				InputTask();
				if (!InputIsCapturing())
				{
					Serial.println("InputIsCapturing false");
					break;
				}
				DisplayColor(InputToDisplayColor(InputGetCurrentValue()));
				DigitalTwinClientHelper_Check();
				delay(LOOP_WAIT_TIME);
			}
			INPUT_TYPE input = InputGetConfirmValue();
			Serial.printf("Button is %s.\n", InputGetInputString(input));
			PushButtonInterface_Telemetry_SendAll();

			client.ReportedActionCount++;
			DisplayStartActionConnected(isConnected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
		}

		pnp_device_run();
		delay(POLLING_INTERVAL);
	}

	pnp_device_close();

    Serial.println("ActionConnectedSendMessage() : Complete");

    return true;
}
