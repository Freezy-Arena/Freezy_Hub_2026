#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../network/network_manager.h"
#include "../role_config.h"
#include "../led/led_manager.h"
#include "../websocket/ws_manager.h"

class WebManager {
public:
    WebManager(EthManager& eth, RoleManager& role, LedManager& leds, WsManager& ws);
    void begin();
    void update();

private:
    AsyncWebServer  _server;
    EthManager&     _eth;
    RoleManager&    _role;
    LedManager&     _leds;
    WsManager&      _ws;

    bool        _rebootPending  = false;
    uint32_t    _rebootAt       = 0;

    String _buildPage(const String& message = "");
    String _buildPageSimple(const String& message = "");
    void   _setupRoutes();
};
