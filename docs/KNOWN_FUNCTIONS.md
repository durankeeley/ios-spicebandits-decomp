# Known functions (Spice Bandits 1.14, armv7 `Tower`)

Functions whose behaviour has been **verified** (by disassembly plus a run-time check). Addresses are image addresses in the decrypted `std` binary (Thumb functions listed without the +1 bit). The auto-generated catalogues (`FUNCTIONS_std.md`, `OBJC_std.md`, `functions_std.csv`, `CLASSES_std.md`) only carry heuristic labels; this file is the curated map. **Rule: whenever a function is identified and its job is understood, add it here in the same change** (see `CLAUDE.md`).

Conventions: `game` = the engine game object created by `sub_f643c` (size 0x500, class not named in the binary, called "Game" here); `engine` = the object embedded in `EAGLView` (ivar offset in `0x47a7b4`); `cmd` = the screen-transition object at `engine+0x14` / `game+0x7c`.

## Engine start-up and scene state machine
| address | name / role | notes |
|---|---|---|
| `0xf6eb0` | engine per-frame function | runs `sub_f7fa4`, `sub_f8018`, `sub_f6b24`, then the transition state machine on `cmd` (kind 0..3, type at +4, target at +8); draws game (`d2768`+`d4f04`), loading screen (`[engine+0x18]` -> `sub_a7a58`), fades |
| `0xf65b8` | `EAGLView receivedRoomData` body | ignores the notification if engine state in {4,11,12}; else, if a join answer says `numplayers==1`, creates the game (`f643c`) and runs `d490c` |
| `0xf643c` | create game | `new CRoom/Game(0x500)` via `sub_c5938(game, config, cmd=engine+0x14)`, `engine+0x10 = game`, sets `game+0x14 = delegate` |
| `0xf682c` | `EAGLView prepareLevelDownload:` body | stores level info at `engine+0xa4`, posts transition request type 5/10 |
| `0xf66c8` | return to menu / unload game | used by request types 4, 11, 12 |
| `0xf781a` | request type 6 handler | deletes the loading screen (`engine+0x18`), `sub_d1238(game)`, posts type 0 |
| `0xf776c` | request type 9 handler | recreates the game (`f643c(...,1)`), creates the loading screen |
| `0xb29e4` / `0xb2a90` | post a transition request / start the iris-open | **ignored unless `cmd` kind is 2 or 3** - the reason level loading used to stall (loader finished before the transition was idle) |
| `0xb25d4` | transition tick (iris wipe) | kind 2 (opening) counts `cmd+0x2c` down by dt, then kind 3 (idle) |
| `0xc64cc` | game-ended notification `(game, success, multi)` | success+multi -> request type 12, success -> 11, failure -> 4 (all lead back to the menu). **Not** "level loaded" |
| `0xd490c` | game init | loads strings, `game+0x2ac` (numplayers); for single player calls `d4c04(game,1)` |
| `0xd4c04` | game start `(game, skipInit)` | sets `game+0x2f6`, starts the loader thread (`dcdac` + `126c08`), `game+0x19=1`; no-op if already started |
| `0xd4e10` / `0xd4dbc` | `resumeGameRender` pieces | clear/set the paused flags `game+0x2f5` / `0x2f4` |
| `0xd4f04` | game update + draw (per frame) | draws the world only when `game+0x18`; 1 s start timer `game+0x314` |
| `0xd2768` / `0xd2ac6` | game logic update | gated by `game+0x2b0`, `game+0x18`, `game+0x48d`, `cmd` |
| `0xd2e50` | per-level first-time tutorial | reads `NSUserDefaults boolForKey:"FirstGameMode-%d"` (`game+0x2e4`), shows `TutorialView` text, sets the key |
| `0xcb5d8` | level loader body (thread) | on success posts request type 6 (`b29e4(cmd,6,0,0xb)`); on failure `c64cc(game,0,0)` |
| `0xdce70` | `CThreadLoader::vf2` (thread run) | makes a shared GL context current, calls `8d360` (map) and `cb5d8` (game), sets `loader+0x18=1` |
| `0x126c38` | pthread entry thunk | calls `vtable[2]` of its argument |

