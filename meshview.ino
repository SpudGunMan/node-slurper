// Example to receive and decode Meshtastic UDP packets
// meshtastic library copyright and licensed by Meshtastic LLC
// Make sure to install the meashtastic library and generate the .pb.h and .pb.c files from the Meshtastic .proto definitions
// https://github.com/meshtastic/protobufs/tree/master/meshtastic

// Example to receive and decode Meshtastic UDP packets with encryption support.
// Uses mbedTLS for AES-CTR decryption (built into ESP32).
//
// FEATURES:
// - Receives UDP multicast packets from Meshtastic devices
// - Decodes outer MeshPacket protobuf structure
// - Handles both encrypted and unencrypted (decoded) packets
// - Decrypts encrypted packets using AES-CTR with provided key
// - Decodes inner Data messages (text, position, telemetry, etc.)
//
// REQUIREMENTS:
// - ESP32 board (for mbedTLS support)
// - WiFi connection
// - Meshtastic protobuf definitions (.pb.h and .pb.c files)
// - nanopb library for protobuf decoding
//
// USAGE:
// 1. Set your WiFi SSID and password below
// 2. Set your Meshtastic channel key (base64 encoded) in default_key_base64
//    - Default key "1PG7OiApB1nwvP+rz05pAQ==" is for public LongFast channel
//    - Get your channel key from Meshtastic app or device settings
// 3. Upload to ESP32
// 4. Open Serial Monitor at 115200 baud
//
// ENCRYPTION:
// - Uses AES-128 in CTR mode (same as Meshtastic firmware)
// - Nonce is generated from packet ID and sender node ID
// - Key is decoded from base64 format
// - See: https://meshtastic.org/docs/overview/encryption/
// Sketch uses 918731 bytes of program storage space
// Global variables use 45220 bytes of dynamic memory

#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

#include "pb_decode.h"
#include "meshtastic/mesh.pb.h"      // MeshPacket, Position, etc.
#include "meshtastic/portnums.pb.h"  // Port numbers enum
#include "meshtastic/telemetry.pb.h" // Telemetry message

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Default Meshtastic key for public channels (base64 encoded)
// Change this to your channel's key if using a custom channel
const char* channel_name = "LongFast";
const char* default_key_base64 = "1PG7OiApB1nwvP+rz05pAQ==";
uint8_t aes_key[16]; // Buffer for decoded key (128 bits)
// hash of channel_name:default_key_base64 

bool key_initialized = false;

const char* MCAST_GRP = "224.0.0.69";
const uint16_t MCAST_PORT = 4403;

unsigned long udpPacketCount = 0;

WiFiUDP udp;
IPAddress multicastIP;

// AES context for mbedTLS
mbedtls_aes_context aes;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize AES context
  mbedtls_aes_init(&aes);

  // Decode the base64 key
  if (!decodeBase64Key()) {
    Serial.println("Failed to decode base64 key!");
  } else {
    Serial.println("AES key decoded successfully.");
    key_initialized = true;
  }

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

bool decodeBase64Key() {
  // Decode base64 key to raw bytes
  size_t olen = 0;
  int ret = mbedtls_base64_decode(aes_key, sizeof(aes_key), &olen, 
                                   (const unsigned char*)default_key_base64, 
                                   strlen(default_key_base64));
  
  if (ret != 0 || olen != 16) {
    Serial.print("Base64 decode failed, ret=");
    Serial.print(ret);
    Serial.print(", olen=");
    Serial.println(olen);
    return false;
  }

  Serial.print("Decoded AES key (hex): ");
  printHex(aes_key, 16);
  return true;
}

