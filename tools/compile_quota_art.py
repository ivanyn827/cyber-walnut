"""Compile the approved raster atlas into per-stage indexed RGB565 firmware assets.
This is a deterministic asset import (grid slicing, resampling and quantization),
not a replacement drawing. Original AI artwork remains alongside the reference.
"""
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
atlas=Image.open(ROOT/'assets/quota-v2/mascot-atlas.png').convert('RGB')
assert atlas.size==(1536,1024)
out=['#pragma once','#include <stdint.h>','namespace quotaArt {','constexpr int W=320,H=224;']
for stage,cell in enumerate([4,3,2,1,0]):
    x,y=(cell%3)*512,(cell//3)*512
    # The generated bottom-row poses have a lower source baseline; align all feet.
    top=44 if stage<2 else 80
    source=atlas.crop((x,y+top,x+512,y+top+368)).resize((320,224),Image.Resampling.LANCZOS)
    # Composite the dark studio backdrop into the app background at import time.
    pixels=[]
    for yy in range(224):
        for xx in range(320):
            r,g,b=source.getpixel((xx,yy))
            alpha=min(1,max(0,(max(r,g,b)-24)/24))
            edge=min(1,min(xx,319-xx,yy,223-yy)/8)
            a=alpha*max(0,edge)
            blended=[v*a+base*(1-a) for v,base in zip((r,g,b),(22,23,25))]
            # Ordered RGB565 quantization keeps soft clay shading from banding.
            threshold=((0,2),(3,1))[yy%2][xx%2]/4+.125
            if a:
                blended=[round(min(levels,max(0,int(v*levels/255+threshold)))*255/levels) for v,levels in zip(blended,(31,63,31))]
            pixels.append(tuple(round(v) for v in blended))
    source.putdata(pixels)
    indexed=source.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    palette=indexed.getpalette()
    colors=[]
    for i in range(256):
        r,g,b=palette[i*3:i*3+3];colors.append(((r>>3)<<11)|((g>>2)<<5)|(b>>3))
    out.append(f'static const uint16_t palette{stage}[256]={{'+','.join(map(str,colors))+'};')
    out.append(f'static const uint8_t image{stage}[W*H]={{'+','.join(map(str,indexed.getdata()))+'};')
out+=['static const uint16_t *const palettes[]={palette0,palette1,palette2,palette3,palette4};',
      'static const uint8_t *const images[]={image0,image1,image2,image3,image4};','}']
(ROOT/'firmware/salary_counter/quota_art.h').write_text('\n'.join(out)+'\n')
print('Compiled five original-art sprites: 360960 bytes, no runtime image decoder')
