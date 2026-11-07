"""Render the original rectangle-only SVG icon into an opaque512x512 RGB PNG.

No game artwork, fonts, image services or third-party raster library is used.
Supported source elements are intentionally restricted; unknown content fails.
"""
from pathlib import Path
import argparse
import binascii
import hashlib
import struct
import xml.etree.ElementTree as ET
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).with_name("icon0.svg"))
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("icon0.png"))
    args = parser.parse_args()
    data = args.source.read_bytes()
    svg = ET.fromstring(data)
    tag = lambda element: element.tag.rsplit("}", 1)[-1]
    if tag(svg) != "svg" or svg.attrib != {"width":"512", "height":"512", "viewBox":"0 0 512 512"}:
        raise ValueError("Expected the bounded512x512 original SVG source")
    pixels = bytearray(512 * 512 * 3)
    rectangles = 0
    for element in svg:
        if tag(element) in ("title", "desc"):
            continue
        if tag(element) != "rect" or set(element.attrib) != {"x", "y", "width", "height", "fill"}:
            raise ValueError("Only explicit solid rectangles are supported")
        x,y,w,h = (int(element.attrib[key]) for key in ("x", "y", "width", "height"))
        color = element.attrib["fill"]
        if not (w > 0 and h > 0 and x >= 0 and y >= 0 and x+w <= 512 and y+h <= 512):
            raise ValueError("Rectangle outside icon")
        if len(color) != 7 or color[0] != "#":
            raise ValueError("Expected explicit RGB color")
        rgb = bytes.fromhex(color[1:])
        for row in range(y, y+h):
            at = (row*512 + x)*3
            pixels[at:at+w*3] = rgb*w
        rectangles += 1
    if not rectangles:
        raise ValueError("Empty icon source")
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", binascii.crc32(kind+payload) & 0xffffffff)
    rows = b"".join(b"\0" + pixels[y*1536:(y+1)*1536] for y in range(512))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 512,512,8,2,0,0,0))
    png += chunk(b"IDAT", zlib.compress(rows,9)) + chunk(b"IEND", b"")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(png)
    print(f"Original icon512x512 RGB8: {len(png)} bytes; sha256={hashlib.sha256(png).hexdigest()}")


if __name__ == "__main__":
    main()
