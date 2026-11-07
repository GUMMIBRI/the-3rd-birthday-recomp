#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original verifier; format/NID/hash model derived from ps5-native-app-boilerplate
# sce_module_writer.cpp, Copyright (C) 2026 BlackBearReloaded, portions derived
# from SharpProspero, Copyright (C) 2026 SvenGDK, GPL-3.0-or-later.
"""Verify loader-visible SCE imports and relocations against the original PIE.

Supported model: PSXS5 revision 5c17ea31aec5791c12451e915386fa98d670adb4,
tooling/native/sce_module_writer.cpp SHA256
c919d29ef08dc4a93f57ce32d44b386c94f91d7d24533829a7f27295a0a75c8e.
Unknown layouts fail closed. This is not a hardware, firmware or gameplay test.
"""
import argparse
import base64
import hashlib
import json
import struct
import sys
from pathlib import Path

MODEL = "PSXS5-5c17ea31-native-sce-writer"
SOURCE_SHA256 = "c919d29ef08dc4a93f57ce32d44b386c94f91d7d24533829a7f27295a0a75c8e"
SUFFIX = bytes.fromhex("518d64a635ded8c1e6b039b1c3e55230")
ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"
NEEDED_MODULE, IMPORT_LIB, IMPORT_ATTR = 0x61000045, 0x61000049, 0x61000019
SYMTABSZ, HASHSZ = 0x6100003F, 0x6100003D


def require(condition, message):
    if not condition:
        raise ValueError(message)


def nid(name):
    digest = hashlib.sha1(name.encode("ascii") + SUFFIX).digest()
    return base64.b64encode(digest[:8][::-1]).decode("ascii").rstrip("=").replace("/", "-")


def encode_id(value):
    if value == 0:
        return "A"
    out = ""
    while value:
        out = ALPHABET[value % 64] + out
        value //= 64
    return out


def normalized_module(name):
    return name[:-5] + ".prx" if name.endswith(".sprx") else name


def string_at(table, offset):
    require(0 <= offset < len(table), "String offset outside loader table")
    end = table.find(b"\0", offset)
    require(end >= 0, "Unterminated loader string")
    return table[offset:end].decode("ascii")


class Elf:
    def __init__(self, path, expected_type):
        self.data = path.read_bytes()
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        require(len(self.data) >= 64 and self.data[:7] == b"\x7fELF\x02\x01\x01", "Expected ELF64 little-endian file")
        require(self.unpack("<HHI", 16) == (expected_type, 62, 1), "Unknown ELF type/machine/version")
        self.entry, phoff, self.shoff = self.unpack("<QQQ", 24)
        ehsize, phsize, phcount, self.shsize, self.shcount, self.shstrings = self.unpack("<HHHHHH", 52)
        require(ehsize == 64 and phsize == 56 and 0 < phcount < 4096, "Unknown ELF header table model")
        self.programs = []
        for i in range(phcount):
            ptype, flags, offset, address, physical, filesz, memsz, alignment = self.unpack("<IIQQQQQQ", phoff + i * phsize)
            self.slice(offset, filesz)
            require(address + memsz <= 1 << 64, "Program address span overflow")
            if ptype == 1:
                require(memsz >= filesz, "LOAD file span exceeds memory span")
                require(alignment == 0x4000 and offset % alignment == address % alignment, "Unknown/incongruent LOAD alignment")
            if ptype == 7:
                require(filesz == 0 and memsz == 0, "Native TLS is unsupported")
            self.programs.append(dict(kind=ptype, flags=flags, offset=offset, address=address,
                                      filesz=filesz, memsz=memsz))
        self.loads = [p for p in self.programs if p["kind"] == 1]
        dynamic = [p for p in self.programs if p["kind"] == 2]
        require(len(dynamic) == 1 and dynamic[0]["filesz"] % 16 == 0, "Unknown dynamic program model")
        self.tags = {}
        self.dynamic_entries = []
        terminated = False
        p = dynamic[0]
        for offset in range(p["offset"], p["offset"] + p["filesz"], 16):
            tag, value = self.unpack("<QQ", offset)
            if not tag:
                terminated = True
                break
            self.tags.setdefault(tag, []).append(value)
            self.dynamic_entries.append((tag, value))
        require(terminated, "Dynamic table has no terminator")

    def slice(self, offset, size):
        require(0 <= offset <= len(self.data) and 0 <= size <= len(self.data) - offset, "File table span outside ELF")
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.slice(offset, struct.calcsize(fmt)))

    def tag(self, tag, default=None):
        values = self.tags.get(tag, [])
        require(len(values) <= 1, f"Duplicate singleton dynamic tag {tag:#x}")
        require(values or default is not None, f"Missing dynamic tag {tag:#x}")
        return values[0] if values else default

    def va(self, address, size):
        candidates = [p for p in self.loads if p["address"] <= address and
                      address + size <= p["address"] + p["filesz"]]
        require(len(candidates) == 1, f"Ambiguous/unbacked loader span {address:#x}+{size:#x}")
        p = candidates[0]
        return self.slice(p["offset"] + address - p["address"], size)

    def mapped(self, address, size=1, writable=False, executable=False):
        return any(p["flags"] and (not writable or p["flags"] & 2) and
                   (not executable or p["flags"] & 1) and
                   p["address"] <= address and address + size <= p["address"] + p["memsz"]
                   for p in self.loads)

    def sections(self):
        require(self.shoff and self.shsize == 64 and 0 < self.shcount < 65535 and self.shstrings < self.shcount,
                "Original native ELF needs ordinary section headers")
        sections = [self.unpack("<IIQQQQIIQQ", self.shoff + i * self.shsize) for i in range(self.shcount)]
        names = self.slice(sections[self.shstrings][4], sections[self.shstrings][5])
        return {string_at(names, section[0]): section for section in sections}


