"""Append native slides, preserving all user-edited visible first-chapter XML."""
from pathlib import Path
from copy import deepcopy
import hashlib, json, posixpath, re, zipfile
import xml.etree.ElementTree as E

ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT/'report/workflow/full'
BASE=ROOT/'report/output/中期汇报_第一章_空间音效处理算法研究与实现_终稿.pptx'
NS={'p':'http://schemas.openxmlformats.org/presentationml/2006/main',
    'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
    'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships',
    'rel':'http://schemas.openxmlformats.org/package/2006/relationships',
    'ct':'http://schemas.openxmlformats.org/package/2006/content-types'}
for k in ['p','a','r']:E.register_namespace(k,NS[k])
def xml(root):
 if root.tag=='{'+NS['ct']+'}Types':E.register_namespace('',NS['ct'])
 if root.tag=='{'+NS['rel']+'}Relationships':E.register_namespace('',NS['rel'])
 return E.tostring(root,encoding='utf-8',xml_declaration=True)
def parse(data):return E.fromstring(data)
def relpath(owner):return posixpath.join(posixpath.dirname(owner),'_rels',posixpath.basename(owner)+'.rels')
def ownerpath(rels):
 d,b=rels.rsplit('/_rels/',1);return d+'/'+b[:-5]
def resolve(owner,target):
 return posixpath.normpath(target.lstrip('/') if target.startswith('/') else posixpath.join(posixpath.dirname(owner),target))
def relations(data):return {r.get('Id'):r for r in parse(data)}

# Restore the template's exact heading run properties in new content only.
styles={}
with zipfile.ZipFile(ROOT/'report/template.pptx') as z:
 for role,number,sid in [('chapter',3,'35'),('number',3,'34'),('header',4,'8'),('sub',4,'26')]:
  rt=parse(z.read(f'ppt/slides/slide{number}.xml'))
  sh=next(s for s in rt.findall('.//p:sp',NS) if s.find('p:nvSpPr/p:cNvPr',NS).get('id')==sid)
  styles[role]=deepcopy(sh.find('.//a:rPr',NS))
with zipfile.ZipFile(BUILD/'append-draft.pptx') as z:added={n:z.read(n) for n in z.namelist()}
heading_checks=[]
for name in list(added):
 if re.fullmatch(r'ppt/slides/slide\d+\.xml',name):
  rt=parse(added[name])
  for sh in rt.findall('.//p:sp',NS):
   prop=sh.find('p:nvSpPr/p:cNvPr',NS);tag=prop.get('name','') if prop is not None else ''
   role=tag.split('.')[1] if tag.startswith('report.') else None
   if role not in styles:continue
   for run in sh.findall('.//a:r',NS):
    prior=run.find('a:rPr',NS);fresh=deepcopy(styles[role]);fill=prior.find('a:solidFill',NS) if prior is not None else None
    if fill is not None:
     old=fresh.find('a:solidFill',NS)
     if old is not None:fresh.remove(old)
     fresh.insert(0,deepcopy(fill))
    if prior is not None:run.remove(prior)
    run.insert(0,fresh)
   heading_checks.append({'part':name,'name':tag,'role':role,'size':styles[role].get('sz'),'bold':styles[role].get('b'),'spacing':styles[role].get('spc')})
  added[name]=xml(rt)

with zipfile.ZipFile(BASE) as z:base={n:z.read(n) for n in z.namelist()}
bpr=parse(base['ppt/presentation.xml']);apr=parse(added['ppt/presentation.xml'])
brel=parse(base['ppt/_rels/presentation.xml.rels']);arel=relations(added['ppt/_rels/presentation.xml.rels'])
bslides=bpr.find('p:sldIdLst',NS);aslides=apr.find('p:sldIdLst',NS)
assert len(bslides)==8 and len(aslides)==10
existing_slide_parts=[resolve('ppt/presentation.xml',relations(base['ppt/_rels/presentation.xml.rels'])[s.get('{'+NS['r']+'}id')].get('Target')) for s in bslides]
mapping={};copy_names=[]
maxslide=max(int(re.search(r'slide(\d+)\.xml$',n).group(1)) for n in base if re.fullmatch(r'ppt/slides/slide\d+\.xml',n))
slide_parts=[resolve('ppt/presentation.xml',arel[s.get('{'+NS['r']+'}id')].get('Target')) for s in aslides]
for i,n in enumerate(slide_parts):mapping[n]=f'ppt/slides/slide{maxslide+i+1}.xml'
base_media={hashlib.sha256(v).hexdigest():n for n,v in base.items() if n.startswith('ppt/media/')}
for n in added:
 if not n.startswith('ppt/') or n.endswith('.rels') or n=='ppt/presentation.xml' or n.startswith('ppt/fonts/'):continue
 if n in ['ppt/notesMasters/notesMaster1.xml','ppt/presProps.xml','ppt/viewProps.xml','ppt/tableStyles.xml'] and n in base:
  mapping[n]=n;continue
 if n.startswith('ppt/media/') and hashlib.sha256(added[n]).hexdigest() in base_media:
  mapping[n]=base_media[hashlib.sha256(added[n]).hexdigest()];continue
 mapping.setdefault(n,'ppt/append/'+n[4:]);copy_names.append(n)
