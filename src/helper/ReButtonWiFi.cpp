#include <Arduino.h>
#include "../Common.h"
#include "ReButtonWiFi.h"

#include <az3166-driver/EMW10xxInterface.h>
#include <system/SystemWiFi.h>
#include <system/SystemTime.h>
#include <IPAddress.h>

#define WIFI				((EMW10xxInterface*)WiFiInterface())

bool ReButtonWiFiConnect()
{

	////////////////////
	// Connect Wi-Fi

	Serial.println("ActionConnectedSendMessage() : Wi-Fi - Connecting....");
	if (strlen(Config.WiFiSSID) <= 0 && strlen(Config.WiFiPassword) <= 0) return false;

	SetNTPHost(Config.TimeServer);
	InitSystemWiFi();
	WIFI->set_interface(Station);

	if (WIFI->connect(Config.WiFiSSID, Config.WiFiPassword, NSAPI_SECURITY_WPA_WPA2, 0) != 0) 
	{
		Serial.println("Wifi connection failed");
		return false;
	}
	else
	{
		Serial.println("Wifi connected!!");
	}
	
	SyncTime();

	if (IsTimeSynced() == 0)
	{
		time_t t = time(NULL);
		Serial.printf("Now is (UTC): %s\r\n", ctime(&t));
	}
	else
	{
		Serial.println("Time sync failed");
	}

	IPAddress ip;
	ip.fromString(WIFI->get_ip_address());
	Serial.printf("ActionConnectedSendMessage() : IP address is %s.\n", ip.get_address());

    return true;
}