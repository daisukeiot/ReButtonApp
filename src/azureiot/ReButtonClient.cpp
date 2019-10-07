#include <Arduino.h>
#include "../Common.h"
#include "ReButtonClient.h"
#include "../gencode/pnp_device.h"
#include "../helper/SasToken.h"

#include <ReButton.h>
#include <DevkitDPSClient.h>

static const int PROVISIONING_TRY_COUNT = 10;
static const int PROVISIONING_TRY_INTERVAL = 1000;

static void ConnectionStateCallbackFunc(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* context)
{
	ReButtonClient* client = (ReButtonClient*)context;

	if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED && reason == IOTHUB_CLIENT_CONNECTION_OK)
	{
		Serial.println("ConnectionStateCallbackFunc() : Connected");
		client->SetConnected(true);
	}
	else
	{
		Serial.printf("ConnectionStateCallbackFunc() : Disconnected (result:%d reason:%d)\n", result, reason);
		client->SetConnected(false);
	}
}

static void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* context)
{
	ReButtonClient* client = (ReButtonClient*)context;

	if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
	{
		Serial.println("SendEventCallback() : Message sent to Azure IoT Hub");
		client->SetMessageSent();
	}
	else
	{
		Serial.println("SendEventCallback() : Failed to send message to Azure IoT Hub");
	}

	client->DestroyMessage();
}

static void DeviceTwinCallbackFunc(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size, void* context)
{
	ReButtonClient* client = (ReButtonClient*)context;

	Serial.printf("DeviceTwinCallbackFunc() \n%.*s\n", size, payLoad);

	client->DeviceTwinUpdateCallbackInvoke(update_state, payLoad, size);
}

static void DeviceTwinReportCallbackFunc(int result, void* context)
{
	ReButtonClient* client = (ReButtonClient*)context;

	Serial.printf("DeviceTwinReportCallbackFunc() : %d\n", result);
	client->SetDeviceTwinReported();
}

ReButtonClient::ReButtonClient()
{
	_ClientHandle = NULL;
	_MessageHandle = NULL;
	_DeviceTwinUpdateCallbackFunc = NULL;
	_Connected = false;
	_MessageSent = false;
	_DeviceTwinReported = false;
	_IsPnPEnabled = true;
}

ReButtonClient::~ReButtonClient()
{
	Disconnect();
}

void ReButtonClient::DoWork()
{
	IoTHubClient_LL_DoWork(_ClientHandle);
}

