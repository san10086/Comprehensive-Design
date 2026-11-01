import fs from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
import {finalizePresentation} from 'file:///C:/Users/30522/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations/container_tools/artifact_tool_utils.mjs';
const ROOT='D:/develop/vscode/projects/ex04', DIR=ROOT+'/report/workflow/notes-revision';
const SKILL='C:/Users/30522/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations';
const check=JSON.parse(await fs.readFile(DIR+'/change-check.json','utf8'));
const p=await PresentationFile.importPptx(await FileBlob.load(DIR+'/candidate.pptx'));
await fs.mkdir(DIR+'/previews',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){
 const png=await p.export({slide:p.slides.items[i],format:'png',scale:1});
 await fs.writeFile(DIR+'/previews/slide-'+String(i+1).padStart(2,'0')+'.png',new Uint8Array(await png.arrayBuffer()));
}
const expected=JSON.parse(await fs.readFile(DIR+'/notes-after.json','utf8'));
for(const item of expected)if(p.slides.items[item.slide-1].speakerNotes.text!==item.text)throw new Error('Note reimport differs: '+item.slide);
const referencePath=ROOT+'/report/template.pptx';
const fontPolicy={basis:'reference',families:['思源黑体 1 Heavy','思源黑体-超粗体','思源黑体 2'],referencePath,referenceSha256:createHash('sha256').update(await fs.readFile(referencePath)).digest('hex')};
await fs.mkdir(DIR+'/output',{recursive:true});
const result=await finalizePresentation({
 workspaceDir:ROOT,candidatePath:DIR+'/candidate.pptx',finalPath:DIR+'/output/validated.pptx',
 pythonExecutable:'C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
 integrityValidatorPath:SKILL+'/container_tools/inspect_presentation_package_integrity.py',
 layoutValidatorPath:SKILL+'/container_tools/inspect_presentation_layout_geometry.py',
 layoutArgs:['--expected-slide-size-emu','18288000,10287000','--validate-bullet-geometry','--validate-heading-fit','--expected-slide-count',String(check.slides),...check.native_table_slides.flatMap(n=>['--require-native-table-slide',String(n)])],
 requiredNativeTableOwnerSlides:check.native_table_slides,fontPolicy,verifyArtifactToolImport:true,
 receiptPath:DIR+'/validation.json'
});
console.log('VALIDATED',result.finalSha256,'SLIDES',check.slides,'NOTES',expected.length);
