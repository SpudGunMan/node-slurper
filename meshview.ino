// Example to receive and decode Meshtastic UDP packets
// Make sure to install the meshtastic library and generate the .pb.h and .pb.c files from the Meshtastic .proto definitions
// https://github.com/meshtastic/protobufs/tree/master/meshtastic

// This example supports both encrypted and unencrypted Meshtastic packets.
// Encryption is handled using mbedTLS (included with ESP32 Arduino core) for AES-CTR decryption.

// === SETUP INSTRUCTIONS ===
// 1. Install the Nanopb library from Arduino Library Manager
// 2. Generate protobuf files from Meshtastic protobufs (mesh.pb.h, mesh.pb.c, etc.)
// 3. Update WiFi credentials (ssid, password)
// 4. Update Meshtastic credentials:
//    - mesh_ch: Your channel name (e.g., "LongFast", "MyCustomChannel")
//    - default_key: Your channel's Base64-encoded PSK from Meshtastic app
//
// === HOW TO GET YOUR CHANNEL KEY ===
// 1. Open your Meshtastic mobile app
// 2. Go to Channel settings
// 3. Tap on the channel you want to monitor
// 4. Look for the "Encryption Key" or "PSK" field (Base64 encoded)
// 5. Copy that value to default_key below
//
// === KEY DERIVATION ===
// The actual encryption key is derived as: SHA256(PSK + channel_name)
// This ensures the channel name is part of the encryption scheme.
//
// === SUPPORTED ENCRYPTION ===
// - AES-256-CTR mode (standard Meshtastic encryption)
// - Nonce format: packet_id (4 bytes) + from_node (4 bytes) + zero padding (8 bytes)
//
// === NOTE ON "LongFast" and DEFAULT KEY ===
// The default Meshtastic channels (LongFast, MediumSlow, etc.) use a well-known
// Base64 PSK: "AQ==" which decodes to 0x01. If you're using a custom channel,
// you MUST update both mesh_ch and default_key to match your configuration.

#include <WiFi.h>
#include <WiFiUdp.h>
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

#include "pb_decode.h"
#include "meshtastic/mesh.pb.h"      // MeshPacket, Position, etc.
#include "meshtastic/portnums.pb.h"  // Port numbers enum
#include "meshtastic/telemetry.pb.h" // Telemetry message

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Meshtastic network credentials
const char* mesh_ch = "LongFast"; // Your Channel name (must match the channel on your devices)
const char* default_key = "1PG7OiApB1nwvP+rz05pAQ=="; // Your network key here (Base64-encoded PSK)

uint8_t channel_key[32]; // Buffer for derived channel key (SHA256 result)
bool encryption_ready = false;

const char* MCAST_GRP = "224.0.0.69";
const uint16_t MCAST_PORT = 4403;

unsigned long udpPacketCount = 0;

WiFiUDP udp;
IPAddress multicastIP;

// Forward declaration
void processDataPayload(const meshtastic_Data& data);

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

    // Initialize encryption
    Serial.println("\n=== Initializing Encryption ===");
    Serial.printf("Channel name: %s\n", mesh_ch);
    Serial.printf("PSK (Base64): %s\n", default_key);
    
    if (deriveChannelKey(default_key, mesh_ch, channel_key)) {
      encryption_ready = true;
      Serial.println("Encryption initialized successfully!");
    } else {
      Serial.println("WARNING: Encryption initialization failed. Only unencrypted packets will be readable.");
    }
    Serial.println("================================\n");

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

/**
 * Derive the channel key from PSK and channel name using SHA256
 * This matches the Meshtastic firmware key derivation logic:
 * channel_key = SHA256(PSK + channel_name)
 */
bool deriveChannelKey(const char* psk_base64, const char* channel_name, uint8_t* out_key) {
  // Step 1: Decode Base64 PSK to raw bytes
  uint8_t psk_bytes[32]; // Max PSK size
  size_t psk_len = 0;
  
  int ret = mbedtls_base64_decode(psk_bytes, sizeof(psk_bytes), &psk_len, 
                                   (const unsigned char*)psk_base64, strlen(psk_base64));
  if (ret != 0) {
    Serial.printf("Failed to decode Base64 PSK: %d\n", ret);
    return false;
  }
  
  Serial.printf("Decoded PSK length: %d bytes\n", psk_len);
  Serial.print("PSK bytes: ");
  printHex(psk_bytes, psk_len);
  
  // Step 2: Prepare input for SHA256: PSK + channel_name
  size_t channel_name_len = strlen(channel_name);
  size_t input_len = psk_len + channel_name_len;
  uint8_t* input = (uint8_t*)malloc(input_len);
  if (!input) {
    Serial.println("Failed to allocate memory for key derivation");
    return false;
  }
  
  memcpy(input, psk_bytes, psk_len);
  memcpy(input + psk_len, channel_name, channel_name_len);
  
  // Step 3: Compute SHA256 hash
  ret = mbedtls_sha256(input, input_len, out_key, 0); // 0 = SHA256 (not SHA224)
  free(input);
  
  if (ret != 0) {
    Serial.printf("Failed to compute SHA256: %d\n", ret);
    return false;
  }
  
  Serial.print("Derived channel key: ");
  printHex(out_key, 32);
  
  return true;
}

