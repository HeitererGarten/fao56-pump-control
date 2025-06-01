#include "config.h"
#include "WebPortal.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define CONFIG_BUTTON_PIN 0  // GPIO 0 (BOOT button)

// System states
enum PumpState {
    IDLE,
    IRRIGATING,
    EMERGENCY_HALT,
    FAULT
};

// Global variables
PumpState currentState = IDLE;
unsigned long irrigationStartTime = 0;
unsigned long irrigationDuration = 0;
unsigned long remainingTime = 0;
bool pumpActive = false;
unsigned long lastButtonCheck = 0;
bool buttonPressed = false;

WiFiClient espClient;
PubSubClient client(espClient);
JsonDocument doc;
WebPortal portal;

// Dynamic configuration variables
String deviceId;
String wifiSSID;
String wifiPassword;
String mqttServer;
int mqttPort;
String mqttTopicSub;
String mqttTopicPub;

// Function declarations
void setupWiFi();
void setupMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void handleStateTransitions();
void controlPump(bool state);
void publishStatus();
bool isIrrigationTime();
void setupTime();
void checkConfigButton();

void setup() {
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize portal
    portal.begin();
    
    // Load configuration
    if (!portal.loadConfig() || !portal.isConfigValid()) {
        Serial.println("No valid configuration found, starting portal...");
        portal.startPortal();
        while (portal.isPortalActive()) {
            portal.handle();
            delay(10);
        }
    }
    
    // Get configuration values
    deviceId = portal.getDeviceId();
    wifiSSID = portal.getWifiSSID();
    wifiPassword = portal.getWifiPassword();
    mqttServer = portal.getMqttServer();
    mqttPort = portal.getMqttPort();
    mqttTopicSub = portal.getMqttTopicSub();
    mqttTopicPub = portal.getMqttTopicPub();
    
    Serial.println("Configuration loaded:");
    Serial.println("Device ID: " + deviceId);
    Serial.println("WiFi SSID: " + wifiSSID);
    Serial.println("MQTT Server: " + mqttServer);
    
    // Setup connections
    setupWiFi();
    setupTime();
    setupMQTT();
    
    // Connect to MQTT broker
    while (!client.connected()) {
        reconnectMQTT();
    }
    
    Serial.println("Pump Control System Initialized");
    publishStatus();
}

void loop() {
    // Check config button (only in IDLE state)
    if (currentState == IDLE) {
        checkConfigButton();
    }
    
    // Handle portal if active
    if (portal.isPortalActive()) {
        portal.handle();
        return;  // Skip normal operation while portal is active
    }
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        setupWiFi();
    }
    
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
    
    handleStateTransitions();
    delay(1000);
}

void checkConfigButton() {
    if (millis() - lastButtonCheck > 100) {  // Debounce
        bool currentButtonState = !digitalRead(CONFIG_BUTTON_PIN);  // Inverted because of pullup
        
        if (currentButtonState && !buttonPressed) {
            buttonPressed = true;
            Serial.println("Config button pressed - Starting portal...");
            portal.startPortal();
        } else if (!currentButtonState) {
            buttonPressed = false;
        }
        
        lastButtonCheck = millis();
    }
}

void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(wifiSSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed - starting config portal");
        portal.startPortal();
    }
}

void setupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Waiting for time synchronization...");
    
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    if (attempts < 10) {
        Serial.println("Time synchronized");
    } else {
        Serial.println("Time sync failed, continuing...");
    }
}

