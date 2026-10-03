"""Package the approved UI artwork verbatim; never redraw or edit image pixels."""
import base64
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
preview = Path(sys.argv[1]).resolve()
html = preview.read_text(encoding='utf-8')
out = ROOT / 'ProjectResources/SourceArt/UI/Heist'
out.mkdir(parents=True, exist_ok=True)

def payload(name):
    found = re.search(r'<script[^>]*id="' + name + r'"[^>]*>(.*?)</script>', html, re.S)
    assert found, name
    return json.loads(found.group(1))

textures = payload('composition-textures')
names = {'panel': 'Panel', 'keycap': 'Keycap', 'button': 'Button',
         'button-hover': 'ButtonHover', 'button-pressed': 'ButtonPressed',
         'button-primary': 'ButtonPrimary', 'random': 'RandomMap'}
for key, name in names.items():
    data = textures[key]
    assert data.startswith('data:image/png;base64,')
    (out / ('T_Heist_' + name + '.png')).write_bytes(base64.b64decode(data.split(',', 1)[1]))

atlas_source = preview.parent / 'museum-ui-icons-study-v2.png'
atlas_path = out / 'T_Heist_IconAtlas.png'
shutil.copyfile(atlas_source, atlas_path)
atlas = Image.open(atlas_path).convert('RGB')
glyphs = ['coin', 'bag', 'profile', 'flashlight_off', 'flashlight_on', 'microphone',
          'ready', 'map', 'observe', 'painting', 'stunned', 'detention', 'heavy',
          'warning', 'copy', 'settings']
cells = {}
for index, name in enumerate(glyphs):
    col, row = index % 4, index // 4
    x0, y0 = col * atlas.width // 4, row * atlas.height // 4
    x1, y1 = (col + 1) * atlas.width // 4, (row + 1) * atlas.height // 4
    points = [(x, y) for y in range(y0, y1) for x in range(x0, x1)
              if min(atlas.getpixel((x, y))) > 220]
    lo_x, hi_x = min(x for x, y in points), max(x for x, y in points)
    lo_y, hi_y = min(y for x, y in points), max(y for x, y in points)
    side = (max(hi_x - lo_x + 1, hi_y - lo_y + 1) + 27) // 4 * 4
    left, top = (lo_x + hi_x + 1 - side) / 2, (lo_y + hi_y + 1 - side) / 2
    cells[name] = {'index': index, 'uv': [left / atlas.width, top / atlas.height,
                                      side / atlas.width, side / atlas.height]}

metadata = {'approved_preview': str(preview), 'preview_sha256': hashlib.sha256(preview.read_bytes()).hexdigest(),
            'atlas_source': str(atlas_source), 'atlas_pixels_unchanged': True,
            'glyphs': cells, 'textures': {name: 'T_Heist_' + value for name, value in names.items()},
            'image_policy': 'Original approved pixels; UI material samples atlas luminance as opacity.',
            'layout_grid': 4, 'design_size': [1280, 720]}
(out / 'ApprovedComposition.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding='utf-8')
hierarchy = payload('composition-spec')
# The visible approved Title CSS supersedes its older embedded description.
hierarchy['screens']['title']['components']['titlemenu']['geometry'] = '버튼 288×56 · 간격 12 · 문자 중앙 정렬'
(out / 'CompositionHierarchy.json').write_text(json.dumps(hierarchy, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'textures': len(names) + 1, 'glyphs': len(cells), 'out': str(out)}))
