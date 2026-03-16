#pragma once
/*
  backend/audio_out.hpp
  Platform-specific AudioOut struct.
  Supports: ALSA (Linux), CoreAudio (macOS), WinMM (Windows).
*/

#include "ring.hpp"

// ═══════════════════════════════════════════════════════════════════════════
//  ALSA
// ═══════════════════════════════════════════════════════════════════════════
#ifdef CMUSPP_ALSA
struct AudioOut {
    snd_pcm_t*         pcm = nullptr;
    Ring*              ring = nullptr;
    std::atomic<bool>  running{false};
    std::atomic<bool>  paused{false};
    std::atomic<float> gain{1.0f};
    std::thread        thr;

    bool open() {
        const char* devs[] = {"default", "hw:0,0", "plughw:0,0", nullptr};
        for (int i = 0; devs[i]; ++i)
            if (snd_pcm_open(&pcm, devs[i], SND_PCM_STREAM_PLAYBACK, 0) >= 0) break;
        if (!pcm) return false;
        snd_pcm_hw_params_t* hw;
        snd_pcm_hw_params_alloca(&hw);
        snd_pcm_hw_params_any(pcm, hw);
        snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
        unsigned rate = OUT_RATE;
        snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, nullptr);
        snd_pcm_hw_params_set_channels(pcm, hw, OUT_CH);
        snd_pcm_uframes_t bs = 8192;
        snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &bs);
        return snd_pcm_hw_params(pcm, hw) >= 0;
    }

    void attach(Ring& r) {
        ring = &r;
        if (!running.exchange(true))
            thr = std::thread([this]() { run(); });
    }

    void swap_ring(Ring& r) { ring = &r; }

    void run() {
        std::vector<float> buf(PERIOD * OUT_CH);
        while (running.load()) {
            if (paused.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(8)); continue; }
            {
                std::unique_lock<std::mutex> lk(ring->mx);
                ring->cv_data.wait_for(lk, std::chrono::milliseconds(20),
                    [&]{ return ring->avail() >= PERIOD || !running.load(); });
            }
            size_t got = ring->pop(buf.data(), PERIOD);
            if (got < PERIOD) std::fill(buf.begin() + got * OUT_CH, buf.end(), 0.f);
            float g = gain.load();
            if (g != 1.0f) for (auto& s : buf) s *= g;
            int rc = snd_pcm_writei(pcm, buf.data(), PERIOD);
            if (rc < 0) snd_pcm_recover(pcm, rc, 1);
        }
    }

    void stop() {
        running = false;
        if (ring) { ring->cv_data.notify_all(); ring->cv_space.notify_all(); }
        if (thr.joinable()) thr.join();
        if (pcm) { snd_pcm_drain(pcm); snd_pcm_close(pcm); pcm = nullptr; }
    }
    ~AudioOut() { stop(); }
};
#endif  // CMUSPP_ALSA

// ═══════════════════════════════════════════════════════════════════════════
//  CoreAudio
// ═══════════════════════════════════════════════════════════════════════════
#ifdef CMUSPP_COREAUDIO
struct AudioOut {
    AudioUnit          unit = nullptr;
    Ring*              ring = nullptr;
    std::atomic<bool>  paused{false};
    std::atomic<float> gain{1.0f};
    bool               started = false;

    static OSStatus cb(void* ref, AudioUnitRenderActionFlags*,
                       const AudioTimeStamp*, UInt32, UInt32 nf,
                       AudioBufferList* data)
    {
        auto* s = (AudioOut*)ref;
        float* dst = (float*)data->mBuffers[0].mData;
        if (s->paused.load() || !s->ring) { memset(dst, 0, nf * OUT_CH * 4); return noErr; }
        size_t got = s->ring->pop(dst, nf);
        if (got < nf) memset(dst + got * OUT_CH, 0, (nf - got) * OUT_CH * 4);
        float g = s->gain.load();
        if (g != 1.f) for (size_t i = 0; i < nf * OUT_CH; ++i) dst[i] *= g;
        return noErr;
    }

    bool open() {
        AudioComponentDescription d{kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0};
        AudioComponent comp = AudioComponentFindNext(nullptr, &d);
        if (!comp) return false;
        AudioComponentInstanceNew(comp, &unit);
        AURenderCallbackStruct cs{cb, this};
        AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input, 0, &cs, sizeof(cs));
        AudioStreamBasicDescription fmt{(double)OUT_RATE, kAudioFormatLinearPCM,
            kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked, (UInt32)(OUT_CH * 4),
            1, (UInt32)(OUT_CH * 4), (UInt32)OUT_CH, 32, 0};
        AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));
        return AudioUnitInitialize(unit) == noErr;
    }

    void attach(Ring& r)   { ring = &r; if (!started) { AudioOutputUnitStart(unit); started = true; } }
    void swap_ring(Ring& r){ ring = &r; }
    void stop() {
        if (unit) {
            AudioOutputUnitStop(unit);
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            unit = nullptr;
        }
    }
    ~AudioOut() { stop(); }
};
#endif  // CMUSPP_COREAUDIO

// ═══════════════════════════════════════════════════════════════════════════
//  WinMM
// ═══════════════════════════════════════════════════════════════════════════
#ifdef CMUSPP_WINMM
struct AudioOut {
    HWAVEOUT           hwo = nullptr;
    Ring*              ring = nullptr;
    std::atomic<bool>  running{false};
    std::atomic<bool>  paused{false};
    std::atomic<float> gain{1.0f};
    std::thread        thr;

    static constexpr int NB = 3, BF = 2048;
    WAVEHDR hdrs[NB]{}; std::vector<float> bufs[NB];

    bool open() {
        WAVEFORMATEX wfx{WAVE_FORMAT_IEEE_FLOAT, (WORD)OUT_CH, (DWORD)OUT_RATE,
                         (DWORD)(OUT_RATE * OUT_CH * 4), (WORD)(OUT_CH * 4), 32, 0};
        return waveOutOpen(&hwo, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
    }

    void attach(Ring& r) {
        ring = &r;
        for (int i = 0; i < NB; ++i) {
            bufs[i].resize(BF * OUT_CH);
            hdrs[i] = {(LPSTR)bufs[i].data(), (DWORD)(BF * OUT_CH * 4), 0, 0, WHDR_DONE};
            waveOutPrepareHeader(hwo, &hdrs[i], sizeof(WAVEHDR));
        }
        running = true;
        thr = std::thread([this]() { run(); });
    }

    void swap_ring(Ring& r) { ring = &r; }

    void run() {
        int cur = 0;
        while (running.load()) {
            WAVEHDR& h = hdrs[cur];
            while (!(h.dwFlags & WHDR_DONE) && running.load()) Sleep(1);
            h.dwFlags &= ~WHDR_DONE;
            float* d = bufs[cur].data();
            if (paused.load()) memset(d, 0, BF * OUT_CH * 4);
            else {
                size_t got = ring->pop(d, BF);
                if (got < BF) memset(d + got * OUT_CH, 0, (BF - got) * OUT_CH * 4);
                float g = gain.load();
                if (g != 1.f) for (int i = 0; i < BF * OUT_CH; ++i) d[i] *= g;
            }
            waveOutWrite(hwo, &h, sizeof(WAVEHDR));
            cur = (cur + 1) % NB;
        }
    }

    void stop() {
        running = false;
        if (thr.joinable()) thr.join();
        if (hwo) { waveOutReset(hwo); waveOutClose(hwo); hwo = nullptr; }
    }
    ~AudioOut() { stop(); }
};
#endif  // CMUSPP_WINMM
