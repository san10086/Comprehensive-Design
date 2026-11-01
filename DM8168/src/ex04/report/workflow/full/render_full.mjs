import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
const ROOT='D:/develop/vscode/projects/ex04/report/workflow/full';
const p=await PresentationFile.importPptx(await FileBlob.load(ROOT+'/candidate.pptx'));
await fs.mkdir(ROOT+'/final-previews',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){
 const slide=p.slides.items[i], png=await p.export({slide,format:'png',scale:1});
 await fs.writeFile(ROOT+'/final-previews/slide-'+String(i+1).padStart(2,'0')+'.png',new Uint8Array(await png.arrayBuffer()));
 const layout=await slide.export({format:'layout'});
 await fs.writeFile(ROOT+'/final-previews/slide-'+String(i+1).padStart(2,'0')+'.json',new Uint8Array(await layout.arrayBuffer()));
}
await fs.writeFile(ROOT+'/reimport.ndjson',(await p.inspect({kind:'slide,textbox,notes,table',maxChars:500000})).ndjson);
console.log('REIMPORT_RENDER_OK',p.slides.items.length);
