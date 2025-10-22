// Meshtastic UDP Packet Decoder for ESP32
// 
// This sketch receives and decodes Meshtastic UDP packets (both encrypted and unencrypted).
// It listens on the Meshtastic multicast address 224.0.0.69:4403
//
// SETUP INSTRUCTIONS:
// 1. Install required libraries in Arduino IDE:
//    - Meshtastic protobufs (generate .pb.h and .pb.c files from https://github.com/meshtastic/protobufs)
//    - mbedtls (included with ESP32 Arduino core)
//
// 2. Configure your WiFi credentials below (ssid and password)
//
// 3. Configure your Meshtastic channels:
//    - Set the channel name (e.g., "LongFast", "MyChannel")
//    - Set the base64-encoded PSK (Pre-Shared Key)
//    - You can add up to 4 channels
//
// FINDING YOUR CHANNEL KEY:
// - Use the Meshtastic app or CLI to get your channel settings
// - The PSK is shown as a base64 string (e.g., "1PG7OiApB1nwvP+rz05pAQ==")
// - Default Meshtastic channels use PSK "AQ==" (base64 for 0x01)
//
// ENCRYPTION DETAILS:
// - Uses AES-CTR mode with 128 or 256-bit keys
// - Nonce is derived from packet ID and sender node ID
// - Supports Meshtastic's special PSK values (1-10 use default key variants)
//
// References:
// - Meshtastic encryption: https://meshtastic.org/docs/overview/encryption/
// - Protocol buffers: https://github.com/meshtastic/protobufs

#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

#include "pb_decode.h"
#include "meshtastic/mesh.pb.h"      // MeshPacket, Position, etc.
#include "meshtastic/portnums.pb.h"  // Port numbers enum
#include "meshtastic/telemetry.pb.h" // Telemetry message

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Channel configurations - Add your channels here
// You can configure multiple channels with different names and keys
struct ChannelConfig {
  const char* name;        // Channel name (e.g., "LongFast", "MyChannel")
  const char* key_base64;  // Base64 encoded PSK
  uint8_t key[32];         // Decoded key (up to 32 bytes for AES-256)
  size_t key_len;          // Actual key length
  uint8_t hash;            // Channel hash (computed from name + key)
};

// Default Meshtastic key (when PSK is "AQ==" which is base64 for 0x01)
const uint8_t DEFAULT_KEY[16] = {
  0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59, 
  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

// Configure your channels here
// Format: {channel_name, base64_psk, {0}, 0, 0}
// 
// Examples:
// - Default LongFast: {"LongFast", "AQ==", {0}, 0, 0}
// - Custom channel: {"MyTeam", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0}
// - Default variant 2: {"Fast", "Ag==", {0}, 0, 0}
//
// Note: PSK "AQ==" is base64 for 0x01, which triggers use of the default Meshtastic key
//       PSK "Ag==" is base64 for 0x02, which uses default key with last byte incremented by 1
//       You can have up to MAX_CHANNELS different channel configurations
#define MAX_CHANNELS 4
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "AQ==", {0}, 0, 0},  // Default LongFast channel with default key
  // Add more channels as needed (uncomment and configure):
  // {"MyChannel", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},  // Custom channel example
  // {"", "", {0}, 0, 0},
  // {"", "", {0}, 0, 0},
};
int num_channels = 1;  // Update this count when you add more channels

const char* MCAST_GRP = "224.0.0.69";
const uint16_t MCAST_PORT = 4403;

unsigned long udpPacketCount = 0;

