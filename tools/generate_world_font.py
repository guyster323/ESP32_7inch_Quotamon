"""Build the shared 16px LCD fallback for the expanded language catalog."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request

from generate_locales import ROOT, lcd_catalog

sys.path.insert(0,str(ROOT/'.tools/python'))
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
import uharfbuzz as hb

FONTS={
    'hi':('NotoSansDevanagari','notosansdevanagari','wdth,wght',
          '02b053e964463622c753f474918846343eb45407545eeb9ed517ac0136ab211b'),
    'bn':('NotoSansBengali','notosansbengali','wdth,wght',
          '83c25162b92f22ef58de84bec4487c4ec4e704d29ada127a61f02c1c09a63d3d'),
    'ar':('NotoSansArabic','notosansarabic','wdth,wght',
          '0034c0191a40ff93d03409e666be59c1dbb5b7a54f0e300a864b588ba850ad5b'),
}
PIN='2894aab31764f10f29c421bdfd2340d3b382d384'
CACHE=ROOT/'.tools/fonts'
GENERATED=ROOT/'native/generated'


def download(path,url):
    if not path.exists():
        with urllib.request.urlopen(url,timeout=45) as response:
            path.write_bytes(response.read())


def word_font(code,words):
    face,folder,axes,expected=FONTS[code]
    source=f'https://raw.githubusercontent.com/google/fonts/{PIN}/ofl/{folder}'
    path=CACHE/f'{face}.ttf'
    download(path,f'{source}/{face}%5B{axes}%5D.ttf')
    license_file=CACHE/f'{face}-OFL.txt'
    download(license_file,f'{source}/OFL.txt')
    original=path.read_bytes()
    assert hashlib.sha256(original).hexdigest()==expected,f'Unreviewed {face} font'
    shutil.copyfile(license_file,GENERATED/license_file.name)
    font=instantiateVariableFont(TTFont(path),{'wght':500,'wdth':100},inplace=True)
    glyphs=font.getGlyphSet()
    names=font.getGlyphOrder()
    shaped=hb.Font(hb.Face(original))
    shaped.set_variations({'wght':500,'wdth':100})
    for language,word,char in words:
        buffer=hb.Buffer()
        buffer.add_str(word)
        buffer.guess_segment_properties()
        buffer.language=language
        hb.shape(shaped,buffer)
        pen=TTGlyphPen(glyphs)
        x=y=0
        for glyph,position in zip(buffer.glyph_infos,buffer.glyph_positions):
            assert glyph.codepoint,f'Missing {face} glyph in {word}'
            pen.addComponent(names[glyph.codepoint],(1,0,0,1,x+position.x_offset,y+position.y_offset))
            x+=position.x_advance
            y+=position.y_advance
        name=f'quotamon{ord(char):04x}'
        font['glyf'][name]=pen.glyph()
        font['glyf'][name].recalcBounds(font['glyf'])
        font['hmtx'][name]=(x,font['glyf'][name].xMin)
        for cmap in font['cmap'].tables:
            if cmap.isUnicode():
                cmap.cmap[ord(char)]=name
    output=CACHE/f'{face}-LCD.ttf'
    font.save(output)
    return output


def main():
    messages=json.loads((ROOT/'locales/messages.json').read_text(encoding='utf-8'))
    names,rows,words=lcd_catalog(messages)
    args=['node','.tools/fontconv/node_modules/lv_font_conv/lv_font_conv.js',
          '--size','16','--bpp','2','--format','lvgl','--no-kerning']
    for code in FONTS:
        subset=[(language,word,char) for (language,word),char in words.items()
                if language==code or (code=='ar' and language=='ur')]
        font=word_font(code,subset)
        args+=['--font',str(font.relative_to(ROOT)),'--symbols',''.join(c for _,_,c in subset)]

    kr=CACHE/'NotoSansKR-Medium.ttf'
    arabic=TTFont(CACHE/'NotoSansArabic.ttf').getBestCmap()
    latin=TTFont(kr).getBestCmap()
    cjk=TTFont(CACHE/'NotoSansSC-Medium.ttf').getBestCmap()
    old=set(''.join(names[:5])+''.join(''.join(row[:5]) for row in rows))
    new=set(''.join(names)+''.join(''.join(row) for row in rows))
    extra={c for c in new-old if ord(c)>0x24f and c not in words.values()}
    devanagari=CACHE/'NotoSansDevanagari.ttf'
    for path,cmap in [(kr,latin),(CACHE/'NotoSansArabic.ttf',arabic),(CACHE/'NotoSansSC-Medium.ttf',cjk),
                      (devanagari,TTFont(devanagari).getBestCmap())]:
        covered={c for c in extra if ord(c) in cmap}
        if covered:
            args+=['--font',str(path.relative_to(ROOT)),'--symbols',''.join(sorted(covered))]
            extra-=covered
    assert not extra,f'Missing LCD glyphs: {sorted(extra)}'
    args+=['-o','native/generated/font_world16.c']
    subprocess.run(args,check=True,cwd=ROOT)

    manifest_path=GENERATED/'asset-manifest.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    from generate_locales import LANGUAGES
    manifest['ui_languages']['codes']=list(LANGUAGES)
    manifest['ui_languages']['world_fallback']={
        'font':'font_world16','size':16,'bpp':2,'compressed':True,
        'shaped_words':len(words),
        'sources':{face:{
            'source':f'https://github.com/google/fonts/tree/{PIN}/ofl/{folder}',
            'source_sha256':sha,
        } for face,folder,_,sha in FONTS.values()}
    }
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'LCD fallback: {len(words)} pre-shaped words')


if __name__=='__main__':
    main()
