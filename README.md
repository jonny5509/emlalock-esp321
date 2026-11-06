# EmlaLock ESP32

ESP32 dashboard for monitoring an EmlaLock session, with a wearer-to-keyholder handover setup flow.

## Handover flow

1. The wearer initially configures Wi-Fi, their EmlaLock User ID, and their API key.
2. The wearer selects **Hand Over** from the dashboard.
3. The firmware preserves the wearer User ID and API key in ESP32 NVS, clears the saved Wi-Fi SSID/password and any previous keyholder API key, and restarts the setup access point.
4. The keyholder connects to the setup network and enters their own Wi-Fi details and their own EmlaLock API key. The wearer User ID and API key are not shown or requested.
5. The dashboard runs in Key Holder mode.

The firmware uses the stored wearer credentials for session information and for the documented Add Time endpoint. For Subtract Time, it sends both the stored wearer credentials and the keyholder API key. EmlaLock's documented Add Time endpoint does not accept a holder API key; this is an API limitation, not a firmware setting.

## Security

- Credentials are stored locally in ESP32 NVS and are not committed to this repository.
- The wearer User ID and API key are never returned by the local configuration/status endpoints.
- The setup AP credentials in the inherited firmware are defaults; change them before deployment.
- The inherited firmware uses `WiFiClientSecure::setInsecure()` and an unauthenticated local HTTP dashboard. Do not expose the device or setup AP to untrusted networks. Review and harden these areas before real-world use.
- This project is not a certified security product.

## Build

Install PlatformIO Core and run:

```sh
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1
pio run -e esp32-s2-saola-1
pio run -e esp32-s3-devkitc-1
```

Supported boards: ESP32, ESP32-C3, ESP32-S2, and ESP32-S3.
