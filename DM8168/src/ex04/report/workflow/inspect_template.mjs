import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
const root='D:/develop/vscode/projects/ex04/report/workflow/template';
const p=await PresentationFile.importPptx(await FileBlob.load('D:/develop/vscode/projects/ex04/report/template.pptx'));
const inspect=await p.inspect({kind:'slide,layout,textbox,shape',maxChars:60000});
await fs.writeFile(root+'/artifact-inspect.ndjson',inspect.ndjson);
await fs.mkdir(root+'/slides',{recursive:true});
console.log('Imported',p.slides.items.length,'slides');
for(let i=0;i<p.slides.items.length;i++){
  const slide=p.slides.items[i];
  const png=await p.export({slide,format:'png',scale:0.67});
  await fs.writeFile(root+'/slides/slide-'+String(i+1).padStart(2,'0')+'.png',new Uint8Array(await png.arrayBuffer()));
  console.log('rendered',i+1);
}
