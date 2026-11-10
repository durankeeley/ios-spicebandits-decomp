#!/usr/bin/env bash
# Unpack the IPAs into work/<tag>/ (never modifies the IPAs).
#   std = "Spice Bandits 1.14.ipa"  (decrypted, cryptid 0)  <- the one we analyse and run
#   hd  = com.chillingo...hd...ipa  (FairPlay-encrypted, cryptid 1) <- cannot be analysed or run
set -euo pipefail
cd "$(dirname "$0")/.."
for pair in "std:Spice Bandits 1.14.ipa" "hd:com.chillingo.spicepirateshd_1.14_ios-4.3_spice-bandits_a5c53ff5.ipa"; do
  tag=${pair%%:*}; ipa=${pair#*:}
  [ -d "work/$tag/Payload" ] || { mkdir -p "work/$tag"; unzip -q "$ipa" -d "work/$tag"; }
  echo "$tag -> work/$tag/Payload/Tower.app"
done
