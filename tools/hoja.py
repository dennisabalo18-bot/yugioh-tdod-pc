"""Contact sheet of ygo-apex/runs/auto/frame_*.bmp: hoja.py <out.png> [first] [last]"""
import glob, sys
from PIL import Image, ImageDraw
fs = sorted(glob.glob(r"C:\Portear Juegos Xbox Clasica\Yu-Gi-Oh Port\ygo-apex\runs\auto\frame_*.bmp"))
a = int(sys.argv[2]) if len(sys.argv) > 2 else 0
b = int(sys.argv[3]) if len(sys.argv) > 3 else len(fs)
fs = fs[a:b]
W, H, C = 320, 240, 4
rows = (len(fs) + C - 1) // C
sh = Image.new("RGB", (C * W, rows * H))
for i, f in enumerate(fs):
    im = Image.open(f).convert("RGB").resize((W, H))
    ImageDraw.Draw(im).text((4, 4), f[-8:-4], fill=(0, 255, 0))
    sh.paste(im, ((i % C) * W, (i // C) * H))
sh.save(sys.argv[1])
print(len(fs))
