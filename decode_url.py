import base64
import json

def decode_mesh_url(url):
    # Split the URL to get the base64 encoded channel settings
    split_url = url.split("#")
    if len(split_url) < 2:
        print("Invalid URL format")
        return None
    b64 = split_url[-1]

    print(f"Extracted base64 string: {b64}")

    # Add back any missing padding
    missing_padding = len(b64) % 4
    if missing_padding:
        b64 += "=" * (4 - missing_padding)

    print(f"Base64 string with padding: {b64}")

    # Decode the base64 string
    try:
        decoded_bytes = base64.urlsafe_b64decode(b64)
        try:
            decoded_str = decoded_bytes.decode('utf-8')
            decoded_json = json.loads(decoded_str)
            return decoded_json
        except (UnicodeDecodeError, json.JSONDecodeError):
            print("Data is not JSON, treating as raw binary data use protobuf(apponly_pb2) to decode")
            return decoded_bytes
    except Exception as e:
        print(f"Error decoding base64: {e}")
        return None