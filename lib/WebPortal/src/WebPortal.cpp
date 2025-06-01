#include "WebPortal.h"

WebPortal::WebPortal() : server(80), portalActive(false) {
    // Initialize default config
    config.deviceId = "";
    config.wifiSSID = "";
    config.wifiPassword = "";
    config.mqttServer = "";
    config.mqttPort = 1883;
    config.mqttTopicSub = "topic/pump/command";
    config.mqttTopicPub = "topic/pump/status";
}

bool WebPortal::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return false;
    }
    return true;
}

bool WebPortal::loadConfig() {
    File file = SPIFFS.open("/config.json", "r");
    if (!file) {
        Serial.println("Config file not found");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }
    
    config.deviceId = doc["deviceId"].as<String>();
    config.wifiSSID = doc["wifiSSID"].as<String>();
    config.wifiPassword = doc["wifiPassword"].as<String>();
    config.mqttServer = doc["mqttServer"].as<String>();
    config.mqttPort = doc["mqttPort"] | 1883;
    config.mqttTopicSub = doc["mqttTopicSub"].as<String>();
    config.mqttTopicPub = doc["mqttTopicPub"].as<String>();
    
    Serial.println("Config loaded successfully");
    return true;
}

bool WebPortal::saveConfig() {
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to create config file");
        return false;
    }
    
    JsonDocument doc;
    doc["deviceId"] = config.deviceId;
    doc["wifiSSID"] = config.wifiSSID;
    doc["wifiPassword"] = config.wifiPassword;
    doc["mqttServer"] = config.mqttServer;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttTopicSub"] = config.mqttTopicSub;
    doc["mqttTopicPub"] = config.mqttTopicPub;
    
    serializeJson(doc, file);
    file.close();
    
    Serial.println("Config saved successfully");
    return true;
}

bool WebPortal::isConfigValid() {
    return (config.deviceId.length() > 0 && 
            config.wifiSSID.length() > 0 && 
            config.wifiPassword.length() > 0 && 
            config.mqttServer.length() > 0);
}

