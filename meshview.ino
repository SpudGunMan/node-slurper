// Example to receive and decode Meshtastic UDP packets
// Make sure to install the meashtastic library and generate the .pb.h and .pb.c files from the Meshtastic .proto definitions
// https://github.com/meshtastic/protobufs/tree/master/meshtastic

// Example to receive and decode Meshtastic UDP packets with encryption support.
// Implements AES-CTR decryption compatible with Meshtastic's CryptoEngine.
//
// USAGE:
// 1. Configure WiFi credentials (ssid, password)
// 2. Configure your Meshtastic channels in the 'channels' array:
//    - name: Channel name (e.g., "LongFast")
//    - psk_base64: Base64-encoded PSK key from your Meshtastic device
//      OR use special values: "1" for default key, "2"-"10" for default key variants
// 3. Upload to ESP32
// 4. Open serial monitor at 115200 baud
//
// FEATURES:
// - Decrypts encrypted packets using AES-CTR (matches Meshtastic firmware)
// - Supports multiple channels with different keys
// - Handles both encrypted and unencrypted packets
// - Decodes Position, Telemetry, and Text messages
// - Calculates channel hash for channel identification
//
// LIBRARIES REQUIRED:
// - WiFi (built-in)
// - WiFiUdp (built-in)
// - mbedtls (built-in on ESP32)
// - nanopb (for protobuf decoding)
// - Meshtastic protobuf definitions

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

// Meshtastic network credentials
// You can configure multiple channels here
struct ChannelConfig {
  const char* name;
  const char* psk_base64;
  uint8_t key[32];  // Decoded key (16 bytes for AES128, 32 for AES256)
  size_t key_length;
  uint8_t hash;
};

// Default Meshtastic key used for PSK value of 1
const uint8_t DEFAULT_MESHTASTIC_KEY[16] = {
  0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

// Configure your channels here
#define MAX_CHANNELS 4
ChannelConfig channels[MAX_CHANNELS] = {
  {"LongFast", "1PG7OiApB1nwvP+rz05pAQ==", {0}, 0, 0},  // Channel 0
  {"", "", {0}, 0, 0},  // Channel 1 (unused)
  {"", "", {0}, 0, 0},  // Channel 2 (unused)
  {"", "", {0}, 0, 0}   // Channel 3 (unused)
};

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

    // Initialize channel keys and hashes
    initializeChannels();

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

// XOR hash function for channel identification (matches Meshtastic)
uint8_t xorHash(const uint8_t* data, size_t len) {
  uint8_t hash = 0;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
  }
  return hash;
}

// Calculate channel hash from name and key
uint8_t calculateChannelHash(const char* name, const uint8_t* key, size_t keyLen) {
  uint8_t hash = 0;
  if (name && strlen(name) > 0) {
    hash = xorHash((const uint8_t*)name, strlen(name));
  }
  if (key && keyLen > 0) {
    hash ^= xorHash(key, keyLen);
  }
  return hash;
}

// Decode base64 PSK to binary key
bool decodeBase64Key(const char* base64_psk, uint8_t* key, size_t* key_length) {
  if (!base64_psk || strlen(base64_psk) == 0) {
    return false;
  }

  // Handle special single-byte PSK cases
  if (strlen(base64_psk) <= 2) {
    uint8_t psk_val = atoi(base64_psk);
    if (psk_val >= 1 && psk_val <= 10) {
      // Use default key with modification
      memcpy(key, DEFAULT_MESHTASTIC_KEY, 16);
      if (psk_val > 1) {
        key[15] += (psk_val - 1);  // Increment last byte
      }
      *key_length = 16;
      Serial.printf("Using default key variant (PSK=%d)\n", psk_val);
      return true;
    }
  }

  // Decode base64
  size_t olen = 0;
  int ret = mbedtls_base64_decode(key, 32, &olen, 
                                   (const unsigned char*)base64_psk, 
                                   strlen(base64_psk));
  
  if (ret == 0 && (olen == 16 || olen == 32)) {
    *key_length = olen;
    Serial.printf("Decoded base64 key: %d bytes\n", olen);
    return true;
  } else {
    Serial.printf("Failed to decode base64 key, ret=%d, olen=%d\n", ret, olen);
    return false;
  }
}