def symbols(table):
    require(len(table) % 24 == 0, "Unsupported symbol entry width")
    return [struct.unpack_from("<IBBHQQ", table, i) for i in range(0, len(table), 24)]


def relocations(table):
    require(len(table) % 24 == 0, "Unsupported relocation entry width")
    return [struct.unpack_from("<QQq", table, i) for i in range(0, len(table), 24)]


def expected_hash(names):
    count = len(names)
    buckets, chains = [0] * count, [0] * count
    for i in range(count - 1, 0, -1):
        value = 0
        for char in names[i].encode("ascii"):
            value = ((value << 4) + char) & 0xffffffff
            carry = value & 0xf0000000
            if carry:
                value ^= carry >> 24
            value &= ~carry
        bucket = value % count
        chains[i], buckets[bucket] = buckets[bucket], i
    return struct.pack("<" + "I" * (2 + count * 2), count, count, *buckets, *chains)


def parameter_relocations(sce):
    headers = [p for p in sce.programs if p["kind"] == 0x61000001]
    require(len(headers) == 1 and headers[0]["filesz"] == 0x60, "Unknown process parameter model")
    process = headers[0]["address"]
    data = sce.va(process, 0x60)
    require(struct.unpack_from("<Q", data)[0] == 0x60 and data[8:12] == b"ORBI" and
            struct.unpack_from("<I", data, 12)[0] == 5, "Unknown process parameter header")
    blocks = process + 0x60
    sizes, counts = [0xa8, 0x38, 0x10, 0x78, 0xc0, 0x38], [0x10000000e, 0, 0, 2, 3, 1]
    offsets, at = [], 0
    for size, count in zip(sizes, counts):
        offsets.append(at)
        block = sce.va(blocks + at, size)
        require(struct.unpack_from("<QQ", block) == (size, count), "Unknown converter parameter block")
        at += size
    heap = sce.va(blocks + at, 16)
    require(struct.unpack_from("<QI", heap) == (0xffffffffffffffff, 1), "Unknown heap parameter block")
    pointers = [(process + 0x38, blocks + offsets[0]), (process + 0x40, blocks + offsets[1]),
                (process + 0x48, blocks + offsets[2]), (blocks + 0x30, blocks + offsets[3]),
                (blocks + 0x38, blocks + offsets[4]), (blocks + 0x60, blocks + offsets[5]),
                (blocks + 0x10, blocks + at), (blocks + 0x20, blocks + at + 8)]
    return [(offset, 8, addend) for offset, addend in pointers]


