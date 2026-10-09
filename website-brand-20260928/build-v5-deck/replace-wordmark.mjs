import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import JSZip from 'jszip';
import { FileBlob, PresentationFile } from '@oai/artifact-tool';
import { finalizePresentation } from '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations/container_tools/artifact_tool_utils.mjs';

const root = '/Users/simeongerodetti/AegisOS/website-brand-20260928';
const build = root + '/build-v5-deck';
const sourcePath = root + '/output-v4/Aegis-OS-Praesentation.pptx';
const finalPath = root + '/output-v5/Aegis-OS-Praesentation.pptx';
const skill = '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations';
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const sourceBytes = await fs.readFile(sourcePath);
const source = await JSZip.loadAsync(sourceBytes);
const entries = Object.keys(source.files).sort();
const originals = new Map(await Promise.all(entries.filter(n => !source.files[n].dir).map(async n => [n, await source.file(n).async('nodebuffer')])));
const changed = [];
for (let i = 1; i <= 8; i++) {
  const name = `ppt/slides/slide${i}.xml`;
  const before = originals.get(name).toString('utf8');
  const needle = '<a:t>AEGIS OS</a:t>';
  if (before.split(needle).length !== 2) throw new Error(`Expected exactly one wordmark: ${name}`);
  source.file(name, before.replace(needle, '<a:t>ægis.os</a:t>'), {date: source.file(name).date, createFolders: false});
  changed.push(name);
}
const candidatePath = build + '/candidate.pptx';
await fs.writeFile(candidatePath, await source.generateAsync({type: 'nodebuffer', compression: 'DEFLATE'}));
const result = await finalizePresentation({
  workspaceDir: root, candidatePath, finalPath,
  explicitTotalSlideCount: 8,
  requiredNativeTableOwnerSlides: [5],
  requiredNativeChartOwnerSlides: [],
  pythonExecutable: '/Users/simeongerodetti/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3',
  integrityValidatorPath: skill + '/container_tools/inspect_presentation_package_integrity.py',
  layoutValidatorPath: skill + '/container_tools/inspect_presentation_layout_geometry.py',
  layoutArgs: ['--expected-slide-size-emu', '12192000,6858000', '--validate-bullet-geometry', '--validate-heading-fit', '--require-native-table-slide', '5'],
  fontPolicy: {basis: 'reference', families: ['Rubik'], referencePath: sourcePath, referenceSha256: sha(sourceBytes)},
  verifyArtifactToolImport: true,
  receiptPath: build + '/validation-final.json',
});
const finalBytes = await fs.readFile(finalPath);
const final = await JSZip.loadAsync(finalBytes);
if (Object.keys(final.files).sort().join('\n') !== entries.join('\n')) throw new Error('Package inventory changed.');
let preserved = 0;
for (const [name, bytes] of originals) {
  const output = await final.file(name).async('nodebuffer');
  if (changed.includes(name)) {
    const expected = bytes.toString('utf8').replace('<a:t>AEGIS OS</a:t>', '<a:t>ægis.os</a:t>');
    if (output.toString('utf8') !== expected) throw new Error(`Unexpected change in ${name}`);
  } else {
    if (!bytes.equals(output)) throw new Error(`Unexpected package change: ${name}`);
    preserved++;
  }
}
await fs.writeFile(build + '/preservation.json', JSON.stringify({sourcePath, finalPath, sourceSha256: sha(sourceBytes), finalSha256: sha(finalBytes), exactReplacementCount: 8, changedEntries: changed, byteIdenticalOtherEntryCount: preserved}, null, 2));
console.log(JSON.stringify({finalPath, preservationPassed: true, validation: result.receiptPath}));
const presentation = await PresentationFile.importPptx(await FileBlob.load(finalPath));
await fs.mkdir(build + '/final-rendered', {recursive: true});
for (let i = 0; i < presentation.slides.items.length; i++) {
  const preview = await presentation.export({slide: presentation.slides.items[i], format: 'png', scale: 1});
  await fs.writeFile(`${build}/final-rendered/slide-${i+1}.png`, new Uint8Array(await preview.arrayBuffer()));
  console.log(`Rendered slide ${i+1}`);
}