for old in copy_names:
 base[mapping[old]]=added[old]
 oldrels=relpath(old)
 if oldrels not in added:continue
 rt=parse(added[oldrels]);newowner=mapping[old]
 for rel in rt:
  if rel.get('TargetMode')=='External':continue
  target=resolve(old,rel.get('Target'))
  newtarget=mapping.get(target,target)
  assert newtarget in base or target in copy_names,(old,target,newtarget)
  rel.set('Target',posixpath.relpath(newtarget,posixpath.dirname(newowner)))
 base[relpath(newowner)]=xml(rt)

ridnames={r.get('Id') for r in brel}
def addrel(kind,target):
 k=1
 while 'rIdAppend'+str(k) in ridnames:k+=1
 rid='rIdAppend'+str(k);ridnames.add(rid)
 E.SubElement(brel,'{'+NS['rel']+'}Relationship',{'Id':rid,'Type':NS['r']+'/'+kind,'Target':posixpath.relpath(target,'ppt')})
 return rid
maxid=max(int(s.get('id')) for s in bslides)
for i,old in enumerate(slide_parts):
 rid=addrel('slide',mapping[old]);E.SubElement(bslides,'{'+NS['p']+'}sldId',{'id':str(maxid+i+1),'{'+NS['r']+'}id':rid})
masters=bpr.find('p:sldMasterIdLst',NS);nextid=max(int(x.get('id')) for x in masters)+1
for m in apr.findall('p:sldMasterIdLst/p:sldMasterId',NS):
 target=resolve('ppt/presentation.xml',arel[m.get('{'+NS['r']+'}id')].get('Target'))
 rid=addrel('slideMaster',mapping[target]);E.SubElement(masters,'{'+NS['p']+'}sldMasterId',{'id':str(nextid),'{'+NS['r']+'}id':rid});nextid+=1
base['ppt/presentation.xml']=xml(bpr);base['ppt/_rels/presentation.xml.rels']=xml(brel)

ct=parse(base['[Content_Types].xml']);act=parse(added['[Content_Types].xml'])
defaults={x.get('Extension') for x in ct if x.tag.endswith('Default')};overrides={x.get('PartName') for x in ct if x.tag.endswith('Override')}
for item in act:
 if item.tag.endswith('Default'):
  if item.get('Extension') not in defaults:ct.append(deepcopy(item));defaults.add(item.get('Extension'))
 else:
  old=item.get('PartName').lstrip('/')
  if old not in copy_names:continue
  new='/'+mapping[old]
  if new not in overrides:
   fresh=deepcopy(item);fresh.set('PartName',new);ct.append(fresh);overrides.add(new)
base['[Content_Types].xml']=xml(ct)

# Synchronize only obsolete first-chapter notes. Visible user-edited slides remain byte-identical.
updated_notes=[]
for n in list(base):
 if not re.fullmatch(r'ppt/notesSlides/notesSlide\d+\.xml',n):continue
 rt=parse(base[n]);changed=False
 for text in rt.findall('.//a:t',NS):
  before=text.text or '';after=before.replace('本文件先完成第一章，介绍空间音效处理算法的研究过程、实现与验证。','本文件覆盖三章，依次介绍空间算法、Qt 图形交互开发和不足与后续工作。').replace('当前新角度通过 CLI 配置，Qt 仍使用兼容模式。','当前 GUI 已接入 710 个方向及 spatial / dual 具名参数，旧 CLI 兼容模式继续保留。')
  if after!=before:text.text=after;changed=True
 if changed:base[n]=xml(rt);updated_notes.append(n)
if 'docProps/app.xml' in base:
 rt=parse(base['docProps/app.xml'])
 for el in rt.iter():
  if el.tag.endswith('}Slides'):el.text='18'
 base['docProps/app.xml']=xml(rt)

with zipfile.ZipFile(BASE) as original:
 preservation=[{'part':n,'byte_identical':original.read(n)==base[n]} for n in existing_slide_parts]
assert all(x['byte_identical'] for x in preservation)
out=BUILD/'candidate.pptx'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,v in base.items():z.writestr(n,v)
(BUILD/'title-style-check.json').write_text(json.dumps(heading_checks,ensure_ascii=False,indent=2),encoding='utf-8')
(BUILD/'merge-check.json').write_text(json.dumps({'slides':18,'first_chapter_preservation':preservation,'notes_updated':updated_notes,'source_base_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),'output_size':out.stat().st_size},ensure_ascii=False,indent=2),encoding='utf-8')
print('MERGED',out,'slides',18,'bytes',out.stat().st_size)
