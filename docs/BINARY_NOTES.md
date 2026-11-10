# Binary notes - Spice Bandits (Chillingo, bundle `Tower`) 1.14

Verified facts only.

## The two IPAs
| file | `cryptid` | usable |
|---|---|---|
| `Spice Bandits 1.14.ipa` (tag `std`) | **0** (decrypted) | yes - everything here uses it |
| `com.chillingo.spicepirateshd_1.14_ios-4.3_..._a5c53ff5.ipa` (tag `hd`) | **1** (FairPlay-encrypted) | no - code is AES-encrypted with device keys; cannot be analysed or run. Its resources are the same universal app. |

## Binary
- armv7 only, PIE, `__text` 3.57 MB (0x1000-0x40a000 image, DATA to 0x47f000): loads at slide 0 in the host, so no rebasing is needed.
- Mixed ARM/Thumb-2: 13,032 of 15,964 discovered functions are Thumb. 84% of `__text` decoded.
- **Almost pure Objective-C**: 11,548 methods named from ObjC metadata (`docs/OBJC_std.md`), only 24 C++ classes (RTTI) - the game logic is not in a C++ engine like Red Alert's.
- Exports trie has 1,900 symbols (mostly SDK C functions).
- GLES: 157 functions call GL; `UIRequiredDeviceCapabilities: opengles-2` (shaders), with `ES1Renderer`/`ES2Renderer` classes.
- Frameworks: UIKit, OpenGLES, OpenAL, AVFoundation, StoreKit, GameKit, iAd, MapKit, Twitter, AdSupport, CoreTelephony, ...
- Language is blocks/GCD era (iOS 4.3+): `_NSConcreteStackBlock`, `dispatch_*`.

## Code layout (by ObjC class prefix)
| prefix | what | port plan |
|---|---|---|
| `Tower*`, `EAGLView*`, `ES1/ES2Renderer`, `Texture2D`, `TS*` (sound, crypto), `GS*`? | the game (`TowerAppDelegate`, `TowerView`, `TowerViewController`) | run it |
| `CC*`, `Crystal*` | Chillingo **Crystal** social SDK (login, leaderboards, offers) | stub to nil / offline |
| `Burstly*`, `Flurry*`, `GAD*` (AdMob), `MM*` (MillennialMedia), `IM*` (InMobi), `TJC*` (Tapjoy), `Greystripe`, `OAI*` | ad / analytics SDKs | stub |
| `FB*`, `MGTwitter*`, `OA*` | Facebook / Twitter / OAuth | stub |
| `ASI*`, `Async*`, `CReachability`, `GTM*`, `SBJSON`, `CJSON*`, `CZipArchive` | networking, JSON, zip helpers | host Foundation or stub |
| `O5*`, `TD*`, `Currency`, `InApp*`, `SpiceShop*` | game online services + IAP | stub (offline play) |

## Data
`maps/ creeps/ towers/ gui/ sounds/ music/` (PNG/PVR art, CAF audio, `.mov` splash), 44 compiled **`NIBArchive`** nibs (`MainWindow.nib`, `EAGLViewController.nib`, ...). Not encrypted.

## NIBArchive layout (decoded from `MainWindow.nib`)
`"NIBArchive"`, then little-endian u32: version(1), ?(9), objectCount, objectsOffset, keyCount, keysOffset, valueCount, valuesOffset, classNameCount, classNamesOffset. e.g. MainWindow.nib: 23 objects, 30 keys, 62 values, 8 class names (UILabel, UIView, proxies...). Objects/keys/values use varint encodings; connections (`UINibConnectionsKey`) wire outlets.

## Startup flow observed in the host
`main -> UIApplicationMain(nil principal, nil delegate)`: the delegate (`TowerAppDelegate`) and window normally come from `MainWindow.nib` (`NSMainNibFile~ipad`). Host fallback instantiates `*AppDelegate`. Launch then runs the SDK stack (Crystal session starts; Burstly/Flurry/Tapjoy init), asks for `-[NSBundle loadNibNamed:owner:options:]` (unhandled -> nil) and so **never creates the GL view**.