## Game HUD, tower bar and input
| address | name / role | notes |
|---|---|---|
| `0xd5738` | HUD draw | runs only when `game+0x18`; calls the bar (`d78f0`), info (`d6184`), tower bar background (`d6fe8`), players list (`d7530`, multi-player only) |
| `0xd78f0` | tower bar draw | iterates `game+0x2bc..0x2c0` (6 items: type at +0x14, x at +0), sets the "cannot afford" flag `item+0x19 = spice < price`, calls `[TDPlayer hasUnlock:]`, then draws `icon` (iPad) or `icon_x2` (iPhone, `game+0x1dd`) textures from each tower XML |
| `0xd6fe8` | bar background | tiles `game+0x45c/0x464/0x3ac` |
| `0xd6184` | HUD money/xp/spice labels | uses `[TDPlayer xppoints/levelpoints/spice]` |
| `0xd60b0` | full-screen fade overlay | `game+0x324` |
| `0xd9768` | tower bar hit-test (touches must reach `EAGLView`; a clear plain `UIView` container used to swallow them) | looks up the `icon` texture of each item (`sub_1177e0`) and tests the touch rectangle |
| `0xd9cf8` | game touch-began | needs `game+0x2b0`, `game+0x18`, state `[game+0x384]>=2` |
| `0xd9df0` / `0xd9e10` | game touch-ended / moved | |
| `0xf8568` / `0xf86dc` / `0xf8700` | engine touch begin / end / move `(x, y)` | end is not reached from `touchesEnded:` on the host; `uikit_touch` calls `0xf86dd` explicitly |
| `0xc96e4` | GUI touch begin | `[input+0x10]` handler |
| `0x1177e0` | find texture/XML child by name | returns null when the named texture (e.g. `icon_x2`) was not loaded for this device |

## Pause menu, tutorial script, config table
| address | name / role | notes |
|---|---|---|
| `0xf5970` | engine constructor (`-[EAGLView .cxx_construct]` via `f5960`) | allocates the message-box manager (`0xa99bc`) into `[0x47ecdc]`, inits engine fields; `f5afc` is the matching destructor |
| `0xa9af4` / `0xa9bf8` | add a modal message box to the manager / manager update+draw | drawn from `f6eb0` at `f7c5c` while `[[0x40a234]]` is non-null |
| `0xad24c` | pause menu touch handler `(menu, x, y, ..., down)` | called by `c9768` at touch begin and end; tests each button rect with `110c50`; shows `"Do you want to abandon the current game?"` (engine-drawn dialog, texts via `drawInRect:`) for Replay / To the map |
| `0x110c50` | rect hit test `(x, y, w, h, [flag, touchX, touchY, &down])` | returns 1 on release inside the rect after a press inside it |
| `0xc9768` | game GUI touch processing | returns early while the pause menu is shown (`adc8c(game+0x390)` = slide progress > 0) |
| `0xadc8c` | pause menu visible? | `[menu+8] > 0` |
| `0x8cccc` | in-level tutorial script ("Welcome to the camp, cadet!", "build here" markers, ...) | never runs on the host; created by `0x109300` (`world+0x1d4`) which is reached from `0x8fc70` (world update from the engine frame function `f6eb0`, request type 12 handler `f7bd8`) |
| `0x77f0c` / `0x77be4` | config table add / lookup by id | filled by `-[TDEventProxy updateConfigs:]` (`0x3bf54`, input = array of config dicts with `configid gametype grantsunlock needsunlock numplayers location map msgwin msglost settings`) and by `0x8df10` |
| `0xfac04` | config struct constructor | strings assigned with `std::string::assign` |

## Level config, tutorial and magic bar (root cause of the missing prompts)
| address | name / role | notes |
|---|---|---|
| `0xfac04` | config struct constructor/updater | parses the `settings` JSON: `+0x30 dumbcreeps`, `+0x31 build_around_base`, `+0x32 tutorial`; **`[parsed isKindOfClass:[NSNull class]] ? nil : parsed`** - an `isKindOfClass:` that always answered YES made every settings dictionary nil, so the tutorial flag, the magic icons and other per-level settings were all off |
| `0xcb5d8` | level loader | `if (config+0x32) { game+0x334 = new CTutorial(0x2c) via sub_b314c; world+0x2a0 = same }` where `config = game+0x2d4` (looked up by id with `0x77be4`) |
| `0xb31c8` | CTutorial constructor/script | all in-level prompt strings ("Welcome to the camp, cadet!", "build here", ...); step number at `tutorial+8` |
| `0xb7c98` | CTutorial update `(tut, dt, x, y, beginX, beginY, down)` | called every frame from `d2768` (`d2c64`) |
| `0xb7d68` / `0xb7dd0` | tutorial gating helpers | ask whether the current step allows a UI action |
| `0xc9768` | per-frame GUI/touch processing | tower bar press, tutorial; `[game+0x334]` set -> tutorial owns the bar |

Tutorial drag: the ghost tower is drawn about 58 px above the finger, so drop 58 px **below** the marker centre.

## Upgrades yard (spending upgrade points)

