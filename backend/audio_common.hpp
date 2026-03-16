#pragma once
/*
  backend/audio_common.hpp
  Shared audio constants and platform-detection macros.
*/

// ── Platform audio ─────────────────────────────────────────────────────────
#if defined(__linux__)
  #define CMUSPP_ALSA 1
  #include <alsa/asoundlib.h>
#elif defined(__APPLE__)
  #define CMUSPP_COREAUDIO 1
  #include <AudioToolbox/AudioToolbox.h>
  #include <CoreAudio/CoreAudio.h>
#elif defined(_WIN32)
  #define CMUSPP_WINMM 1
  #include <windows.h>
  #include <mmsystem.h>
#else
  #error "Unsupported platform"
#endif

#include <sndfile.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <time.h>

static constexpr int    OUT_RATE    = 44100;
static constexpr int    OUT_CH      = 2;
static constexpr size_t RING_FRAMES = 131072;   // ~3 s buffer
static constexpr size_t PERIOD      = 512;

inline double mono_now() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
