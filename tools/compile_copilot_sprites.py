"""Compile the imagegen sprite-sheet master into the dashboard's pixel asset format.

This is the asset build pipeline: fixed-cell extraction, integer pixel sampling,
binary transparency and RGB565 palette. It does not repaint the generated art.
"""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
master=ROOT/'assets/copilot/copilot-transparent.png'
image=Image.open(master).convert('RGBA')
assert image.size==(1536,1024), 'Reviewed 3x2 sheet geometry changed'
states=['idle','caution','critical','exhausted','offline','stale']
destination=ROOT/'assets/providers/copilot'
destination.mkdir(parents=True,exist_ok=True)
for index,state in enumerate(states):
    cell=image.crop(((index%3)*512,(index//3)*512,(index%3+1)*512,(index//3+1)*512))
    # Compile the source alpha into the same hard transparency as upstream runtime sprites.
    mask=cell.getchannel('A').point(lambda a:255 if a>=128 else 0)
    bbox=mask.getbbox()
    assert bbox is not None
    cell=cell.crop(bbox); mask=mask.crop(bbox)
    scale=min(84/cell.width,104/cell.height)
    size=(max(1,round(cell.width*scale)),max(1,round(cell.height*scale)))
    cell=cell.resize(size,Image.Resampling.NEAREST)
    mask=mask.resize(size,Image.Resampling.NEAREST)
    rgb=cell.convert('RGB').quantize(colors=48,method=Image.Quantize.MEDIANCUT).convert('RGB')
    compiled=Image.new('RGBA',(88,108),(0,0,0,0))
    offset=((88-size[0])//2,106-size[1])
    rgba=Image.merge('RGBA',(*rgb.split(),mask))
    compiled.paste(rgba,offset)
    # Snap colors to exactly representable 16-bit display values.
    pixels=[]
    for r,g,b,a in compiled.get_flattened_data():
        pixels.append((((r>>3)*255)//31,((g>>2)*255)//63,((b>>3)*255)//31,a) if a else (0,0,0,0))
    compiled.putdata(pixels); compiled.save(destination/f'{state}_00.png')
metadata={'generated_with':'Built-in image_gen; original design using F108pro sprite references',
          'master':'assets/copilot/copilot-transparent.png','master_sha256':hashlib.sha256(master.read_bytes()).hexdigest(),
          'style':'Original lavender pilot/goggles robot; no official mascot or logo',
          'states':states,'runtime_size':[88,108],'source_grid':[3,2],
          'pipeline':'Fixed cell -> source-alpha >=128 -> nearest sampling -> 48-color median palette -> RGB565 + alpha',
          'license':'MIT project license; third-party reference artwork attributed separately',
          'prompts':'assets/copilot/prompts.json'}
(ROOT/'assets/copilot/provenance.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Compiled 6 Copilot sprites with hard transparency at 88x108.')
