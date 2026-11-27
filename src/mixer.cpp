#include "mixer.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD      // Pentium III: no SSE2
#include "../third_party/dr_mp3.h"

// The game mixes everything itself in this format; SDL converts it to the
// device's if they differ (on the Xbox DirectSound takes it as is).
#define MIX_FREQ         48000
#define MIX_CHANNELS_OUT 2
// Most bytes mixed per step of the audio callback.
#define MIX_STEP_BYTES   4096

struct Mix_Music
{
    Uint8*           fileData;
    size_t           fileSize;
    drmp3            mp3;
    SDL_AudioStream* stream;   // MP3 rate/channels -> mix format
};

namespace {

struct Channel
{
    Mix_Chunk* chunk;
    Uint32     pos;      // byte offset
    int        loops;    // -1 = forever
};

const SDL_AudioSpec g_spec = { SDL_AUDIO_S16, MIX_CHANNELS_OUT, MIX_FREQ };

// The device stream. SDL runs its callback with the stream locked, so
// locking it is also what guards the channel and music state below.
SDL_AudioStream*  g_out = NULL;
Channel           g_channels[MIX_CHANNELS];

Mix_Music*        g_music = NULL;
int               g_musicLoops = 0;
bool              g_musicPaused = false;

const int         MIX_STEP_SAMPLES = MIX_STEP_BYTES / (int)sizeof(Sint16);
Sint32            g_accum[MIX_STEP_SAMPLES];
Sint16            g_musicBuf[MIX_STEP_SAMPLES];
Sint16            g_mixBuf[MIX_STEP_SAMPLES];

const int MP3_DECODE_FRAMES = 1152;
Sint16            g_decodeBuf[MP3_DECODE_FRAMES * 2];

void lock()   { if (g_out) SDL_LockAudioStream(g_out); }
void unlock() { if (g_out) SDL_UnlockAudioStream(g_out); }

// Pull `samples` S16 samples (interleaved) of music into out. Called locked.
void readMusic(Sint16* out, int samples)
{
    Mix_Music* m = g_music;
    int bytesWanted = samples * (int)sizeof(Sint16);

    while (SDL_GetAudioStreamAvailable(m->stream) < bytesWanted)
    {
        drmp3_uint64 got = drmp3_read_pcm_frames_s16(&m->mp3, MP3_DECODE_FRAMES, g_decodeBuf);
        if (got == 0)
        {
            if (g_musicLoops == 0) { SDL_FlushAudioStream(m->stream); break; }
            if (g_musicLoops > 0) g_musicLoops--;
            drmp3_seek_to_pcm_frame(&m->mp3, 0);
            continue;
        }
        SDL_PutAudioStreamData(m->stream, g_decodeBuf, (int)(got * m->mp3.channels * sizeof(Sint16)));
    }

    int got = SDL_GetAudioStreamData(m->stream, out, bytesWanted);
    if (got < 0) got = 0;
    if (got < bytesWanted)
    {
        memset((Uint8*)out + got, 0, bytesWanted - got);
        if (g_musicLoops == 0 && SDL_GetAudioStreamAvailable(m->stream) == 0)
            g_music = NULL;                 // finished
    }
}

// Mix `samples` (<= MIX_STEP_SAMPLES) interleaved samples into out.
void mix(Sint16* out, int samples)
{
    memset(g_accum, 0, samples * sizeof(Sint32));

    if (g_music && !g_musicPaused)
    {
        readMusic(g_musicBuf, samples);
        for (int i = 0; i < samples; i++) g_accum[i] += g_musicBuf[i];
    }

    for (int c = 0; c < MIX_CHANNELS; c++)
    {
        Channel& ch = g_channels[c];
        if (!ch.chunk) continue;
        const int vol = ch.chunk->volume;
        int i = 0;
        while (i < samples && ch.chunk)
        {
            const Sint16* src = (const Sint16*)(ch.chunk->abuf + ch.pos);
            int avail = (int)((ch.chunk->alen - ch.pos) / sizeof(Sint16));
            int n = samples - i < avail ? samples - i : avail;
            for (int k = 0; k < n; k++) g_accum[i + k] += (src[k] * vol) >> 7;
            i += n;
            ch.pos += n * sizeof(Sint16);
            if (ch.pos >= ch.chunk->alen)
            {
                if (ch.loops == 0) ch.chunk = NULL;
                else { if (ch.loops > 0) ch.loops--; ch.pos = 0; }
            }
        }
    }

    for (int i = 0; i < samples; i++)
    {
        Sint32 v = g_accum[i];
        out[i] = (Sint16)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
}

// SDL asks for `additional` more bytes of mixed audio.
void SDLCALL audioCallback(void*, SDL_AudioStream* stream, int additional, int)
{
    while (additional > 0)
    {
        int bytes = additional < MIX_STEP_BYTES ? additional : MIX_STEP_BYTES;
        bytes &= ~3;                       // whole stereo frames
        if (bytes <= 0) bytes = 4;
        const int samples = bytes / (int)sizeof(Sint16);
        mix(g_mixBuf, samples);
        SDL_PutAudioStreamData(stream, g_mixBuf, bytes);
        additional -= bytes;
    }
}

} // namespace

int Mix_OpenAudio(int, Uint16, int, int)
{
    if (g_out) return 0;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        Platform_Log("2H4U: SDL audio init failed: %s\n", SDL_GetError());
        return -1;
    }

