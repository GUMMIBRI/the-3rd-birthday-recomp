// SPDX-License-Identifier: GPL-2.0-or-later
// Compare retained application bytes and memory extents; never executes a title.
const fs = require('fs');
function read(file) {
  const bytes = fs.readFileSync(file);
  const need = (at, size) => { if (!Number.isSafeInteger(at) || at < 0 || at + size > bytes.length) throw Error('ELF bounds'); };
  const u16 = at => { need(at, 2); return bytes.readUInt16LE(at); };
  const u32 = at => { need(at, 4); return bytes.readUInt32LE(at); };
  const u64 = at => { need(at, 8); const v = Number(bytes.readBigUInt64LE(at)); if (!Number.isSafeInteger(v)) throw Error('ELF integer range'); return v; };
  need(0, 64);
  if (u32(0) !== 0x464c457f || bytes[4] !== 2 || bytes[5] !== 1 || u16(18) !== 62) throw Error('Expected ELF64 x86-64');
  const phoff = u64(32), phsize = u16(54), shoff = u64(40), shsize = u16(58);
  if (phsize < 56 || (u16(60) && shsize < 64)) throw Error('ELF record size');
  const segments = Array.from({length: u16(56)}, (_, i) => {
    const p = phoff + i * phsize; need(p, phsize);
    const s = {type:u32(p), offset:u64(p+8), address:u64(p+16), filesz:u64(p+32), memsz:u64(p+40), align:u64(p+48)};
    if (s.type === 1) { need(s.offset, s.filesz); if (s.filesz > s.memsz || s.offset % 0x4000 !== s.address % 0x4000) throw Error('LOAD layout'); }
    if (s.type === 7 && (s.filesz || s.memsz)) throw Error('Meaningful TLS segment not audited');
    return s;
  });
  const sections = Array.from({length:u16(60)}, (_, i) => {
    const p = shoff + i * shsize; need(p, shsize);
    return {name:u32(p), type:u32(p+4), flags:u64(p+8), address:u64(p+16), offset:u64(p+24), size:u64(p+32)};
  });
  const names = sections[u16(62)];
  if (sections.length && !names) throw Error('Missing section name table');
  for (const s of sections) {
    const start = names.offset + s.name, end = bytes.indexOf(0, start);
    need(start, 1); if (end < start || end >= names.offset + names.size) throw Error('Invalid section name');
    s.name = bytes.toString('utf8', start, end);
    if (s.flags & 0x400) throw Error('SHF_TLS section not audited');
    if (s.type !== 8) need(s.offset, s.size);
  }
  return {bytes, segments, sections, type:u16(16), entry:u64(24)};
}
if (process.argv.length !== 4) throw Error('Usage: node verify_ps5_conversion.cjs <input.elf> <converted.elf>');
const source = read(process.argv[2]), target = read(process.argv[3]);
if (source.type !== 3 || target.type !== 0xfe10 || source.entry !== target.entry) throw Error('Application type/entry mismatch');
let count = 0;
for (const s of source.sections.filter(s => (s.flags & 2) && s.size)) {
  if (/^\.(dynstr|dynsym|dynamic|hash|gnu\.hash|rela\.)/.test(s.name)) continue;
  const p = target.segments.find(p => p.type === 1 && s.address >= p.address && s.address + s.size <= p.address + p.memsz);
  if (!p) throw Error('Retained section unmapped: ' + s.name);
  if (s.type === 8) {
    if (s.address < p.address + p.filesz) throw Error('BSS overlaps stored bytes');
  } else {
    if (s.address + s.size > p.address + p.filesz) throw Error('Retained bytes outside LOAD');
    const at = p.offset + s.address - p.address;
    if (!source.bytes.subarray(s.offset, s.offset+s.size).equals(target.bytes.subarray(at, at+s.size))) throw Error('Changed section: ' + s.name);
  }
  ++count;
}
console.log(JSON.stringify({retainedSections:count, entryPreserved:true, bssPreserved:true, meaningfulTLS:false, passed:true, consoleExecuted:false}));