void setupMQTT() {
    client.setServer(mqttServer.c_str(), mqttPort);
    client.setCallback(callback);
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "PumpController-" + deviceId;
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            client.subscribe(mqttTopicSub.c_str());
        } 
        else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("Message received: ");
    Serial.println(message);
    
    // Parse JSON
    doc.clear();
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    String signal = doc["signal"];
    float irr_time = doc["irr_time"];
    String targetId = doc["id"];
    
    // Check if message is for this device
    if (targetId != deviceId) {
        Serial.println("Message not for this device, ignoring...");
        return;
    }
    
    Serial.println("Message is for this device!");
    
    // Validate irrigation time
    if (signal == "On" && irr_time <= minIrrMinutes) {
        Serial.print("Invalid irrigation time: must be greater than ");
		Serial.println(minIrrMinutes);
        return;
    }
    
    if (signal == "On" && irr_time > maxIrrMinutes) {
        Serial.print("Irrigation time too long: maximum is ");
		Serial.print(maxIrrMinutes);
		Serial.println(" minutes");
		Serial.print("Received: ");
		Serial.println(irr_time);
        return;
    }
    
    // Handle commands based on current state and signal
    if (signal == "On" && currentState == IDLE && isIrrigationTime()) {
        Serial.println("Starting irrigation");
        irrigationDuration = (unsigned long) (irr_time * 60 * 1000);
        remainingTime = irrigationDuration;
        irrigationStartTime = millis();
        currentState = IRRIGATING;
        controlPump(true);
        publishStatus();
    }
    else if (signal == "On" && currentState == EMERGENCY_HALT && isIrrigationTime()) {
        Serial.println("Resuming from emergency halt");
        irrigationDuration = remainingTime;
        irrigationStartTime = millis();
        currentState = IRRIGATING;
        controlPump(true);
        publishStatus();
    }
    else if (signal == "Emergency Halt" && currentState == IRRIGATING) {
        Serial.println("Emergency halt");
        unsigned long elapsed = millis() - irrigationStartTime;
        remainingTime = irrigationDuration - elapsed;
        currentState = EMERGENCY_HALT;
        controlPump(false);
        publishStatus();
    }
    else if (signal == "Stop") {
        Serial.println("Stopping");
        currentState = IDLE;
        irrigationDuration = 0;
        remainingTime = 0;
        controlPump(false);
        publishStatus();
    }
	else {
        Serial.println("No conditions met! Checking why:");
        Serial.print("Signal == 'On'? ");
        Serial.println(signal == "On");
        Serial.print("State == IDLE? ");
        Serial.println(currentState == IDLE);
        Serial.print("Is irrigation time? ");
        Serial.println(isIrrigationTime());
    }
}

void handleStateTransitions() {
    unsigned long currentTime = millis();
    
    switch (currentState) {
        case IDLE:
            break;
            
        case IRRIGATING:
            if (!isIrrigationTime()) {
                currentState = EMERGENCY_HALT;
                controlPump(false);
                publishStatus();
            } else {
                unsigned long elapsed = currentTime - irrigationStartTime;
                if (elapsed >= irrigationDuration) {
                    currentState = IDLE;
                    irrigationDuration = 0;
                    remainingTime = 0;
                    controlPump(false);
                    publishStatus();
                    Serial.println("Irrigation completed!");
                } else {
                    remainingTime = irrigationDuration - elapsed;
                    unsigned long remainingMinutes = remainingTime / (60 * 1000);
                    unsigned long remainingSeconds = (remainingTime % (60 * 1000)) / 1000;
                    
                    Serial.print("Remaining: ");
                    Serial.print(remainingMinutes);
                    Serial.print(":");
                    Serial.println(remainingSeconds);
                }
            }
            break;
            
        case EMERGENCY_HALT:
            break;
            
        case FAULT:
            controlPump(false);
            break;
    }
}

void controlPump(bool state) {
    pumpActive = state;
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    digitalWrite(LED_PIN, state ? HIGH : LOW);
    
    Serial.print("Pump ");
    Serial.println(state ? "ON" : "OFF");
}

void publishStatus() {
    doc.clear();
    
    String stateStr;
    switch (currentState) {
        case IDLE: stateStr = "IDLE"; break;
        case IRRIGATING: stateStr = "IRRIGATING"; break;
        case EMERGENCY_HALT: stateStr = "EMERGENCY_HALT"; break;
        case FAULT: stateStr = "FAULT"; break;
    }
    
    doc["id"] = deviceId;
    doc["state"] = stateStr;
    doc["pump_active"] = pumpActive;
    doc["remaining_time_minutes"] = remainingTime / (60 * 1000);
    doc["irrigation_allowed"] = isIrrigationTime();
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        doc["current_time"] = timeStr;
    }
    
    String output;
    serializeJson(doc, output);
    
    client.publish(mqttTopicPub.c_str(), output.c_str());
    Serial.println("Status published: " + output);
}

bool isIrrigationTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    
    int hour = timeinfo.tm_hour;
    return ((hour >= 7 && hour < 9) || (hour >= 16 && hour < 19));
}