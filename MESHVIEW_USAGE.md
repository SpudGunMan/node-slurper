# Meshview.ino - Meshtastic UDP Packet Decoder

## Overview

This Arduino sketch for ESP32 receives and decodes Meshtastic UDP packets, supporting both encrypted and unencrypted transmissions. It listens on the Meshtastic multicast address (224.0.0.69:4403) and decrypts packets using AES-CTR encryption.

## Features

- ✅ Receives UDP packets from Meshtastic devices
- ✅ Decrypts encrypted packets using AES-CTR mode
- ✅ Supports multiple channel configurations (up to 4)
- ✅ Handles Meshtastic default key variants (PSK 1-10)
- ✅ Decodes Position, Telemetry, and Text messages
- ✅ Channel hash calculation for identification
- ✅ Extensive debugging output

## Setup Instructions

### 1. Hardware Requirements
- ESP32 development board
- WiFi connection

### 2. Software Requirements
- Arduino IDE with ESP32 support
- Meshtastic protobuf files (.pb.h and .pb.c)
  - Download from: https://github.com/meshtastic/protobufs
  - Generate using nanopb compiler
- mbedtls library (included with ESP32 Arduino core)

### 3. Configuration

#### WiFi Settings
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

#### Channel Configuration
Add your Meshtastic channels to the `channels` array:

```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "AQ==", {0}, 0, 0},  // Default channel
  {"MyChannel", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},  // Custom channel
  // ... add more as needed
};
int num_channels = 2;  // Update count
```

### 4. Finding Your Channel Keys

#### Using Meshtastic App
1. Open channel settings
2. View QR code or share link
3. The PSK is shown as a base64 string

#### Using Meshtastic CLI
```bash
meshtastic --info
```

## Channel Key Types

### Default Keys (PSK "AQ==")
- Base64 "AQ==" decodes to 0x01
- Uses Meshtastic default key: `{0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01}`

### Default Variants (PSK "Ag==", "Aw==", etc.)
- Base64 "Ag==" decodes to 0x02, "Aw==" to 0x03, etc.
- Uses default key with last byte incremented: `0x01 + (value - 1)`
- Example: PSK "Ag==" uses key ending in 0x02

### Custom Keys
- Any base64-encoded key (16 or 32 bytes)
- Used directly for AES-128 or AES-256

## How Decryption Works

1. **Nonce Generation**: Created from packet ID and sender node ID
   - Format: `packetId (8 bytes) + fromNode (4 bytes) + padding (4 zeros)`
   
2. **AES-CTR Decryption**: Uses mbedtls library
   - Supports AES-128 (16-byte keys) and AES-256 (32-byte keys)
   
3. **Key Selection**: Tries each configured channel key until successful
   
4. **Validation**: Attempts to decode decrypted data as protobuf

## Debugging Output

The sketch provides detailed debugging information:

```
UDP packets seen: 1
Raw UDP payload (hex): 0D EF BE AD DE ...
Encrypted packet detected. Attempting decryption...
Nonce (hex): 05 56 55 42 00 00 00 00 EF BE AD DE 00 00 00 00
Encrypted data (hex): 55 C8 57 01 21 F1 EE 43 07 A9 14 ...
Trying channel 0 (LongFast, hash=0x8C)...
Decrypted data (hex): 08 01 12 05 68 65 6C 6C 6F ...
Successfully decrypted with channel 0 (LongFast)
Decoded text message: hello
```

## Troubleshooting

### "Decryption worked but protobuf decode failed"
- The key might be incorrect
- Try different channel configurations
- Verify your PSK matches the sender's channel

### "Failed to decrypt packet with any configured key"
- Check that you've added the correct channel
- Verify the base64 key is correct
- Ensure the channel name matches exactly

### No packets received
- Verify WiFi connection
- Check that Meshtastic devices are broadcasting on same network
- Ensure multicast is enabled on your network

## References

- [Meshtastic Encryption Documentation](https://meshtastic.org/docs/overview/encryption/)
- [Meshtastic Protocol Buffers](https://github.com/meshtastic/protobufs)
- [Meshtastic Firmware](https://github.com/meshtastic/firmware)
- [mbedtls Documentation](https://tls.mbed.org/)

## License

This sketch is part of the node-slurper project. See LICENSE file for details.
