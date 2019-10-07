#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

double ReadBatteryVoltage();
char* GetSwVersion();
float GetTemperature();
float GetHumidity();

#ifdef __cplusplus
}
#endif


