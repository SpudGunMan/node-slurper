# node-slurper
Meshtastic bulk node management tools, run from linux command line

## Usage

```sh
$ ./nodeSlurp.sh
```

```
    o   o        o            
    |\  |        |            
    | \ | o-o  o-O o-o        
    |  \| | | |  | |-'        
    o   o o-o  o-o o-o        
                              
 o-o  o                       
|     |                       
 o-o  | o  o o-o o-o  o-o o-o 
    | | |  | |   |  | |-' |   
o--o  o o--o o   O-o  o-o o   
                 |            
                 o            
                              
 slurping node...           
                              
node data slurped for node 425675309 named "DEMO"
channel keys: 
"psk": "ABC123=", "name": "Banter", "channelNum": 0, "id": 0, "uplinkEnabled": false, "downlinkEnabled"
"psk": "ABC123=", "name": "Chat", "channelNum": 0, "id": 0, "uplinkEnabled": false, "downlinkEnabled"
fixed pin: 123456
wifi password: password
mqtt password: lpassword
admin key: ABC123=
dm private key: ABC123=
dm public key: ABC123=
node data saved to 425675309-Info.txt and 425675309.yaml
```

---

## Other Tools

### meshview.ino

An Arduino sketch for ESP32 to receive and decode Meshtastic UDP packets with full encryption support.

**Features:**
- Receives Meshtastic packets via UDP multicast (224.0.0.69:4403)
- Supports both encrypted and unencrypted packets
- AES-256-CTR decryption using mbedTLS
- Channel-based key derivation (SHA256 of PSK + channel name)
- Decodes TEXT_MESSAGE_APP, POSITION_APP, and TELEMETRY_APP portnums

**Requirements:**
- ESP32 board (for mbedTLS library support)
- Nanopb library (for protobuf decoding)
- Meshtastic protobuf files (mesh.pb.h, mesh.pb.c, portnums.pb.h, telemetry.pb.h)

**Setup:**
1. Install the Nanopb library from Arduino Library Manager
2. Generate protobuf files from Meshtastic protobufs: https://github.com/meshtastic/protobufs
3. Update WiFi credentials in the sketch
4. Get your channel's encryption key from the Meshtastic app:
   - Open Meshtastic app → Channel settings
   - Tap on your channel → Copy the "Encryption Key" (Base64 encoded)
5. Update `mesh_ch` (channel name) and `default_key` (Base64 PSK) in the sketch
6. Upload to your ESP32

**Configuration Example:**
```cpp
// For the default LongFast channel:
const char* mesh_ch = "LongFast";
const char* default_key = "AQ==";  // Well-known default PSK

// For a custom channel:
const char* mesh_ch = "MyPrivateChannel";
const char* default_key = "1PG7OiApB1nwvP+rz05pAQ==";  // Your PSK from Meshtastic app
```

**How it works:**
- The sketch derives the actual encryption key as: `channel_key = SHA256(PSK + channel_name)`
- For encrypted packets, it generates a nonce from the packet ID and sender node
- Decrypts using AES-256-CTR mode (matching Meshtastic firmware implementation)
- Parses and displays the decrypted protobuf data

**Example output:**
```
=== Initializing Encryption ===
Channel name: MyChannel
PSK (Base64): 1PG7OiApB1nwvP+rz05pAQ==
Decoded PSK length: 16 bytes
Derived channel key: A1 B2 C3 D4 ...
Encryption initialized successfully!
================================

UDP packets seen: 42
id: 1234567890
from: 123456789
to: 4294967295
channel: 0
Packet is ENCRYPTED. Attempting to decrypt...
Encrypted payload size: 64
Nonce: 12 34 56 78 ...
Decryption successful!
Decrypted Portnum: 1
Decoded text message: Hello from Meshtastic!
```

**Troubleshooting:**
- If decryption fails, verify your channel name matches exactly (case-sensitive)
- Ensure your PSK is correctly copied from the Meshtastic app
- Default channels use PSK "AQ==" (0x01), custom channels have unique keys
- Check that your ESP32 is on the same WiFi network where Meshtastic UDP packets are broadcast

### mudpSlurp

A cli to slup the UDP data from mesh nodes

### qconfig.sh

A graphical quick-configurator for Meshtastic nodes using YAD (Yet Another Dialog).  not maintained.  
**Usage:**
```sh
$ ./qconfig.sh
```

---

## Requirements

- Bash YAD (for qconfig.sh)
- Python and stuff


