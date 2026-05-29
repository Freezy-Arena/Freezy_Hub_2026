#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../network/network_manager.h"

class WebManager {
public:
    WebManager(EthManager& eth);
    void begin();

private:
    AsyncWebServer  _server;
    EthManager&     _eth;

    String _buildPage(const String& message = "");
    String _buildPageSimple(const String& message = "");
    void   _setupRoutes();
};