// Initialize all configured channels
void initializeChannels() {
  Serial.println("Initializing channels...");
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (strlen(channels[i].psk_base64) > 0) {
      if (decodeBase64Key(channels[i].psk_base64, channels[i].key, &channels[i].key_length)) {
        channels[i].hash = calculateChannelHash(channels[i].name, 
                                                channels[i].key, 
                                                channels[i].key_length);
        Serial.printf("Channel %d: name='%s', key_length=%d, hash=0x%02X\n", 
                     i, channels[i].name, channels[i].key_length, channels[i].hash);
      } else {
        Serial.printf("Channel %d: Failed to decode key\n", i);
      }
    }
  }
}

// Generate nonce for AES-CTR decryption (matches Meshtastic's initNonce)
void generateNonce(uint32_t packetId, uint32_t fromNode, uint8_t* nonce) {
  // Nonce structure: packetId (4 bytes) + fromNode (4 bytes) + padding (8 bytes)
  memset(nonce, 0, 16);
  
  // Pack packet ID (little-endian)
  nonce[0] = packetId & 0xFF;
  nonce[1] = (packetId >> 8) & 0xFF;
  nonce[2] = (packetId >> 16) & 0xFF;
  nonce[3] = (packetId >> 24) & 0xFF;
  
  // Pack from node (little-endian)
  nonce[4] = fromNode & 0xFF;
  nonce[5] = (fromNode >> 8) & 0xFF;
  nonce[6] = (fromNode >> 16) & 0xFF;
  nonce[7] = (fromNode >> 24) & 0xFF;
  
  // Remaining bytes are zero (already set by memset)
}

// Decrypt payload using AES-CTR
bool decryptPayload(const uint8_t* encrypted, size_t len, uint8_t* decrypted,
                   const uint8_t* key, size_t keyLen, uint32_t packetId, uint32_t fromNode) {
  if (!encrypted || !decrypted || !key || keyLen == 0 || len == 0) {
    return false;
  }

  // Generate nonce
  uint8_t nonce[16];
  generateNonce(packetId, fromNode, nonce);

  // Setup AES context
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  int ret = mbedtls_aes_setkey_enc(&aes, key, keyLen * 8);
  if (ret != 0) {
    Serial.printf("AES setkey failed: %d\n", ret);
    mbedtls_aes_free(&aes);
    return false;
  }

  // Perform AES-CTR decryption
  size_t nc_off = 0;
  uint8_t stream_block[16];
  memset(stream_block, 0, sizeof(stream_block));
  
  ret = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce, stream_block, encrypted, decrypted);
  
  mbedtls_aes_free(&aes);
  
  if (ret != 0) {
    Serial.printf("AES decrypt failed: %d\n", ret);
    return false;
  }

  return true;
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

  // Check if packet is encrypted
  if (pkt.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
    Serial.println("Encrypted packet detected. Attempting decryption...");
    
    const pb_bytes_array_t& encrypted = pkt.encrypted;
    Serial.printf("Encrypted payload size: %d bytes\n", encrypted.size);
    
    if (encrypted.size == 0) {
      Serial.println("Empty encrypted payload.");
      delay(50);
      return;
    }

    // Try decrypting with each configured channel
    // Strategy: Try the channel specified in packet first, then try all others
    bool decrypted = false;
    int try_order[MAX_CHANNELS];
    int try_count = 0;
    
    // First, add the channel specified in packet (if valid)
    if (pkt.channel < MAX_CHANNELS && channels[pkt.channel].key_length > 0) {
      try_order[try_count++] = pkt.channel;
    }
    
    // Then add all other channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
      if (i != pkt.channel && channels[i].key_length > 0) {
        try_order[try_count++] = i;
      }
    }
    
    // Try decryption with each channel in order
    for (int idx = 0; idx < try_count; idx++) {
      int i = try_order[idx];
      Serial.printf("Trying channel %d (%s)...\n", i, channels[i].name);
      
      uint8_t decrypted_buffer[256];
      if (decryptPayload(encrypted.bytes, encrypted.size, decrypted_buffer,
                        channels[i].key, channels[i].key_length, 
                        pkt.id, pkt.from)) {
        
        // Try to decode as Data protobuf
        pb_istream_t dstream = pb_istream_from_buffer(decrypted_buffer, encrypted.size);
        if (pb_decode(&dstream, meshtastic_Data_fields, &data)) {
          Serial.printf("Successfully decrypted with channel %d!\n", i);
          decrypted = true;
          break;
        } else {
          Serial.printf("Decryption worked but protobuf decode failed for channel %d\n", i);
        }
      }
    }
    
    if (!decrypted) {
      Serial.println("Failed to decrypt packet with any configured key.");
      delay(50);
      return;
    }
  } else if (pkt.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
    // Packet is already decoded (unencrypted)
    Serial.println("Unencrypted packet (decoded).");
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
