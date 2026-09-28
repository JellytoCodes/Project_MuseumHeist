"""Draw the M03 authoring report in the same world bounds used by the HUD map."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[3]
report=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/M03/M03GalleryLayout.json').read_text())
out=ROOT/'ProjectResources/SourceArt/Gallery/M03';out.mkdir(parents=True,exist_ok=True)
W,H=1536,1060
im=Image.new('RGB',(W,H),'#121923');d=ImageDraw.Draw(im)
def p(x,y):return ((x-3)/55*W,(y-3)/38*H)
for r in report['rooms']:
 x0,y0,x1,y1=r['bounds']
 col='#303b47' if r['name'].startswith('Staff') else '#665543' if r['name'] in ('Cell','Evidence','Guard') else '#344c50' if r['name']=='Vault' else '#45505b'
 d.rectangle((*p(x0,y0),*p(x1,y1)),fill=col)
for r in report['walls']:
 if r['bottom']>0:continue
 d.line([p(*r['a']),p(*r['b'])],fill='#d7c7a9',width=max(5,round(r['thickness']/100/55*W)))
# Door bars identify the actual detention boundary, not live security coverage.
d.line([p(52,16.1),p(52,17.9)],fill='#b3946e',width=5)
for y in (16.2,16.5,16.8,17.1,17.4,17.7):d.line([p(51.9,y),p(52.1,y)],fill='#dfccb0',width=3)
im.resize((1024,704),Image.Resampling.LANCZOS).save(out/'T_FloorPlan_M03.png')
# Review image contains room labels only; gameplay adds its own dynamic markers.
font=ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf',28)
small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',22)
labels=[('중앙 갤러리',25.5,26.5),('전시실 A',14,14),('전시실 B',26,11),('전시실 C',37.5,14),('직원 통로',25,6),('침입·정산',13,36.5),('경비실',52,11),('압수품',49.5,17),('구금실',54.5,17),('고가 전시실',52,26.5)]
for text,x,y in labels:d.text(p(x,y),text,font=small if text in ('압수품','구금실') else font,anchor='mm',fill='#fff0d7',stroke_width=1,stroke_fill='#25313b')
im.save(out/'M03_LayoutReview.png')
rows=json.loads((ROOT/'ProjectResources/DataTableImports/DT_MapPresentation.json').read_text(encoding='utf-8'))
r=next(r for r in rows if r['Name']=='M03')
r['MapDisplayName']='글래스하우스 갤러리';r['WorldMin']={'X':-2700,'Y':-1900};r['WorldMax']={'X':2800,'Y':1900}
r['MapNorthAxis']='PositiveY'
r['ZoneAnchors']=[{'ZoneId':z,'DisplayName':text,'WorldLocation':{'X':round((x-30)*100),'Y':round((22-y)*100)}} for z,text,x,y in [
 ('Zone_V','침입·정산',13,36.5),('Zone_H','중앙 갤러리',25.5,26.5),('Zone_A','전시실 A',14,14),('Zone_B','전시실 B',26,11),('Zone_C','전시실 C',37.5,14),('Zone_HV','고가 전시실',52,26.5),('Zone_EV','압수품 보관실',49.5,17),('Zone_DT','구금실',54.5,17)]]
r['ContractTargetGalleryZoneId']='Zone_C'
r['DefaultExitAnchors']=[{'ExitId':'Exit_Main','DisplayName':'진입·배출 벤트','WorldLocation':{'X':-1320,'Y':-1670}}]
(ROOT/'ProjectResources/DataTableImports/DT_MapPresentation.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('M03 floor-plan source and presentation row updated')
