#!/usr/bin/env python3
"""Dump a live worker framebuffer (POSIX shm) to PNG for visual inspection.

Usage: lab_frame.py WORKER_ID [OUT.png] [--scale N]

Reads /dev/shm/aoslab_fb_<id> published by aos_lab_worker and writes a
PNG that Devin's read tool can display. Requires Pillow.
"""
import os
import struct
import sys

W, H = 240, 160
HDR = 32
SHM = "/dev/shm/aoslab_fb_%d"


def grab(worker_id, out_path, scale=3):
    name = SHM % worker_id
    fd = os.open(name, os.O_RDONLY)
    try:
        hdr = os.read(fd, HDR)
        if len(hdr) < HDR or struct.unpack("<I", hdr[:4])[0] != 0x414F5346:
            sys.exit("bad shm header (worker %d not running?)" % worker_id)
        data = os.read(fd, W * H * 4)
    finally:
        os.close(fd)
    if len(data) < W * H * 4:
        sys.exit("short framebuffer read")
    # RRGGBB u32 little-endian -> RGB bytes
    px = bytearray(W * H * 3)
    j = 0
    for i in range(0, W * H * 4, 4):
        b0, b1, b2 = data[i], data[i + 1], data[i + 2]
        # memory is u32 0xRRGGBB stored LE: bytes are BB GG RR 00
        px[j] = b2
        px[j + 1] = b1
        px[j + 2] = b0
        j += 3
    from PIL import Image
    img = Image.frombytes("RGB", (W, H), bytes(px))
    if scale != 1:
        img = img.resize((W * scale, H * scale), Image.NEAREST)
    img.save(out_path)
    print(out_path)


if __name__ == "__main__":
    wid = int(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/lab_w%d.png" % wid
    scale = 3
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
    grab(wid, out, scale)
