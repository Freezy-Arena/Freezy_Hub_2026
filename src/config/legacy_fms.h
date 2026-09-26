#pragma once
#include <Preferences.h>
#include "../role_config.h"

// Run before any manager loads preferences. Existing hub settings win, and
// legacy keys remain untouched for rollback to the original firmware.
inline void migrateLegacyFmsSettings() {
    Preferences old, role;
    if (!old.begin("settings", true)) return;
    String name = old.getString("deviceRole", "");
    if (name != "FMS_TABLE" && name != "RED_ALLIANCE" && name != "BLUE_ALLIANCE") {
        old.end();
        return;
    }
    role.begin("role", false);
    if (role.isKey("role")) {
        role.end();
        old.end();
        return;
    }
    Preferences net, ws;
    net.begin("network", false);
    if (!net.isKey("useDHCP")) net.putBool("useDHCP", old.getBool("useDHCP", true));
    if (!net.isKey("staticIP")) net.putString("staticIP", old.getString("deviceIP", "10.0.100.240"));
    // deviceGWIP is the key used at boot and saved by the legacy web form.
    if (!net.isKey("staticGW")) net.putString("staticGW", old.getString("deviceGWIP", "10.0.100.3"));
    net.end();
    ws.begin("websocket", false);
    if (!ws.isKey("arenaHost")) ws.putString("arenaHost", old.getString("arenaIP", "10.0.100.5"));
    long port = old.getString("arenaPort", "8080").toInt();
    if (!ws.isKey("arenaPort")) ws.putUShort("arenaPort", port > 0 && port <= 65535 ? port : 8080);
    // Both HTTP and WebSocket intentionally use the saved arena destination.
    ws.end();
    uint8_t id = name == "RED_ALLIANCE" ? ROLE_RED_ALLIANCE
               : name == "BLUE_ALLIANCE" ? ROLE_BLUE_ALLIANCE : ROLE_FMS_TABLE;
    role.putUChar("role", id); // Commit migration last; repeat safely if interrupted.
    role.end();
    old.end();
    Serial.printf("[CONFIG] Imported legacy %s settings; original keys retained\n", name.c_str());
}
