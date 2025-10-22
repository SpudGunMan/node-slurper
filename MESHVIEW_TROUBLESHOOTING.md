# Meshview.ino Troubleshooting Guide

This guide helps you debug common issues with the meshview.ino sketch.

## Common Issues and Solutions

### 1. Compilation Errors

#### Error: "mbedtls/aes.h: No such file or directory"
**Cause:** ESP32 board support not installed or outdated.

**Solution:**
1. In Arduino IDE, go to Tools → Board → Boards Manager
2. Search for "ESP32"
3. Install or update "ESP32 by Espressif Systems" (version 2.0.0 or newer)
4. Restart Arduino IDE

#### Error: "pb_decode.h: No such file or directory"
**Cause:** Nanopb library not installed.

**Solution:**
1. Download nanopb from: https://github.com/nanopb/nanopb
2. Copy the `pb.h`, `pb_common.h`, `pb_decode.h`, `pb_decode.c` files to your sketch folder
3. Or install via Library Manager: Sketch → Include Library → Manage Libraries → Search "Nanopb"

#### Error: "meshtastic/mesh.pb.h: No such file or directory"
**Cause:** Meshtastic protobuf files not generated.

**Solution:**
1. Clone https://github.com/meshtastic/protobufs
2. Use protoc with nanopb plugin to generate .pb.h and .pb.c files:
   ```bash
   protoc --nanopb_out=. meshtastic/*.proto
   ```
3. Copy generated files to a `meshtastic/` subfolder in your sketch directory

### 2. Runtime Errors

#### "Failed to connect to WiFi"
**Possible causes:**
- Wrong SSID or password
- WiFi network out of range
- Special characters in SSID/password

