# Imports used by the game (shim surface)

Only imports that are *called* are listed (the Mach-O imports ~290 symbols; many are never referenced). `n` = static call sites, `fns` = functions containing them. Implement these in the host shim.


## std — 605 distinct imports

| symbol | n | fns | example callers |
|---|---|---|---|
| `glActiveTexture` | 13 | 7 | `0x95dd8`, `0xde1f8`, `0xe0428` |
| `glAttachShader` | 2 | 1 | `0x38538` |
| `glBindAttribLocation` | 2 | 1 | `0x38538` |
| `glBindFramebuffer` | 2 | 2 | `0x3824c`, `0x38358` |
| `glBindFramebufferOES` | 2 | 2 | `0x37bf8`, `0x37f6c` |
| `glBindRenderbuffer` | 3 | 3 | `0x3824c`, `0x38358`, `0x386dc` |
| `glBindRenderbufferOES` | 5 | 3 | `0x37bf8`, `0x37f6c`, `0x38044` |
| `glBindTexture` | 17 | 17 | `0x11520c`, `0x1152d0`, `0x115434` |
| `glBlendEquationOES` | 2 | 1 | `0x8621c` |
| `glBlendFunc` | 1 | 1 | `0x11515c` |
| `glCheckFramebufferStatus` | 1 | 1 | `0x386dc` |
| `glCheckFramebufferStatusOES` | 2 | 1 | `0x38044` |
| `glClear` | 4 | 4 | `0x37f6c`, `0x38358`, `0x115234` |
| `glClearColor` | 2 | 2 | `0x37f6c`, `0x38358` |
| `glClearStencil` | 1 | 1 | `0x115234` |
| `glClientActiveTexture` | 9 | 6 | `0x95dd8`, `0xde1f8`, `0xe0428` |
| `glColor4f` | 218 | 84 | `0x76c48`, `0x76eb4`, `0x7f5c8` |
| `glColorMask` | 2 | 2 | `0x115234`, `0x115280` |
| `glColorPointer` | 6 | 5 | `0xde1f8`, `0xe0428`, `0x111880` |
| `glCreateProgram` | 1 | 1 | `0x38538` |
| `glCullFace` | 1 | 1 | `0xf5f3c` |
| `glDeleteFramebuffers` | 1 | 1 | `0x38784` |
| `glDeleteFramebuffersOES` | 1 | 1 | `0x37ee0` |
| `glDeleteProgram` | 2 | 2 | `0x38538`, `0x38784` |
| `glDeleteRenderbuffers` | 1 | 1 | `0x38784` |
| `glDeleteRenderbuffersOES` | 1 | 1 | `0x37ee0` |
| `glDeleteShader` | 4 | 1 | `0x38538` |
| `glDeleteTextures` | 12 | 11 | `0x836e8`, `0xc862c`, `0xff908` |
| `glDepthMask` | 2 | 2 | `0x115234`, `0x115280` |
| `glDisable` | 14 | 12 | `0x95dd8`, `0xf5f3c`, `0x114a84` |
| `glDisableClientState` | 54 | 27 | `0x90cd8`, `0x95dd8`, `0xde1f8` |
| `glDrawArrays` | 28 | 27 | `0x90cd8`, `0x95dd8`, `0xde1f8` |
| `glEnable` | 33 | 29 | `0x82430`, `0x90cd8`, `0x95dd8` |
| `glEnableClientState` | 56 | 27 | `0x90cd8`, `0x95dd8`, `0xde1f8` |
| `glFlush` | 2 | 2 | `0x83048`, `0x10da28` |
| `glFramebufferRenderbuffer` | 1 | 1 | `0x3824c` |
| `glFramebufferRenderbufferOES` | 2 | 2 | `0x37bf8`, `0x38044` |
| `glGenFramebuffers` | 1 | 1 | `0x3824c` |
| `glGenFramebuffersOES` | 1 | 1 | `0x37bf8` |
| `glGenRenderbuffers` | 1 | 1 | `0x3824c` |
| `glGenRenderbuffersOES` | 2 | 1 | `0x37bf8` |
| `glGenTextures` | 6 | 6 | `0x8327c`, `0x1071f8`, `0x122718` |
| `glGetError` | 3 | 3 | `0x83048`, `0xdce70`, `0x10da28` |
| `glGetFloatv` | 1 | 1 | `0xd5738` |
| `glGetProgramInfoLog` | 1 | 1 | `0x384d4` |
| `glGetProgramiv` | 3 | 2 | `0x384ac`, `0x384d4` |
| `glGetRenderbufferParameteriv` | 2 | 1 | `0x386dc` |
| `glGetRenderbufferParameterivOES` | 2 | 1 | `0x38044` |
| `glGetUniformLocation` | 1 | 1 | `0x38538` |
| `glIsTexture` | 1 | 1 | `0xc862c` |
| `glLinkProgram` | 1 | 1 | `0x384ac` |
| `glLoadIdentity` | 5 | 5 | `0x90cd8`, `0x1102f0`, `0x1103b8` |
| `glLoadMatrixf` | 1 | 1 | `0x1103b8` |
| `glMatrixMode` | 12 | 5 | `0x90cd8`, `0x1102f0`, `0x1103b8` |
| `glOrthof` | 2 | 2 | `0x1102f0`, `0x1103e0` |
| `glPopMatrix` | 23 | 22 | `0x8a1ec`, `0x90cd8`, `0x95dd8` |
| `glPushMatrix` | 19 | 18 | `0x90cd8`, `0x95dd8`, `0xa8748` |
| `glRenderbufferStorageOES` | 1 | 1 | `0x38044` |
| `glRotatef` | 11 | 11 | `0x90cd8`, `0xde1f8`, `0x114ff8` |
| `glScalef` | 4 | 4 | `0xa8748`, `0x1102f0`, `0x110ce0` |
| `glStencilFunc` | 2 | 2 | `0x115234`, `0x115280` |
| `glStencilOp` | 2 | 2 | `0x115234`, `0x115280` |
| `glTexCoordPointer` | 21 | 19 | `0x90cd8`, `0x95dd8`, `0xde1f8` |
| `glTexImage2D` | 8 | 7 | `0x8327c`, `0x1071f8`, `0x122718` |
| `glTexParameterf` | 20 | 5 | `0x8327c`, `0x1071f8`, `0x124698` |
| `glTexParameteri` | 3 | 2 | `0x122718`, `0x124698` |
| `glTranslatef` | 24 | 16 | `0x90cd8`, `0x95dd8`, `0xa8748` |
| `glUseProgram` | 1 | 1 | `0x38358` |
| `glValidateProgram` | 1 | 1 | `0x384d4` |
| `glVertexPointer` | 29 | 27 | `0x90cd8`, `0x95dd8`, `0xde1f8` |
| `glViewport` | 2 | 2 | `0x37f6c`, `0x38358` |
| `ABAddressBookCopyArrayOfAllPeople` | 3 | 3 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `ABAddressBookCreate` | 4 | 4 | `0x171738`, `0x22e6f8`, `0x22e8ac` |
| `ABAddressBookCreateWithOptions` | 1 | 1 | `0x22f3b4` |
| `ABAddressBookGetAuthorizationStatus` | 1 | 1 | `0x22f3b4` |
| `ABAddressBookGetPersonWithRecordID` | 1 | 1 | `0x171738` |
| `ABAddressBookRequestAccessWithCompletion` | 1 | 1 | `0x22f3b4` |
| `ABMultiValueCopyArrayOfAllValues` | 4 | 3 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `ABMultiValueCopyLabelAtIndex` | 2 | 1 | `0x22ecbc` |
| `abort` | 3 | 3 | `0x1a1da8`, `0x1a246c`, `0x1cda28` |
| `ABPersonCopyImageData` | 1 | 1 | `0x171738` |
| `ABPersonHasImageData` | 1 | 1 | `0x171738` |
| `ABRecordCopyValue` | 8 | 3 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `ABRecordGetRecordID` | 2 | 2 | `0x22e8ac`, `0x22ecbc` |
| `ABRecordGetRecordType` | 3 | 3 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `alcCloseDevice` | 1 | 1 | `0x11ecc0` |
| `alcDestroyContext` | 1 | 1 | `0x11ecc0` |
| `alcGetCurrentContext` | 1 | 1 | `0x11f2a8` |
| `alcMakeContextCurrent` | 2 | 2 | `0x11ecc0`, `0x11ed08` |
| `alcOpenDevice` | 1 | 1 | `0x11ec38` |
| `alcProcessContext` | 1 | 1 | `0x11ed08` |
| `alDeleteBuffers` | 1 | 1 | `0x11ef50` |
| `alDistanceModel` | 1 | 1 | `0x11ee1c` |
| `alGenSources` | 1 | 1 | `0x11ed64` |
| `alGetError` | 2 | 2 | `0x11ed40`, `0x11ed64` |
| `alGetSourcei` | 5 | 3 | `0x11f7e4`, `0x11faf8`, `0x120b88` |
| `alListenerfv` | 2 | 1 | `0x11edb8` |
| `alSourcef` | 8 | 3 | `0x11f950`, `0x11fa08`, `0x120948` |
| `alSourcefv` | 3 | 2 | `0x11f950`, `0x120948` |
| `alSourcei` | 5 | 3 | `0x11f950`, `0x11fb24`, `0x1207d0` |
| `alSourcePlay` | 1 | 1 | `0x11f950` |
| `alSourceStop` | 3 | 3 | `0x11f5fc`, `0x11f7e4`, `0x1207d0` |
| `arc4random` | 7 | 5 | `0x12fa88`, `0x267cc4`, `0x267d08` |
| `assert_rtn` | 5 | 4 | `0x11e05c`, `0x1236f8`, `0x123d94` |
| `atan` | 2 | 1 | `0x110524` |
| `atof` | 76 | 12 | `0xfcae8`, `0x10000e`, `0x10010e` |
| `atoi` | 198 | 14 | `0xcd8e0`, `0xcf274`, `0xe7a08` |
| `AudioFileClose` | 1 | 1 | `0x11f128` |
| `AudioFileGetProperty` | 1 | 1 | `0x11f128` |
| `AudioFileOpenURL` | 1 | 1 | `0x11f128` |
| `AudioSessionGetProperty` | 2 | 2 | `0x2c3a9c`, `0x321418` |
| `AudioSessionInitialize` | 4 | 4 | `0x11ec38`, `0x2c3700`, `0x2c39e0` |
| `AudioSessionSetActive` | 4 | 3 | `0x11ed08`, `0x2c39e0`, `0x2c3a9c` |
| `AudioSessionSetProperty` | 3 | 3 | `0x11ec38`, `0x11ed08`, `0x2c3a9c` |
| `Block_copy` | 1 | 1 | `0x36a830` |
| `Block_object_assign` | 68 | 51 | `0x5c44`, `0x7c3c`, `0x157dc` |
| `Block_object_dispose` | 68 | 51 | `0x5c68`, `0x7c6c`, `0x15800` |
| `bzero` | 4 | 4 | `0x2455ec`, `0x245660`, `0x2c1680` |
| `CACurrentMediaTime` | 1 | 1 | `0x22298` |
| `calloc` | 22 | 16 | `0x122a54`, `0x122e74`, `0x12a82c` |
| `CATransform3DMakeTranslation` | 1 | 1 | `0x131a8` |
| `CATransform3DRotate` | 1 | 1 | `0x131a8` |
| `CC_MD5` | 6 | 6 | `0x12fa88`, `0x162414`, `0x1c86b8` |
| `CC_SHA1` | 6 | 6 | `0x13f698`, `0x2270c4`, `0x24b774` |
| `CC_SHA256` | 1 | 1 | `0x13eba0` |
| `CCCrypt` | 1 | 1 | `0x369578` |
| `CCHmac` | 1 | 1 | `0x369134` |
| `ceil` | 12 | 7 | `0x31230`, `0x206748`, `0x21d450` |
| `ceilf` | 11 | 7 | `0x6aab4`, `0x6b970`, `0x97628` |
| `CFAbsoluteTimeGetCurrent` | 4 | 4 | `0x31a910`, `0x32c310`, `0x331334` |
| `CFArrayAppendValue` | 1 | 1 | `0x2c1b24` |
| `CFArrayCreateMutable` | 1 | 1 | `0x2c1acc` |
| `CFArrayGetCount` | 16 | 7 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `CFArrayGetValueAtIndex` | 14 | 7 | `0x22e6f8`, `0x22e8ac`, `0x22ecbc` |
| `CFArrayRemoveAllValues` | 1 | 1 | `0x2c1958` |
| `CFBundleGetMainBundle` | 1 | 1 | `0x310790` |
| `CFBundleGetValueForInfoDictionaryKey` | 1 | 1 | `0x310790` |
| `CFDataGetBytePtr` | 2 | 2 | `0xbce8`, `0xbd8c` |
| `CFDictionaryCreateMutable` | 1 | 1 | `0x31fb48` |
| `CFDictionaryGetValue` | 1 | 1 | `0x31fbe0` |
| `CFDictionaryGetValueIfPresent` | 2 | 2 | `0x3350c4`, `0x335120` |
| `CFDictionaryRemoveValue` | 1 | 1 | `0x31fc00` |
| `CFDictionarySetValue` | 1 | 1 | `0x31fbc0` |
| `CFHTTPAuthenticationCopyMethod` | 2 | 2 | `0x1d8314`, `0x1d8f8c` |
| `CFHTTPAuthenticationCopyRealm` | 2 | 2 | `0x1d8314`, `0x1d8f8c` |
| `CFHTTPAuthenticationCreateFromResponse` | 2 | 2 | `0x1d8314`, `0x1d8f8c` |
| `CFHTTPAuthenticationIsValid` | 2 | 2 | `0x1d8314`, `0x1d8f8c` |
| `CFHTTPAuthenticationRequiresAccountDomain` | 4 | 4 | `0x1d72e0`, `0x1d7770`, `0x1d8314` |
| `CFHTTPAuthenticationRequiresUserNameAndPassword` | 2 | 2 | `0x1d8314`, `0x1d8f8c` |
| `CFHTTPMessageApplyCredentialDictionary` | 4 | 3 | `0x1ce7c8`, `0x1d6cc0`, `0x1d6fd8` |
| `CFHTTPMessageCopyAllHeaderFields` | 1 | 1 | `0x1d572c` |
| `CFHTTPMessageCopyResponseStatusLine` | 1 | 1 | `0x1d572c` |
| `CFHTTPMessageCopyVersion` | 1 | 1 | `0x1d572c` |
| `CFHTTPMessageCreateRequest` | 1 | 1 | `0x1cda28` |
| `CFHTTPMessageGetResponseStatusCode` | 1 | 1 | `0x1d572c` |
| `CFHTTPMessageIsHeaderComplete` | 1 | 1 | `0x1d572c` |
| `CFHTTPMessageSetHeaderFieldValue` | 2 | 2 | `0x1cda28`, `0x1d1fbc` |
| `CFMakeCollectable` | 18 | 11 | `0x1c8bdc`, `0x1cf6cc`, `0x1d1fbc` |
| `CFNetworkCopyProxiesForAutoConfigurationScript` | 1 | 1 | `0x1de19c` |
| `CFNetworkCopyProxiesForURL` | 2 | 2 | `0x1dd410`, `0x1de19c` |
| `CFNetworkCopySystemProxySettings` | 1 | 1 | `0x1dd410` |
| `CFNotificationCenterAddObserver` | 3 | 1 | `0x335dc8` |
| `CFNotificationCenterGetLocalCenter` | 1 | 1 | `0x335dc8` |
| `CFNumberGetType` | 1 | 1 | `0x264f28` |
| `CFNumberGetValue` | 1 | 1 | `0x335120` |
| `CFReadStreamClose` | 1 | 1 | `0xaeec` |
| `CFReadStreamCopyError` | 1 | 1 | `0x1dc644` |
| `CFReadStreamCopyProperty` | 6 | 6 | `0xabdc`, `0x1d1fbc`, `0x1d572c` |
| `CFReadStreamCreateForHTTPRequest` | 1 | 1 | `0x1cf6cc` |
| `CFReadStreamCreateForStreamedHTTPRequest` | 2 | 1 | `0x1cf6cc` |
| `CFReadStreamGetError` | 3 | 3 | `0xb4c4`, `0xcb64`, `0xdb5c` |
| `CFReadStreamGetStatus` | 2 | 2 | `0xbc88`, `0xbdd4` |
| `CFReadStreamHasBytesAvailable` | 2 | 2 | `0xb270`, `0x1da128` |
| `CFReadStreamOpen` | 2 | 2 | `0xaa5c`, `0x1cf6cc` |
| `CFReadStreamRead` | 1 | 1 | `0xb270` |
| `CFReadStreamSetClient` | 5 | 5 | `0x8e1c`, `0xa758`, `0x1cf6cc` |
| `CFReadStreamSetProperty` | 7 | 2 | `0xd998`, `0x1cf6cc` |
| `CFReadStreamUnscheduleFromRunLoop` | 1 | 1 | `0x8e1c` |
| `CFRelease` | 109 | 58 | `0x9570`, `0xa508`, `0xaeec` |
| `CFRetain` | 8 | 7 | `0x1071f8`, `0x1cd19c`, `0x1d1ad0` |
| `CFRunLoopAddSource` | 4 | 4 | `0x8c2c`, `0xe514`, `0x1e2b50` |
| `CFRunLoopAddTimer` | 2 | 2 | `0x8d24`, `0xe60c` |
| `CFRunLoopGetCurrent` | 14 | 13 | `0xa1c4`, `0xa758`, `0xdfac` |
| `CFRunLoopGetMain` | 2 | 2 | `0x31b1ec`, `0x31b2bc` |
| `CFRunLoopRemoveSource` | 3 | 3 | `0x8ca8`, `0xe590`, `0x1e2b50` |
| `CFRunLoopRemoveTimer` | 2 | 2 | `0x8da0`, `0xe688` |
| `CFRunLoopRun` | 1 | 1 | `0x1e2b50` |
| `CFRunLoopRunInMode` | 1 | 1 | `0x2becc4` |
| `CFRunLoopSourceCreate` | 3 | 3 | `0x1e2b50`, `0x2bdb4c`, `0x2be55c` |
| `CFRunLoopSourceInvalidate` | 1 | 1 | `0x2bf244` |
| `CFRunLoopSourceSignal` | 1 | 1 | `0x2bf178` |
| `CFRunLoopStop` | 3 | 3 | `0x1dafe0`, `0x1dbbd4`, `0x2bf244` |
| `CFRunLoopWakeUp` | 1 | 1 | `0x2bf178` |
| `CFSocketConnectToAddress` | 5 | 3 | `0xa300`, `0xf6b8`, `0xf8f0` |
| `CFSocketCopyAddress` | 2 | 2 | `0xbbe8`, `0xbc20` |
| `CFSocketCopyPeerAddress` | 2 | 2 | `0xbb78`, `0xbbb0` |
| `CFSocketCreate` | 5 | 3 | `0x9ffc`, `0xa104`, `0xdfac` |
| `CFSocketCreateRunLoopSource` | 4 | 2 | `0xa1c4`, `0xdfac` |
| `CFSocketEnableCallBacks` | 2 | 2 | `0x112a0`, `0x11a14` |
| `CFSocketGetNative` | 21 | 14 | `0x9570`, `0xa508`, `0xf42c` |
| `CFSocketInvalidate` | 7 | 5 | `0x9570`, `0xa508`, `0xaeec` |
| `CFSocketIsValid` | 1 | 1 | `0xbc58` |
| `CFSocketSetAddress` | 3 | 2 | `0x9570`, `0xf42c` |
| `CFSocketSetSocketFlags` | 3 | 2 | `0xa508`, `0xdfac` |
| `CFStreamCreatePairWithSocket` | 1 | 1 | `0xa5f4` |
| `CFStreamCreatePairWithSocketToHost` | 1 | 1 | `0xa6b0` |
| `CFStringAppend` | 1 | 1 | `0x337214` |
| `CFStringAppendCharacters` | 4 | 4 | `0x6246c`, `0x19d2f4`, `0x19efe4` |
| `CFStringAppendFormat` | 4 | 4 | `0x335164`, `0x335218`, `0x336bd0` |
| `CFStringCompare` | 4 | 2 | `0x321418`, `0x336bd0` |
| `CFStringConvertEncodingToIANACharSetName` | 2 | 2 | `0x1c98a8`, `0x1c9f98` |
| `CFStringConvertEncodingToNSStringEncoding` | 1 | 1 | `0x1e331c` |
| `CFStringConvertIANACharSetNameToEncoding` | 1 | 1 | `0x1e331c` |
| `CFStringConvertNSStringEncodingToEncoding` | 3 | 3 | `0x1c8bdc`, `0x1c98a8`, `0x1c9f98` |
| `CFStringCreateArrayBySeparatingStrings` | 2 | 1 | `0x336bd0` |
| `CFStringCreateCopy` | 2 | 2 | `0x335fe8`, `0x336bd0` |
| `CFStringCreateMutable` | 2 | 2 | `0x335164`, `0x335218` |
| `CFStringCreateMutableCopy` | 1 | 1 | `0x336bd0` |
| `CFStringCreateWithCString` | 4 | 2 | `0x335dc8`, `0x337054` |
| `CFStringCreateWithFormat` | 1 | 1 | `0x3352c8` |
| `CFStringCreateWithSubstring` | 1 | 1 | `0x336bd0` |
| `CFStringFind` | 24 | 6 | `0x12be90`, `0x12c2dc`, `0x132204` |
| `CFStringGetCString` | 2 | 2 | `0x3350c4`, `0x3360b0` |
| `CFStringGetCStringPtr` | 2 | 2 | `0x12fa88`, `0x3360b0` |
| `CFStringGetFastestEncoding` | 1 | 1 | `0x12fa88` |
| `CFStringGetLength` | 7 | 5 | `0x321418`, `0x3360b0`, `0x336174` |
| `CFStringGetMaximumSizeForEncoding` | 1 | 1 | `0x3360b0` |
| `CFStringHasPrefix` | 2 | 1 | `0x336bd0` |
| `CFStringHasSuffix` | 2 | 1 | `0x336bd0` |
| `CFURLCreateFromFileSystemRepresentation` | 1 | 1 | `0x11f128` |
| `CFURLCreateStringByAddingPercentEscapes` | 18 | 16 | `0x559c4`, `0x6af50`, `0x6d0f8` |
| `CFURLCreateStringByReplacingPercentEscapesUsingEncoding` | 4 | 4 | `0x2248dc`, `0x230e84`, `0x232720` |
| `CFUUIDCreate` | 9 | 9 | `0x155e0`, `0x5a4c8`, `0x12fa88` |
| `CFUUIDCreateString` | 9 | 9 | `0x155e0`, `0x5a4c8`, `0x12fa88` |
| `CFWriteStreamCanAcceptBytes` | 1 | 1 | `0xd534` |
| `CFWriteStreamClose` | 1 | 1 | `0xaeec` |
| `CFWriteStreamGetError` | 2 | 2 | `0xd534`, `0xdc28` |
| `CFWriteStreamGetStatus` | 1 | 1 | `0xbdd4` |
| `CFWriteStreamSetClient` | 1 | 1 | `0x8ebc` |
| `CFWriteStreamSetProperty` | 1 | 1 | `0xd998` |
| `CFWriteStreamUnscheduleFromRunLoop` | 1 | 1 | `0x8ebc` |
| `CFWriteStreamWrite` | 1 | 1 | `0xd534` |
| `CGAffineTransformConcat` | 22 | 10 | `0x1071f8`, `0x162228`, `0x18ec68` |
| `CGAffineTransformMakeRotation` | 40 | 19 | `0x20868`, `0x4bc30`, `0x52fa0` |
| `CGAffineTransformMakeScale` | 20 | 13 | `0x319cc`, `0x650dc`, `0x68278` |
| `CGAffineTransformMakeTranslation` | 16 | 8 | `0x1071f8`, `0x175bd4`, `0x1ace74` |
| `CGAffineTransformRotate` | 2 | 2 | `0x1a8700`, `0x2cfc90` |
| `CGAffineTransformScale` | 18 | 10 | `0x6ad70`, `0x6c3b0`, `0x12cae0` |
| `CGAffineTransformTranslate` | 28 | 12 | `0x190e1c`, `0x191c88`, `0x1f28e4` |
| `CGBitmapContextCreate` | 10 | 9 | `0x17ea8`, `0x1851c`, `0x8327c` |
| `CGBitmapContextCreateImage` | 11 | 11 | `0x17ea8`, `0x1851c`, `0x1884c` |
| `CGColorCreate` | 1 | 1 | `0x122a54` |
| `CGColorGetComponents` | 6 | 5 | `0x1884c`, `0x236df8`, `0x2da708` |
| `CGColorGetNumberOfComponents` | 3 | 2 | `0x236df8`, `0x30b870` |
| `CGColorRelease` | 1 | 1 | `0x122a54` |
| `CGColorSpaceCreateDeviceGray` | 2 | 2 | `0x122a54`, `0x122e74` |
| `CGColorSpaceCreateDeviceRGB` | 19 | 18 | `0x17ea8`, `0x1851c`, `0x18774` |
| `CGColorSpaceRelease` | 20 | 18 | `0x17ea8`, `0x1851c`, `0x18774` |
| `CGContextAddArc` | 8 | 2 | `0x2d37e0`, `0x2d3a88` |
| `CGContextAddArcToPoint` | 32 | 5 | `0x13a8c`, `0x6a66c`, `0x21cce8` |
| `CGContextAddCurveToPoint` | 8 | 1 | `0x2d10a8` |
| `CGContextAddEllipseInRect` | 1 | 1 | `0x23664c` |
| `CGContextAddLineToPoint` | 35 | 8 | `0x23664c`, `0x2d10a8`, `0x2d37e0` |
| `CGContextAddRect` | 4 | 4 | `0x6a66c`, `0x21cce8`, `0x2d4070` |
| `CGContextBeginPath` | 8 | 7 | `0x13a8c`, `0x6a66c`, `0x21cce8` |
| `CGContextClearRect` | 7 | 7 | `0x1884c`, `0x48574`, `0x4873c` |
| `CGContextClip` | 1 | 1 | `0x2d10a8` |
| `CGContextClipToMask` | 2 | 2 | `0x1851c`, `0x1884c` |
| `CGContextClipToRect` | 1 | 1 | `0x236c58` |
| `CGContextClosePath` | 6 | 6 | `0x6a66c`, `0x21cce8`, `0x2d4070` |
| `CGContextConcatCTM` | 1 | 1 | `0x1071f8` |
| `CGContextDrawImage` | 10 | 9 | `0x17ea8`, `0x1884c`, `0x48da8` |
| `CGContextDrawLayerAtPoint` | 1 | 1 | `0x2d0b38` |
| `CGContextDrawLinearGradient` | 2 | 2 | `0x236c58`, `0x2d10a8` |
| `CGContextFillEllipseInRect` | 1 | 1 | `0x23664c` |
| `CGContextFillPath` | 13 | 8 | `0x6a828`, `0x21d03c`, `0x2d37e0` |
| `CGContextFillRect` | 5 | 5 | `0x1851c`, `0x1884c`, `0x6a828` |
| `CGContextMoveToPoint` | 23 | 12 | `0x13a8c`, `0x6a66c`, `0x21cce8` |
| `CGContextRelease` | 13 | 13 | `0x17ea8`, `0x1851c`, `0x1884c` |
| `CGContextRestoreGState` | 14 | 14 | `0x52fa0`, `0x6a66c`, `0x6a828` |
| `CGContextSaveGState` | 15 | 15 | `0x52fa0`, `0x6a66c`, `0x6a828` |
| `CGContextScaleCTM` | 6 | 6 | `0x6a66c`, `0x8327c`, `0x122a54` |
| `CGContextSelectFont` | 1 | 1 | `0x52fa0` |
| `CGContextSetAlpha` | 1 | 1 | `0x1071f8` |
| `CGContextSetFillColor` | 5 | 5 | `0x6a828`, `0x21d03c`, `0x2da708` |
| `CGContextSetFillColorWithColor` | 9 | 4 | `0x1851c`, `0x175324`, `0x176fa0` |
| `CGContextSetGrayFillColor` | 2 | 2 | `0x122a54`, `0x122e74` |
| `CGContextSetInterpolationQuality` | 1 | 1 | `0x1bbce0` |
| `CGContextSetLineWidth` | 9 | 9 | `0x6a8c8`, `0x21d108`, `0x23664c` |
| `CGContextSetRGBFillColor` | 7 | 6 | `0x1884c`, `0x52fa0`, `0x122a54` |
| `CGContextSetRGBStrokeColor` | 5 | 5 | `0x23664c`, `0x2d10a8`, `0x2d3a88` |
| `CGContextSetShadow` | 1 | 1 | `0x52fa0` |
| `CGContextSetShadowWithColor` | 1 | 1 | `0x122a54` |
| `CGContextSetStrokeColor` | 3 | 3 | `0x6a8c8`, `0x21d108`, `0x3088c8` |
| `CGContextSetStrokeColorSpace` | 3 | 3 | `0x6a8c8`, `0x21d108`, `0x3088c8` |
| `CGContextSetStrokeColorWithColor` | 1 | 1 | `0x315698` |
| `CGContextSetTextDrawingMode` | 1 | 1 | `0x52fa0` |
| `CGContextSetTextMatrix` | 1 | 1 | `0x52fa0` |
| `CGContextShowTextAtPoint` | 1 | 1 | `0x52fa0` |
| `CGContextStrokeLineSegments` | 12 | 3 | `0x6a8c8`, `0x21d108`, `0x3088c8` |
| `CGContextStrokePath` | 6 | 6 | `0x23664c`, `0x2d10a8`, `0x2d3a88` |
| `CGContextTranslateCTM` | 9 | 6 | `0x6a66c`, `0x8327c`, `0x122a54` |
| `CGGradientCreateWithColorComponents` | 2 | 2 | `0x236c58`, `0x2d10a8` |
| `CGGradientRelease` | 2 | 2 | `0x236c58`, `0x2d10a8` |
| `CGImageCreateWithImageInRect` | 2 | 2 | `0x18970`, `0x1bbf1c` |
| `CGImageGetBitmapInfo` | 1 | 1 | `0x1bbce0` |
| `CGImageGetBitsPerComponent` | 2 | 1 | `0x1bbce0` |
| `CGImageGetBitsPerPixel` | 1 | 1 | `0x1bbce0` |
| `CGImageGetColorSpace` | 1 | 1 | `0x1bbce0` |
| `CGImageGetHeight` | 5 | 4 | `0x1071f8`, `0x1265cc`, `0x1267b8` |
| `CGImageGetWidth` | 5 | 4 | `0x1071f8`, `0x1265cc`, `0x1267b8` |
| `CGImageRelease` | 21 | 21 | `0x17ea8`, `0x1851c`, `0x1884c` |
| `CGLayerCreateWithContext` | 1 | 1 | `0x2d10a8` |
| `CGLayerGetContext` | 1 | 1 | `0x2d10a8` |
| `CGLayerRelease` | 2 | 2 | `0x2d0ae0`, `0x2d10a8` |
| `CGPathAddCurveToPoint` | 2 | 1 | `0x203e4` |
| `CGPathAddLineToPoint` | 14 | 7 | `0x1a1d0`, `0x1a608`, `0x1a9c8` |
| `CGPathCreateMutable` | 7 | 7 | `0x1a1d0`, `0x1a608`, `0x1a9c8` |
| `CGPathMoveToPoint` | 8 | 8 | `0x1a1d0`, `0x1a608`, `0x1a9c8` |
| `CGPathRelease` | 8 | 8 | `0x1a1d0`, `0x1a608`, `0x1a9c8` |
| `CGRectApplyAffineTransform` | 1 | 1 | `0x175750` |
| `CGRectContainsPoint` | 9 | 8 | `0x66a88`, `0x6f25c`, `0x73910` |
| `CGRectEqualToRect` | 2 | 2 | `0x239488`, `0x2cea10` |
| `CGRectFromString` | 2 | 2 | `0x175658`, `0x1b8a00` |
| `CGRectGetHeight` | 10 | 9 | `0x6a66c`, `0x21cce8`, `0x2361ec` |
| `CGRectGetMaxX` | 2 | 1 | `0x2361ec` |
| `CGRectGetMidX` | 1 | 1 | `0x29eb34` |
| `CGRectGetMidY` | 1 | 1 | `0x29eb34` |
| `CGRectGetMinX` | 9 | 6 | `0x6a66c`, `0x21cce8`, `0x2361ec` |
| `CGRectGetMinY` | 7 | 4 | `0x6a66c`, `0x21cce8`, `0x238428` |
| `CGRectGetWidth` | 7 | 7 | `0x6a66c`, `0x21cce8`, `0x238428` |
| `CGRectInset` | 9 | 9 | `0x6a66c`, `0x6bf6c`, `0x6c070` |
| `CGRectIntegral` | 1 | 1 | `0x1bbce0` |
| `CGRectIntersection` | 1 | 1 | `0x336624` |
| `CGRectIsNull` | 1 | 1 | `0x336624` |
| `CGRectOffset` | 20 | 8 | `0x6a66c`, `0x6b970`, `0x21cce8` |
| `CGRectUnion` | 1 | 1 | `0x2d0b70` |
| `class_addMethod` | 3 | 2 | `0x26184c`, `0x36a534` |
| `class_copyMethodList` | 5 | 4 | `0x26184c`, `0x2bcc9c`, `0x2c29b0` |
| `class_copyPropertyList` | 1 | 1 | `0x250db0` |
| `class_getClassMethod` | 1 | 1 | `0x336c08` |
| `class_getInstanceMethod` | 3 | 3 | `0x26184c`, `0x336c4c`, `0x36a534` |
| `class_getInstanceSize` | 1 | 1 | `0x36a714` |
| `class_getInstanceVariable` | 2 | 2 | `0x36a624`, `0x36a660` |
| `class_getIvarLayout` | 2 | 2 | `0x36a660`, `0x36a714` |
| `class_getName` | 2 | 2 | `0x97628`, `0x32327c` |
| `class_getProperty` | 1 | 1 | `0x336be4` |
| `class_getSuperclass` | 3 | 2 | `0x36a660`, `0x36a714` |
| `cosf` | 17 | 16 | `0x7ead0`, `0x95dd8`, `0x9dd78` |
| `crc32` | 7 | 6 | `0x605f8`, `0x7175c`, `0x153ec8` |
| `cxa_atexit` | 68 | 53 | `0x25df0`, `0x25ef8`, `0x26000` |
| `cxa_begin_catch` | 1 | 1 | `0xfe59c` |
| `cxa_guard_acquire` | 5 | 5 | `0x25df0`, `0x25ef8`, `0x91318` |
| `cxa_guard_release` | 5 | 5 | `0x25df0`, `0x25ef8`, `0x91318` |
| `cxa_rethrow` | 1 | 1 | `0xfe59c` |
| `deflate` | 7 | 7 | `0x7175c`, `0x71920`, `0x153ec8` |
| `deflateEnd` | 4 | 4 | `0x71920`, `0x1540e0`, `0x213d90` |
| `deflateInit2_` | 4 | 4 | `0x70f90`, `0x1534dc`, `0x213c78` |
| `dispatch_async` | 6 | 6 | `0x7bac`, `0x155e0`, `0x12dc28` |
| `dispatch_get_global_queue` | 1 | 1 | `0x7bac` |
| `divsi3` | 24 | 14 | `0x8f76c`, `0x9a564`, `0xa7a58` |
| `dladdr` | 2 | 2 | `0x32327c`, `0x369090` |
| `dyld_get_image_header` | 1 | 1 | `0x336ca4` |
| `dyld_get_image_name` | 1 | 1 | `0x336ca4` |
| `dyld_image_count` | 1 | 1 | `0x336ca4` |
| `dyld_register_func_for_add_image` | 1 | 1 | `0x36a290` |
| `error` | 4 | 2 | `0xb370`, `0x103f0` |
| `exit` | 1 | 1 | `0x38a0` |
| `exp` | 2 | 1 | `0x1254c8` |
| `fabs` | 1 | 1 | `0x20a6b8` |
| `fclose` | 10 | 10 | `0x53d7c`, `0x71e50`, `0x74ab4` |
| `ferror` | 4 | 4 | `0x71e5c`, `0x1548cc`, `0x2331ac` |
| `fixdfdi` | 14 | 11 | `0x1bd128`, `0x285a20`, `0x28612c` |
| `fixunsdfdi` | 2 | 1 | `0x1220a8` |
| `floatdidf` | 7 | 7 | `0x18c40`, `0x1e1dc8`, `0x28576c` |
| `floatundidf` | 5 | 3 | `0x1220a8`, `0x1d4698`, `0x2e5c94` |
| `floor` | 7 | 5 | `0x40588`, `0x21d450`, `0x220120` |
| `floorf` | 28 | 10 | `0x4eed4`, `0x5da18`, `0x6aab4` |
| `fmaxf` | 1 | 1 | `0x18f410` |
| `fopen` | 12 | 12 | `0x53d7c`, `0x71db4`, `0x74ab4` |
| `fread` | 5 | 5 | `0x71e00`, `0x11f094`, `0x1547c8` |
| `free` | 136 | 88 | `0x11a14`, `0x17ea8`, `0x384d4` |
| `freeaddrinfo` | 3 | 3 | `0x9570`, `0xec0c`, `0xefc0` |
| `freeifaddrs` | 1 | 1 | `0x2e6060` |
| `fseek` | 5 | 4 | `0x11f094`, `0x154840`, `0x233170` |
| `ftell` | 5 | 5 | `0x71e28`, `0x11f094`, `0x154828` |
| `fwrite` | 12 | 12 | `0x18774`, `0x49228`, `0x52ec8` |
| `gai_strerror` | 6 | 5 | `0x9570`, `0xb89c`, `0xf42c` |
| `get_crc_table` | 4 | 4 | `0x60134`, `0x70f90`, `0x233a18` |
| `getaddrinfo` | 3 | 3 | `0x9570`, `0xec0c`, `0xefc0` |
| `getifaddrs` | 1 | 1 | `0x2e6060` |
| `getpeername` | 4 | 2 | `0x10954`, `0x10a18` |
| `getsockname` | 4 | 2 | `0x1080c`, `0x108d0` |
| `gettimeofday` | 1 | 1 | `0x122070` |
| `if_indextoname` | 1 | 1 | `0x10b58` |
| `if_nametoindex` | 1 | 1 | `0x13f50c` |
| `inet_aton` | 1 | 1 | `0x2c1a08` |
| `inet_ntop` | 3 | 3 | `0xbce8`, `0xf2cc`, `0xf354` |
| `inflate` | 4 | 4 | `0x17070`, `0x17310`, `0x155e54` |
| `inflateEnd` | 7 | 7 | `0x17070`, `0x17310`, `0x5fbac` |
| `inflateInit2_` | 5 | 5 | `0x17310`, `0x60134`, `0x1558f8` |
| `inflateInit_` | 1 | 1 | `0x17070` |
| `ioctl` | 1 | 1 | `0x10b58` |
| `ivar_getName` | 1 | 1 | `0x36a660` |
| `ivar_getOffset` | 1 | 1 | `0x36a660` |
| `ldexpf` | 2 | 2 | `0xe16bc`, `0xe17c0` |
| `mach_absolute_time` | 2 | 2 | `0x1220a8`, `0x31bb70` |
| `mach_thread_self` | 1 | 1 | `0x274fe4` |
| `mach_timebase_info` | 2 | 2 | `0x1220a8`, `0x31bb70` |
| `mach_wait_until` | 1 | 1 | `0x1220a8` |
| `malloc` | 74 | 59 | `0x11a14`, `0x17ea8`, `0x18774` |
| `maskrune` | 22 | 12 | `0x61cc0`, `0x61f94`, `0x62194` |
| `memchr` | 1 | 1 | `0x1693c` |
| `memcmp` | 6 | 6 | `0x8238`, `0x83b8`, `0xcb64` |
| `memcpy` | 57 | 29 | `0xcab4`, `0x54740`, `0x54efc` |
| `memcpy_chk` | 9 | 6 | `0x22aca4`, `0x22aeb8`, `0x22e03c` |
| `memmove` | 382 | 180 | `0x3c718`, `0x3d400`, `0x3d800` |
| `memmove_chk` | 4 | 4 | `0x22a0e4`, `0x22a25c`, `0x288f34` |
| `memset` | 576 | 307 | `0x17ea8`, `0x53d7c`, `0x716b4` |
| `memset_chk` | 7 | 4 | `0x22aca4`, `0x22ae94`, `0x22e1b0` |
| `method_getImplementation` | 4 | 3 | `0x26184c`, `0x32327c`, `0x336c90` |
| `method_getName` | 3 | 3 | `0x2bcc9c`, `0x2c29b0`, `0x32327c` |
| `method_getTypeEncoding` | 1 | 1 | `0x26184c` |
| `method_setImplementation` | 1 | 1 | `0x26184c` |
| `modsi3` | 30 | 18 | `0x1cd4c`, `0x1f470`, `0x1f668` |
| `NSAllocateObject` | 1 | 1 | `0x31fecc` |
| `NSClassFromString` | 191 | 134 | `0x15854`, `0x284f8`, `0x35458` |
| `NSDefaultMallocZone` | 1 | 1 | `0x151394` |
| `NSHomeDirectory` | 2 | 2 | `0x129214`, `0x129270` |
| `NSIntersectionRange` | 2 | 1 | `0x54870` |
| `NSLog` | 1140 | 635 | `0x3afe`, `0x7180`, `0x73d0` |
| `NSLogv` | 2 | 2 | `0x129688`, `0x274fe4` |
| `NSSearchPathForDirectoriesInDomains` | 48 | 48 | `0x1803c`, `0x18350`, `0x1ace0` |
| `NSSelectorFromString` | 7 | 6 | `0x33980`, `0x23960c`, `0x239a10` |
| `NSStringFromCGRect` | 2 | 2 | `0x17409c`, `0x186488` |
| `NSStringFromCGSize` | 1 | 1 | `0x29c5a4` |
| `NSStringFromClass` | 51 | 49 | `0x14ffa4`, `0x24b370`, `0x24b40c` |
| `NSStringFromSelector` | 8 | 7 | `0x24b370`, `0x24b40c`, `0x267240` |
| `NSTemporaryDirectory` | 10 | 6 | `0x18aec`, `0x18c70`, `0x18f44` |
| `objc_begin_catch` | 25 | 22 | `0x19505c`, `0x197624`, `0x1a1da8` |
| `objc_copyStruct` | 45 | 45 | `0x1e4764`, `0x1e47a4`, `0x1e48e8` |
| `objc_end_catch` | 47 | 22 | `0x19505c`, `0x197624`, `0x1a1da8` |
| `objc_enumerationMutation` | 440 | 337 | `0x4c84`, `0x4e02`, `0x5402` |
| `objc_exception_rethrow` | 3 | 3 | `0x1a1da8`, `0x1a246c`, `0x1cda28` |
| `objc_exception_throw` | 25 | 24 | `0x4c84`, `0x4e02`, `0x5244` |
| `objc_getClassList` | 2 | 1 | `0x32327c` |
| `objc_getProperty` | 189 | 189 | `0x30f0c`, `0x32514`, `0x32550` |
| `objc_lookUpClass` | 10 | 10 | `0x3352c8`, `0x335590`, `0x33571c` |
| `objc_msgSend` | 60475 | 7265 | `0x38ae`, `0x38ea`, `0x3902` |
| `objc_msgSend_stret` | 1240 | 486 | `0x12b04`, `0x12fa8`, `0x13080` |
| `objc_msgSendSuper2` | 988 | 980 | `0x41e8`, `0x4460`, `0x446a` |
| `objc_msgSendSuper2_stret` | 2 | 2 | `0x23758c`, `0x2a5010` |
| `objc_setProperty` | 1103 | 1103 | `0x6398`, `0x6402`, `0x6430` |
| `objc_sync_enter` | 150 | 150 | `0x65c4`, `0x9570`, `0xec0c` |
| `objc_sync_exit` | 209 | 151 | `0x65c4`, `0x9570`, `0xec0c` |
| `object_getClass` | 5 | 4 | `0x32327c`, `0x36a624`, `0x36a660` |
| `object_getClassName` | 3 | 1 | `0x1b4b7c` |
| `opendir` | 1 | 1 | `0x11e4dc` |
| `OSAtomicAdd32` | 2 | 2 | `0x23d9f4`, `0x23ddac` |
| `perror` | 1 | 1 | `0x2e5b78` |
| `printf` | 21 | 12 | `0xffd90`, `0x10000e`, `0x10010e` |
| `property_getAttributes` | 1 | 1 | `0x250db0` |
| `property_getName` | 1 | 1 | `0x250db0` |
| `protocol_getMethodDescription` | 2 | 2 | `0x36a514`, `0x36a5d4` |
| `pthread_cond_destroy` | 1 | 1 | `0x82f20` |
| `pthread_cond_init` | 1 | 1 | `0x82d6c` |
| `pthread_cond_signal` | 2 | 2 | `0x83028`, `0x10d9e4` |
| `pthread_cond_wait` | 2 | 2 | `0x83048`, `0x10da28` |
| `pthread_create` | 1 | 1 | `0x126c08` |
| `pthread_join` | 3 | 3 | `0x126afc`, `0x126bb4`, `0x126bdc` |
| `pthread_mutex_destroy` | 2 | 1 | `0x82f20` |
| `pthread_mutex_init` | 3 | 2 | `0x82d6c`, `0x10d7f4` |
| `pthread_mutex_lock` | 9 | 7 | `0x83008`, `0x83028`, `0x83048` |
| `pthread_mutex_unlock` | 9 | 7 | `0x83008`, `0x83028`, `0x83048` |
| `puts` | 17 | 16 | `0x11ec38`, `0x2d34c0`, `0x2dfd7c` |
| `qsort` | 1 | 1 | `0x336ca4` |
| `rand` | 115 | 57 | `0x19960`, `0x19e90`, `0x1cd4c` |
| `random` | 11 | 8 | `0x8d360`, `0xdee98`, `0xe1268` |
| `readdir` | 1 | 1 | `0x11e4dc` |
| `realloc` | 7 | 3 | `0x11a14`, `0x123680`, `0x337fcc` |
| `recvfrom` | 2 | 1 | `0x11a14` |
| `round` | 1 | 1 | `0x43d30` |
| `roundf` | 23 | 11 | `0x29c00`, `0x43b24`, `0x47f00` |
| `SCNetworkReachabilityCreateWithAddress` | 7 | 7 | `0x50160`, `0x1298c4`, `0x1e93cc` |
| `SCNetworkReachabilityCreateWithName` | 8 | 8 | `0x500e0`, `0x129844`, `0x1e9280` |
| `SCNetworkReachabilityGetFlags` | 29 | 29 | `0x5036c`, `0x50398`, `0x129ad0` |
| `SCNetworkReachabilityScheduleWithRunLoop` | 6 | 6 | `0x4ff74`, `0x1296d8`, `0x1e8edc` |
| `SCNetworkReachabilitySetCallback` | 6 | 6 | `0x4ff74`, `0x1296d8`, `0x1e8edc` |
| `SCNetworkReachabilityUnscheduleFromRunLoop` | 4 | 4 | `0x1e91c8`, `0x2c1958`, `0x2d2044` |
| `SecItemAdd` | 1 | 1 | `0x14f034` |
| `SecItemCopyMatching` | 1 | 1 | `0x14eeec` |
| `SecItemDelete` | 1 | 1 | `0x14f280` |
| `SecItemUpdate` | 1 | 1 | `0x14f12c` |
| `sel_getName` | 2 | 2 | `0x26184c`, `0x32327c` |
| `sel_registerName` | 22 | 11 | `0x238ff0`, `0x26184c`, `0x3352c8` |
| `send` | 1 | 1 | `0x112a0` |
| `sendto` | 1 | 1 | `0x112a0` |
| `setsockopt` | 7 | 4 | `0x9570`, `0xf42c`, `0xfaf0` |
| `setxattr` | 1 | 1 | `0x3265c` |
| `sinf` | 20 | 16 | `0x76788`, `0x7bfa8`, `0x84f28` |
| `snprintf` | 12 | 3 | `0xacba8`, `0xadcfc`, `0xd6184` |
| `sprintf` | 72 | 18 | `0x76eb4`, `0x7a62c`, `0x92644` |
| `sqlite3_close` | 3 | 3 | `0x2bb074`, `0x2bb174`, `0x2bb4cc` |
| `sqlite3_column_blob` | 1 | 1 | `0x2bb304` |
| `sqlite3_column_bytes` | 1 | 1 | `0x2bb304` |
| `sqlite3_column_double` | 1 | 1 | `0x2bb304` |
| `sqlite3_column_int64` | 1 | 1 | `0x2bb304` |
| `sqlite3_column_text` | 1 | 1 | `0x2bb304` |
| `sqlite3_column_type` | 1 | 1 | `0x2bb304` |
| `sqlite3_data_count` | 1 | 1 | `0x2bb304` |
| `sqlite3_errmsg` | 1 | 1 | `0x2bb4cc` |
| `sqlite3_finalize` | 1 | 1 | `0x2bb304` |
| `sqlite3_open` | 2 | 2 | `0x2bafc4`, `0x2bb074` |
| `sqlite3_prepare_v2` | 1 | 1 | `0x2bb304` |
| `sqlite3_step` | 1 | 1 | `0x2bb304` |
| `srand` | 8 | 8 | `0x1cd4c`, `0x70f90`, `0xf5f3c` |
| `srandom` | 5 | 5 | `0x8d360`, `0x224de0`, `0x2771d8` |
| `srandomdev` | 1 | 1 | `0x2437dc` |
| `sscanf` | 17 | 4 | `0x10000e`, `0x10010e`, `0x10050e` |
| `stack_chk_fail` | 136 | 133 | `0x148c88`, `0x14be50`, `0x14ffa4` |
| `stat` | 2 | 2 | `0x31a624`, `0x336e8c` |
| `statfs` | 1 | 1 | `0x2e5c94` |
| `strchr` | 2 | 1 | `0x12a300` |
| `strcmp` | 9 | 7 | `0x5f74c`, `0x92644`, `0x11f030` |
| `strcpy` | 2 | 2 | `0xb06cc`, `0xb092c` |
| `strcspn` | 5 | 5 | `0x6246c`, `0x19d2f4`, `0x24de4c` |
| `strerror` | 2 | 2 | `0xb370`, `0x103f0` |
| `strlen` | 257 | 101 | `0x52fa0`, `0x5a4f8`, `0x5ffb0` |
| `strncasecmp` | 4 | 3 | `0x1230d0`, `0x123250`, `0x123bdc` |
| `strncmp` | 18 | 18 | `0x61e18`, `0x61e98`, `0x61f18` |
| `strnstr` | 1 | 1 | `0x263e28` |
| `strptime` | 1 | 1 | `0x58664` |
| `strstr` | 13 | 9 | `0x5bbd4`, `0x78374`, `0x7a150` |
| `strtoull` | 1 | 1 | `0x1d572c` |
| `sysctl` | 2 | 2 | `0x129158`, `0x2e5b78` |
| `sysctlbyname` | 14 | 9 | `0x129080`, `0x24bc98`, `0x2b0194` |
| `time` | 24 | 23 | `0x1cd4c`, `0x539cc`, `0x70f90` |
| `timegm` | 1 | 1 | `0x58664` |
| `tolower` | 1 | 1 | `0x1202b0` |
| `udivsi3` | 10 | 7 | `0x31230`, `0x1bbce0`, `0x1e19c4` |
| `UIApplicationMain` | 1 | 1 | `0x3902` |
| `UIGraphicsBeginImageContext` | 3 | 3 | `0x16ca10`, `0x2393ec`, `0x326748` |
| `UIGraphicsEndImageContext` | 3 | 3 | `0x16ca10`, `0x2393ec`, `0x326748` |
| `UIGraphicsGetCurrentContext` | 24 | 18 | `0x6a828`, `0x6a8c8`, `0x175324` |
| `UIGraphicsGetImageFromCurrentImageContext` | 3 | 3 | `0x16ca10`, `0x2393ec`, `0x326748` |
| `UIGraphicsPopContext` | 4 | 4 | `0x8327c`, `0x122a54`, `0x122e74` |
| `UIGraphicsPushContext` | 4 | 4 | `0x8327c`, `0x122a54`, `0x122e74` |
| `UIImageJPEGRepresentation` | 4 | 4 | `0x17eca4`, `0x1b25c4`, `0x1fd9bc` |
| `UIImagePNGRepresentation` | 6 | 6 | `0x47e24`, `0x6d424`, `0x16cbfc` |
| `UIRectFillUsingBlendMode` | 1 | 1 | `0x8327c` |
| `umodsi3` | 30 | 18 | `0x1f39c`, `0x1f8a4`, `0x24c48` |
| `uname` | 3 | 3 | `0x160014`, `0x1a2d58`, `0x321398` |
| `Unwind_SjLj_Register` | 696 | 696 | `0x65c4`, `0x9570`, `0xec0c` |
| `Unwind_SjLj_Resume` | 73 | 73 | `0xe000c`, `0x120010`, `0x120510` |
| `Unwind_SjLj_Unregister` | 708 | 704 | `0x65c4`, `0x9570`, `0xec0c` |
| `utimes` | 1 | 1 | `0x31ad7c` |
| `UTTypeCopyPreferredTagWithClass` | 1 | 1 | `0x1e14e4` |
| `UTTypeCreatePreferredIdentifierForTag` | 1 | 1 | `0x1e14e4` |
| `xmlReaderForMemory` | 1 | 1 | `0x582cc` |
| `xmlStrEqual` | 22 | 8 | `0x587ec`, `0x58a10`, `0x58ba4` |
| `xmlTextReaderConstName` | 16 | 8 | `0x587ec`, `0x58a10`, `0x58ba4` |
| `xmlTextReaderIsEmptyElement` | 5 | 5 | `0x58520`, `0x587ec`, `0x58a10` |
| `xmlTextReaderNodeType` | 18 | 9 | `0x58520`, `0x587ec`, `0x58a10` |
| `xmlTextReaderRead` | 17 | 9 | `0x58520`, `0x587ec`, `0x58a10` |
| `xmlTextReaderValue` | 1 | 1 | `0x58520` |
| `ZdaPv` | 14 | 8 | `0x95d20`, `0xdd64c`, `0xe091c` |
| `ZdlPv` | 654 | 332 | `0x3c718`, `0x3d400`, `0x3d800` |
| `ZdlPvRKSt9nothrow_t` | 1 | 1 | `0x97628` |
| `Znam` | 15 | 9 | `0x95d60`, `0xdd1e4`, `0xe0518` |
| `ZNKSs4findEPKcmm` | 16 | 6 | `0xcf274`, `0x12184c`, `0x121994` |
| `ZNKSs5rfindEPKcmm` | 5 | 2 | `0x121a44`, `0x121a84` |
| `ZNKSs6substrEmm` | 8 | 4 | `0xbf8f0`, `0xc3db4`, `0xcf274` |
| `ZNKSs7compareEPKc` | 12 | 8 | `0x8a634`, `0x8c038`, `0x8d360` |
| `ZNKSs7compareERKSs` | 178 | 98 | `0x771b4`, `0x77294`, `0x773b0` |
| `ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv` | 43 | 21 | `0x84f28`, `0x8621c`, `0x87430` |
| `ZNSolsEi` | 45 | 21 | `0x83d6c`, `0x84f28`, `0x87430` |
| `ZNSs12_M_leak_hardEv` | 3 | 2 | `0x11e878`, `0x1202b0` |
| `ZNSs5eraseEmm` | 6 | 5 | `0x12184c`, `0x121994`, `0x121a84` |
| `ZNSs6appendEPKcm` | 114 | 39 | `0x84f28`, `0xa1d88`, `0xa3df4` |
| `ZNSs6appendERKSs` | 9 | 9 | `0x8ab80`, `0xa3df4`, `0xb12e8` |
| `ZNSs6assignEPKcm` | 141 | 44 | `0x3c3bc`, `0x8312c`, `0x83d6c` |
| `ZNSs6assignERKSs` | 64 | 35 | `0x84f28`, `0x8a634`, `0x9484c` |
| `ZNSs7reserveEm` | 1 | 1 | `0xa3df4` |
| `ZNSs9_M_mutateEmmm` | 1 | 1 | `0xf6eb0` |
| `ZNSsC1EPKcRKSaIcE` | 1032 | 122 | `0x1cd4c`, `0x75720`, `0x75c94` |
| `ZNSsC1ERKSs` | 152 | 82 | `0x76af4`, `0x771b4`, `0x77294` |
| `ZNSsD2Ev` | 1360 | 214 | `0x1cd4c`, `0x75720`, `0x75c94` |
| `ZNSt15_List_node_base4hookEPS_` | 4 | 4 | `0x9d528`, `0x9dcf0`, `0x120dac` |
| `ZNSt15_List_node_base6unhookEv` | 4 | 4 | `0x9c16c`, `0x9d644`, `0x9d760` |
| `ZNSt15basic_stringbufIcSt11char_traitsIcESaIcEE17_M_stringbuf_initESt13_Ios_Openmode` | 1 | 1 | `0xaa430` |
| `ZNSt18basic_stringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode` | 41 | 21 | `0x83d6c`, `0x84f28`, `0x87430` |
| `ZNSt18basic_stringstreamIcSt11char_traitsIcESaIcEED1Ev` | 39 | 21 | `0x84ae8`, `0x84f28`, `0x87430` |
| `ZNSt8ios_base4InitC1Ev` | 22 | 22 | `0x26000`, `0x35b98`, `0x37bac` |
| `ZNSt9basic_iosIcSt11char_traitsIcEE5clearESt12_Ios_Iostate` | 1 | 1 | `0x94cac` |
| `Znwm` | 995 | 246 | `0x3ff98`, `0x3ffdc`, `0x75720` |
| `ZnwmRKSt9nothrow_t` | 1 | 1 | `0x9a9a8` |
| `ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_i` | 45 | 12 | `0x83d6c`, `0x84f28`, `0x92644` |
| `ZSt17__throw_bad_allocv` | 79 | 79 | `0x3ff98`, `0x3ffdc`, `0x77ab4` |
| `ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base` | 40 | 40 | `0x77400`, `0x7753c`, `0x77700` |
| `ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base` | 6 | 5 | `0x75720`, `0x75c94`, `0x78374` |
| `ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base` | 94 | 72 | `0x76160`, `0x76884`, `0x76eb4` |
| `ZSt20__throw_length_errorPKc` | 77 | 77 | `0x3feb4`, `0x779d0`, `0x78090` |
| `ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_` | 5 | 5 | `0x77388`, `0x99d74`, `0xa1ca4` |
| `ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_` | 20 | 20 | `0x774ec`, `0x777ec`, `0x7fe2c` |
| `ZSt9terminatev` | 2 | 2 | `0xfe59c`, `0x120010` |
