# URLs are of the form https://meshtastic.org/d/#{base64_channel_set}
# base64_channel_set is a base64 encoded string that contains the channel settings
# The channel settings are a protobuf message that contains the channel name, key, and other settings
import base64
from urllib.parse import urlparse
import json

try:
    from meshtastic.protobuf import apponly_pb2 # pip install meshtastic
    MESHTASTIC_AVAILABLE = True
except ImportError:
    print("Meshtastic protobuf library not found. pip install meshtastic to enable protobuf decoding")
    MESHTASTIC_AVAILABLE = False

def decode_mesh_url(url):
    # Parse the URL to get the fragment
    parsed_url = urlparse(url)
    b64 = parsed_url.fragment

    print(f"Extracted base64 string: {b64}")

    # Add back any missing padding
    missing_padding = len(b64) % 4
    if missing_padding:
        b64 += "=" * (4 - missing_padding)

    print(f"Base64 string with padding: {b64}")

    # Decode the base64 string
    try:
        decoded_bytes = base64.urlsafe_b64decode(b64)
        
        if MESHTASTIC_AVAILABLE:
            # Parse the protobuf message
            try:
                channel_set = apponly_pb2.ChannelSet.FromString(decoded_bytes)
                print(channel_set)
            except Exception as e:
                print(f"Error parsing protobuf: {e}")
        else:
            # Try to decode as JSON
            try:
                print("Meshtastic protobuf library not found. Decoding as JSON")
                json_data = json.loads(decoded_bytes.decode('utf-8'))
                print(json.dumps(json_data, indent=2))
            except json.JSONDecodeError as e:
                print(f"Error decoding JSON: {e}")
                print(f"Binary data: {decoded_bytes}")
    except Exception as e:
        print(f"Error decoding base64 string: {e}")