/**
 * Initialize the nonce for AES-CTR decryption
 * Nonce format (16 bytes):
 * - Bytes 0-3: packet_id (little-endian)
 * - Bytes 4-7: from_node (little-endian)
 * - Bytes 8-15: zero padding
 */
void initNonce(uint32_t packet_id, uint32_t from_node, uint8_t* nonce) {
  memset(nonce, 0, 16);
  
  // Pack packet_id (little-endian)
  nonce[0] = (packet_id >> 0) & 0xFF;
  nonce[1] = (packet_id >> 8) & 0xFF;
  nonce[2] = (packet_id >> 16) & 0xFF;
  nonce[3] = (packet_id >> 24) & 0xFF;
  
  // Pack from_node (little-endian)
  nonce[4] = (from_node >> 0) & 0xFF;
  nonce[5] = (from_node >> 8) & 0xFF;
  nonce[6] = (from_node >> 16) & 0xFF;
  nonce[7] = (from_node >> 24) & 0xFF;
  
  Serial.print("Nonce: ");
  printHex(nonce, 16);
}

/**
 * Decrypt the encrypted payload using AES-CTR mode
 * This matches the Meshtastic firmware decryption logic
 */
bool decryptPayload(const uint8_t* encrypted, size_t len, uint8_t* decrypted, 
                   uint32_t packet_id, uint32_t from_node) {
  if (!encryption_ready) {
    Serial.println("Encryption not initialized");
    return false;
  }
  
  // Initialize nonce
  uint8_t nonce[16];
  initNonce(packet_id, from_node, nonce);
  
  // Setup AES context
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  // Set encryption key (AES-CTR uses encryption for both encrypt and decrypt)
  int ret = mbedtls_aes_setkey_enc(&aes, channel_key, 256);
  if (ret != 0) {
    Serial.printf("Failed to set AES key: %d\n", ret);
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
    Serial.printf("Failed to decrypt: %d\n", ret);
    return false;
  }
  
  Serial.println("Decryption successful!");
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

  // Check if packet is encrypted
  if (pkt.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
    Serial.println("Packet is ENCRYPTED. Attempting to decrypt...");
    
    if (!encryption_ready) {
      Serial.println("ERROR: Encryption not initialized. Cannot decrypt packet.");
      delay(50);
      return;
    }
    
    size_t encrypted_len = pkt.encrypted.size;
    if (encrypted_len == 0) {
      Serial.println("ERROR: Encrypted payload is empty.");
      delay(50);
      return;
    }
    
    Serial.print("Encrypted payload size: "); Serial.println(encrypted_len);
    Serial.print("Encrypted payload (hex): ");
    printHex(pkt.encrypted.bytes, encrypted_len);
    
    // Decrypt the payload
    uint8_t decrypted_buffer[256];
    if (!decryptPayload(pkt.encrypted.bytes, encrypted_len, decrypted_buffer, pkt.id, pkt.from)) {
      Serial.println("ERROR: Decryption failed.");
      delay(50);
      return;
    }
    
    Serial.print("Decrypted payload (hex): ");
    printHex(decrypted_buffer, encrypted_len);
    
    // Parse the decrypted Data message
    meshtastic_Data decrypted_data = meshtastic_Data_init_zero;
    pb_istream_t dec_stream = pb_istream_from_buffer(decrypted_buffer, encrypted_len);
    
    if (!pb_decode(&dec_stream, meshtastic_Data_fields, &decrypted_data)) {
      Serial.println("ERROR: Failed to decode decrypted Data message.");
      delay(50);
      return;
    }
    
    Serial.print("Decrypted Portnum: "); Serial.println(decrypted_data.portnum);
    Serial.print("Decrypted Payload size: "); Serial.println(decrypted_data.payload.size);
    
    // Process the decrypted data
    processDataPayload(decrypted_data);
    
    delay(50);
    return;
  }

  // Only proceed if we have a decoded Data variant
  if (pkt.which_payload_variant != meshtastic_MeshPacket_decoded_tag) {
    Serial.println("Packet does not contain decoded Data (unknown variant).");
    delay(50);
    return;
  }

  const meshtastic_Data& data = pkt.decoded;
  Serial.print("Portnum: "); Serial.println(data.portnum);
  Serial.print("Payload size: "); Serial.println(data.payload.size);

  if (data.payload.size == 0) {
    Serial.println("No inner payload bytes.");
    delay(50);
    return;
  }

  // Process the unencrypted data
  processDataPayload(data);

  delay(50);
}

/**
 * Process a Data payload (either decrypted or originally unencrypted)
 */
void processDataPayload(const meshtastic_Data& data) {
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
}
