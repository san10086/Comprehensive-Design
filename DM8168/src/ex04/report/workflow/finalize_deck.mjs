import fs from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {finalizePresentation} from 'file:///C:/Users/30522/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations/container_tools/artifact_tool_utils.mjs';
const ROOT='D:/develop/vscode/projects/ex04';
const SKILL='C:/Users/30522/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations';
const referencePath=ROOT+'/report/template.pptx';
const fontPolicy={basis:'reference',families:['思源黑体 1 Heavy','思源黑体-超粗体','思源黑体 2'],referencePath,referenceSha256:createHash('sha256').update(await fs.readFile(referencePath)).digest('hex')};
try{
  const result=await finalizePresentation({
    workspaceDir:ROOT,candidatePath:ROOT+'/report/workflow/candidate.pptx',
    finalPath:ROOT+'/report/output/中期汇报_第一章_空间音效处理算法研究与实现_终稿.pptx',
    pythonExecutable:'C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
    integrityValidatorPath:SKILL+'/container_tools/inspect_presentation_package_integrity.py',
    layoutValidatorPath:SKILL+'/container_tools/inspect_presentation_layout_geometry.py',
    layoutArgs:['--expected-slide-size-emu','18288000,10287000','--validate-bullet-geometry','--validate-heading-fit'],
    requiredNativeTableOwnerSlides:[],fontPolicy,verifyArtifactToolImport:process.env.PPT_SKIP_REDUNDANT_IMPORT!=='1',
    receiptPath:ROOT+'/report/workflow/qa/final-v2.validation.json'
  });
  console.log('FINALIZED',JSON.stringify(result));
}catch(e){console.error('FINALIZE_ERROR',e.message);process.exitCode=1;}
