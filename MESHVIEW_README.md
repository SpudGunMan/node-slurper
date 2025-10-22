# Meshview.ino - Meshtastic UDP Packet Decoder for ESP32

A fully-featured Arduino sketch for ESP32 that receives and decodes Meshtastic UDP packets with support for AES-CTR encryption.

## Features

- ✅ **Encrypted Packet Support**: Decrypts packets using AES-CTR encryption (compatible with Meshtastic firmware)
- ✅ **Multiple Channel Support**: Configure up to 4 channels with different encryption keys
- ✅ **Auto-detection**: Automatically tries configured channels when decrypting
- ✅ **Channel Hash Calculation**: Implements Meshtastic's XOR-based channel hash
- ✅ **Message Type Decoding**: Supports Position, Telemetry, and Text messages
- ✅ **Base64 Key Decoding**: Converts base64 PSK keys to binary
- ✅ **Default Key Support**: Handles special PSK values (1-10) with default Meshtastic key

## Requirements

### Hardware
- ESP32 development board (any variant with WiFi)

### Software Libraries
- **WiFi** (built-in with ESP32)
- **WiFiUdp** (built-in with ESP32)
- **mbedtls** (built-in with ESP32)
- **nanopb** - For Protocol Buffer decoding
- **Meshtastic protobuf definitions** - `.pb.h` and `.pb.c` files

### Getting Protobuf Files

1. Clone the Meshtastic protobufs repository:
   ```bash
   git clone https://github.com/meshtastic/protobufs.git
   ```

2. Generate the nanopb files using the protobuf compiler. You'll need:
   - `mesh.pb.h` and `mesh.pb.c`
   - `portnums.pb.h` and `portnums.pb.c`
   - `telemetry.pb.h` and `telemetry.pb.c`

3. Place these files in the same directory as `meshview.ino`

## Configuration

### 1. WiFi Settings

Edit the WiFi credentials in the sketch:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 2. Channel Configuration

Configure your Meshtastic channels in the `channels` array:

```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},  // Channel 0
  {"", "", {0}, 0, 0},  // Channel 1 (unused)
  {"", "", {0}, 0, 0},  // Channel 2 (unused)
  {"", "", {0}, 0, 0}   // Channel 3 (unused)
};
```

Each channel needs:
- **name**: Channel name (e.g., "LongFast", "MediumSlow")
- **psk_base64**: Base64-encoded PSK from your Meshtastic device

### Getting Your PSK Key

#### Method 1: Meshtastic CLI
```bash
meshtastic --info
```

Look for the channel settings and copy the base64 PSK.

#### Method 2: Meshtastic App
1. Open Meshtastic app
2. Go to Settings → Radio Config → Channels
3. Select your channel
4. Copy the PSK value (base64 encoded)

#### Method 3: Special PSK Values
- Use `"1"` for the default Meshtastic key
- Use `"2"` through `"10"` for default key variants (increments last byte)

### 3. Multicast Settings (Optional)

Default Meshtastic UDP multicast settings (usually don't need to change):
```cpp
const char* MCAST_GRP = "224.0.0.69";
const uint16_t MCAST_PORT = 4403;
```

## Installation

1. Install Arduino IDE with ESP32 board support
2. Install required libraries (nanopb)
3. Copy protobuf files to sketch directory
4. Open `meshview.ino` in Arduino IDE
5. Configure WiFi and channel settings
6. Select your ESP32 board from Tools → Board
7. Upload to your ESP32

## Usage

1. Power on your ESP32
2. Open Serial Monitor at 115200 baud
3. Watch as it:
   - Scans for WiFi networks
   - Connects to your WiFi
   - Initializes channels and decodes keys
   - Starts listening for UDP multicast packets
   - Receives and decrypts Meshtastic packets

### Example Output

```
WiFi connected.
IP address: 192.168.1.100
Initializing channels...
Channel 0: name='LongFast', key_length=16, hash=0x42
UDP multicast listener started.

UDP packets seen: 1
Raw UDP payload (hex): 08 A1 B2 ...
id: 123456789
from: 987654321
to: 4294967295
channel: 0
Encrypted packet detected. Attempting decryption...
Trying channel 0 (LongFast)...
Successfully decrypted with channel 0!
Portnum: 1
Decoded text message: Hello Mesh!
```

## How It Works

### Encryption/Decryption

Meshtastic uses AES encryption in CTR (Counter) mode:

1. **Nonce Generation**: Combines packet ID and sender node ID
   ```
   nonce = [packetId (4 bytes)] + [fromNode (4 bytes)] + [zeros (8 bytes)]
   ```

2. **AES-CTR Decryption**: Uses the nonce and channel key
   - Supports both AES-128 (16-byte key) and AES-256 (32-byte key)

3. **Channel Matching**: 
   - Tries each configured channel
   - Verifies decryption by attempting protobuf decode
   - Reports which channel successfully decrypted the packet

### Channel Hash

Meshtastic uses a simple XOR hash for channel identification:
```cpp
hash = XOR(channel_name_bytes) XOR XOR(psk_key_bytes)
```

This helps quickly identify which channel a packet belongs to.

## Troubleshooting

### "Failed to decode base64 key"
- Verify your PSK is valid base64
- Check for extra spaces or newlines
- Ensure PSK length is correct (typically 22 or 24 characters for base64)

### "Failed to decrypt packet with any configured key"
- Verify you're using the correct PSK from your Meshtastic device
- Check that the channel name matches exactly
- Ensure your Meshtastic device is on the same WiFi network
- Verify multicast is enabled on your Meshtastic device

### No packets received
- Check WiFi connection
- Verify multicast IP and port match your Meshtastic settings
- Ensure your router allows multicast traffic
- Check that Meshtastic device has network module enabled

### Packet decrypts but "protobuf decode failed"
- Incorrect channel key (decryption succeeds but produces garbage)
- Try other configured channels
- Verify protobuf files are up to date

## Advanced Configuration

### Adding More Channels

Increase `MAX_CHANNELS` and add more entries:
```cpp
#define MAX_CHANNELS 8

ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "ABC123==", {0}, 0, 0},
  {"MediumSlow", "DEF456==", {0}, 0, 0},
  {"ShortSlow", "GHI789==", {0}, 0, 0},
  // ... more channels
};
```

### Custom Multicast Settings

If your Meshtastic network uses different multicast settings:
```cpp
const char* MCAST_GRP = "224.0.0.70";  // Custom multicast IP
const uint16_t MCAST_PORT = 4404;       // Custom port
```

## Security Notes

- ⚠️ **PSK Keys**: Keep your PSK keys private. They provide access to your mesh network.
- ⚠️ **WiFi Credentials**: Consider using WPA2/WPA3 for WiFi security.
- ⚠️ **Default Keys**: The default Meshtastic key (PSK=1) is well-known and provides minimal security.

## References

- [Meshtastic Encryption Documentation](https://meshtastic.org/docs/overview/encryption/)
- [Meshtastic Protobufs](https://github.com/meshtastic/protobufs)
- [Meshtastic Firmware CryptoEngine](https://github.com/meshtastic/firmware/blob/master/src/mesh/CryptoEngine.cpp)
- [ESP32 mbedTLS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mbedtls.html)

## Contributing

Contributions are welcome! Please test thoroughly with your Meshtastic hardware before submitting pull requests.

## License

See the main repository LICENSE file.
