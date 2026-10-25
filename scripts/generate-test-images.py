#!/usr/bin/env python3
"""Deterministic original image fixtures; requires Pillow, never reads user media."""
from pathlib import Path
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent/'tests/fixtures'
im=Image.new('RGB',(320,180),(28,29,35));d=ImageDraw.Draw(im)
for x in range(0,320,32):d.rectangle((x,0,x+15,179),fill=(139,35,52))
d.rectangle((40,40,280,140),outline=(235,230,240),width=4)
im.save(root/'image-test.jpg',quality=92,subsampling=0)
im=Image.new('RGBA',(256,256),(24,25,31,255));d=ImageDraw.Draw(im)
for y in range(0,256,32):
 for x in range(0,256,32):
  if (x//32+y//32)%2:d.rectangle((x,y,x+31,y+31),fill=(165,38,55,255))
d.line((0,0,256,256),fill=(242,232,240,255),width=4)
im.save(root/'surface-test.png')
