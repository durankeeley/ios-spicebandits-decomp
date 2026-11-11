"""Shared helpers for analysing the Spice Bandits iOS Mach-O (armv7).

Setup:  python3 -m venv .venv && .venv/bin/pip install capstone lief
Facts established so far (see docs/BINARY_NOTES.md):
  * __text is ARM-mode (not Thumb); VFP/NEON present; SjLj exceptions.
  * Binary is stripped; imports come from lazy stubs in __symbol_stub4.
"""
import re, subprocess, struct, sys
import lief, capstone
from capstone import arm_const as A

class Binary:
    def __init__(self, path, arch="armv7"):
        fat = self.fat = lief.MachO.parse(path)  # keep alive: lief slices dangle otherwise
        self.path = path
        self.f = None
        for b in fat:                      # fat IPA (1.6.8) has armv6+armv7: we want armv7 (subtype 9)
            if int(b.header.cpu_subtype) == 9 or self.f is None:
                self.f = b
        f = self.f
        self.sec = {s.name + ("" if s.name != "__const" else f"@{s.virtual_address:x}"): s for s in f.sections}
        self.raw = open(path, "rb").read()
        self.fat_off = getattr(f, "fat_offset", 0)
        t = [s for s in f.sections if s.name == "__text"][0]
        self.text_va, self.text = t.virtual_address, self.section_bytes(t)
        self.text_end = self.text_va + len(self.text)
        self.md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
        self.md.detail = True
        self.md.skipdata = True  # literal pools/data in __text become ".byte" pseudo-insns
        self.stubs = self._stubs(path)

    def _stubs(self, path):
        """map stub VA -> imported symbol name (from the indirect symbol table)"""
        out = subprocess.run(["llvm-objdump", "--macho", "--arch=armv7", "--indirect-symbols", path],
                             capture_output=True, text=True).stdout
        m, cur = {}, False
        for line in out.splitlines():
            if line.startswith("Indirect symbols for"):
                cur = ("symbolstub" in line or "symbol_stub" in line)
                continue
            p = line.split()
            if cur and len(p) == 3 and p[0].startswith("0x"):
                m[int(p[0], 16)] = p[2].lstrip("_")
        return m

    def section_bytes(self, sec):
        o = self.fat_off + sec.offset
        return self.raw[o:o + sec.size]

    def in_text(self, va): return self.text_va <= va < self.text_end

    def cstring_at(self, va, maxlen=96):
        if not hasattr(self, "_cs"):
            cs = [x for x in self.f.sections if x.name == "__cstring"][0]
            self._cs = (cs.virtual_address, self.section_bytes(cs))
        base, d = self._cs
        if not (base <= va < base + len(d)): return None
        return d[va - base:].split(b"\0", 1)[0][:maxlen].decode("latin1")

    def disasm(self, start, end):
        data = self.text[start - self.text_va:end - self.text_va]
        return list(self.md.disasm(data, start))

    def insn_iter(self):
        return self.md.disasm(self.text, self.text_va)
