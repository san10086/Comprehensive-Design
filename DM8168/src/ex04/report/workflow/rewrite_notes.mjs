import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
const ROOT='D:/develop/vscode/projects/ex04', DIR=ROOT+'/report/workflow/notes-revision';
await fs.mkdir(DIR,{recursive:true});
const source=ROOT+'/report/output/中期汇报_三章完整稿.pptx';
const data=await fs.readFile(source);
await fs.writeFile(DIR+'/source.pptx',data);
const current=JSON.parse(await fs.readFile(ROOT+'/report/workflow/notes-before.json','utf8'));
const revisions=JSON.parse(await fs.readFile(ROOT+'/report/workflow/notes_spoken.json','utf8'));
const p=await PresentationFile.importPptx(await FileBlob.load(source));
if(p.slides.items.length!==current.length)throw new Error('Slide count changed during note preparation');
const rewritten=[];
for(const r of revisions){
 const prior=current.find(x=>x.slide===r.slide);
 const references=prior.text.split(/\n(?:资料依据：|稿件与证据索引：)\n/)[1];
 const text=[r.spoken,r.support,references?'资料依据：\n'+references.trim():null].filter(Boolean).join('\n\n');
 p.slides.items[r.slide-1].speakerNotes.text=text;
 rewritten.push({...r,text,note_part:prior.note_part});
}
await fs.writeFile(DIR+'/notes-after.json',JSON.stringify(rewritten,null,2));
await(await PresentationFile.exportPptx(p)).save(DIR+'/artifact-notes.pptx');
console.log('NOTES_REWRITTEN',rewritten.length,'OF',p.slides.items.length);