**Solutions:**
1. Double-check SSID and password (case-sensitive)
2. Move ESP32 closer to WiFi router
3. Use 2.4GHz WiFi (ESP32 doesn't support 5GHz)
4. Try escaping special characters or use a simpler WiFi name

**Debug steps:**
```cpp
// Add this after WiFi.begin():
Serial.print("Connecting to: ");
Serial.println(ssid);
Serial.print("Status: ");
Serial.println(WiFi.status());
```

#### "Failed to decode base64 key"
**Possible causes:**
- Invalid base64 string
- Extra spaces or newlines
- Wrong PSK format

**Solutions:**
1. Verify PSK is exactly as shown in Meshtastic app
2. Remove any spaces before/after the PSK
3. Check PSK length (should be ~22-24 characters)
4. Try using PSK value "1" for testing with default key

**Debug steps:**
```cpp
// Add this before decodeBase64Key():
Serial.print("PSK length: ");
Serial.println(strlen(channels[0].psk_base64));
Serial.print("PSK value: ");
Serial.println(channels[0].psk_base64);
```

#### "UDP multicast listener started" but no packets received
**Possible causes:**
- Meshtastic device not on same network
- Multicast blocked by router
- Network module not enabled on Meshtastic
- Wrong multicast IP/port

**Solutions:**
1. Verify Meshtastic device WiFi is connected to same network
2. Check router settings for multicast/IGMP support
3. On Meshtastic device: Settings → Network → Enable
4. Verify multicast settings match (224.0.0.69:4403 is default)

**Debug steps:**
```cpp
// Add at start of loop():
if (millis() % 5000 == 0) {
  Serial.println("Still listening...");
}
```

#### "Failed to decrypt packet with any configured key"
**Possible causes:**
- Wrong PSK key
- Wrong channel configuration
- Packet from different mesh
- Channel name mismatch

**Solutions:**
1. Verify PSK exactly matches your Meshtastic channel
2. Try PSK "1" to test with default key
3. Check channel number matches (0 = primary channel)
4. Verify packet is actually from your mesh network

**Debug steps:**
```cpp
// The sketch already prints:
// - Encrypted payload size
// - Which channels it tries
// - Whether decryption succeeds
// Review these messages to identify the issue
```

#### "Decryption worked but protobuf decode failed"
**Possible causes:**
- Wrong key (decrypts to garbage)
- Protobuf definitions out of date
- Corrupted packet

**Solutions:**
1. This usually means wrong key - try other channels
2. Update protobuf definitions from Meshtastic repo
3. Wait for next packet - might be transmission error

### 3. Validation Steps

#### Step 1: Verify Basic Setup
```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== Meshview Validation ===");
  Serial.println("ESP32 starting...");
  
  // Should see WiFi scan results
  // Should see "WiFi connected"
  // Should see "Initializing channels..."
  // Should see "Channel 0: name='...', key_length=16, hash=0x..."
  // Should see "UDP multicast listener started"
}
```

Expected output:
```
=== Meshview Validation ===
ESP32 starting...
Scanning for WiFi networks...
5 networks found:
...
WiFi connected.
IP address: 192.168.1.100
Initializing channels...
Channel 0: name='LongFast', key_length=16, hash=0x42
UDP multicast listener started.
```

#### Step 2: Verify Channel Configuration
Look for this output during initialization:
```
Channel 0: name='LongFast', key_length=16, hash=0x42
```

- `name`: Should match your channel name
- `key_length`: Should be 16 (AES-128) or 32 (AES-256)
- `hash`: Channel identification hash (any value is ok)

If `key_length=0`: Key decode failed!

#### Step 3: Verify Packet Reception
When Meshtastic sends a packet, you should see:
```
UDP packets seen: 1
Raw UDP payload (hex): 08 A1 B2 C3 ...
id: 123456789
from: 987654321
...
```

If you see this, ESP32 is receiving packets correctly.

#### Step 4: Verify Decryption
For encrypted packets:
```
Encrypted packet detected. Attempting decryption...
Trying channel 0 (LongFast)...
Successfully decrypted with channel 0!
```

For unencrypted packets:
```
Unencrypted packet (decoded).
```

#### Step 5: Verify Message Decoding
Text messages:
```
Portnum: 1
Decoded text message: Hello Mesh!
```

Position:
```
Portnum: 3
Position lat=37.1234567 lon=-122.1234567 alt=100
```

Telemetry:
```
Portnum: 67
Telemetry battery_level=100 voltage=4.2 air_util_tx=1.5
```

## Testing with Known Values

### Test 1: Default Key
Use PSK "1" to test with the well-known default Meshtastic key:
```cpp
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1", {0}, 0, 0},
  ...
};
```

This should work with any Meshtastic device using default key.

### Test 2: Verify Nonce Generation
Add debug output to `generateNonce()`:
```cpp
void generateNonce(uint32_t packetId, uint32_t fromNode, uint8_t* nonce) {
  memset(nonce, 0, 16);
  
  nonce[0] = packetId & 0xFF;
  nonce[1] = (packetId >> 8) & 0xFF;
  nonce[2] = (packetId >> 16) & 0xFF;
  nonce[3] = (packetId >> 24) & 0xFF;
  
  nonce[4] = fromNode & 0xFF;
  nonce[5] = (fromNode >> 8) & 0xFF;
  nonce[6] = (fromNode >> 16) & 0xFF;
  nonce[7] = (fromNode >> 24) & 0xFF;
  
  // Debug output
  Serial.print("Nonce: ");
  for (int i = 0; i < 16; i++) {
    Serial.printf("%02X ", nonce[i]);
  }
  Serial.println();
}
```

Nonce should show packetId and fromNode in little-endian format.

### Test 3: Raw Decryption Test
To verify AES is working, you can manually test with known values:
```cpp
// In setup(), after initializeChannels():
uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
uint8_t test_out[4];
if (decryptPayload(test_data, 4, test_out, 
                   channels[0].key, channels[0].key_length,
                   12345, 67890)) {
  Serial.println("AES test: PASS");
} else {
  Serial.println("AES test: FAIL");
}
```

## Advanced Debugging

### Enable Verbose Logging
Add more Serial.print statements throughout the code:

```cpp
// In decryptPayload():
Serial.printf("Decrypting %d bytes with %d-byte key\n", len, keyLen);
Serial.printf("PacketID: %u, FromNode: %u\n", packetId, fromNode);

// After AES operation:
Serial.printf("AES result: %d (0=success)\n", ret);
```

### Monitor Network Traffic
Use Wireshark to see UDP multicast packets:
1. Start Wireshark
2. Filter: `ip.dst == 224.0.0.69 and udp.port == 4403`
3. Verify packets are reaching your network

### Check Meshtastic Device
On your Meshtastic device:
```bash
meshtastic --info
# Check network module is enabled

meshtastic --noproto
# Watch for packet transmission logs
```

## Getting Help

If you're still having issues:

1. **Collect debug information:**
   - Full serial output from ESP32
   - Your channel configuration (WITHOUT the actual PSK!)
   - Meshtastic firmware version
   - ESP32 board type

2. **Create a GitHub issue:**
   - Repository: https://github.com/SpudGunMan/node-slurper
   - Include debug information above
   - Describe expected vs actual behavior

3. **Community support:**
   - Meshtastic Discord: https://discord.gg/meshtastic
   - Meshtastic Forum: https://meshtastic.discourse.group

## Security Notes

When debugging:
- ⚠️ Never share your actual PSK publicly
- ⚠️ Use a test channel for debugging
- ⚠️ Consider PSK "1" is a well-known key (not secure)
- ⚠️ Rotate your PSK after troubleshooting if exposed

## Quick Checklist

- [ ] ESP32 board support installed (v2.0.0+)
- [ ] Nanopb library available
- [ ] Protobuf files generated and in place
- [ ] WiFi credentials correct
- [ ] Connected to 2.4GHz WiFi
- [ ] PSK copied exactly from Meshtastic
- [ ] Channel name matches
- [ ] Meshtastic network module enabled
- [ ] Same WiFi network as Meshtastic
- [ ] Router allows multicast
- [ ] Serial monitor at 115200 baud
- [ ] Can see "UDP multicast listener started"