WiFiUDP udp;
IPAddress multicastIP;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found.");
  } else {
    Serial.print(n);
    Serial.println(" networks found:");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (RSSI ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [OPEN]" : " [SECURED]");
      delay(10);
    }
  }

  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 20000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize channel keys
    Serial.println("\nInitializing channel keys...");
    for (int i = 0; i < num_channels; i++) {
      if (strlen(channels[i].name) > 0 && strlen(channels[i].key_base64) > 0) {
        decodeChannelKey(&channels[i]);
      }
    }

    multicastIP.fromString(MCAST_GRP);
    if (udp.beginMulticast(multicastIP, MCAST_PORT)) {
      Serial.println("UDP multicast listener started.");
    } else {
      Serial.println("Failed to start UDP multicast listener.");
    }
  } else {
    Serial.print("\nFailed to connect to WiFi. SSID: ");
    Serial.println(ssid);
    Serial.println("Check SSID, range, and password.");
  }
}

void printHex(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    Serial.printf("%02X ", buf[i]);
  }
  Serial.println();
}

void printAscii(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = static_cast<char>(buf[i]);
    Serial.print(isprint(c) ? c : '.');
  }
  Serial.println();
}

// Calculate XOR hash of a buffer (used for channel identification)
uint8_t xorHash(const uint8_t* data, size_t len) {
  uint8_t hash = 0;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
  }
  return hash;
}

// Decode base64 key and handle special Meshtastic key values
bool decodeChannelKey(ChannelConfig* ch) {
  // Decode base64 key
  size_t olen = 0;
  int ret = mbedtls_base64_decode(ch->key, sizeof(ch->key), &olen, 
                                   (const unsigned char*)ch->key_base64, 
                                   strlen(ch->key_base64));
  
  if (ret != 0) {
    Serial.print("Failed to decode base64 key for channel ");
    Serial.println(ch->name);
    return false;
  }
  
  ch->key_len = olen;
  
  // Handle special single-byte PSK values (Meshtastic convention)
  if (ch->key_len == 1) {
    uint8_t val = ch->key[0];
    if (val >= 1 && val <= 10) {
      // Use default key with last byte incremented
      memcpy(ch->key, DEFAULT_KEY, 16);
      ch->key[15] += (val - 1);
      ch->key_len = 16;
      Serial.print("Using default key variant ");
      Serial.print(val);
      Serial.print(" for channel ");
      Serial.println(ch->name);
    }
  }
  
  // Calculate channel hash (XOR of channel name + key bytes)
  uint8_t hash = xorHash((const uint8_t*)ch->name, strlen(ch->name));
  hash ^= xorHash(ch->key, ch->key_len);
  ch->hash = hash;
  
  Serial.print("Channel '");
  Serial.print(ch->name);
  Serial.print("' configured with ");
  Serial.print(ch->key_len);
  Serial.print("-byte key, hash=0x");
  Serial.println(ch->hash, HEX);
  
  return true;
}

// Initialize nonce for AES-CTR decryption
// Meshtastic nonce format: packetId (8 bytes) + fromNode (4 bytes) + padding (4 bytes zero)
void initNonce(uint32_t fromNode, uint32_t packetId, uint8_t* nonce) {
  memset(nonce, 0, 16);
  
  // Pack packetId into first 8 bytes (little-endian, but only using 4 bytes)
  nonce[0] = packetId & 0xff;
  nonce[1] = (packetId >> 8) & 0xff;
  nonce[2] = (packetId >> 16) & 0xff;
  nonce[3] = (packetId >> 24) & 0xff;
  // bytes 4-7 remain zero (upper 32 bits of packet ID)
  
  // Pack fromNode into bytes 8-11 (little-endian)
  nonce[8] = fromNode & 0xff;
  nonce[9] = (fromNode >> 8) & 0xff;
  nonce[10] = (fromNode >> 16) & 0xff;
  nonce[11] = (fromNode >> 24) & 0xff;
  // bytes 12-15 remain zero (padding)
}

