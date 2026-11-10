# Port status - Spice Bandits

Same architecture as `../ios-redalert`: the armv7 Mach-O runs under Unicorn inside `host/ra_host`, with iOS implemented on the host (SDL2, desktop GL, a mini Objective-C runtime, a small UIKit, CoreGraphics, Foundation). Only `Spice Bandits 1.14.ipa` (decrypted) is usable; the `...hd...ipa` is FairPlay-encrypted.

## What works (verified by screenshots in `tests/`)
| area | state |
|---|---|
| Boot | Mach-O (PIE) load, dyld binds, **module initialisers** (C++ statics), UIApplicationMain, `MainWindow.nib` |
| Nibs | **NIBArchive loader** (`host/uikit.inc`): all UIKit classes used by the 44 nibs, outlets (setter/ivar/host property), actions, class swapping, `initWithCoder:` via a fake NSCoder |
| UIKit | view tree, frames/transforms/alpha, autoresizing, `layoutSubviews` pass, images (**Apple CgBI PNG decoder**), labels/fonts (FreeType via SDL_ttf, bundle fonts), buttons + target/action, scroll views, view controllers + modal present/dismiss, animations (run immediately), `CADisplayLink`, notifications, hit-testing |
| CoreGraphics | software bitmap contexts (CTM, DrawImage, FillRect, text), `UIGraphics*` image contexts, geometry helpers |
| Foundation | arrays, dictionaries, data, numbers, strings, dates, character sets, URLs, files, errors, locale, bundle (`host/foundation.inc`); ObjC **categories** |
| C++ runtime | libstdc++ subset: COW `std::string`, red-black tree helpers, list nodes, stringstream (`host/cxxabi.c`) |
| System | zlib passthrough, blocks (`Block_copy` with copy helpers) and GCD, threads run synchronously, nested guest calls (engine pool) |
| Rendering | intro splash -> loading screen -> **main menu** (Play / Career / Tech shop / Upgrades / Spicery) -> **world map** (OpenGL ES 1.1 -> desktop GL, landscape) |
| Offline game data | `offline_data.dat` (JSON) is parsed, so the player profile / shop list / medals load |
| Third-party SDKs | Tapjoy, Flurry, Burstly, AdMob, MM, InMobi, Facebook/Twitter, Crystal, AdManager are disabled (every message is a no-op returning nil) |
| Screens verified | main menu, world map, **Tech shop** (item trees from the offline profile), **Career**, shop tabs (Map / Career / Done) |
| Input | mouse = touch; UIKit buttons, tabs and `Done` work; touches reach the GL view and the engine |
| Window | uses the IPA's `iTunesArtwork` as the window icon |

## Known problems
1. **World map: the first level cannot be started yet.** Findings so far (engine addresses, armv7 `Tower`):
   - `EAGLView` touches reach the engine input object (an ivar of `EAGLView`): press `sub_f8568`, release `sub_f86dc` (the ObjC release path never reaches it, so `uikit_touch` calls it explicitly). The camera/map listener at `[input+0xc]` receives both; its per-touch update `sub_90814` hit-tests the 57 map objects and finds the Tutorial marker (id 1, world 500,1450).
   - The tap lands in the "select / recentre" branch (`0x90a5c`); the marker is already the selected id, so it only retargets the camera (`sub_109f20`, `sub_1081d8`). The camera does not visibly move afterwards, so the camera animation may not be advancing.
   - `[input+0x10]` (the engine GUI object whose `sub_c96e4` handles GUI taps) is NULL; its creator was not found.
   - Level start is the `TDInitiateLevelDownload` notification (observed by `EAGLViewController prepareLevelDownload:`, whose userInfo holds an array). The engine never posts it, so a gate before it is not satisfied. The `hasUnlock:@"tut"` check seen after release is for level 2 (`needsunlock "tut"`), not the tutorial.
   - Debug aids: `RA_BREAK`, `RA_DUMPREG=<reg>:<lo>:<hi>` (dumps guest words at a break), `[notif] post <name>` log lines.
   - Next step: break in `sub_1081d8`/`sub_109f20` to see whether the camera tween advances, then find who sets `[sl+0x89]` on map objects (the second-tap-to-start branch at `0x909e0`..`0x90a08`).
2. Several screens are still 768 px wide inside the 1024x768 window (shop, career): the nib flag `UIAutoresizeSubviews` is False on nearly every view, so portrait-authored containers are not widened (ignoring the flag broke the landscape main menu, so it is honoured).
3. The tutorial bubble is positioned at the bottom-right edge (its layout depends on category helpers that read frames; clipped by the window).
4. Main menu shows some nib placeholder labels (`Intro sequence`, `progress: 0%`) that the device hides at runtime.
5. Audio is not implemented (OpenAL calls are silent stubs; CAF IMA4 + MP3 decoding needed).
6. GL performance is untuned (client-side vertex arrays; each GL call is an emulator trap).

## Tools added for this port
`tools/nibdump.py` (decode NIBArchive), `tools/objcdis.py` (disassemble an ObjC method with selector names), `tools/selxref.py` (who uses a selector), `tools/cgbi2png.py` (view CgBI assets), `tools/shot.sh` (headless run -> PNG + backlog), `tools/hle_todo.sh`. Debug env vars: `RA_TRACE=1|2|3`, `RA_UIDUMP=1` (view tree at the screenshot frame), `RA_FBOSHOT=1` (dump the game's render target), `RA_GLERR=1`, `RA_GLDUMP=1`, `RA_PROF=1`, `RA_FPS=1`, `RA_CLICKS=frame:x:y,...`, `RA_CLICK_HOLD=N`, `RA_DRAG=frame:x0:y0:x1:y1`.
