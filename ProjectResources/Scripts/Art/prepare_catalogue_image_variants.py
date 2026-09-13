"""Prepare whole-image UI canvases with uniform scaling; never slice or stretch art."""
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ProjectResources/SourceArt/UI/Catalogue'
GENERATED = Path.home() / '.codex/generated_images/01a08b8f-da17-7b93-94c0-0baa20f17791'
SOURCES = {
    'Field': 'exec-e9add85e-eea3-4550-bf9c-e783af6fef9f.png',
    'PanelLandscape': 'exec-bb9940c0-b51a-466e-9143-d6bfb3a016b0.png',
    'PanelPortrait': 'exec-699d06d9-4abe-42f7-8412-21dd2c58189d.png',
    'PanelWide': 'exec-58371209-06ff-4f4a-a27f-4ff43d3e9f88.png',
    'PanelStrip': 'exec-457ef277-f63d-476b-8a42-aba7e37f7955.png',
    'PanelCard': 'exec-00340ba5-2da7-47a2-8e49-514a482c99d9.png',
}
report = {'generator': 'OpenAI built-in image_gen', 'draw_as': 'Image', 'brush_margin': 0,
          'processing': 'Remove edge-connected preview matte, crop, uniformly fit into final canvas. No nine-slice, anisotropic resize, or baked stretching.',
          'font_display_dpi': 72, 'font_render_dpi': 96, 'font_display_size_unit': 4,
          'sources': {}, 'textures': {}}


def isolate(path):
    im = Image.open(path).convert('RGBA')
    a = np.asarray(im).copy()
    rgb = a[:, :, :3].astype(np.int16)
    candidate = (rgb.max(axis=2) - rgb.min(axis=2) <= 18) & (rgb.mean(axis=2) >= 80)
    ys, xs = np.nonzero(~candidate)
    bounds = (int(xs.min()), int(ys.min()), int(xs.max()+1), int(ys.max()+1))
    mask = Image.fromarray(np.where(candidate, 255, 0).astype('uint8')).copy()
    for xy in [(0, 0), (im.width-1, 0), (0, im.height-1), (im.width-1, im.height-1)]:
        if mask.getpixel(xy) == 255:
            ImageDraw.floodfill(mask, xy, 128)
    a[:, :, 3][np.asarray(mask) == 128] = 0
    im = Image.fromarray(a)
    return im.crop(bounds), bounds


def canvas(source, key, size):
    fitted = source.copy()
    fitted.thumbnail(size, Image.Resampling.LANCZOS)
    out = Image.new('RGBA', size)
    out.alpha_composite(fitted, ((size[0]-fitted.width)//2, (size[1]-fitted.height)//2))
    path = OUT / ('T_Catalogue_' + key + '.png')
    out.save(path)
    report['textures'][path.stem] = {'size': size, 'source_size': source.size,
        'uniform_fit_size': fitted.size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


art = {}
for key, filename in SOURCES.items():
    path = OUT / (key + 'SourceV2.png')
    if not path.exists():
        shutil.copy2(GENERATED / filename, path)
    art[key], bounds = isolate(path)
    report['sources'][key] = {'file': path.name, 'generated_file': filename, 'crop': bounds,
        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

for key, source, size in [
    ('Field', 'Field', (688, 80)),
    ('SettingsPanel', 'PanelLandscape', (1360, 752)),
    ('JoinPanel', 'PanelLandscape', (1008, 560)),
    ('MapPanel', 'PanelLandscape', (1920, 1080)),
    ('InventoryPanel', 'PanelPortrait', (768, 960)),
    ('RewardPanel', 'PanelPortrait', (540, 700)),
    ('RecapPanel', 'PanelWide', (1400, 352)),
    ('ContributionPanel', 'PanelStrip', (1400, 328)),
    ('PlayerPanel', 'PanelCard', (400, 300)),
    ('HUDMissionPanel', 'PanelCard', (304, 208)),
    ('HUDAlertPanel', 'PanelWide', (344, 112)),
    ('HUDTeamPanel', 'PanelWide', (304, 80)),
    ('HUDTutorialPanel', 'PanelWide', (800, 200)),
]:
    canvas(art[source], key, size)

canvas(Image.open(OUT/'T_Catalogue_Panel.png').convert('RGBA'), 'HUDSlotPanel', (88,88))

for width, height in [(384,108), (224,64), (256,72), (128,36), (200,56), (144,40)]:
    for state in ['Normal', 'Primary', 'Pressed']:
        source = Image.open(OUT / ('T_Catalogue_Button_' + state + '.png')).convert('RGBA')
        canvas(source, f'Button_{state}_{width}x{height}', (width,height))
for size in [(240,68), (256,72)]:
    canvas(Image.open(OUT/'T_Catalogue_Header.png').convert('RGBA'), f'Header_{size[0]}x{size[1]}', size)

(OUT/'CatalogueImageVariants.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'sources': len(report['sources']), 'textures': len(report['textures']),
                  'crops': {k:v['crop'] for k,v in report['sources'].items()}}))