void WebPortal::startPortal() {
    if (portalActive) return;
    
    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PumpConfig", "12345678");
    
    Serial.println("Configuration Portal Started");
    Serial.print("Connect to WiFi: PumpConfig");
    Serial.println(" (Password: 12345678)");
    Serial.print("Open browser to: http://");
    Serial.println(WiFi.softAPIP());
    
    // Setup web server routes
    server.on("/", [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/reset", [this]() { handleReset(); });
    
    server.begin();
    portalActive = true;
}

void WebPortal::stopPortal() {
    if (!portalActive) return;
    
    server.stop();
    WiFi.softAPdisconnect();
    portalActive = false;
    Serial.println("Configuration Portal Stopped");
}

void WebPortal::handle() {
    if (portalActive) {
        server.handleClient();
    }
}

bool WebPortal::isPortalActive() {
    return portalActive;
}

void WebPortal::handleRoot() {
    sendHTML();
}

void WebPortal::handleSave() {
    // Get form data
    config.deviceId = server.arg("deviceId");
    config.wifiSSID = server.arg("wifiSSID");
    String newPassword = server.arg("wifiPassword");
    config.mqttServer = server.arg("mqttServer");
    config.mqttPort = server.arg("mqttPort").toInt();
    config.mqttTopicSub = server.arg("mqttTopicSub");
    config.mqttTopicPub = server.arg("mqttTopicPub");
    
    // Only update password if a new one is provided
    if (newPassword.length() > 0) {
        config.wifiPassword = newPassword;
    }
    // If empty and no existing password, it's an error
    else if (config.wifiPassword.length() == 0) {
        server.send(400, "text/html", 
            "<html><body><h2>Error: WiFi Password is required!</h2>"
            "<a href='/'>Go Back</a></body></html>");
        return;
    }
    
    // Validate required fields
    if (config.deviceId.length() == 0 || config.wifiSSID.length() == 0 || 
        config.mqttServer.length() == 0) {
        server.send(400, "text/html", 
            "<html><body><h2>Error: All fields are required!</h2>"
            "<a href='/'>Go Back</a></body></html>");
        return;
    }
    
    // Save configuration
    if (saveConfig()) {
        server.send(200, "text/html", 
            "<html><body><h2>Configuration Saved!</h2>"
            "<p>Device will restart in 5 seconds...</p>"
            "<script>setTimeout(function(){ window.close(); }, 5000);</script>"
            "</body></html>");
        
        // Restart after 5 seconds
        delay(5000);
        ESP.restart();
    } else {
        server.send(500, "text/html", 
            "<html><body><h2>Failed to save configuration!</h2>"
            "<a href='/'>Go Back</a></body></html>");
    }
}

void WebPortal::handleReset() {
    SPIFFS.remove("/config.json");
    server.send(200, "text/html", 
        "<html><body><h2>Configuration Reset!</h2>"
        "<p>Device will restart in 3 seconds...</p>"
        "<script>setTimeout(function(){ window.location='/'; }, 3000);</script>"
        "</body></html>");
    
    delay(3000);
    ESP.restart();
}

void WebPortal::sendHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Pump Configuration</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 40px; background: #f0f0f0; }
        .container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; }
        h1 { color: #2c3e50; text-align: center; margin-bottom: 30px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }
        input[type="text"], input[type="password"], input[type="number"] { 
            width: 100%; padding: 12px; border: 2px solid #bdc3c7; border-radius: 5px; 
            font-size: 16px; box-sizing: border-box; 
        }
        input:focus { outline: none; border-color: #3498db; }
        .btn { 
            background: #3498db; color: white; padding: 15px 30px; border: none; 
            border-radius: 5px; cursor: pointer; font-size: 16px; margin-right: 10px; 
        }
        .btn:hover { background: #2980b9; }
        .btn-danger { background: #e74c3c; }
        .btn-danger:hover { background: #c0392b; }
        .button-group { text-align: center; margin-top: 30px; }
        .info { background: #ecf0f1; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .password-hint { font-size: 12px; color: #7f8c8d; margin-top: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Pump Configuration</h1>
        
        <div class="info">
            <strong>Current Status:</strong><br>
            Device ID: )rawliteral" + config.deviceId + R"rawliteral(<br>
            WiFi SSID: )rawliteral" + config.wifiSSID + R"rawliteral(<br>
            MQTT Server: )rawliteral" + config.mqttServer + R"rawliteral(
        </div>
        
        <form action="/save" method="POST">
            <div class="form-group">
                <label for="deviceId">Device ID:</label>
                <input type="text" id="deviceId" name="deviceId" value=")rawliteral" + config.deviceId + R"rawliteral(" placeholder="P-1" required>
            </div>
            
            <div class="form-group">
                <label for="wifiSSID">WiFi SSID:</label>
                <input type="text" id="wifiSSID" name="wifiSSID" value=")rawliteral" + config.wifiSSID + R"rawliteral(" placeholder="Your WiFi Network" required>
            </div>
            
            <div class="form-group">
                <label for="wifiPassword">WiFi Password:</label>
                <input type="password" id="wifiPassword" name="wifiPassword" value="" placeholder="Enter new password or leave blank to keep current" required>
                <div class="password-hint">)rawliteral";
    
    // Only show password hint if there's an existing password
    if (config.wifiPassword.length() > 0) {
        html += "Current password is set (hidden for security)";
    } else {
        html += "No password currently set";
    }
    
    html += R"rawliteral(</div>
            </div>
            
            <div class="form-group">
                <label for="mqttServer">MQTT Server IP:</label>
                <input type="text" id="mqttServer" name="mqttServer" value=")rawliteral" + config.mqttServer + R"rawliteral(" placeholder="192.168.1.100" required>
            </div>
            
            <div class="form-group">
                <label for="mqttPort">MQTT Port:</label>
                <input type="number" id="mqttPort" name="mqttPort" value=")rawliteral" + String(config.mqttPort) + R"rawliteral(" placeholder="1883">
            </div>
            
            <div class="form-group">
                <label for="mqttTopicSub">MQTT Subscribe Topic:</label>
                <input type="text" id="mqttTopicSub" name="mqttTopicSub" value=")rawliteral" + config.mqttTopicSub + R"rawliteral(" placeholder="topic/pump/command">
            </div>
            
            <div class="form-group">
                <label for="mqttTopicPub">MQTT Publish Topic:</label>
                <input type="text" id="mqttTopicPub" name="mqttTopicPub" value=")rawliteral" + config.mqttTopicPub + R"rawliteral(" placeholder="topic/pump/status">
            </div>
            
            <div class="button-group">
                <button type="submit" class="btn">Save Configuration</button>
                <button type="button" class="btn btn-danger" onclick="if(confirm('Reset all settings?')) window.location='/reset'">Reset</button>
            </div>
        </form>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}