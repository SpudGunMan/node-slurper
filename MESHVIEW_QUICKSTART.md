# Meshview.ino Quick Start Guide

Get your ESP32 receiving and decrypting Meshtastic packets in 5 minutes!

## Prerequisites

- ESP32 development board
- Arduino IDE with ESP32 support
- Active Meshtastic mesh network
- Meshtastic device with network module enabled

## Step-by-Step Setup

### 1. Install Arduino IDE and ESP32 Support

If not already installed:

1. Download Arduino IDE from https://www.arduino.cc/en/software
2. Open Arduino IDE
3. Go to **File → Preferences**
4. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Go to **Tools → Board → Boards Manager**
6. Search for "ESP32"
7. Install "ESP32 by Espressif Systems" (version 2.0.0 or later)

### 2. Get Meshtastic Protobuf Files

You need the compiled protobuf files. Two options:

#### Option A: Download Pre-compiled (Easiest)
1. Check if pre-compiled files are available in the repository
2. Look for `meshtastic/` folder with `.pb.h` and `.pb.c` files

#### Option B: Compile Yourself
```bash
# Install nanopb
git clone https://github.com/nanopb/nanopb.git

# Get Meshtastic protobufs
git clone https://github.com/meshtastic/protobufs.git

# Generate files (requires protoc and python)
cd protobufs
protoc --plugin=protoc-gen-nanopb=../nanopb/generator/protoc-gen-nanopb \
       --nanopb_out=. meshtastic/*.proto
```

You need these files:
- `mesh.pb.h` / `mesh.pb.c`
- `portnums.pb.h` / `portnums.pb.c`
- `telemetry.pb.h` / `telemetry.pb.c`

### 3. Organize Your Files

Create this folder structure:
```
meshview/
├── meshview.ino
├── pb_decode.h
├── pb_decode.c
├── pb_common.h
├── pb_common.c
├── pb.h
└── meshtastic/
    ├── mesh.pb.h
    ├── mesh.pb.c
    ├── portnums.pb.h
    ├── portnums.pb.c
    ├── telemetry.pb.h
    └── telemetry.pb.c
```

### 4. Get Your Meshtastic PSK Key

#### Using Meshtastic CLI:
```bash
meshtastic --info
```

Look for output like:
```
Channels:
  PRIMARY psk=default { "psk": "1PG7OiApB1nwvP+rz05pAQ==", "name": "LongFast" }
```

Copy the base64 PSK: `1PG7OiApB1nwvP+rz05pAQ==`

#### Using Meshtastic App:
1. Open Meshtastic app
2. Go to **Settings**
3. Select **Radio Configuration**
4. Tap **Channels**
5. Select your channel (usually "LongFast")
6. Find and copy the PSK field

#### For Testing:
Use PSK `"1"` which is the default Meshtastic key (not secure but good for testing).

### 5. Configure meshview.ino

Open `meshview.ino` and edit these lines:

#### WiFi Credentials:
```cpp
const char* ssid = "YOUR_WIFI_SSID";          // Your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";   // Your WiFi password
```

#### Channel Configuration:
```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "YOUR_PSK_HERE", {0}, 0, 0},  // Replace YOUR_PSK_HERE
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0}
};
```

**Example:**
```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0}
};
```

**For testing with default key:**
```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1", {0}, 0, 0},  // Uses default Meshtastic key
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0}
};
```

### 6. Upload to ESP32

1. Connect ESP32 to your computer via USB
2. In Arduino IDE:
   - **Tools → Board** → Select your ESP32 board
   - **Tools → Port** → Select the port with your ESP32
   - **Tools → Upload Speed** → 115200 (or higher)
3. Click **Upload** button (→)
4. Wait for "Done uploading" message

### 7. Monitor Output

1. Open Serial Monitor: **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. You should see:

```
Scanning for WiFi networks...
3 networks found:
...
WiFi connected.
IP address: 192.168.1.100
Initializing channels...
Channel 0: name='LongFast', key_length=16, hash=0x42
UDP multicast listener started.
```

