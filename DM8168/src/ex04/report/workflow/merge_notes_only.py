"""Transfer Artifact Tool note paragraphs while preserving every other source part."""
from pathlib import Path
from copy import deepcopy
from xml.dom.minidom import parseString
import hashlib,json,posixpath,zipfile

ROOT=Path(__file__).resolve().parents[2];DIR=ROOT/'report/workflow/notes-revision'
P='http://schemas.openxmlformats.org/presentationml/2006/main'
A='http://schemas.openxmlformats.org/drawingml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
def targets(parts,part):
 rel=posixpath.join(posixpath.dirname(part),'_rels',posixpath.basename(part)+'.rels')
 return {x.getAttribute('Id'):(x.getAttribute('Type'),posixpath.normpath(posixpath.join(posixpath.dirname(part),x.getAttribute('Target'))).lstrip('/')) for x in parseString(parts[rel]).documentElement.childNodes if x.nodeType==x.ELEMENT_NODE}
def order(parts):
 rel=targets(parts,'ppt/presentation.xml')
 return [rel[s.getAttributeNS(R,'id')][1] for s in parseString(parts['ppt/presentation.xml']).getElementsByTagNameNS(P,'sldId')]
def body(document):
 for sh in document.getElementsByTagNameNS(P,'sp'):
  ph=sh.getElementsByTagNameNS(P,'ph')
  if ph and ph[0].getAttribute('type')=='body':return sh.getElementsByTagNameNS(P,'txBody')[0]
 raise ValueError('No notes body')
with zipfile.ZipFile(DIR/'source.pptx') as z:source={n:z.read(n) for n in z.namelist()};infos=z.infolist()
with zipfile.ZipFile(DIR/'artifact-notes.pptx') as z:generated={n:z.read(n) for n in z.namelist()}
source_order=order(source);generated_order=order(generated)
revised=json.loads((DIR/'notes-after.json').read_text(encoding='utf-8'))
result=dict(source);changed=[]
for record in revised:
 i=record['slide']-1
 original_note=next(path for typ,path in targets(source,source_order[i]).values() if typ.endswith('/notesSlide'))
 generated_note=next(path for typ,path in targets(generated,generated_order[i]).values() if typ.endswith('/notesSlide'))
 original_doc=parseString(source[original_note]);generated_doc=parseString(generated[generated_note])
 original_body=body(original_doc);generated_body=body(generated_doc)
 for item in list(original_body.childNodes):
  if item.nodeType==item.ELEMENT_NODE and item.namespaceURI==A and item.localName=='p':original_body.removeChild(item)
 for para in generated_body.childNodes:
  if para.nodeType==para.ELEMENT_NODE and para.namespaceURI==A and para.localName=='p':original_body.appendChild(original_doc.importNode(para,True))
 result[original_note]=original_doc.toxml(encoding='utf-8');changed.append(original_note)
 actual='\n'.join(''.join(t.firstChild.data if t.firstChild else '' for t in para.getElementsByTagNameNS(A,'t')) for para in body(parseString(result[original_note])).getElementsByTagNameNS(A,'p'))
 assert actual==record['text'],record['slide']
assert len(changed)==15
preserved=[n for n in source if n not in changed]
assert all(source[n]==result[n] for n in preserved)
out=DIR/'candidate.pptx'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for info in infos:z.writestr(info,result[info.filename])
tables=[]
for i,part in enumerate(source_order,1):
 if parseString(source[part]).getElementsByTagNameNS(A,'tbl'):tables.append(i)
report={'slides':len(source_order),'notes_rewritten':len(changed),'changed_parts':changed,'other_parts_byte_identical':True,'native_table_slides':tables,'source_sha256':hashlib.sha256((DIR/'source.pptx').read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256(out.read_bytes()).hexdigest()}
(DIR/'change-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
