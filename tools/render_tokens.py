#!/usr/bin/env python3
"""Render the Muse usage dashboard (128x160 JPEG) for the ESP32-C3 gadget."""
import re, subprocess, sys, os
from datetime import datetime
from PIL import Image, ImageDraw, ImageFont

OUT = "/home/hatch/workspace/muse-gadget-relay/tokens.jpg"
W, H = 128, 160
BG = (11, 16, 38)
CREAM = (245, 240, 220)
DIM = (150, 155, 175)
ORANGE = (255, 150, 60)
GREEN = (90, 210, 130)
TRACK = (45, 52, 78)

def font(size, bold=True):
    cands = [
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ]
    for p in cands:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                continue
    return ImageFont.load_default()

raw = subprocess.run(["subscription-status", "status"], capture_output=True, text=True).stdout

weekly = None
m = re.search(r"Usage:\s*([\d.]+)%", raw)
if m:
    weekly = float(m.group(1))

reset_txt = ""
m = re.search(r"resets\s+(\w+)\s+(\d+)\s+at\s+(\d+):(\d+)\s*(AM|PM)", raw)
if m:
    mon = {"Jan":1,"Feb":2,"Mar":3,"Apr":4,"May":5,"Jun":6,"Jul":7,"Aug":8,"Sep":9,"Oct":10,"Nov":11,"Dec":12}[m.group(1)]
    hh = int(m.group(3)) % 12 + (12 if m.group(5) == "PM" else 0)
    reset_txt = f"{mon}月{int(m.group(2))}日 {hh:02d}:{int(m.group(4)):02d} 重置"

extra_left, extra_used = "", None
m = re.search(r"Additional tokens:\s*([\d.]+)% used \((.+?) tokens left\)", raw)
if m:
    extra_used = float(m.group(1))
    extra_left = m.group(2)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

def center(text, y, fnt, fill):
    b = d.textbbox((0, 0), text, font=fnt)
    d.text(((W - (b[2] - b[0])) / 2, y), text, font=fnt, fill=fill)

def bar(x, y, w, h, pct, color):
    d.rounded_rectangle([x, y, x + w, y + h], radius=h // 2, fill=TRACK)
    fw = max(h, int(w * min(pct, 100) / 100))
    d.rounded_rectangle([x, y, x + fw, y + h], radius=h // 2, fill=color)

center("TOKEN 消耗", 8, font(16), CREAM)

if weekly is not None:
    center(f"{weekly:g}%", 32, font(34), ORANGE)
    center("本周额度已用", 72, font(11), DIM)
    bar(14, 90, 100, 8, weekly, ORANGE)
else:
    center("--%", 32, font(34), ORANGE)

if reset_txt:
    center(reset_txt, 104, font(10), DIM)

d.line([14, 122, 114, 122], fill=TRACK, width=1)

extra_line = f"额外额度 剩 {extra_left}" if extra_left else "额外额度 —"
center(extra_line, 128, font(11), CREAM)
if extra_used is not None:
    center(f"已用 {extra_used:g}%", 144, font(9), DIM)

img.save(OUT, "JPEG", quality=88, progressive=False)
print(f"saved {OUT} weekly={weekly} reset={reset_txt} extra_left={extra_left} extra_used={extra_used}")
