"""Measured before/after review of room roles, physical walls and artwork."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Automation/RoomCorridor'
FONT='C:/Windows/Fonts/malgun.ttf'


def render():
    before=json.loads((OUT/'layout-before.json').read_text(encoding='utf-8'))['maps']
    after=json.loads((ROOT/'ProjectResources/SourceArt/Gallery/MuseumLevelLayout.json').read_text(encoding='utf-8'))['maps']
    image=Image.new('RGB',(2000,2180),'#141917');draw=ImageDraw.Draw(image)
    def text(x,y,value,size=24,fill='#eee6d2'):
        draw.text((x,y),value,font=ImageFont.truetype(FONT,size),fill=fill)
    text(48,28,'방과 복도 구조 개편 · 실제 배치 좌표',36)
    text(48,86,'좌: 확장 직후  /  우: 이번 개편안     ·     노랑: 복도  파랑: 전시실  초록: 정원  빨강: 보안 구역',24)
    for mi,(old,new) in enumerate(zip(before,after)):
        top=150+mi*650
        text(48,top,new['id']+'  '+{'M01':'중앙홀 · 굴절 복도 · 소전시실','M02':'정원 회랑 · 비대칭 전시 날개','M03':'교대로 꺾이는 복도 · 독립 전시실'}[new['id']],28)
        for ci,m in enumerate((old,new)):
            left=48+ci*1000;x0,y0,x1,y1=m['bounds'];scale=min(900/(x1-x0),550/(y1-y0))
            ox=left+(900-(x1-x0)*scale)/2;oy=top+55
            def point(p):return (ox+(p[0]-x0)*scale,oy+(y1-p[1])*scale)
            def rect(b,color):
                a=point([b[0],b[3]]);z=point([b[2],b[1]])
                draw.rectangle((*a,*z),fill=color)
            for r in m['rooms'].values():
                color={'gallery':'#293e4d','corridor':'#756641','service':'#4c4939','garden':'#284e36',
                       'security':'#543335','hall':'#3f4541','entry':'#4c4540'}.get(r['kind'],'#284e36')
                rect(r['bounds'],color)
                b=r['bounds'];pos=point([(b[0]+b[2])/2,(b[1]+b[3])/2])
                draw.text(pos,r['id'],font=ImageFont.truetype(FONT,16),fill='#c6c9be',anchor='mm')
            for p in m['props']:rect(p['bounds'],'#85877c' if p['kind'] in ('wall','screen') else '#555d52')
            for w in m['walls']:
                f,a,b,t=w['fixed'],w['start'],w['end'],w['thickness']
                rect([a,f-t/2,b,f+t/2] if w['axis']=='h' else [f-t/2,a,f+t/2,b],
                     '#f0b574' if w['id'].startswith(('ROOM_CLOSED','CW','CE','CC')) else '#bdbfb1')
            for p in m['paintings']:
                x,y=point(p['approach'] if p['active'] else p['xy']);r=3 if p['active'] else 1.5
                draw.ellipse((x-r,y-r,x+r,y+r),fill='#f4d282' if p['active'] else '#8296a0')
            for key,label,color in [('entry','IN','#a4e7c2'),('vent','EXIT','#e6b585')]:
                x,y=point(m[key]);draw.text((x,y),label,font=ImageFont.truetype(FONT,16),fill=color)
            text(left,top+608,('변경 전' if ci==0 else '변경 후')+f" · {len(m['rooms'])}개 공간 / {len(m['doors'])}개 개구부",20)
    text(48,2112,'색칠된 복도는 용도 구분 · 밝은 주황 벽은 신설/폐쇄 구간 · 점은 작품 위치 · Nav와 실제 화면은 별도 검증',20)
    image.save(OUT/'comparison.png')


if __name__=='__main__':render()
