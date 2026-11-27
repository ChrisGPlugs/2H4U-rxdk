/*
    Minimal stand-in for the parts of SDL_mixer that 2H4U uses.

    There is no SDL3_mixer for RXDK, and the game only needs: WAV sound
    effects played on free channels, and one looping MP3 music track.
    Everything is mixed in 48 kHz stereo S16 and fed to an SDL3 audio
    stream (DirectSound on the Xbox).
    MP3 decoding uses dr_mp3 (public domain / MIT-0, third_party/dr_mp3.h).
*/
#ifndef MIXER_H
#define MIXER_H

#include <SDL3/SDL.h>

#define MIX_DEFAULT_FORMAT SDL_AUDIO_S16LE
#define MIX_MAX_VOLUME     128
#define MIX_CHANNELS       8

typedef struct Mix_Chunk
{
    int     allocated;
    Uint8*  abuf;     // S16 stereo samples in the device format
    Uint32  alen;     // length in bytes
    Uint8   volume;   // 0..128
} Mix_Chunk;

typedef struct Mix_Music Mix_Music;

int        Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void       Mix_CloseAudio(void);

Mix_Chunk* Mix_LoadWAV(const char* file);
void       Mix_FreeChunk(Mix_Chunk* chunk);
int        Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops);

Mix_Music* Mix_LoadMUS(const char* file);
void       Mix_FreeMusic(Mix_Music* music);
int        Mix_PlayMusic(Mix_Music* music, int loops);
void       Mix_PauseMusic(void);
void       Mix_ResumeMusic(void);
void       Mix_HaltMusic(void);
int        Mix_PlayingMusic(void);

#endif
