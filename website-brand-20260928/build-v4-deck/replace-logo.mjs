import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import JSZip from 'jszip';
import { FileBlob, PresentationFile } from '@oai/artifact-tool';
import { finalizePresentation } from '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations/container_tools/artifact_tool_utils.mjs';

const root = '/Users/simeongerodetti/AegisOS/website-brand-20260928';
const build = root + '/build-v4-deck';
const sourcePath = root + '/output-v3/Aegis-OS-Praesentation.pptx';
const finalPath = root + '/output-v4/Aegis-OS-Praesentation.pptx';
const skill = '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations';
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const sourceBytes = await fs.readFile(sourcePath);
const originalLogo = await fs.readFile(root + '/output-v3/brand-assets/aegis-shield.png');
const replacement = await fs.readFile(root + '/output-v4/aegis-core-triangle.png');
const source = await JSZip.loadAsync(sourceBytes);
const mediaName = 'ppt/media/image2.png';
if (sha(await source.file(mediaName).async('nodebuffer')) !== sha(originalLogo)) {
  throw new Error('Logo identity does not match the approved source logo.');
}
const sourceEntries = Object.keys(source.files);
const preservedEntries = new Map(await Promise.all(sourceEntries.filter(n => !source.files[n].dir).map(async n => [n, sha(await source.file(n).async('nodebuffer'))])));
const originalEntry = source.file(mediaName);
source.file(mediaName, replacement, { date: originalEntry.date, binary: true, createFolders: false });
const candidatePath = build + '/candidate.pptx';
await fs.writeFile(candidatePath, await source.generateAsync({type: 'nodebuffer', compression: 'DEFLATE'}));

const result = await finalizePresentation({
  workspaceDir: root,
  candidatePath,
  finalPath,
  explicitTotalSlideCount: 8,
  requiredNativeTableOwnerSlides: [5],
  requiredNativeChartOwnerSlides: [],
  pythonExecutable: '/Users/simeongerodetti/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3',
  integrityValidatorPath: skill + '/container_tools/inspect_presentation_package_integrity.py',
  layoutValidatorPath: skill + '/container_tools/inspect_presentation_layout_geometry.py',
  layoutArgs: ['--expected-slide-size-emu', '12192000,6858000', '--validate-bullet-geometry', '--validate-heading-fit', '--require-native-table-slide', '5'],
  fontPolicy: {basis: 'reference', families: ['Rubik'], referencePath: sourcePath, referenceSha256: sha(sourceBytes)},
  verifyArtifactToolImport: true,
  receiptPath: build + '/validation-final-exact.json',
});

const final = await JSZip.loadAsync(await fs.readFile(finalPath));
if (Object.keys(final.files).sort().join('\n') !== sourceEntries.sort().join('\n')) throw new Error('Package entry inventory changed.');
const changedEntries = [];
for (const [name, hash] of preservedEntries) {
  if (sha(await final.file(name).async('nodebuffer')) !== hash) changedEntries.push(name);
}
if (changedEntries.length !== 1 || changedEntries[0] !== mediaName) throw new Error('Unexpected package changes: ' + changedEntries.join(', '));
if (sha(await final.file(mediaName).async('nodebuffer')) !== sha(replacement)) throw new Error('Logo bytes do not match.');
await fs.writeFile(build + '/preservation.json', JSON.stringify({sourcePath, finalPath, changedEntries, replacementSha256: sha(replacement), preservedEntryCount: preservedEntries.size - 1, sourceSha256: sha(sourceBytes), finalSha256: sha(await fs.readFile(finalPath))}, null, 2));
console.log(JSON.stringify({finalPath, changedEntries, preservedEntryCount: preservedEntries.size - 1, validation: result.receiptPath}));

const presentation = await PresentationFile.importPptx(await FileBlob.load(finalPath));
await fs.mkdir(build + '/final-rendered', {recursive:true});
for (let i = 0; i < 1; i++) {
  const preview = await presentation.export({slide: presentation.slides.items[i], format: 'png', scale: 1});
  await fs.writeFile(build + '/final-rendered/slide-' + (i+1) + '.png', new Uint8Array(await preview.arrayBuffer()));
}
console.log('Rendered changed title slide.');
