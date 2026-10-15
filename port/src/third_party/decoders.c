/* the decoders for the CD's songs as files (cdaudio.c): Ogg Vorbis (stb_vorbis 1.22, Sean
   Barrett), MP3 (dr_mp3 0.7.4) and WAV (dr_wav 0.14.6, David Reid) -- each public domain, or
   MIT / MIT-0, as their own texts at their ends say; built here once */
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO_WIDE
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "stb_vorbis.c"
