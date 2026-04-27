#!/usr/bin/env python3
"""
ESP32-CAM simulator — mimics the exact payload the firmware sends.

Pipeline:
  1. Create a synthetic plate JPEG (Pillow)
  2. Build XML payload  <anpr_payload>...</anpr_payload>
  3. AES-256-CBC encrypt with PKCS7 padding  (cryptography lib)
  4. Base64-encode ciphertext
  5. POST to /api/anpr/process
  6. Print decoded response
"""

import base64
import datetime
import io
import json
import sys
import urllib.request
import urllib.error

# ── AES constants (must match config.h) ───────────────────────────────────────
AES_KEY = b"0123456789ABCDEF0123456789ABCDEF"   # 32 bytes = 256 bit
AES_IV  = b"0000000000000000"                    # 16 bytes = 128 bit

# ── Backend endpoint ───────────────────────────────────────────────────────────
BACKEND_URL = "http://localhost:8000/api/anpr/process"


def make_plate_jpeg(plate_text: str = "34TEST99") -> bytes:
    """Create a minimal JPEG with plate text on a yellow background."""
    try:
        from PIL import Image, ImageDraw, ImageFont
        img = Image.new("RGB", (320, 100), color=(255, 220, 0))
        draw = ImageDraw.Draw(img)
        # Try a larger font; fall back to default if not available
        try:
            font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 48)
        except Exception:
            font = ImageFont.load_default()
        draw.text((20, 20), plate_text, fill=(0, 0, 0), font=font)
        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=85)
        return buf.getvalue()
    except ImportError:
        # Fallback: 1×1 white JPEG (smallest valid JPEG, ~600 bytes)
        return base64.b64decode(
            "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8U"
            "HRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgN"
            "DRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIy"
            "MjIyMjL/wAARCAABAAEDASIAAhEBAxEB/8QAFgABAQEAAAAAAAAAAAAAAAAABgUE/8QAIhAA"
            "AgIBBAMAAAAAAAAAAAAAAQIDBAUREiExQf/EABQBAQAAAAAAAAAAAAAAAAAAAAD/xAAUEQEA"
            "AAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwCvXt1aNSvWrxrFXgjWONF6KqjAA/QAAA=="
        )


def aes_encrypt(plaintext: bytes) -> bytes:
    """AES-256-CBC encrypt with PKCS7 padding."""
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
        from cryptography.hazmat.primitives import padding
    except ImportError as exc:
        raise RuntimeError(
            "Missing dependency: cryptography. Install with: pip install cryptography pillow"
        ) from exc

    padder = padding.PKCS7(128).padder()
    padded = padder.update(plaintext) + padder.finalize()

    cipher = Cipher(algorithms.AES(AES_KEY), modes.CBC(AES_IV))
    enc = cipher.encryptor()
    return enc.update(padded) + enc.finalize()


def build_xml(device_id: str, timestamp: str, image_b64: str) -> str:
    return (
        f"<anpr_payload>"
        f"<device_id>{device_id}</device_id>"
        f"<timestamp>{timestamp}</timestamp>"
        f"<image>{image_b64}</image>"
        f"</anpr_payload>"
    )


def simulate(plate_text: str = "34TEST99"):
    print(f"[SIM] Creating synthetic plate image: {plate_text}")
    jpeg_bytes = make_plate_jpeg(plate_text)
    image_b64  = base64.b64encode(jpeg_bytes).decode()
    print(f"[SIM] JPEG size: {len(jpeg_bytes)} bytes  →  base64: {len(image_b64)} chars")

    ts  = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
    xml = build_xml("ESP32-SIM-001", ts, image_b64)
    print(f"[SIM] XML payload length: {len(xml)} bytes")

    ciphertext = aes_encrypt(xml.encode("utf-8"))
    payload_b64 = base64.b64encode(ciphertext).decode()
    print(f"[SIM] AES-256-CBC ciphertext (base64): {len(payload_b64)} chars")

    body = payload_b64.encode()
    req  = urllib.request.Request(
        BACKEND_URL,
        data=body,
        method="POST",
        headers={
            "Content-Type": "text/plain",
            "Content-Length": str(len(body)),
        },
    )

    print(f"\n[SIM] POST {BACKEND_URL}")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            status = resp.status
            raw    = resp.read().decode()
    except urllib.error.HTTPError as e:
        status = e.code
        raw    = e.read().decode()
    except urllib.error.URLError as e:
        print(f"[ERROR] Could not connect to backend: {e.reason}")
        print("        Make sure backend is running:  bash backend/start.sh")
        sys.exit(1)

    print(f"[SIM] HTTP {status}")
    try:
        data = json.loads(raw)
        print(f"\n{'='*50}")
        print(f"  Plate          : {data.get('plate', '—')}")
        print(f"  Confidence     : {data.get('confidence', '—')}%")
        print(f"  Access granted : {'YES ✓' if data.get('access_granted') else 'NO ✗'}")
        print(f"  Message        : {data.get('message', '—')}")
        print(f"{'='*50}\n")
    except json.JSONDecodeError:
        print(f"[RAW] {raw}")


if __name__ == "__main__":
    plate = sys.argv[1] if len(sys.argv) > 1 else "34TEST99"
    simulate(plate)
