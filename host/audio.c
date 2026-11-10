/* audio.c - AudioQueue/AudioFile/AudioSession stubs (silent for now; see docs/PORT_STATUS.md) */
#include "ra.h"
static void h_ok(void) { ret(0); }
static void h_err(void) { ret((u32)-50); }
static const HleEntry audio_tab[] = {
    {"AudioSessionInitialize", h_ok}, {"AudioSessionSetActive", h_ok}, {"AudioSessionSetProperty", h_ok}, {"AudioSessionGetProperty", h_ok},
    {"AudioFileOpenURL", h_err}, {"AudioFileClose", h_ok}, {"AudioFileGetProperty", h_err}, {"AudioFileReadBytes", h_err}, {"AudioFileReadPacketData", h_err},
    {"AudioQueueNewOutput", h_err}, {"AudioQueueAllocateBuffer", h_err}, {"AudioQueueAllocateBufferWithPacketDescriptions", h_err}, {"AudioQueueDispose", h_ok},
    {"AudioQueueEnqueueBuffer", h_ok}, {"AudioQueueFlush", h_ok}, {"AudioQueuePause", h_ok}, {"AudioQueuePrime", h_ok}, {"AudioQueueSetParameter", h_ok},
    {"AudioQueueSetProperty", h_ok}, {"AudioQueueStart", h_ok}, {"AudioQueueStop", h_ok}, {"AudioQueueAddPropertyListener", h_ok},
    {"SCNetworkReachabilityCreateWithAddress", h_ok}, {"SCNetworkReachabilityGetFlags", h_err},
    {NULL, NULL}
};
void audio_init(void) { hle_register_table(audio_tab); }
