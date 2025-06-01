#ifndef WEBPORTAL_H
#define WEBPORTAL_H

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

class WebPortal {
private:
    WebServer server;
    bool portalActive;
    
    // Configuration structure
    struct Config {
        String deviceId;
        String wifiSSID;
        String wifiPassword;
        String mqttServer;
        int mqttPort;
        String mqttTopicSub;
        String mqttTopicPub;
    };
    
    Config config;
    
    void handleRoot();
    void handleSave();
    void handleReset();
    void sendHTML();
    
public:
    WebPortal();
    bool begin();
    void handle();
    bool loadConfig();
    bool saveConfig();
    void startPortal();
    void stopPortal();
    bool isPortalActive();
    bool isConfigValid();
    
    // Getters
    String getDeviceId() { return config.deviceId; }
    String getWifiSSID() { return config.wifiSSID; }
    String getWifiPassword() { return config.wifiPassword; }
    String getMqttServer() { return config.mqttServer; }
    int getMqttPort() { return config.mqttPort; }
    String getMqttTopicSub() { return config.mqttTopicSub; }
    String getMqttTopicPub() { return config.mqttTopicPub; }
};

#endif