    memset(g_channels, 0, sizeof(g_channels));
    g_out = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &g_spec, audioCallback, NULL);
    if (!g_out)
    {
        Platform_Log("2H4U: can't open audio: %s\n", SDL_GetError());
        return -1;
    }
    SDL_ResumeAudioStreamDevice(g_out);
    return 0;
}

void Mix_CloseAudio(void)
{
    if (!g_out) return;
    SDL_DestroyAudioStream(g_out);     // also closes the device
    g_out = NULL;
    g_music = NULL;
}

Mix_Chunk* Mix_LoadWAV(const char* file)
{
    if (!g_out) return NULL;

    SDL_AudioSpec spec;
    Uint8* buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(file, &spec, &buf, &len))
    {
        Platform_Log("2H4U: can't load %s: %s\n", file, SDL_GetError());
        return NULL;
    }

    Uint8* converted = NULL;
    int convertedLen = 0;
    const bool ok = SDL_ConvertAudioSamples(&spec, buf, (int)len, &g_spec, &converted, &convertedLen);
    SDL_free(buf);
    if (!ok)
    {
        Platform_Log("2H4U: can't convert %s: %s\n", file, SDL_GetError());
        return NULL;
    }

    Mix_Chunk* chunk = (Mix_Chunk*)SDL_malloc(sizeof(Mix_Chunk));
    chunk->allocated = 1;
    chunk->abuf = converted;
    chunk->alen = (Uint32)convertedLen & ~3u;
    chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}

void Mix_FreeChunk(Mix_Chunk* chunk)
{
    if (!chunk) return;
    lock();
    for (int c = 0; c < MIX_CHANNELS; c++)
        if (g_channels[c].chunk == chunk) g_channels[c].chunk = NULL;
    unlock();
    SDL_free(chunk->abuf);
    SDL_free(chunk);
}

int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops)
{
    if (!g_out || !chunk || chunk->alen == 0) return -1;
    lock();
    if (channel < 0 || channel >= MIX_CHANNELS)
    {
        channel = -1;
        for (int c = 0; c < MIX_CHANNELS; c++)
            if (!g_channels[c].chunk) { channel = c; break; }
    }
    if (channel >= 0)
    {
        g_channels[channel].chunk = chunk;
        g_channels[channel].pos = 0;
        g_channels[channel].loops = loops;
    }
    unlock();
    return channel;
}

Mix_Music* Mix_LoadMUS(const char* file)
{
    if (!g_out) return NULL;
    Mix_Music* m = (Mix_Music*)SDL_calloc(1, sizeof(Mix_Music));
    if (!m) return NULL;
    m->fileData = (Uint8*)SDL_LoadFile(file, &m->fileSize);
    if (!m->fileData || !drmp3_init_memory(&m->mp3, m->fileData, m->fileSize, NULL))
    {
        Platform_Log("2H4U: can't load music %s\n", file);
        SDL_free(m->fileData);
        SDL_free(m);
        return NULL;
    }
    const SDL_AudioSpec src = { SDL_AUDIO_S16, (int)m->mp3.channels, (int)m->mp3.sampleRate };
    m->stream = SDL_CreateAudioStream(&src, &g_spec);
    if (!m->stream)
    {
        drmp3_uninit(&m->mp3);
        SDL_free(m->fileData);
        SDL_free(m);
        return NULL;
    }
    return m;
}

void Mix_FreeMusic(Mix_Music* music)
{
    if (!music) return;
    lock();
    if (g_music == music) g_music = NULL;
    unlock();
    SDL_DestroyAudioStream(music->stream);
    drmp3_uninit(&music->mp3);
    SDL_free(music->fileData);
    SDL_free(music);
}

int Mix_PlayMusic(Mix_Music* music, int loops)
{
    if (!g_out || !music) return -1;
    lock();
    drmp3_seek_to_pcm_frame(&music->mp3, 0);
    SDL_ClearAudioStream(music->stream);
    g_music = music;
    g_musicLoops = loops;   // -1 = forever, like SDL_mixer
    g_musicPaused = false;
    unlock();
    return 0;
}

void Mix_PauseMusic(void)  { g_musicPaused = true; }
void Mix_ResumeMusic(void) { g_musicPaused = false; }

void Mix_HaltMusic(void)
{
    if (!g_out) return;
    lock();
    g_music = NULL;
    g_musicPaused = false;
    unlock();
}

int Mix_PlayingMusic(void)
{
    return g_music != NULL ? 1 : 0;
}
