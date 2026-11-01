from pathlib import Path
import zipfile,xml.etree.ElementTree as E
p=Path(__file__).resolve().parent/'append-draft.pptx'
ns={'p':'http://schemas.openxmlformats.org/presentationml/2006/main','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
for k,v in ns.items():E.register_namespace(k,v)
with zipfile.ZipFile(p) as z:parts={n:z.read(n) for n in z.namelist()}
fixes={4:{'report.box.cmd':(770,187),'report.body.commands':(787,162)},6:{'report.box.cmd':(874,131),'report.body.commands':(884,117)},10:{'report.box.cmd':(842,163),'report.body.commands':(854,147)}}
for i,shapes in fixes.items():
 name=f'ppt/slides/slide{i}.xml';root=E.fromstring(parts[name]);found=[]
 for sh in root.findall('.//p:sp',ns):
  prop=sh.find('p:nvSpPr/p:cNvPr',ns);key=prop.get('name','')
  if key not in shapes:continue
  y,h=shapes[key];xf=sh.find('p:spPr/a:xfrm',ns);xf.find('a:off',ns).set('y',str(y*9525));xf.find('a:ext',ns).set('cy',str(h*9525));found.append(key)
 assert set(found)==set(shapes),(i,found)
 parts[name]=E.tostring(root,encoding='utf-8',xml_declaration=True)
with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
 for n,v in parts.items():z.writestr(n,v)
print('Command frames expanded on appended pages 4, 6, 10')
