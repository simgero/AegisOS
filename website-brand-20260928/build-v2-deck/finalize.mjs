import fs from 'node:fs/promises';
import { FileBlob, PresentationFile } from '@oai/artifact-tool';
import { finalizePresentation } from '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations/container_tools/artifact_tool_utils.mjs';
const root='/Users/simeongerodetti/AegisOS/website-brand-20260928';
const build=root+'/build-v2-deck';
const skill='/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations';
const finalPath=root+'/output-v2/Aegis-OS-Praesentation.pptx';
const result=await finalizePresentation({
  workspaceDir:root,
  candidatePath:build+'/candidate.pptx',
  finalPath,
  explicitTotalSlideCount:8,
  requiredNativeTableOwnerSlides:[5],
  requiredNativeChartOwnerSlides:[],
  pythonExecutable:'/Users/simeongerodetti/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3',
  integrityValidatorPath:skill+'/container_tools/inspect_presentation_package_integrity.py',
  layoutValidatorPath:skill+'/container_tools/inspect_presentation_layout_geometry.py',
  layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit','--require-native-table-slide','5'],
  fontPolicy:{basis:'design',families:['Rubik']},
  verifyArtifactToolImport:true,
  receiptPath:build+'/validation-final.json'
});
console.log(JSON.stringify(result));
const p=await PresentationFile.importPptx(await FileBlob.load(finalPath));
await fs.mkdir(build+'/final-rendered',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){
  const b=await p.export({slide:p.slides.items[i],format:'png',scale:1});
  await fs.writeFile(build+'/final-rendered/slide-'+(i+1)+'.png',new Uint8Array(await b.arrayBuffer()));
}
console.log('Rendered all final slides.');
