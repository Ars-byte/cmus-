#pragma once
/*
  backend/decoder.hpp
  Decoder thread: reads audio files via libsndfile,
  resamples to OUT_RATE/OUT_CH, feeds Ring buffer.
*/

#include "ring.hpp"

#include <optional>
#include <string>
#include <thread>

struct Decoder {
    std::thread         thr;
    std::atomic<bool>   stop_req{false};
    std::atomic<bool>   finished{false};
    std::atomic<double> duration{0.0};
    Ring                ring{RING_FRAMES};

    void stop() {
        stop_req.store(true);
        ring.cv_space.notify_all();
        ring.cv_data.notify_all();
        if (thr.joinable()) thr.join();
        stop_req.store(false);
    }

    void start(const std::string& path, double seek_s = 0.0) {
        stop();
        finished.store(false);
        duration.store(0.0);
        ring.clear();
        thr = std::thread([this, path, seek_s]() { run(path, seek_s); });
    }

    bool done() const { return finished.load(); }

private:
    static std::vector<float> resamp(const float* in, size_t frames,
                                     int src_ch, int src_rate)
    {
        if (src_rate == OUT_RATE && src_ch == OUT_CH)
            return { in, in + frames * OUT_CH };
        double ratio = (double)src_rate / OUT_RATE;
        size_t on = (size_t)(frames / ratio);
        std::vector<float> out(on * OUT_CH, 0.f);
        for (size_t i = 0; i < on; ++i) {
            double si = i * ratio;
            size_t a  = (size_t)si, b = std::min(a + 1, frames - 1);
            double t  = si - a;
            for (int c = 0; c < OUT_CH; ++c) {
                int sc = std::min(c, src_ch - 1);
                out[i * OUT_CH + c] = (float)(in[a * src_ch + sc] * (1.0 - t) +
                                              in[b * src_ch + sc] * t);
            }
        }
        return out;
    }

    void run(const std::string& path, double seek_s) {
        SF_INFO info{};
        SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
        if (!sf) { finished.store(true); ring.cv_data.notify_all(); return; }

        if (info.samplerate > 0)
            duration.store((double)info.frames / info.samplerate);

        if (seek_s > 0 && info.samplerate > 0)
            sf_seek(sf, (sf_count_t)(seek_s * info.samplerate), SEEK_SET);

        constexpr size_t CHUNK = 4096;
        std::vector<float> raw(CHUNK * info.channels);

        while (!stop_req.load()) {
            sf_count_t got = sf_readf_float(sf, raw.data(), CHUNK);
            if (got <= 0) break;

            auto pcm = resamp(raw.data(), (size_t)got, info.channels, info.samplerate);
            size_t pushed = 0, total = pcm.size() / OUT_CH;
            while (!stop_req.load() && pushed < total) {
                {
                    std::unique_lock<std::mutex> lk(ring.mx);
                    ring.cv_space.wait_for(lk, std::chrono::milliseconds(10),
                        [&]{ return ring.space() > 0 || stop_req.load(); });
                }
                pushed += ring.push(pcm.data() + pushed * OUT_CH, total - pushed);
            }
        }
        sf_close(sf);
        finished.store(true);
        ring.cv_data.notify_all();
    }
};
