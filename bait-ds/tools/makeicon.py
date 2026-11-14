# Draws the 32x32, 16-colour DS menu icon and writes it as a 4-bit BMP (icon.bmp).
import struct
from PIL import Image, ImageDraw
im = Image.new('RGB', (32, 32), (90, 160, 230))
d = ImageDraw.Draw(im)
d.rectangle([0, 14, 31, 31], fill=(40, 100, 150))           # water
d.line([(0, 15), (31, 15)], fill=(150, 200, 230))
d.line([(3, 20), (9, 20)], fill=(150, 200, 230)); d.line([(20, 26), (27, 26)], fill=(150, 200, 230))
d.ellipse([6, 18, 22, 27], fill=(255, 200, 40))             # golden fish
d.polygon([(21, 22), (28, 17), (28, 28)], fill=(255, 200, 40))
d.rectangle([9, 21, 10, 22], fill=(0, 0, 0))
d.line([(24, 0), (14, 9)], fill=(240, 240, 240))            # line
d.ellipse([11, 7, 17, 13], fill=(255, 255, 255))            # bobber
d.pieslice([11, 7, 17, 13], 180, 360, fill=(220, 40, 40))
q = im.quantize(colors=16)
pal = q.getpalette()[:48] + [0] * (48 - len(q.getpalette()[:48]))
px = list(q.getdata())
rows = []
for y in range(31, -1, -1):
    row = bytearray()
    for x in range(0, 32, 2):
        row.append((px[y * 32 + x] << 4) | px[y * 32 + x + 1])
    rows.append(bytes(row))
data = b''.join(rows)
paltab = b''.join(struct.pack('<BBBB', pal[i*3+2], pal[i*3+1], pal[i*3], 0) for i in range(16))
off = 14 + 40 + 64
hdr = b'BM' + struct.pack('<IHHI', off + len(data), 0, 0, off)
info = struct.pack('<IiiHHIIiiII', 40, 32, 32, 1, 4, 0, len(data), 2835, 2835, 16, 16)
open('icon.bmp', 'wb').write(hdr + info + paltab + data)
im.resize((128, 128), Image.NEAREST).save('icon_preview.png')
