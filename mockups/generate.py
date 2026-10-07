"""Rebuild the ESP32 visual reference from checked-in firmware assets.

No firmware files are changed. Run with a Python environment containing Pillow.
"""
import re, json, shutil
from pathlib import Path
from PIL import Image

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent
SRC = ROOT / 'targets/esp32-p4/main/assets'
for path in SRC.glob('*.c'):
    s = path.read_text()
    if 'const lv_image_dsc_t' not in s or '.header = {' not in s: continue
    name = path.stem
    header = s[s.index('const lv_image_dsc_t'):]
    w,h,stride = [int(re.search(r'\.'+key+r'\s*=\s*(\d+)',header)[1]) for key in ('w','h','stride')]
    cf = re.search(r'\.cf\s*=\s*LV_COLOR_FORMAT_(\w+)',header)[1]
    array = s[s.index('{')+1:s.index('};')]
    data = bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',array))
    assert len(data) == stride*h, (name,len(data),stride*h)
    if cf == 'ARGB8888':
        im = Image.frombytes('RGBA',(w,h),data,'raw','BGRA',stride)
    elif cf == 'RGB565':
        pixels=[]
        for y in range(h):
            for x in range(w):
                v=int.from_bytes(data[y*stride+x*2:y*stride+x*2+2],'little')
                r,g,b=(v>>11)&31,(v>>5)&63,v&31
                pixels.append(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)))
        im=Image.new('RGB',(w,h)); im.putdata(pixels)
    elif cf == 'A8':
        im=Image.new('RGBA',(w,h),'#ffb040' if name=='conditions_edge_indicator' else '#b8ddff')
        im.putalpha(Image.frombytes('L',(w,h),data,'raw','L',stride))
    else: raise ValueError(cf)
    im.save(OUT/'assets'/f'{name}.png')
    if cf == 'A8':
        tinted=Image.new('RGBA',im.size,'#ec9682' if name=='conditions_edge_indicator' else '#c17a7a')
        tinted.putalpha(im.getchannel('A'))
        tinted.save(OUT/'assets'/f'{name}_night.png')
    # LVGL recolor is a linear mix with the original RGB; preserve alpha.
    if name.startswith('weather_icon_') or name.startswith('analog_'):
        rgba=im.convert('RGBA')
        if name.startswith('weather_icon_'): color,amount=(176,107,107),.4
        elif name=='analog_second_hand': color,amount=(187,132,132),.6
        elif name=='analog_center_cap': color,amount=(214,170,170),.3
        else: color,amount=(236,192,192),.8
        solid=Image.new('RGBA',rgba.size,(*color,255))
        tinted=Image.blend(rgba,solid,amount);tinted.putalpha(rgba.getchannel('A'))
        tinted.save(OUT/'assets'/f'{name}_tinted.png')
shutil.copy(ROOT/'targets/esp32-p4/managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf',OUT/'assets/Montserrat-Medium.ttf')
# Preserve the actual web page and replace its API with isolated sample data.
s=(ROOT/'targets/esp32-p4/main/web/messages.html').read_text()
stub='''<script>
const sampleMessages=[{id:'sample',target:'Kitchen',sender:'Alex',text:'Dinner is ready.',createdAt:'2026-10-05T22:30:00Z'}];
window.fetch=async function(url,opts){let data={}; if(url.includes('message-targets'))data={targets:[{id:'all',label:'All clocks'},{id:'Kitchen',label:'Kitchen'}]}; else if(!opts||!opts.method)data={messages:sampleMessages}; else if(opts.method==='POST') data={success:true}; return {ok:true,json:async()=>data};};
</script>'''
s=s.replace('</head>',stub+'</head>')
(OUT/'house-messages.html').write_text(s)
print('Decoded firmware artwork and copied the original font and messaging page.')