bool ReButtonClient::IsAllEventsSent()
{
	IOTHUB_CLIENT_STATUS status;
	if ((IoTHubClient_LL_GetSendStatus(_ClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void ReButtonClient::DeviceTwinUpdateCallbackInvoke(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size)
{
	if (_DeviceTwinUpdateCallbackFunc != NULL) _DeviceTwinUpdateCallbackFunc(update_state, payLoad, size);
}

bool ReButtonClient::Connect(DeviceTwinUpdateCallback callback)
{
	_DeviceTwinUpdateCallbackFunc = callback;

	String iotHubConnectionString;
	if (strlen(Config.IoTHubConnectionString) >= 1)
	{
		iotHubConnectionString = Config.IoTHubConnectionString;
	}
	else if (strlen(Config.IoTHubConnectionString) <= 0 && strlen(Config.ScopeId) >= 1 && strlen(Config.DeviceId) >= 1 && strlen(Config.SasKey) >= 1)
	{
		Serial.println("Device provisioning...");

		Serial.println("Generate SAS token");
		char* deviceSasKeyPtr;
		if (GenerateDeviceSasToken(Config.SasKey, Config.DeviceId, &deviceSasKeyPtr))
		{
			Serial.println("ActionSendMessagePnP() : Generate SAS token failed");
			return false;
		}

		String deviceSasKey(deviceSasKeyPtr);
		free(deviceSasKeyPtr);

		DevkitDPSSetLogTrace(true);
		DevkitDPSSetAuthType(DPS_AUTH_SYMMETRIC_KEY);

		customProvisioningData = CUSTOM_PROVISIONING_DATA;

		for (int i = 0; ; i++)
		{
			Serial.printf("DevkitDPSClientStart %s %s %s %s\r\n", Config.ScopeId, Config.DeviceId, deviceSasKey.c_str(), customProvisioningData);
			if (DevkitDPSClientStart(GLOBAL_DEVICE_ENDPOINT, Config.ScopeId, Config.DeviceId, (char*)deviceSasKey.c_str(), NULL, 0, customProvisioningData))
			{
				Serial.println("DevkitDPSClientStart Success.");
				break;
			}
			else
			{
				Serial.println("DevkitDPSClientStart ERROR.");
			}

			if (i + 1 >= PROVISIONING_TRY_COUNT)
			{
				Serial.println("DPS client for GroupSAS has failed.");
				return false;
			}
			else
			{
				Serial.println("Retrying provisioning.");
			}
			delay(PROVISIONING_TRY_INTERVAL);
		}

		Serial.println("DPS client success!!.");

		iotHubConnectionString = "HostName=";
		iotHubConnectionString += DevkitDPSGetIoTHubURI();
		iotHubConnectionString += ";DeviceId=";
		iotHubConnectionString += DevkitDPSGetDeviceID();
		iotHubConnectionString += ";SharedAccessKey=";
		iotHubConnectionString += Config.SasKey;
	}

	if (pnp_device_initialize(iotHubConnectionString.c_str(), certificates) == 0)
	{
		Serial.println("ActionSendMessagePnP() : pnp_device_initialize Success");
	}
	else
	{
		Serial.println("ActionSendMessagePnP() : pnp_device_initialize Fail");
		return false;
	}

	return true;
}

void ReButtonClient::Disconnect()
{
	if (_ClientHandle != NULL)
	{
		IoTHubClient_LL_Destroy(_ClientHandle);
	}
}

bool ReButtonClient::IsConnected() const
{
	return _Connected;
}

void ReButtonClient::SetConnected(bool connect)
{
	_Connected = connect;
}

bool ReButtonClient::SendMessageAsync(const char* payload)
{
	if (_MessageHandle != NULL) return false;

	_MessageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)payload, strlen(payload));
	if (_MessageHandle == NULL)
	{
		Serial.println("SendMessageAsync() : Message Handle = NULL!");
		return false;
	}

	//MAP_HANDLE propMap = IoTHubMessage_Properties(_MessageHandle);
	//time_t now_utc = time(NULL); // utc time
	//MAP_RESULT res = Map_AddOrUpdate(propMap, "timestamp", ctime(&now_utc));
	//if (res != MAP_OK)
	//{
	//	Serial.println("Adding timestamp property failed");
	//}

	IoTHubMessage_SetContentEncodingSystemProperty(_MessageHandle, "utf-8");
	IoTHubMessage_SetContentTypeSystemProperty(_MessageHandle, "application%2fjson");

	MAP_HANDLE propMap = IoTHubMessage_Properties(_MessageHandle);
	Map_AddOrUpdate(propMap, "productId", Config.ProductId);

	Serial.printf("SendMessageAsync() \n%s\r\n", payload);
	if (IoTHubClient_LL_SendEventAsync(_ClientHandle, _MessageHandle, SendEventCallback, this) != IOTHUB_CLIENT_OK)
	{
		DestroyMessage();
		Serial.println("SendMessageAsync() : Failed to hand over the message to IoTHubClient.");
		return false;
	}
	Serial.println("SendMessageAsync() : IoTHubClient accepted the message for delivery.");

	return true;
}

bool ReButtonClient::IsMessageSent() const
{
	return _MessageSent;
}

void ReButtonClient::SetMessageSent()
{
	_MessageSent = true;
}

void ReButtonClient::DestroyMessage()
{
	IoTHubMessage_Destroy(_MessageHandle);
	_MessageHandle = NULL;
}

bool ReButtonClient::DeviceTwinReport(const char* jsonString)
{
	_DeviceTwinReported = false;

	if (IoTHubClient_LL_SendReportedState(_ClientHandle, (unsigned char*)jsonString, strlen(jsonString), DeviceTwinReportCallbackFunc, this) != IOTHUB_CLIENT_OK)
	{
		Serial.println("DeviceTwinReport() : Failed");
		return false;
	}

	return true;
}

bool ReButtonClient::IsDeviceTwinReported() const
{
	return _DeviceTwinReported;
}

void ReButtonClient::SetDeviceTwinReported()
{
	_DeviceTwinReported = true;
}
