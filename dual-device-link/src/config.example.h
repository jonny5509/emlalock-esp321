#pragma once
// Copy this file to src/config.h and fill in real values. Never commit src/config.h.

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define MQTT_HOST "mqtt.example.com"
#define MQTT_PORT 8883

// Use a separate broker username/password on each device.
// Configure broker ACLs to restrict each client to its own topics.
#define DEVICE_MQTT_USER "DEVICE_SPECIFIC_USERNAME"
#define DEVICE_MQTT_PASSWORD "DEVICE_SPECIFIC_PASSWORD"

// Use the same long random, non-identifying value on both devices.
// Treat this as a topic namespace, not as authentication.
#define PAIR_ID "REPLACE_WITH_LONG_RANDOM_PAIR_ID"

// Paste the broker's CA certificate here (PEM). Do not use setInsecure().
static const char BROKER_ROOT_CA[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_BROKER_ROOT_CA_CERTIFICATE_HERE
-----END CERTIFICATE-----
)PEM";