| Address | Function | What it does |
|---|---|---|
| `0x31230` | `-[UpgradesViewController viewDidLoad]` | builds the tile grid from `userData[tower_upgrades]` via `keysSortedByValueUsingComparator:` (host implements it; before, the list was empty), then `makeObjectsPerformSelector:` refreshes the tiles |
| `0x69350` / `0x69370` | `+[UpgradeLevel LevelFromPoints:]` / `PointsToLevel:` | cumulative cost table (max level 10); `up_*` userData values are points, not levels |
| - | `-[TDPlayer modBuyUpgrade:withSign:]` -> `-[TDServer modBuyUpgrade:]` | offline buy: the game itself updates `up_<key>` and `specpoints` in the response and `processResponse:` merges it; persisted via `AnonymousUserData` |

## Touch input facts
- The engine polls the touch state once per frame (`[game+0x344/0x348]` position, `[game+0x34c]` down flag, `[game+0x33c/0x340]` begin position) in `c9768`, `d2768` (tutorial update `b7c98`) and the pause menu (`ad24c`); a press and release inside one frame is invisible to it, so the host keeps a mouse click "down" for 4 frames.

## Objective-C / game services
| selector | address | role |
|---|---|---|
| `-[O5Globals deviceType]` | `0x1523c` | **2 if `[UIDevice userInterfaceIdiom]==Pad`, else 1** (cached). Drives `game+0x1dd` and which texture set is used; must report iPad |
| `-[TowerViewController dissolveSplash]` | `0x1d430` | `if ([TutorialView canShowTutorial:2]) enterMenu:101 (map) else lobby` |
| `-[EAGLViewController enterMenu:liveGame:]` | `0x375cc` | type 100 = home, 101 = map; hides the top menu bar and shows the welcome tutorial when pending |
| `-[EAGLViewController setHideMenu:]` | `0x36af0` | hides/shows `MenuTopView`/`MenuBottomView` with a fade |
| `-[IntroSequenceView startIntroSequence]` / `finishedIntroSequence` | `0x71ed0` / `0x7206c` | studio splash; the host redirects start -> finished |
| `-[IntroSequenceView animateOn5Mask]` | `0x72bf8` | loops until `[moviePlayer currentPlaybackTime]` passes a threshold |
| `+[TutorialView canShowTutorial:]` | `0x655dc` | `TutorialType-%i` default + `hasUnlock:` checks |
| `-[TutorialView adjustFrame]` | `0x64d38` | sizes the bubble to the measured text; relies on autoresized children |
| `-[TDPlayer checkForOfflineModeTutorial]` | `0x4036c` | offline welcome tutorial gate |
| `-[TDPlayer hasUnlock:]` / `hasSingleUnlock:` | `0x44558` / `0x4449c` | comma-separated unlock string tests (`T4L1`, `tut`, ...) |
| `-[TDPlayer playerWithSlot:]` | `0x44d8c` | scans `roomData["players"]` for `slot`; the offline join does not send `players` (host adds it) |
| `-[TDServer mod_joinGameConfig:]` | `0x45248` | offline join: builds `roomid status configid numplayers location map wavedelay gametype sources bases waves` only (no `settings`/`grantsunlock`) |
| `-[TDServer sendEvent:]` | `0x6758` | retried every second when the socket cannot send (offline) |
| `-[TDEventProxy handleGameStarted]` | `0x3ea94` | calls `d4c04(game,0)` |
| `-[MapDownloadTracker CheckAndDownloadMissingMaps]` | `0x94cac` | posts `TDInitiateLevelDownload` when maps are missing |
| `-[O5Server executeRequestQueueOffline]` | `0x6c5c` | offline request queue |

## Data formats
- `offline_data.dat`: JSON `{initial, userData, status}`; `userData.offlineConfigs[]` has per-level `configid, location, map, numplayers, gametype, wavedelay, sources, bases, waves, needsunlock, grantsunlock, msgwin, msglost, settings`. The Training level is `configid 1` (`wavedelay 1000000`, `grantsunlock "tut"`, settings `dumbcreeps, build_around_base, tutorial, cam_x, cam_y, creep_hp`).
- Tower XML (`towers/towerN_M.xml`): `<textures><t>` entries `icon`, `icon_gray` (`<iphone>0`) and `icon_x2`, `icon_gray_x2` (`<iphone>1`); `work_zone` is the clickable rectangle.
- Game fields: `+0x18` level ready, `+0x19` loader started, `+0x1dd` iPhone flag, `+0x2ac` numplayers, `+0x2b0`, `+0x2bc/0x2c0` tower bar items, `+0x2e4` mode/tutorial id, `+0x2f4/0x2f5/0x2f6` pause/started flags, `+0x4bc` player spice, `+0x78` loader done flag.
