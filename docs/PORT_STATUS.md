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
| Screens verified | intro splash, first-run welcome tutorial, world map (flyouts with text), **Training level**, main menu, **Tech shop**, **Career** (avatar), **Spice shop** (offline App Store catalogue), shop tabs |
| Input | mouse = touch; UIKit buttons, tabs and `Done` work; touches reach the GL view and the engine |
| Window | uses the IPA's `iTunesArtwork` as the window icon |

## Known problems
0. **Levels load and render (fixed).** Root cause of the old "loading screen never finishes": the engine's level loader runs on a pthread and reports completion by posting a request to its screen-transition object (`b29e4(cmd, 6, 0, 0xb)` in `sub_cb5d8`); that request is ignored unless the transition is idle (kind 3). The host used to run every `pthread_create` start routine *inside* the call, so the post arrived while the transition was still closing and was dropped. Threads are now cooperative (own stack, `uc_context_save/restore`, parked in `pthread_cond_wait`, woken by `pthread_cond_signal`) and are started ~20 frames after `pthread_create`, once the transition is idle. Also fixed on the way: name-less `addObserver` calls (unbound `UI*Notification` constants) fired on every notification; the guest could delete the host's render-target texture (`glDeleteTextures` id collision) which drew a giant upside-down glyph; `TutorialView` sizes itself from measured text and needs its nib children to follow (autoresize).
   - Remaining level work: tutorial prompts inside the level, tower placement/waves are untested beyond zoom/tap, audio, win/lose screens, and writing the result back to the career.
1. **Career persistence.** `NSUserDefaults` is now real and persistent (`<save dir>/defaults.bin`); the game stores the player profile backup, last location, tutorial flags and the request queue there. Verified: a second launch reloads the saved keys and plays the returning-player flow (intro splash -> map). Not yet verified: that xp / unlocks earned by finishing a level survive a restart.
2. Several screens are still 768 px wide inside the 1024x768 window (shop, career): the nib flag `UIAutoresizeSubviews` is False on nearly every view, so portrait-authored containers are not widened (ignoring the flag broke the landscape main menu, so it is honoured).
3. The tutorial bubble is now sized correctly (nib autoresize) and sits bottom-right as on the device.
4. Main menu shows some nib placeholder labels (`Intro sequence`, `progress: 0%`) that the device hides at runtime.
5. Audio is not implemented (OpenAL calls are silent stubs; CAF IMA4 + MP3 decoding needed).
6. GL performance is untuned (client-side vertex arrays; each GL call is an emulator trap).

## Tools added for this port
`tools/nibdump.py` (decode NIBArchive), `tools/objcdis.py` (disassemble an ObjC method with selector names), `tools/selxref.py` (who uses a selector), `tools/cgbi2png.py` (view CgBI assets), `tools/shot.sh` (headless run -> PNG + backlog), `tools/hle_todo.sh`. Debug env vars: `RA_TRACE=1|2|3`, `RA_UIDUMP=1` (view tree at the screenshot frame), `RA_FBOSHOT=1` (dump the game's render target), `RA_GLERR=1`, `RA_GLDUMP=1`, `RA_PROF=1`, `RA_FPS=1`, `RA_CLICKS=frame:x:y,...`, `RA_CLICK_HOLD=N`, `RA_DRAG=frame:x0:y0:x1:y1`.