### 8. Test Reception

When Meshtastic devices send packets, you'll see:

#### Encrypted Packet:
```
UDP packets seen: 1
Raw UDP payload (hex): 08 A1 B2 C3 ...
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

#### Unencrypted Packet:
```
UDP packets seen: 2
...
Unencrypted packet (decoded).
Portnum: 3
Position lat=37.1234567 lon=-122.1234567 alt=100
```

## Verification Checklist

- [ ] ESP32 connects to WiFi
- [ ] Serial output shows "Initializing channels..."
- [ ] Channel shows `key_length=16` (not 0)
- [ ] Serial output shows "UDP multicast listener started"
- [ ] Packets appear when Meshtastic device transmits
- [ ] Encrypted packets decrypt successfully
- [ ] Messages decode correctly (text/position/telemetry)

## Common First-Time Issues

### "Failed to decode base64 key"
- ✅ Check PSK has no extra spaces
- ✅ Verify PSK is in quotes: `"1PG7OiApB1nwvP+rz05pAQ=="`
- ✅ Try PSK `"1"` for testing

### "Failed to decrypt packet"
- ✅ Verify PSK matches your Meshtastic channel exactly
- ✅ Check channel name matches
- ✅ Try PSK `"1"` to test with default key

### No packets received
- ✅ Verify Meshtastic device is on same WiFi network
- ✅ Check Meshtastic network module is enabled
- ✅ Ensure router doesn't block multicast
- ✅ Try pinging multicast: `ping 224.0.0.69`

## Next Steps

### Monitor Multiple Channels
```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},
  {"Admin", "AQIDBAUGBwgJCgsMDQ4PEA==", {0}, 0, 0},
  {"", "", {0}, 0, 0},
  {"", "", {0}, 0, 0}
};
```

### Process Messages
Add your own code to the message handling sections:
```cpp
case meshtastic_PortNum_TEXT_MESSAGE_APP: {
  Serial.print("Decoded text message: ");
  printAscii(data.payload.bytes, data.payload.size);
  
  // Add your custom code here:
  // - Forward to MQTT
  // - Display on OLED screen
  // - Trigger actions
  // - Log to SD card
  
  break;
}
```

### Save to Database
Log received packets:
```cpp
// After successful decode:
logToSD(data.portnum, data.payload.bytes, data.payload.size);
// or
sendToMQTT(pkt.from, data.portnum, data.payload.bytes);
```

## Resources

- **Full Documentation**: [MESHVIEW_README.md](MESHVIEW_README.md)
- **Troubleshooting**: [MESHVIEW_TROUBLESHOOTING.md](MESHVIEW_TROUBLESHOOTING.md)
- **Config Examples**: [meshview_config_examples.txt](meshview_config_examples.txt)
- **Meshtastic Docs**: https://meshtastic.org/docs
- **Meshtastic Protobufs**: https://github.com/meshtastic/protobufs

## Getting Help

If you run into issues:

1. Check [MESHVIEW_TROUBLESHOOTING.md](MESHVIEW_TROUBLESHOOTING.md)
2. Review serial output for error messages
3. Verify each step of this guide
4. Ask in Meshtastic Discord or create GitHub issue

## Security Note

⚠️ **The default PSK (`"1"`) is well-known and provides no security.**

For production use:
- Generate a unique PSK on your Meshtastic device
- Keep your PSK private
- Don't share PSK in public forums or repositories
- Use WPA2/WPA3 for WiFi security

## Success!

If you see "Successfully decrypted with channel 0!" - congratulations! Your ESP32 is now receiving and decrypting Meshtastic packets. 🎉

From here, you can:
- Add custom message processing
- Forward packets to other systems
- Build a mesh network monitor
- Create automated responses
- Display data on screens or LEDs
- Log all mesh traffic
- Integrate with home automation

Happy meshing! 📡
