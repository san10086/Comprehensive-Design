"""Copy exact source run properties for each heading level after native authoring."""
from pathlib import Path
from copy import deepcopy
import json, zipfile, xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
NS={'p':'http://schemas.openxmlformats.org/presentationml/2006/main',
    'a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
for key,value in NS.items(): ET.register_namespace(key,value)
styles={}
with zipfile.ZipFile(ROOT/'report/template.pptx') as source:
    for role,number,shapeid in [('cover',1,'49'),('coverSub',1,'50'),('toc',2,'11'),
                               ('tocTitle',2,'9'),('chapter',3,'35'),('number',3,'34'),
                               ('header',4,'8'),('sub',4,'26')]:
        rt=ET.fromstring(source.read(f'ppt/slides/slide{number}.xml'))
        sh=next(s for s in rt.findall('.//p:sp',NS)
                if s.find('p:nvSpPr/p:cNvPr',NS).get('id')==shapeid)
        styles[role]=deepcopy(sh.find('.//a:rPr',NS))

records=[]
src=ROOT/'report/workflow/draft.pptx'
dst=ROOT/'report/workflow/candidate.pptx'
with zipfile.ZipFile(src) as archive,zipfile.ZipFile(dst,'w',zipfile.ZIP_DEFLATED) as out:
    for item in archive.infolist():
        data=archive.read(item.filename)
        if item.filename.startswith('ppt/slides/slide') and item.filename.endswith('.xml'):
            root=ET.fromstring(data)
            for shape in root.findall('.//p:sp',NS):
                prop=shape.find('p:nvSpPr/p:cNvPr',NS)
                name=prop.get('name','') if prop is not None else ''
                if not name.startswith('report.'):continue
                role=name.split('.')[1]
                if role not in styles:continue
                for run in shape.findall('.//a:r',NS):
                    prior=run.find('a:rPr',NS)
                    fill=deepcopy(prior.find('a:solidFill',NS)) if prior is not None and prior.find('a:solidFill',NS) is not None else None
                    fresh=deepcopy(styles[role])
                    # White / green are intentional template-derived chapter variants.
                    if fill is not None:
                        old=fresh.find('a:solidFill',NS)
                        if old is not None:fresh.remove(old)
                        fresh.insert(0,fill)
                    if prior is not None:run.remove(prior)
                    run.insert(0,fresh)
                records.append({'slide_part':item.filename,'name':name,'role':role,'reference_font':styles[role].find('a:latin',NS).get('typeface'),
                                'font_size_hundredth_pt':styles[role].get('sz'),'bold':styles[role].get('b'),'spacing_hundredth_pt':styles[role].get('spc')})
            data=ET.tostring(root,encoding='utf-8',xml_declaration=True)
        out.writestr(item,data)
(ROOT/'report/workflow/qa/title-style-check.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Copied exact template heading run properties:',len(records),'heading objects')
print('Candidate:',dst)
