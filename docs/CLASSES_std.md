# C++ classes — Red Alert iOS std

From Itanium RTTI (`tools/rtti.py`). Slot index = vtable position; `thumb` marks Thumb-2 entry points. Methods that are `-` are pure-virtual / imports.

## Hierarchy

- `<d50a0ffbe3e52578fab1c9007e2335826674642ea852936c38fb07820c1eee80b43640967d27ae226>`
- `CBaseInterface`
  - `CExtraInterface`
- `CCreep`
- `CMapBuildingObject`
  - `CBase`
  - `CMine`
  - `CPortalGates`
  - `CTower`
- `CTextureObject`
- `CThread`
  - `CThreadCreeps`
  - `CThreadLoader`
  - `CThreadMap`
- `e7e<91deb7354f70989a149b4aaf0f65184b5550bea45a>`
- `e7e<91deb7354f77939910914aaf0f65184b5550bea45a>`
- `f62<81cf963e48578f9b1496159102781f63>`
- `f7f<8dccaa3575569893019c16b10363>`
- `f<58a0d8a9395f57>`
- `f<58a5cdaf3c55519c891c9a1e8f0f7d1c5c4646b9b70e25c588a9612bebe4ee586b5e2564cc76ee3c>`
- `f<58a5cdaf3c55519c891c9a1e9c0f75326b405abdb52107d295b67200cafff35764542771d170ef>`
- `f<58a5cdaf3c55519c891c9a1e9c0f7536674d5ca3b82c05d392a37f27cbecd45e795e206cdb7ef53b030c>`
- `f<58b2d4ba27>`
- `f<58b6d8ac20535c999807>`

## Vtables


### CBase : CMapBuildingObject
`typeinfo 0x479a30  vtable 0x479a08  slots 10`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x77374` | thumb | 2 |  |
| 1 | `0x76eb4` | thumb | 706 | ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, floorf, glColor4f, sprintf |
| 2 | `0x77378` | thumb | 2 |  |
| 3 | `0x76c48` | thumb | 566 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZNSsC1EPKcRKSaIcE, ZNSsD2Ev |
| 4 | `0x76af4` | thumb | 296 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZNSsC1ERKSs, ZNSsD2Ev |
| 5 | `0x7737c` | thumb | 2 |  |
| 6 | `0x77380` | thumb | 2 |  |
| 7 | `0x77384` | thumb | 4 |  |
| 8 | `0x76150` | thumb | 14 |  |
| 9 | `0x760ac` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |

### CBaseInterface
`typeinfo 0x479b68  vtable 0x479b64  slots 1`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0xa09d4` | thumb | 124 | ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, glColor4f |

### CCreep
`typeinfo 0x479a70  vtable 0x479a48  slots 10`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x77374` | thumb | 2 |  |
| 1 | `0x7f5c8` | thumb | 894 | ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, glColor4f |
| 2 | `0x7f518` | thumb | 44 |  |
| 3 | `0x7f55c` | thumb | 94 |  |
| 4 | `0x77b00` | thumb | 2 |  |
| 5 | `0x7737c` | thumb | 2 |  |
| 6 | `0x77380` | thumb | 2 |  |
| 7 | `0x77384` | thumb | 4 |  |
| 8 | `0x79ae4` | thumb | 14 |  |
| 9 | `0x79a40` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |

### CExtraInterface : CBaseInterface
`typeinfo 0x47c4b4  vtable 0x47c4b0  slots 1`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0xa09d4` | thumb | 124 | ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, glColor4f |

### CMine : CMapBuildingObject
`typeinfo 0x479b10  vtable 0x479ae8  slots 10`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x77374` | thumb | 2 |  |
| 1 | `0x9b328` | thumb | 374 | glColor4f |
| 2 | `0x77378` | thumb | 2 |  |
| 3 | `0x77afc` | thumb | 2 |  |
| 4 | `0x77b00` | thumb | 2 |  |
| 5 | `0x7737c` | thumb | 2 |  |
| 6 | `0x77380` | thumb | 2 |  |
| 7 | `0x77384` | thumb | 4 |  |
| 8 | `0x9acb0` | thumb | 14 |  |
| 9 | `0x9ac0c` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |

### CPortalGates : CMapBuildingObject
`typeinfo 0x479b50  vtable 0x479b28  slots 10`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x77374` | thumb | 2 |  |
| 1 | `0x9c7b8` | thumb | 2 |  |
| 2 | `0x9c77c` | thumb | 14 |  |
| 3 | `0x77afc` | thumb | 2 |  |
| 4 | `0x77b00` | thumb | 2 |  |
| 5 | `0x7737c` | thumb | 2 |  |
| 6 | `0x77380` | thumb | 2 |  |
| 7 | `0x77384` | thumb | 4 |  |
| 8 | `0x9bfcc` | thumb | 14 |  |
| 9 | `0x9bf28` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |

### CTextureObject
`typeinfo 0x479c90  vtable 0x479c88  slots 2`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x125f74` | thumb | 14 |  |
| 1 | `0x125ed0` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |

### CThread
`typeinfo 0x479cb4  vtable 0x479ca8  slots 3`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x126bb4` | thumb | 38 | pthread_join |
| 1 | `0x126afc` | thumb | 140 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv, pthread_join |
| 2 | `0x126c04` | thumb | 4 |  |

### CThreadCreeps : CThread
`typeinfo 0x479a94  vtable 0x479a88  slots 3`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x82f10` | thumb | 14 |  |
| 1 | `0x82e6c` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |
| 2 | `0x83048` | thumb | 212 | glFlush, glGetError, objc_msgSend, pthread_cond_wait |

### CThreadLoader : CThread
`typeinfo 0x479bd4  vtable 0x479bc8  slots 3`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0xdce60` | thumb | 14 |  |
| 1 | `0xdcdbc` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |
| 2 | `0xdce70` | thumb | 254 | NSLog, glGetError, objc_msgSend |

### CThreadMap : CThread
`typeinfo 0x479c74  vtable 0x479c68  slots 3`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0x10d994` | thumb | 30 |  |
| 1 | `0x10d8e0` | thumb | 134 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |
| 2 | `0x10da28` | thumb | 234 | glFlush, glGetError, objc_msgSend, pthread_cond_wait |

### CTower : CMapBuildingObject
`typeinfo 0x479c14  vtable 0x479be8  slots 11`

| slot | addr | mode | size | notes |
|---|---|---|---|---|
| 0 | `0xe8008` | thumb | 16 |  |
| 1 | `0xe881c` | thumb | 1066 | glColor4f |
| 2 | `0x77378` | thumb | 2 |  |
| 3 | `0xe8028` | thumb | 212 |  |
| 4 | `0x77b00` | thumb | 2 |  |
| 5 | `0xe8320` | thumb | 234 |  |
| 6 | `0xe840c` | thumb | 12 |  |
| 7 | `0xe8290` | thumb | 134 |  |
| 8 | `0xe3124` | thumb | 14 |  |
| 9 | `0xe3080` | thumb | 118 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZdlPv |
| 10 | `0xe53b4` | thumb | 5498 | Unwind_SjLj_Register, Unwind_SjLj_Unregister, ZNKSs7compareERKSs, ZNSs6assignERKSs |