def verify(native_path, sce_path, audit_path):
    native, sce = Elf(native_path, 3), Elf(sce_path, 0xfe10)
    audit = json.loads(audit_path.read_text(encoding="utf-8-sig"))
    require(native.sha256 == audit["SHA256"].lower(), "Import audit belongs to another native ELF")
    require(sce.entry == native.entry and sce.mapped(sce.entry, executable=True), "Entry point changed/not executable")
    known = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 21, 23, 25, 26, 27, 28, 32, 33,
             0x6ffffff9, 0x61000011, IMPORT_ATTR, HASHSZ, SYMTABSZ, 0x61000041, 0x61000043,
             NEEDED_MODULE, IMPORT_LIB}
    require(not set(sce.tags) - known, f"Unknown converted dynamic tags: {set(sce.tags) - known}")
    repeated = {1, NEEDED_MODULE, IMPORT_LIB, IMPORT_ATTR}
    for tag, values in sce.tags.items():
        require(tag in repeated or len(values) == 1, f"Repeated converted tag {tag:#x}")
    require(sce.tag(11) == 24 and sce.tag(9) == 24 and sce.tag(20) == 7, "Unsupported symbols/RELA model")
    strings = sce.va(sce.tag(5), sce.tag(10))
    require(strings and strings[0] == 0, "Missing null string-table entry")
    sym = symbols(sce.va(sce.tag(6), sce.tag(SYMTABSZ)))
    require(sym and sym[0] == (0, 0, 0, 0, 0, 0), "Null symbol corrupted")
    sections = native.sections()
    source_section = sections[".dynsym"]
    require(source_section[9] == 24, "Unsupported original dynsym entry width")
    original = symbols(native.slice(source_section[4], source_section[5]))
    source_strings = native.va(native.tag(5), native.tag(10))
    needed = [normalized_module(string_at(source_strings, value)) for value in native.tags.get(1, [])]
    converted_needed = [string_at(strings, value) for value in sce.tags.get(1, [])]
    require(needed == converted_needed and len(set(needed)) == len(needed), "DT_NEEDED module order changed/duplicated")
    audit_modules = [normalized_module(item["Module"]) for item in audit["Modules"]]
    require(needed == audit_modules, "Audit module order differs from native ELF")
    module_records, library_records = sce.tags.get(NEEDED_MODULE, []), sce.tags.get(IMPORT_LIB, [])
    require(len(module_records) == len(needed) == len(library_records), "Module/library record count mismatch")
    expected_attributes = []
    for index, (module, mrecord, lrecord) in enumerate(zip(needed, module_records, library_records)):
        basename = module.replace("\\", "/").rsplit("/", 1)[-1].split(".", 1)[0]
        require(mrecord >> 48 == index + 1 and (mrecord >> 32) & 0xffff == 1 and
                string_at(strings, mrecord & 0xffffffff) == basename, "Needed-module ID/name/version mismatch")
        require(lrecord >> 48 == index and (lrecord >> 32) & 0xffff == 1 and
                string_at(strings, lrecord & 0xffffffff) == basename, "Import-library ID/name/version mismatch")
        expected_attributes.append(index << 48 | 9)
    require(sce.tags.get(IMPORT_ATTR, []) == expected_attributes, "Import-library attributes mismatch")
    imports = audit["Imports"]
    require(len(imports) == audit["ImportCount"] == len(original) - 1 == len(sym) - 1, "Import/symbol count changed")
    by_name = {item["Symbol"]: item for item in imports}
    require(len(by_name) == len(imports), "Duplicated audit symbols")
    hash_names, mapped = [""], []
    for index in range(1, len(original)):
        source, converted = original[index], sym[index]
        plain = string_at(source_strings, source[0])
        require(plain in by_name and source[3] == 0 and source[1] >> 4 == 1, "Unknown/non-global original import")
        item = by_name[plain]
        require(item["Binding"] == "GLOBAL", "Weak imports are unsupported")
        module = normalized_module(item["Module"])
        require(module in needed, "Audit import provider absent from DT_NEEDED")
        provider = needed.index(module)
        expected = nid(plain) + "#" + encode_id(provider) + "#" + encode_id(provider + 1)
        require(string_at(strings, converted[0]) == expected, f"Converted NID/provider mismatch at symbol {index}: {plain}")
        require(source[1:] == converted[1:], f"Converted import attributes changed: {plain}")
        basename = module.rsplit("/", 1)[-1].split(".", 1)[0]
        hash_names.append(nid(plain) + "#" + basename + "#" + basename)
        mapped.append(dict(index=index, symbol=plain, module=item["Module"], nid=nid(plain)))
    hash_bytes = expected_hash(hash_names)
    require(sce.tag(HASHSZ) == len(hash_bytes) and sce.va(sce.tag(4), len(hash_bytes)) == hash_bytes,
            "Loader symbol hash buckets/chains differ from canonical NID/library/module model")
    source_rela = {}
    for name in (".rela.dyn", ".rela.plt"):
        section = sections.get(name)
        if section:
            require(section[9] == 24, "Unsupported original relocation model")
            source_rela[name] = relocations(native.slice(section[4], section[5]))
        else:
            source_rela[name] = []
    dynamic = relocations(sce.va(sce.tag(7), sce.tag(8)))
    plt = relocations(sce.va(sce.tag(23), sce.tag(2)))
    extra = parameter_relocations(sce)
    relatives = [r for r in source_rela[".rela.dyn"] if r[1] & 0xffffffff == 8]
    symbolic = [r for r in source_rela[".rela.dyn"] if r[1] & 0xffffffff != 8]
    require(dynamic == extra + relatives + symbolic, "Converted dynamic relocations changed beyond known process parameters")
    require(plt == source_rela[".rela.plt"], "Converted PLT relocations changed")
    require(sce.tag(0x6ffffff9) == len(extra) + len(relatives), "DT_RELACOUNT mismatch")
    destinations = set()
    for offset, info, addend in dynamic + plt:
        kind, symbol = info & 0xffffffff, info >> 32
        require(kind in (1, 6, 7, 8), f"Unsupported relocation type {kind}")
        require(sce.mapped(offset, 8, writable=True), f"Relocation destination outside writable mapped memory: {offset:#x}")
        require(offset not in destinations, "Duplicate relocation destination")
        destinations.add(offset)
        if kind == 8:
            require(symbol == 0 and addend >= 0 and sce.mapped(addend), f"Relative relocation target outside mapped memory: {addend:#x}")
        else:
            require(0 < symbol < len(sym), "Symbolic relocation references unknown import")
    for pointer_tag, size_tag in ((32, 33), (25, 27), (26, 28)):
        require(sce.tag(pointer_tag) == native.tag(pointer_tag, 0) and
                sce.tag(size_tag) == native.tag(size_tag, 0), "Initializer/finalizer array changed")
        if sce.tag(size_tag):
            require(sce.tag(size_tag) % 8 == 0 and sce.mapped(sce.tag(pointer_tag), sce.tag(size_tag)), "Initializer array outside mapping")
    for tag in (12, 13):
        require(sce.tag(tag) == native.tag(tag, 0), "Init/fini function changed")
        if sce.tag(tag):
            require(sce.mapped(sce.tag(tag), executable=True), "Init/fini function outside executable mapping")
    require(sce.mapped(sce.tag(3), 8, writable=True), "PLTGOT outside writable mapping")
    return dict(passed=True, model=MODEL, format_source_sha256=SOURCE_SHA256,
                native_sha256=native.sha256, sce_sha256=sce.sha256,
                imports=len(mapped), modules=needed, mapped_imports=mapped,
                dynamic_relocations=len(dynamic), plt_relocations=len(plt),
                added_parameter_relocations=len(extra), console_executed=False,
                limit="Static loader-table/provider/relocation verification; no firmware or gameplay qualification")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("sce", type=Path)
    parser.add_argument("imports", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.native, args.sce, args.imports)
    except (ValueError, OSError, KeyError, UnicodeError, struct.error) as error:
        result = dict(passed=False, model=MODEL, issues=[str(error)], console_executed=False)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if not result["passed"]:
        print("SCE verification failed: " + "; ".join(result["issues"]), file=sys.stderr)
        return 1
    print(f"SCE loader tables verified: {result['imports']} imports, {result['dynamic_relocations']} dynamic relocations; no runtime claim.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