void initNonce(uint32_t fromNode, uint32_t packetId, uint8_t* nonce) {
  // Initialize 16-byte nonce for AES-CTR
  // Based on Meshtastic CryptoEngine::initNonce
  memset(nonce, 0, 16);
  
  // Pack the block counter (starts at 0) in first 8 bytes (little-endian)
  nonce[0] = 0;
  nonce[1] = 0;
  nonce[2] = 0;
  nonce[3] = 0;
  nonce[4] = 0;
  nonce[5] = 0;
  nonce[6] = 0;
  nonce[7] = 0;
  
  // Pack packetId in next 4 bytes (little-endian)
  nonce[8] = (packetId >> 0) & 0xff;
  nonce[9] = (packetId >> 8) & 0xff;
  nonce[10] = (packetId >> 16) & 0xff;
  nonce[11] = (packetId >> 24) & 0xff;
  
  // Pack fromNode in last 4 bytes (little-endian)
  nonce[12] = (fromNode >> 0) & 0xff;
  nonce[13] = (fromNode >> 8) & 0xff;
  nonce[14] = (fromNode >> 16) & 0xff;
  nonce[15] = (fromNode >> 24) & 0xff;
}

bool decryptPayload(uint32_t fromNode, uint32_t packetId, const uint8_t* encrypted, size_t len, uint8_t* decrypted) {
  if (!key_initialized) {
    Serial.println("Key not initialized, cannot decrypt.");
    return false;
  }

  if (len == 0 || len > 256) {
    Serial.println("Invalid encrypted payload length.");
    return false;
  }

  // Generate nonce for this packet
  uint8_t nonce[16];
  initNonce(fromNode, packetId, nonce);

  Serial.print("Nonce (hex): ");
  printHex(nonce, 16);

  // Set up AES key for encryption (CTR mode uses encryption for both encrypt and decrypt)
  int ret = mbedtls_aes_setkey_enc(&aes, aes_key, 128);
  if (ret != 0) {
    Serial.print("Failed to set AES key, ret=");
    Serial.println(ret);
    return false;
  }

  // Decrypt using AES-CTR
  size_t nc_off = 0;
  uint8_t stream_block[16];
  memset(stream_block, 0, 16);
  
  uint8_t scratch[256];
  memcpy(scratch, encrypted, len);
  
  ret = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce, stream_block, scratch, decrypted);
  if (ret != 0) {
    Serial.print("AES-CTR decryption failed, ret=");
    Serial.println(ret);
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
  bool has_data = false;

  // Check if packet is encrypted
  if (pkt.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
    Serial.println("Packet is encrypted, attempting to decrypt...");
    
    if (!key_initialized) {
      Serial.println("Cannot decrypt: encryption key not initialized.");
      delay(50);
      return;
    }

    // Decrypt the payload
    uint8_t decrypted[256];
    size_t encrypted_len = pkt.encrypted.size;
    
    Serial.print("Encrypted payload size: ");
    Serial.println(encrypted_len);
    Serial.print("Encrypted payload (hex): ");
    printHex(pkt.encrypted.bytes, encrypted_len);

    if (decryptPayload(pkt.from, pkt.id, pkt.encrypted.bytes, encrypted_len, decrypted)) {
      Serial.println("Decryption successful!");
      Serial.print("Decrypted payload (hex): ");
      printHex(decrypted, encrypted_len);
      
      // Try to decode the decrypted Data message
      pb_istream_t data_stream = pb_istream_from_buffer(decrypted, encrypted_len);
      if (pb_decode(&data_stream, meshtastic_Data_fields, &data)) {
        Serial.println("Successfully decoded Data from decrypted payload.");
        has_data = true;
      } else {
        Serial.println("Failed to decode Data from decrypted payload.");
      }
    } else {
      Serial.println("Decryption failed.");
      delay(50);
      return;
    }
  } else if (pkt.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
    // Packet is already decoded (unencrypted or from licensed mode)
    Serial.println("Packet contains decoded Data (unencrypted).");
    data = pkt.decoded;
    has_data = true;
  } else {
    Serial.println("Packet does not contain decoded or encrypted data.");
    delay(50);
    return;
  }

  if (!has_data) {
    Serial.println("No data to process.");
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
      Serial.print("Payload as ASCII: ");
      printAscii(data.payload.bytes, data.payload.size);
      break;
    }
  }

  delay(50);
}
