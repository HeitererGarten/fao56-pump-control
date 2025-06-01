#include <Arduino.h>
// Pin definitions
#define RELAY_PIN 32
#define LED_PIN 2
#define DEVICE_ID "P-1"

// WiFi credentials
#define WIFI_SSID "asus"
#define WIFI_PASSWORD "12345678"

// MQTT settings
#define MQTT_SERVER "192.168.1.245"
#define MQTT_PORT 1883
#define MQTT_TOPIC_SUB "topic/pump/command"
#define MQTT_TOPIC_PUB "topic/pump/status"

// NTP settings for Bangkok time (UTC+7)
#define ntpServer "pool.ntp.org"
#define gmtOffset_sec (7 * 3600)
#define daylightOffset_sec 0

#define minIrrMinutes 0 
#define maxIrrMinutes 480 