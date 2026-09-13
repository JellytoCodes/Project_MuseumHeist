"""Prepare generated catalogue sprites for Unreal import; retain original source images."""
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ProjectResources/SourceArt/UI/Catalogue'
OUT.mkdir(parents=True, exist_ok=True)
GENERATED = Path('C:/Users/User/.codex/generated_images/01a08b8f-da17-7b93-94c0-0baa20f17791')
SOURCES = {
    'ButtonsSource.png': 'exec-796da14f-cdbf-41c0-96b3-f522368350fd.png',
    'PanelSource.png': 'exec-433a0afb-e693-4099-b81f-156739825b44.png',
    'LogoSource.png': 'exec-8e8b3a1e-32c3-423c-bced-6a847e4bd077.png',
    'BackdropSource.png': 'exec-0f5a604e-97ce-40eb-9a7d-82dc063d0d63.png',
}

def isolate(im, logo=False):
    """Remove neutral preview matte; do not alter the generated foreground artwork."""
    if im.mode == 'RGBA' and im.getchannel('A').getextrema()[0] == 0:
        return im
    rgb = np.array(im.convert('RGB'))
    neutral = (rgb.max(axis=2).astype(int)-rgb.min(axis=2).astype(int) < 14)
    if logo:
        # Preserve warm printed letters, including holes and the cut across the mark.
        alpha = np.clip((rgb[:, :, 0].astype(float)-rgb[:, :, 2]-8)*25.5, 0, 255).astype('uint8')
    else:
        candidate = neutral & (rgb.mean(axis=2) > 64)
        alpha = np.where(candidate, 0, 255).astype('uint8')
    rgba = Image.fromarray(rgb).convert('RGBA')
    rgba.putalpha(Image.fromarray(alpha))
    return rgba

def save_sprite(im, name, size, logo=False):
    im = isolate(im, logo=logo)
    box = im.getchannel('A').getbbox()
    if box is None:
        raise ValueError('Empty sprite '+name)
    im = im.crop(box)
    result = Image.new('RGBA', size)
    im.thumbnail((size[0]-8, size[1]-8), Image.Resampling.LANCZOS)
    result.alpha_composite(im, ((size[0]-im.width)//2, (size[1]-im.height)//2))
    result.save(OUT / (name+'.png'))
    assert size[0] % 4 == size[1] % 4 == 0

for target, source in SOURCES.items():
    if not (OUT/target).exists():
        shutil.copy2(GENERATED/source, OUT/target)
atlas = Image.open(OUT/'ButtonsSource.png')
w,h = atlas.size
for name, box in [
    ('T_Catalogue_Button_Normal',(0,0,w//2,h//2)),
    ('T_Catalogue_Button_Primary',(w//2,0,w,h//2)),
    ('T_Catalogue_Button_Pressed',(0,h//2,w//2,h)),
    ('T_Catalogue_Header',(w//2,h//2,w,h)),
]:
    save_sprite(atlas.crop(box), name, (512,144))
save_sprite(Image.open(OUT/'PanelSource.png'), 'T_Catalogue_Panel', (1024,1024))
save_sprite(Image.open(OUT/'LogoSource.png'), 'T_Catalogue_Logo', (1024,576), logo=True)
Image.open(OUT/'BackdropSource.png').convert('RGB').resize((1920,1080),Image.Resampling.LANCZOS).save(OUT/'T_Catalogue_Backdrop.png')

# Small native geometric control marks do not need an illustration texture.
thumb = Image.new('RGBA',(32,32))
d = ImageDraw.Draw(thumb)
d.polygon([(16,2),(30,16),(16,30),(2,16)],fill=(24,23,21),outline=(224,213,190),width=2)
thumb.save(OUT/'T_Catalogue_Thumb.png')
arrow = Image.new('RGBA',(32,32))
d = ImageDraw.Draw(arrow)
d.line([(6,12),(16,22),(26,12)],fill=(224,213,190),width=3)
arrow.save(OUT/'T_Catalogue_Chevron.png')

font_dir = ROOT/'ProjectResources/SourceArt/UI/Fonts/Nanum'
font_dir.mkdir(parents=True,exist_ok=True)
for family, files in {
    'nanumgothic':['NanumGothic-Regular.ttf','NanumGothic-Bold.ttf'],
    'nanummyeongjo':['NanumMyeongjo-Bold.ttf'],
}.items():
    for file in files+['OFL.txt']:
        dest=font_dir/(family+'-'+file if file=='OFL.txt' else file)
        if not dest.exists():
            urllib.request.urlretrieve('https://raw.githubusercontent.com/google/fonts/main/ofl/'+family+'/'+file,dest)

metadata={'generator':'OpenAI built-in image_gen','approved_concepts':[
    'exec-06256363-c79e-4c6e-8926-79b694e2da75.png','exec-4bb493ce-a3ac-41e7-8104-340a4130f603.png'],
    'processing':'sprite isolation, transparent matte, padded resizing; foreground illustration preserved',
    'texture_size_unit':4,'widget_layout_unit':4,'font_size_unit':4,
    'fonts':{'NanumGothic':'https://github.com/google/fonts/tree/main/ofl/nanumgothic','NanumMyeongjo':'https://github.com/google/fonts/tree/main/ofl/nanummyeongjo','license':'SIL Open Font License 1.1; original OFL files staged by RuntimeDependencies'},'textures':{}}
for p in OUT.glob('T_*.png'):
    im=Image.open(p)
    metadata['textures'][p.stem]={'origin':'Unreal current-map scene capture' if '_Map_' in p.stem else 'native geometric control' if p.stem.endswith(('Thumb','Chevron')) else 'OpenAI image_gen', 'size':im.size,'alpha':im.getchannel('A').getextrema() if im.mode=='RGBA' else None, 'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
(OUT/'CatalogueSource.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print(json.dumps(metadata,indent=2))
