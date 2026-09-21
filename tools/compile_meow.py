"""Pack both generated atlases as baseline JPEGs for the C6 ROM decoder."""
from pathlib import Path
from PIL import Image
import io
ROOT=Path(__file__).resolve().parents[1]
W=H=160
atlases=[Image.open(ROOT/'assets/meow'/name).convert('RGB') for name in ['grooming-atlas-v2.png','happy-atlas-v3.png']]
data=[];offsets=[0];host=[]
frames=[(0,i) for i in range(16)]+[(1,i) for i in [0,1,2,3,11,10,9,8]]
for variant,i in frames:
    atlas=atlases[variant]
    x,y=i%4,i//4
    im=atlas.crop((round(x*atlas.width/4),round(y*atlas.height/4),round((x+1)*atlas.width/4),round((y+1)*atlas.height/4))).resize((W,H),Image.Resampling.LANCZOS)
    buf=io.BytesIO();im.save(buf,format='JPEG',quality=65,subsampling=2,optimize=True,progressive=False)
    raw=buf.getvalue();data.extend(raw);offsets.append(len(data))
    # Host preview decodes the same JPEG bitstream; device uses ROM TJpgDec.
    rgb=Image.open(io.BytesIO(raw)).convert('RGB')
    host.extend(((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in rgb.getdata())
out=['#pragma once','#include <stdint.h>','namespace meowArt {',f'constexpr int W={W},H={H},COUNT=16,VARIANTS=2;',
     'static const uint8_t frames[]={'+','.join(map(str,list(range(16))+list(range(16,24))*2))+'};',
     'static const uint32_t offsets[]={'+','.join(map(str,offsets))+'};',
     'static const uint8_t data[]={'+','.join(map(str,data))+'};',
     '#ifndef ARDUINO','static const uint16_t hostPixels[]={'+','.join(map(str,host))+'};','#endif','}']
(ROOT/'firmware/salary_counter/meow_art.h').write_text('\n'.join(out)+'\n')
print('24 unique JPEG frames bytes:',len(data)+len(offsets)*4+32)
