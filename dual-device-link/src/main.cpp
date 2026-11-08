#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <time.h>
#include "config.h"

#if !defined(DEVICE_ROLE_WEARER) && !defined(DEVICE_ROLE_KEYHOLDER)
#error "Select the wearer or keyholder PlatformIO environment"
#endif

#if defined(DEVICE_ROLE_WEARER) && defined(DEVICE_ROLE_KEYHOLDER)
#error "Select exactly one device role"
#endif

static constexpr uint32_t HEARTBEAT_MS = 10000;
static constexpr uint32_t ONLINE_TIMEOUT_MS = 35000;
static constexpr uint32_t RECONNECT_MS = 5000;

WiFiClientSecure tls;
PubSubClient mqtt(tls);
WebServer server(80);

String rootTopic;
String wearerStatusTopic, keyholderStatusTopic;
String wearerInboxTopic, keyholderInboxTopic;
String localStatus = "Starting";
String lastMessage = "No messages yet";
uint32_t lastHeartbeatSent = 0;
uint32_t lastReconnectAttempt = 0;
uint32_t lastWearerSeen = 0;
uint32_t lastPingSent = 0;
bool wearerOnline = false;
bool pingPending = false;

#if defined(DEVICE_ROLE_WEARER)
static constexpr bool IS_WEARER = true;
static const char *ROLE_NAME = "Wearer";
#else
static constexpr bool IS_WEARER = false;
static const char *ROLE_NAME = "Keyholder";
#endif

String dashboardHtml() {
  String html = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Two-device link</title><style>body{font:16px system-ui;max-width:620px;margin:3rem auto;padding:0 1rem}"
                  ".status{padding:1rem;background:#eee;border-radius:8px}button{padding:.8rem 1rem}</style></head><body>");
  html += F("<h1>");
  html += ROLE_NAME;
  html += F(" dashboard</h1><div class='status'><b>Wi-Fi:</b> ");
  html += WiFi.isConnected() ? "Connected" : "Disconnected";
  html += F("<br><b>Relay:</b> ");
  html += mqtt.connected() ? "Connected" : "Disconnected";
  html += F("<br><b>Wearer device:</b> ");
  html += IS_WEARER ? "This device" : (wearerOnline ? "Online" : "Offline");
  html += F("<br><b>Last event:</b> ");
  html += lastMessage;
  html += F("</div>");
#if defined(DEVICE_ROLE_KEYHOLDER)
  html += F("<p><form method='POST' action='/ping'><button ");
  html += wearerOnline ? "" : "disabled";
  html += F(">Send test ping</button></form></p>");
  html += F("<small>The test ping is diagnostic only; it cannot operate a lock or actuator.</small>");
#else
  html += F("<p>This dashboard reports link diagnostics. No physical control is exposed.</p>");
#endif
  html += F("<p><small>Refresh this page to update status.</small></p></body></html>");
  return html;
}

void publishStatus(const char *status) {
  if (!mqtt.connected()) return;
  const String topic = IS_WEARER ? wearerStatusTopic : keyholderStatusTopic;
  mqtt.publish(topic.c_str(), status, false);
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String body;
  for (unsigned int i = 0; i < length; ++i) body += static_cast<char>(payload[i]);
  const String receivedTopic(topic);

#if defined(DEVICE_ROLE_WEARER)
  if (receivedTopic == wearerInboxTopic && body == "PING") {
    lastMessage = "Received diagnostic PING; replying PONG";
    mqtt.publish(keyholderInboxTopic.c_str(), "PONG", false);
  }
#else
  if (receivedTopic == keyholderInboxTopic && body == "PONG") {
    pingPending = false;
    lastMessage = "Wearer replied PONG";
  }
  if (receivedTopic == wearerStatusTopic && body == "ONLINE") {
    lastWearerSeen = millis();
    wearerOnline = true;
  }
  if (receivedTopic == wearerStatusTopic && body == "OFFLINE") {
    wearerOnline = false;
  }
#endif
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return false;
  const String clientId = String(IS_WEARER ? "wearer-" : "keyholder-") +
                          String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  if (!mqtt.connect(clientId.c_str(), DEVICE_MQTT_USER, DEVICE_MQTT_PASSWORD)) {
    localStatus = "Relay connection failed";
    return false;
  }

  if (IS_WEARER) {
    mqtt.subscribe(wearerInboxTopic.c_str(), 1);
  } else {
    mqtt.subscribe(wearerStatusTopic.c_str(), 1);
    mqtt.subscribe(keyholderInboxTopic.c_str(), 1);
  }
  localStatus = "Relay connected";
  publishStatus("ONLINE");
  return true;
}

void handlePing() {
#if defined(DEVICE_ROLE_KEYHOLDER)
  if (!mqtt.connected() || !wearerOnline ||
      millis() - lastWearerSeen > ONLINE_TIMEOUT_MS) {
    wearerOnline = false;
    server.send(409, "text/plain", "Wearer device is offline; diagnostic ping not sent.");
    return;
  }
  if (pingPending && millis() - lastPingSent < 5000) {
    server.send(429, "text/plain", "A diagnostic ping is already pending.");
    return;
  }
  pingPending = mqtt.publish(wearerInboxTopic.c_str(), "PING", false);
  lastPingSent = millis();
  lastMessage = pingPending ? "Diagnostic PING sent; awaiting PONG" : "Could not send PING";
  server.send(pingPending ? 202 : 503, "text/plain", lastMessage);
#else
  server.send(405, "text/plain", "Not available on wearer dashboard.");
#endif
}

void setup() {
  Serial.begin(115200);

  rootTopic = String("emlalock/") + PAIR_ID;
  wearerStatusTopic = rootTopic + "/wearer/status";
  keyholderStatusTopic = rootTopic + "/keyholder/status";
  wearerInboxTopic = rootTopic + "/wearer/inbox";
  keyholderInboxTopic = rootTopic + "/keyholder/inbox";

  tls.setCACert(BROKER_ROOT_CA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(5);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  localStatus = "Connecting to Wi-Fi";

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", dashboardHtml());
  });
  server.on("/ping", HTTP_POST, handlePing);
  server.begin();
}

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED && !mqtt.connected() &&
      millis() - lastReconnectAttempt >= RECONNECT_MS) {
    lastReconnectAttempt = millis();
    connectMqtt();
  }

  if (mqtt.connected()) {
    mqtt.loop();
#if defined(DEVICE_ROLE_WEARER)
    if (millis() - lastHeartbeatSent >= HEARTBEAT_MS) {
      lastHeartbeatSent = millis();
      publishStatus("ONLINE");
    }
#else
    wearerOnline = wearerOnline && (millis() - lastWearerSeen <= ONLINE_TIMEOUT_MS);
#endif
  } else {
    wearerOnline = false;
  }

  delay(2);
}
