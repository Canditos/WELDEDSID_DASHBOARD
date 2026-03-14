#pragma once
#include <ArduinoOTA.h>
#include "../common/Constants.h"
#include "../common/Types.h"

class OTAManager {
public:
    void begin(const char* deviceId, const char* otaPassword);
    void loop();
};
