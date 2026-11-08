# Two-device link proof of concept

This folder is an isolated communications prototype for the two-device design. It does **not** replace the existing dashboard firmware.

## Scope and safety

- Two ESP32 boards use separate Wi-Fi networks.
- Both connect outbound over MQTT/TLS to a broker you control.
- The wearer unit publishes an online heartbeat and answers a harmless `PING` diagnostic with `PONG`.
- The keyholder unit displays whether the wearer has been seen recently and enables the diagnostic ping only while the wearer is online.
- No lock, actuator, timer, or release command is implemented here. Do not connect this prototype to a physical locking mechanism.
- A communication test is not proof of safety or of compatibility with the original project.

## Why MQTT

ESP-NOW is local radio and cannot bridge two unrelated internet connections. MQTT through a broker provides the relay. Each ESP32 makes an outbound TLS connection, so port forwarding is not needed.

## Broker requirements

Use a private MQTT broker you administer (for example, a Mosquitto instance) with:
- TLS listener on port 8883 and a valid server certificate;
- unique credentials for the wearer and keyholder clients;
- ACLs limiting each client to its own publish topics and the partner's status/inbox topics;
- retained messages disabled for inbox/command topics;
- no anonymous access.

Do not use a public test broker or reuse the same broker password on both devices.

## Configure

1. Copy `config.example.h` to `src/config.h`.
2. Fill in Wi-Fi, broker host, per-device MQTT username/password, the broker root CA PEM, and a long random `PAIR_ID` (same on both devices).
3. Use different `DEVICE_MQTT_USER` and `DEVICE_MQTT_PASSWORD` values for each device.
4. Build and upload one environment per board:

```sh
pio run -e wearer
pio run -e wearer -t upload
pio run -e keyholder
pio run -e keyholder -t upload
```

The local dashboard is served on port 80 at the device's LAN IP. The keyholder dashboard's **Send test ping** button sends only a diagnostic message.

## Expected end-to-end test

1. Connect the wearer board to Wi-Fi A and the keyholder board to Wi-Fi B.
2. Open each board's local dashboard using its own LAN IP.
3. The keyholder page should show **Wearer online** within about 15 seconds.
4. Press **Send test ping**. The keyholder page should report a successful send, and the wearer page should show the received ping and reply.
5. The keyholder page should change to **Wearer offline** after the wearer is powered off and the heartbeat expires (about 35 seconds). The ping button is disabled while offline.
6. Power the wearer board back on; it should reconnect and return to online status.

## Limits

This prototype has not been built against your exact ESP32 boards or tested on your broker/hardware from this environment. It provides a concrete end-to-end diagnostic transport, but you must run the build and two-network test above before calling the link verified. The MQTT broker is a required third-party service; it is not included or hosted by this repository.
