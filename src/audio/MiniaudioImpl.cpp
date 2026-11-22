// The miniaudio implementation (third_party/miniaudio, v0.11.21, public domain / MIT-0): compiled once, here.
// The engine mixes on its own audio thread - many sounds at once (the WinMM PlaySound backend before it had ONE
// channel: every new sound cut the one playing, the author heard it as "the sound breaks off").
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
