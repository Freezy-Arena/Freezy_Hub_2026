#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../network/network_manager.h"
#include "../role_config.h"

class WebManager {
public:
    WebManager(EthManager& eth, RoleManager& role);
    void begin();
    void update();

private:
    AsyncWebServer  _server;
    EthManager&     _eth;
    RoleManager&    _role;

    bool        _rebootPending  = false;
    uint32_t    _rebootAt       = 0;

    String _buildPage(const String& message = "");
    String _buildPageSimple(const String& message = "");
    void   _setupRoutes();
};