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

An Arduino/ESP32 sketch to receive and decode Meshtastic UDP packets. Supports both encrypted and unencrypted packets with AES-CTR decryption.

**Features:**
- Receives UDP packets from Meshtastic multicast (224.0.0.69:4403)
- Decrypts encrypted packets using AES-CTR mode
- Supports multiple channel configurations
- Handles Meshtastic default key variants
- Decodes Position, Telemetry, and Text messages

**Setup:**
1. Open meshview.ino in Arduino IDE
2. Configure WiFi credentials
3. Configure your Meshtastic channel names and keys
4. Upload to ESP32
5. Open Serial Monitor to view decoded packets

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


