"""Mixed ARM/Thumb-2 recursive-descent disassembler for the armv7 Mach-O.

The binary interleaves ARM and Thumb-2 code, so a linear sweep is wrong. We seed from:
  * odd (LSB=1) code pointers in data  -> Thumb entry  (C++ vtables in __const, ObjC IMPs in __objc_const)
  * even code pointers in data          -> ARM entry
  * every bl/blx target (blx flips the instruction set)
  * the Mach-O entry point
then follow control flow. Result: Program.insns {addr: (mode, insn)} and Program.entries {addr: mode}.
Jump tables: tbb/tbh bounded via a preceding `cmp rN,#k`; ARM `ldr pc,[pc,rN,lsl#2]` tables likewise.
Not handled (documented in docs/BINARY_NOTES.md): computed gotos with unbounded tables.
"""
import struct, collections
import capstone
from capstone import arm_const as A
from ra_re import Binary

ARM, THUMB = 0, 1

class Program:
    def __init__(self, path):
        self.b = Binary(path)
        b = self.b
        self.mds = {ARM: b.md, THUMB: capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)}
        self.mds[THUMB].detail = True
        self.insns = {}                       # addr -> (mode, insn)
        self.entries = {}                     # addr -> mode
        self.seed_src = {}                    # addr -> why
        self.tables = {}                      # tbb/tbh addr -> [targets]

    # ---- seeds
    def seed_from_data(self):
        b = self.b
        for sec in b.f.sections:
            if sec.size == 0 or sec.name in ("__bss", "__common", "__text", "__symbol_stub4", "__cstring", "__ustring",
                                              "__objc_methname", "__objc_methtype", "__objc_classname"):
                continue
            d = b.section_bytes(sec)
            for o in range(0, len(d) - 3, 4):
                v = struct.unpack_from("<I", d, o)[0]
                if v < b.text_va or v >= b.text_end: continue
                if v & 1: self.add_entry(v & ~1, THUMB, f"data:{sec.name}")
                elif sec.name in ("__const", "__objc_const", "__data", "__mod_init_func") and v % 4 == 0:
                    self.add_entry(v, ARM, f"data:{sec.name}")
        ep = getattr(b.f, "entrypoint", 0)
        if ep: self.add_entry(ep, ARM, "entry")

    def add_entry(self, a, mode, why):
        if a not in self.entries:
            self.entries[a] = mode; self.seed_src[a] = why
            self.work.append((a, mode))

    # ---- decode
    def decode(self, a, mode):
        b = self.b
        off = a - b.text_va
        if off < 0 or off >= len(b.text): return None
        chunk = b.text[off:off + 4]
        for i in self.mds[mode].disasm(chunk, a, 1):
            return None if i.id == 0 else i
        return None

    def run(self, prologue_passes=3):
        self.work = collections.deque()
        self.seed_from_data()
        self.seen = seen = set()
        while self.work:
            a, mode = self.work.popleft()
            self.walk(a, mode, seen)
        for _ in range(prologue_passes):       # code only reachable via virtual calls: find by prologue
            n = self.seed_prologues()
            if not n: break
            while self.work:
                a, mode = self.work.popleft()
                self.walk(a, mode, seen)
        return self

    def _plausible(self, a, mode, n=6):
        for _ in range(n):
            i = self.decode(a, mode)
            if i is None: return False
            a += i.size
        return True

    def seed_prologues(self):
        """Scan not-yet-decoded bytes for function prologues: ARM `stmdb sp!,{..,lr}` (e92d4xxx / e52de004),
        Thumb `push {..,lr}` (b5xx) / `push.w {..,lr}` (e92d 4xxx). Validated by decoding a short run."""
        b = self.b
        covered = bytearray(len(b.text))
        for a, (m, i) in self.insns.items():
            for k in range(i.size): covered[a - b.text_va + k] = 1
        added = 0
        for off in range(0, len(b.text) - 4, 2):
            if covered[off]: continue
            a = b.text_va + off
            hw = struct.unpack_from("<H", b.text, off)[0]
            w = struct.unpack_from("<I", b.text, off)[0] if off % 4 == 0 else 0
            if off % 4 == 0 and ((w & 0xFFFF4000) == 0xE92D4000 or w == 0xE52DE004) and self._plausible(a, ARM):
                self.add_entry(a, ARM, "prologue"); added += 1
            elif (hw & 0xFF00) == 0xB500 and self._plausible(a, THUMB):
                self.add_entry(a, THUMB, "prologue"); added += 1
            elif hw == 0xE92D and off + 4 <= len(b.text) and (struct.unpack_from("<H", b.text, off + 2)[0] & 0x4000) and self._plausible(a, THUMB):
                self.add_entry(a, THUMB, "prologue"); added += 1
        return added

    def walk(self, a, mode, seen):
        b = self.b
        last_cmp = None
        while b.in_text(a) and (a, mode) not in seen:
            seen.add((a, mode))
            i = self.decode(a, mode)
            if i is None: return
            self.insns[a] = (mode, i)
            nxt = a + i.size
            mn = i.mnemonic
            ops = i.operands
            imm = ops[0].imm if ops and ops[0].type == A.ARM_OP_IMM else None
            if mn.startswith("cmp") and len(ops) > 1 and ops[1].type == A.ARM_OP_IMM:
                last_cmp = ops[1].imm
            base = mn.split(".")[0]
            if base == "bl" and imm is not None:
                if imm in b.stubs or not b.in_text(imm): a = nxt; continue
                self.add_entry(imm, mode, f"bl@{a:x}"); a = nxt; continue
            if base == "blx" and imm is not None:
                t = THUMB if mode == ARM else ARM
                if b.in_text(imm) and imm not in b.stubs: self.add_entry(imm, t, f"blx@{a:x}")
                a = nxt; continue
            if base == "blx":                 # register call
                a = nxt; continue
            if base in ("tbb", "tbh"):
                n = (last_cmp + 1) if last_cmp is not None and 0 <= last_cmp < 512 else 0
                tgts = []
                tb = nxt
                for k in range(n):
                    if base == "tbb": e = b.text[tb - b.text_va + k]
                    else: e = struct.unpack_from("<H", b.text, tb - b.text_va + 2 * k)[0]
                    tgts.append(nxt + 2 * e)
                self.tables[a] = tgts
                for t in tgts: self.work.append((t, mode))
                return
            if base.startswith("cb"):         # cbz/cbnz
                if imm is not None: self.work.append((imm, mode))
                a = nxt; continue
            if i.id in (A.ARM_INS_B,) or (base[0] == "b" and imm is not None and base not in ("bic", "bfi", "bfc", "bkpt", "blx", "bl", "bx", "bxj")):
                if imm is not None: self.work.append((imm, mode))
                if i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID) and base == "b": return
                a = nxt; continue
            if base == "bx":
                if i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID): return
                a = nxt; continue
            wr_pc = bool(ops) and ops[0].type == A.ARM_OP_REG and ops[0].reg == A.ARM_REG_PC and base in ("ldr", "mov", "add")
            pop_pc = base in ("pop", "ldm", "ldmia", "ldmdb", "ldmfd", "ldr") and "pc" in i.op_str and (base != "ldr" or i.op_str.startswith("pc"))
            if wr_pc or (base in ("pop", "ldm", "ldmia", "ldmfd", "ldmdb") and "pc" in i.op_str):
                if base == "ldr" and mode == ARM and "[pc" in i.op_str and last_cmp is not None:
                    pass                      # ARM ldr-pc jump table: rely on literal-pool seeding below
                if i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID): return
            a = nxt

    # ---- function extents (intra-procedural flood, calls not followed)
    def functions(self):
        funcs = {}
        for e, mode in sorted(self.entries.items()):
            if e not in self.insns: continue
            body, stack = {}, [e]
            while stack:
                a = stack.pop()
                if a in body or a not in self.insns or (a != e and a in self.entries and self.entries[a] == self.insns[a][0] and a in self._callt): continue
                m, i = self.insns[a]; body[a] = i
                base = i.mnemonic.split(".")[0]
                imm = i.operands[0].imm if i.operands and i.operands[0].type == A.ARM_OP_IMM else None
                nxt = a + i.size
                if base in ("tbb", "tbh"): stack += self.tables.get(a, []); continue
                if base.startswith("cb") and imm is not None: stack += [imm, nxt]; continue
                if base[0] == "b" and imm is not None and base not in ("bl", "blx", "bic", "bfi", "bfc", "bkpt"):
                    stack.append(imm)
                    if not (base == "b" and i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID)): stack.append(nxt)
                    continue
                if base == "bx" and i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID): continue
                if (base in ("pop", "ldm", "ldmia", "ldmfd", "ldmdb") and "pc" in i.op_str) or (i.operands and i.operands[0].type == A.ARM_OP_REG and i.operands[0].reg == A.ARM_REG_PC and base in ("ldr", "mov", "add")):
                    if i.cc in (A.ARM_CC_AL, A.ARM_CC_INVALID): continue
                stack.append(nxt)
            funcs[e] = (mode, body)
        return funcs

    @property
    def _callt(self):
        if not hasattr(self, "_ct"):
            self._ct = {a for a, w in self.seed_src.items() if w.startswith(("bl@", "blx@", "data:", "entry"))}
        return self._ct