// Decrypt payload using AES-CTR mode
bool decryptPayload(const uint8_t* encrypted, size_t len, uint8_t* decrypted,
                   const uint8_t* key, size_t key_len, const uint8_t* nonce) {
  if (key_len == 0 || len == 0) {
    return false;
  }
  
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  // Set encryption key (CTR uses encryption for both encrypt and decrypt)
  int ret = mbedtls_aes_setkey_enc(&aes, key, key_len * 8);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }
  
  // Prepare for CTR mode
  uint8_t nonce_copy[16];
  uint8_t stream_block[16];
  size_t nc_off = 0;
  
  memcpy(nonce_copy, nonce, 16);
  memset(stream_block, 0, 16);
  
  // Decrypt using AES-CTR
  ret = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce_copy, stream_block, 
                               encrypted, decrypted);
  
  mbedtls_aes_free(&aes);
  
  return (ret == 0);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (!packetSize) {
    delay(50);
    return;
  }

  udpPacketCount++;
  Serial.print("UDP packets seen: ");
  Serial.println(udpPacketCount);

  uint8_t buffer[512];
  int len = udp.read(buffer, sizeof(buffer));
  if (len <= 0) {
    Serial.println("Failed to read UDP packet.");
    delay(50);
    return;
  }

  // Always show raw payload
  Serial.print("Raw UDP payload (hex): ");
  printHex(buffer, len);
  Serial.print("Raw UDP payload (ASCII): ");
  printAscii(buffer, len);

  // Decode outer MeshPacket
  meshtastic_MeshPacket pkt = meshtastic_MeshPacket_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(buffer, len);

  if (!pb_decode(&stream, meshtastic_MeshPacket_fields, &pkt)) {
    Serial.println("Failed to decode meshtastic_MeshPacket.");
    delay(50);
    return;
  }

  // Basic MeshPacket fields
  Serial.print("id: "); Serial.println(pkt.id);
  Serial.print("rx_time: "); Serial.println(pkt.rx_time);
  Serial.print("rx_snr: "); Serial.println(pkt.rx_snr, 2);
  Serial.print("rx_rssi: "); Serial.println(pkt.rx_rssi);
  Serial.print("hop_limit: "); Serial.println(pkt.hop_limit);
  Serial.print("priority: "); Serial.println(pkt.priority);
  Serial.print("from: "); Serial.println(pkt.from);
  Serial.print("to: "); Serial.println(pkt.to);
  Serial.print("channel: "); Serial.println(pkt.channel);

  meshtastic_Data data = meshtastic_Data_init_zero;
  
  // Check if packet is encrypted and needs decryption
  if (pkt.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
    Serial.println("Encrypted packet detected. Attempting decryption...");
    
    size_t encrypted_len = pkt.encrypted.size;
    Serial.print("Encrypted payload size: ");
    Serial.print(encrypted_len);
    Serial.println(" bytes");
    
    if (encrypted_len == 0) {
      Serial.println("Empty encrypted payload.");
      delay(50);
      return;
    }
    
    // Check for buffer overflow protection
    if (encrypted_len > 256) {
      Serial.print("Encrypted payload too large: ");
      Serial.print(encrypted_len);
      Serial.println(" bytes (max 256). Skipping packet.");
      delay(50);
      return;
    }
    
    // Initialize nonce for decryption
    uint8_t nonce[16];
    initNonce(pkt.from, pkt.id, nonce);
    
    // Debug output for nonce
    Serial.print("Nonce (hex): ");
    printHex(nonce, 16);
    Serial.print("Encrypted data (hex): ");
    printHex(pkt.encrypted.bytes, min(encrypted_len, (size_t)32));  // Show first 32 bytes
    
    // Try each configured channel key
    // If the packet has a channel field, we could use it to select the right key,
    // but for now we try all keys since the channel field in the packet might not
    // directly correspond to our channel array index
    bool decrypted = false;
    uint8_t decrypted_data[256];
    
    for (int i = 0; i < num_channels && !decrypted; i++) {
      if (channels[i].key_len == 0) continue;
      
      Serial.print("Trying channel ");
      Serial.print(i);
      Serial.print(" (");
      Serial.print(channels[i].name);
      Serial.print(", hash=0x");
      Serial.print(channels[i].hash, HEX);
      Serial.println(")...");
      
      // Decrypt the payload
      if (decryptPayload(pkt.encrypted.bytes, encrypted_len, decrypted_data,
                        channels[i].key, channels[i].key_len, nonce)) {
        
        // Show the decrypted data for debugging
        Serial.print("Decrypted data (hex): ");
        printHex(decrypted_data, encrypted_len);
        
        // Try to decode as protobuf Data
        pb_istream_t dec_stream = pb_istream_from_buffer(decrypted_data, encrypted_len);
        if (pb_decode(&dec_stream, meshtastic_Data_fields, &data)) {
          Serial.print("Successfully decrypted with channel ");
          Serial.print(i);
          Serial.print(" (");
          Serial.print(channels[i].name);
          Serial.println(")");
          decrypted = true;
        } else {
          Serial.print("Protobuf decode failed for channel ");
          Serial.print(i);
          Serial.println(" - trying next key...");
        }
      } else {
        Serial.println("Decryption failed for this channel.");
      }
    }
    
    if (!decrypted) {
      Serial.println("Failed to decrypt packet with any configured key.");
      delay(50);
      return;
    }
  } else if (pkt.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
    // Unencrypted packet
    Serial.println("Unencrypted packet.");
    data = pkt.decoded;
  } else {
    Serial.println("Packet does not contain decoded or encrypted Data variant.");
    delay(50);
    return;
  }
  Serial.print("Portnum: "); Serial.println(data.portnum);
  Serial.print("Payload size: "); Serial.println(data.payload.size);

  if (data.payload.size == 0) {
    Serial.println("No inner payload bytes.");
    delay(50);
    return;
  }

  // Decode by portnum
  switch (data.portnum) {

    case meshtastic_PortNum_TEXT_MESSAGE_APP: {
      // Current schemas do not use a separate user.pb.h. Text payload is plain bytes.
      Serial.print("Decoded text message: ");
      printAscii(data.payload.bytes, data.payload.size);
      break;
    }

    case meshtastic_PortNum_POSITION_APP: {
      meshtastic_Position pos = meshtastic_Position_init_zero;
      pb_istream_t ps = pb_istream_from_buffer(data.payload.bytes, data.payload.size);
      if (pb_decode(&ps, meshtastic_Position_fields, &pos)) {
        Serial.print("Position lat="); Serial.print(pos.latitude_i / 1e7, 7);
        Serial.print(" lon="); Serial.print(pos.longitude_i / 1e7, 7);
        Serial.print(" alt="); Serial.println(pos.altitude);
      } else {
        Serial.println("Failed to decode Position payload.");
      }
      break;
    }

    case meshtastic_PortNum_TELEMETRY_APP: {
      meshtastic_Telemetry tel = meshtastic_Telemetry_init_zero;
      pb_istream_t ts = pb_istream_from_buffer(data.payload.bytes, data.payload.size);
      if (pb_decode(&ts, meshtastic_Telemetry_fields, &tel)) {
        // Print a few common fields if present
        if (tel.which_variant == meshtastic_Telemetry_device_metrics_tag) {
          const meshtastic_DeviceMetrics& m = tel.variant.device_metrics;
          Serial.print("Telemetry battery_level="); Serial.print(m.battery_level);
          Serial.print(" voltage="); Serial.print(m.voltage);
          Serial.print(" air_util_tx="); Serial.println(m.air_util_tx);
        } else {
          Serial.println("Telemetry decoded, different variant. Raw bytes:");
          printHex(data.payload.bytes, data.payload.size);
        }
      } else {
        Serial.println("Failed to decode Telemetry payload.");
      }
      break;
    }

    default: {
      Serial.print("Unhandled portnum "); Serial.print((int)data.portnum);
      Serial.println(", showing payload as hex:");
      printHex(data.payload.bytes, data.payload.size);
      break;
    }
  }

  delay(50);
}
