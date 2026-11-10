#!/usr/bin/env python3
"""Static-analysis tests for the Spice Bandits (Tower) toolkit output. Run tools/run_all.sh first."""
import json, os, struct, subprocess, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W = f"{ROOT}/work/std"; APP = f"{W}/Payload/Tower.app"; BIN = f"{APP}/Tower"
sys.path.insert(0, f"{ROOT}/tools")

def need(p):
    if not os.path.exists(p): raise unittest.SkipTest(f"{p} missing (run tools/run_all.sh)")
def load(p):
    need(p)
    with open(p) as f: return json.load(f)

def cryptid(path):
    d = open(path, "rb").read(); base = 0
    if struct.unpack(">I", d[:4])[0] == 0xCAFEBABE: base = struct.unpack(">I", d[16:20])[0]
    ncmds = struct.unpack_from("<I", d, base + 16)[0]; o = base + 28
    for _ in range(ncmds):
        cmd, sz = struct.unpack_from("<II", d, o)
        if cmd == 0x21: return struct.unpack_from("<I", d, o + 16)[0]     # LC_ENCRYPTION_INFO
        o += sz
    return None

class Ipa(unittest.TestCase):
    def test_std_is_decrypted(self): need(BIN); self.assertEqual(cryptid(BIN), 0)
    def test_hd_is_encrypted_and_unusable(self):
        p = f"{ROOT}/work/hd/Payload/Tower.app/Tower"; need(p); self.assertEqual(cryptid(p), 1)

class Functions(unittest.TestCase):
    def setUp(self): self.f = load(f"{W}/functions.json"); self.st = load(f"{W}/stats.json")
    def test_count(self): self.assertGreater(len(self.f), 15000)
    def test_thumb_dominant(self): self.assertGreater(sum(1 for v in self.f.values() if v["mode"] == "thumb"), 10000)
    def test_coverage(self): self.assertGreater(self.st["covered"] / self.st["text"], 0.80)
    def test_gl_callers(self): self.assertGreater(sum(1 for v in self.f.values() if any(k.startswith("gl") for k in v["imports"])), 100)
    def test_pic_stubs_resolved(self):
        imp = {k for v in self.f.values() for k in v["imports"]}; self.assertGreater(len(imp), 400); self.assertIn("objc_msgSend", imp)

class ObjC(unittest.TestCase):
    def setUp(self): self.m = load(f"{W}/objc_methods.json")
    def test_count(self): self.assertGreater(len(self.m), 11000)
    def test_game_classes(self):
        names = set(self.m.values())
        for n in ("-[TowerView initWithCoder:]",): self.assertIn(n, names)
        self.assertTrue(any(v.startswith("-[TowerAppDelegate ") for v in names)); self.assertTrue(any(v.startswith("-[EAGLView ") for v in names))
    def test_has_class_and_instance_methods(self): self.assertTrue(any(v.startswith("+[") for v in self.m.values())); self.assertTrue(any(v.startswith("-[") for v in self.m.values()))
    def test_categories(self): self.assertTrue(any("(" in v.split(" ")[0] for v in self.m.values()))
    def test_imps_even(self): self.assertTrue(all(int(k) % 2 == 0 for k in self.m))

class Exports(unittest.TestCase):
    def test_count(self): self.assertGreater(len(load(f"{W}/exports.json")), 1500)

class Nib(unittest.TestCase):
    """NIBArchive header decode used by the planned nib loader."""
    def test_mainwindow_header(self):
        p = f"{APP}/MainWindow.nib"; need(p); d = open(p, "rb").read()
        self.assertEqual(d[:10], b"NIBArchive"); v = struct.unpack_from("<10I", d, 10)
        self.assertEqual(v[0], 1); self.assertEqual(v[2], 23); self.assertEqual(v[4], 30); self.assertEqual(v[6], 62); self.assertEqual(v[8], 8)
    def test_all_nibs_are_nibarchives(self):
        need(APP); nibs = [f for f in os.listdir(APP) if f.endswith(".nib")]; self.assertGreater(len(nibs), 30)
        for n in nibs: self.assertEqual(open(f"{APP}/{n}", "rb").read(10), b"NIBArchive", n)

class Docs(unittest.TestCase):
    def test_docs(self):
        for f in ("BINARY_NOTES.md", "PORT_STATUS.md", "FUNCTIONS_std.md", "OBJC_std.md", "IMPORTS.md", "HLE_TODO.md"):
            need(f"{ROOT}/docs/{f}"); self.assertGreater(os.path.getsize(f"{ROOT}/docs/{f}"), 300, f)

if __name__ == "__main__": unittest.main(verbosity=2)
