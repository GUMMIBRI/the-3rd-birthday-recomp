"""Static native game audit; streamed disassembly, never target execution."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("imports", type=Path)
    parser.add_argument("llvm", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    issues = []
    imports = json.loads(args.imports.read_text(encoding="utf-8-sig"))
    data = args.elf.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest.lower() != imports["SHA256"].lower():
        raise ValueError("Import audit belongs to another executable")
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 16)[0] != 3:
        raise ValueError("Expected native ELF64 application PIE")
    phoff, shoff = struct.unpack_from("<QQ", data, 32)
    phsize, phcount, shsize, shcount = struct.unpack_from("<HHHH", data, 54)
    for i in range(phcount):
        at = phoff + i * phsize
        kind = struct.unpack_from("<I", data, at)[0]
        if kind == 7 and any(struct.unpack_from("<QQ", data, at + 32)):
            issues.append("Native PT_TLS is unsupported")
    for i in range(shcount):
        at = shoff + i * shsize
        if struct.unpack_from("<Q", data, at + 8)[0] & 0x400:
            issues.append("Native SHF_TLS section is unsupported")
    def capture(tool, *flags):
        return subprocess.check_output([str(args.llvm / tool), *flags, str(args.elf)], text=True, errors="replace")
    symbols = capture("llvm-nm.exe")
    emutls = re.findall(r"^\S+\s+[tT]\s+__emutls_get_address$", symbols, re.M)
    if len(emutls) != 1:
        issues.append("Expected exactly one local emulated-TLS implementation")
    for symbol in re.findall(r"^\S+\s+[tT]\s+(\S+)$", symbols, re.M):
        if re.match(r"(__crt|payload_|kernel_)", symbol):
            issues.append("Payload definition: " + symbol)
    required = ["sr_register_all", "sr_native_game", "vk_icdGetInstanceProcAddr",
                "vk_common_GetPhysicalDeviceProperties2", "sr_h264_frame", "sr_sysv_swap"]
    definitions = {name: (int(address, 16), kind) for address, kind, name in
                   re.findall(r"^(\S+)\s+(\w)\s+(\S+)$", symbols, re.M)}
    for symbol in required:
        if symbol not in definitions or not definitions[symbol][0] or definitions[symbol][1].upper() in ("U", "W"):
            issues.append("Missing real definition: " + symbol)
    for item in imports["Imports"]:
        symbol, module = item["Symbol"], item["Module"]
        if item["Binding"] == "WEAK" or re.match(r"(__dl|__crt|payload_|kernel_)", symbol):
            issues.append("Unsupported import: " + symbol)
        if module in ("libkernel_sys.sprx", "libkernel_stub_weak.sprx"):
            issues.append("Unsupported system provider: " + module)
        expected = "libkernel.sprx" if re.match(r"pthread_|__error$|_exit$", symbol) else \
                   "libSceLibcInternal.sprx" if re.match(r"(_init_env|atexit|exit|malloc|calloc|realloc|free|fopen|fclose|fread|fwrite|fseek|ftell|setvbuf)$", symbol) else None
        if expected and module != expected:
            issues.append("Unexpected provider: " + symbol + " -> " + module)
    relocations = capture("llvm-readelf.exe", "-rW")
    for kind in set(re.findall(r"\bR_X86_64_\w+", relocations)):
        if kind not in {"R_X86_64_64", "R_X86_64_RELATIVE", "R_X86_64_GLOB_DAT", "R_X86_64_JUMP_SLOT"}:
            issues.append("Unsupported relocation: " + kind)
    process = subprocess.Popen([str(args.llvm / "llvm-objdump.exe"), "-d", "--no-show-raw-insn", str(args.elf)],
                               stdout=subprocess.PIPE, text=True, errors="replace")
    syscalls = []
    function = ""
    for line in process.stdout:
        match = re.match(r"^[0-9a-f]+ <(.+)>:", line)
        if match:
            function = match.group(1)
        match = re.match(r"^\s*([0-9a-f]+):\s+syscall\s*$", line)
        if match:
            syscalls.append({"function": function, "address": match.group(1)})
    if process.wait() != 0:
        raise ValueError("Target instruction decode failed")
    if syscalls:
        issues.append("Inline syscalls remain outside public import stubs")
    result = {"passed": not issues, "issues": issues, "sha256": digest, "imports": imports["ImportCount"],
              "inline_syscalls": syscalls, "emutls_implementations": len(emutls), "required_definitions": required,
              "console_executed": False, "limit": "Static source/link evidence; not playability or a console ABI test"}
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if issues:
        raise ValueError("; ".join(issues))
    print("Native game static audit passed; not a hardware/gameplay result.")


if __name__ == "__main__":
    main()
