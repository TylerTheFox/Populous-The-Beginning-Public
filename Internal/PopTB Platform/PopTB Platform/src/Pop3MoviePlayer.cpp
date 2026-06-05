#include "Pop3MoviePlayer.h"

// FFmpeg — silence the int->smaller-int conversion warnings in
// libavutil/common.h inlines (av_clip_uint8 etc.). Platform builds with
// /WX so the otherwise-harmless C4244 turns into a hard error.
#pragma warning(push)
#pragma warning(disable: 4244)   // 'return': conversion, possible loss of data
#pragma warning(disable: 4245)   // signed/unsigned conversion in return
#pragma warning(disable: 4267)   // size_t -> int conversions in FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#pragma warning(pop)

// SFML — same audio backend the rest of the engine uses (see Pop3Sound.cpp).
#include <SFML/Audio/SoundStream.hpp>
#include <SFML/Audio/SoundChannel.hpp>
#include <SFML/System/Time.hpp>

// Platform surface.
#include <Windows.h>
#include <d3d9.h>
#include "Pop3App.h"      // ProcessWindowsMessages + Pop3InputDispatchTable

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Pop3MoviePlayer {
namespace {

// =================================================================
//  SFML SoundStream — fed by the decode thread (the caller).
// =================================================================
class FmvAudioStream : public sf::SoundStream
{
public:
    void start(unsigned int channels, unsigned int sampleRate)
    {
        std::vector<sf::SoundChannel> map;
        if (channels <= 1) map = { sf::SoundChannel::FrontCenter };
        else               map = { sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight };

        initialize(channels <= 1 ? 1u : 2u, sampleRate, map);
        m_eof.store(false, std::memory_order_release);
        play();
    }

    void push(const std::int16_t* samples, std::size_t count)
    {
        if (!samples || count == 0) return;
        std::lock_guard<std::mutex> lk(m_mtx);
        m_queue.insert(m_queue.end(), samples, samples + count);
    }

    void markEof() { m_eof.store(true, std::memory_order_release); }

    // Interleaved sample count currently queued. Used by the decode
    // loop to decide whether to read more packets vs. sleep.
    std::size_t queued() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_queue.size();
    }

private:
    bool onGetData(Chunk& out) override
    {
        constexpr std::size_t kChunkSamples = 4410;
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_queue.empty())
        {
            if (m_eof.load(std::memory_order_acquire)) return false;
            // ~20 ms of silence per pull during an underrun — small enough
            // not to mask audible recovery, big enough not to spin SFML's
            // onGetData loop at thousands of Hz.
            m_silence.assign(882 * 2, 0);
            out.samples = m_silence.data();
            out.sampleCount = m_silence.size();
            return true;
        }
        std::size_t n = (std::min)(kChunkSamples, m_queue.size());
        m_scratch.assign(m_queue.begin(), m_queue.begin() + (std::ptrdiff_t)n);
        m_queue.erase(m_queue.begin(), m_queue.begin() + (std::ptrdiff_t)n);
        out.samples = m_scratch.data();
        out.sampleCount = m_scratch.size();
        return true;
    }

    void onSeek(sf::Time /*t*/) override {}

    mutable std::mutex          m_mtx;
    std::deque<std::int16_t>    m_queue;
    std::vector<std::int16_t>   m_scratch;
    std::vector<std::int16_t>   m_silence;
    std::atomic<bool>           m_eof{ false };
};

// =================================================================
//  D3D9 fullscreen RGBA presenter.
// =================================================================
class D3D9Presenter
{
public:
    bool init(IDirect3DDevice9* dev, int videoW, int videoH)
    {
        m_device = dev;
        m_videoW = videoW;
        m_videoH = videoH;
        HRESULT hr = m_device->CreateTexture(
            (UINT)videoW, (UINT)videoH, 1,
            D3DUSAGE_DYNAMIC,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &m_tex,
            nullptr);
        return SUCCEEDED(hr) && m_tex != nullptr;
    }

    void shutdown()
    {
        if (m_tex) { m_tex->Release(); m_tex = nullptr; }
        m_device = nullptr;
    }

    bool uploadBGRA(const unsigned char* src, int srcPitch)
    {
        if (!m_tex) return false;
        D3DLOCKED_RECT lr{};
        if (FAILED(m_tex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD))) return false;
        const int rowBytes = m_videoW * 4;
        unsigned char* dst = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < m_videoH; ++y)
        {
            std::memcpy(dst + (std::size_t)y * lr.Pitch,
                        src + (std::size_t)y * srcPitch,
                        (std::size_t)rowBytes);
        }
        m_tex->UnlockRect(0);
        return true;
    }

    bool presentFrame()
    {
        if (!m_device || !m_tex) return false;

        D3DVIEWPORT9 vp;
        if (FAILED(m_device->GetViewport(&vp))) return false;
        const float bbW = (float)vp.Width;
        const float bbH = (float)vp.Height;
        const float srcAspect = (float)m_videoW / (float)m_videoH;
        const float bbAspect  = bbW / bbH;
        float quadW, quadH;
        if (srcAspect >= bbAspect) { quadW = bbW; quadH = bbW / srcAspect; }
        else                       { quadH = bbH; quadW = bbH * srcAspect; }
        const float x0 = (bbW - quadW) * 0.5f;
        const float y0 = (bbH - quadH) * 0.5f;
        const float x1 = x0 + quadW;
        const float y1 = y0 + quadH;

        struct V { float x, y, z, rhw, u, v; };
        const float hp = 0.5f;
        V v[4] = {
            { x0 - hp, y0 - hp, 0.f, 1.f, 0.f, 0.f },
            { x1 - hp, y0 - hp, 0.f, 1.f, 1.f, 0.f },
            { x0 - hp, y1 - hp, 0.f, 1.f, 0.f, 1.f },
            { x1 - hp, y1 - hp, 0.f, 1.f, 1.f, 1.f },
        };

        m_device->BeginScene();
        m_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
        m_device->SetRenderState(D3DRS_LIGHTING,         FALSE);
        m_device->SetRenderState(D3DRS_ZENABLE,          FALSE);
        m_device->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
        m_device->SetRenderState(D3DRS_FOGENABLE,        FALSE);
        m_device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
            D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        m_device->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_device->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
        m_device->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
        m_device->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
        m_device->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
        m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

        m_device->SetTexture(0, m_tex);
        m_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(V));
        m_device->SetTexture(0, nullptr);
        m_device->EndScene();

        return SUCCEEDED(m_device->Present(nullptr, nullptr, nullptr, nullptr));
    }

private:
    IDirect3DDevice9*  m_device = nullptr;
    IDirect3DTexture9* m_tex    = nullptr;
    int                m_videoW = 0;
    int                m_videoH = 0;
};

// =================================================================
//  FFmpeg RAII
// =================================================================
struct AvFormatCtxDeleter { void operator()(AVFormatContext* p) const { if (p) avformat_close_input(&p); } };
struct AvCodecCtxDeleter  { void operator()(AVCodecContext*  p) const { if (p) avcodec_free_context(&p); } };
struct AvFrameDeleter     { void operator()(AVFrame*         p) const { if (p) av_frame_free(&p); } };
struct AvPacketDeleter    { void operator()(AVPacket*        p) const { if (p) av_packet_free(&p); } };
struct SwsCtxDeleter      { void operator()(SwsContext*      p) const { if (p) sws_freeContext(p); } };
struct SwrCtxDeleter      { void operator()(SwrContext*      p) const { if (p) { SwrContext* q = p; swr_free(&q); } } };

using AvFormatCtxPtr = std::unique_ptr<AVFormatContext, AvFormatCtxDeleter>;
using AvCodecCtxPtr  = std::unique_ptr<AVCodecContext,  AvCodecCtxDeleter>;
using AvFramePtr     = std::unique_ptr<AVFrame,         AvFrameDeleter>;
using AvPacketPtr    = std::unique_ptr<AVPacket,        AvPacketDeleter>;
using SwsCtxPtr      = std::unique_ptr<SwsContext,      SwsCtxDeleter>;
using SwrCtxPtr      = std::unique_ptr<SwrContext,      SwrCtxDeleter>;

static AvCodecCtxPtr open_decoder(AVFormatContext* fmt, int streamIdx)
{
    if (streamIdx < 0) return nullptr;
    AVStream* st = fmt->streams[streamIdx];
    const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) return nullptr;
    AvCodecCtxPtr ctx(avcodec_alloc_context3(dec));
    if (!ctx) return nullptr;
    if (avcodec_parameters_to_context(ctx.get(), st->codecpar) < 0) return nullptr;
    ctx->pkt_timebase = st->time_base;
    if (avcodec_open2(ctx.get(), dec, nullptr) < 0) return nullptr;
    return ctx;
}

static bool file_readable(const char* path)
{
    if (!path || !*path) return false;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f) return false;
    fclose(f);
    return true;
}

// Pump messages + observe quit_flag. Helper used in two places (between
// packets, and inside the per-frame PTS wait).
static inline bool pump_and_check_quit(const Pop3InputDispatchTable* idt,
                                       volatile int* quit_flag)
{
    if (idt) Pop3App::ProcessWindowsMessages(*idt);
    return quit_flag && *quit_flag != 0;
}

} // namespace

// =================================================================
//  Public entry — blocking playback.
// =================================================================
bool play_blocking(const char*                   path,
                   int                           audio_track,
                   int                           audio_volume_0_255,
                   IDirect3DDevice9*             device,
                   const Pop3InputDispatchTable* idt,
                   volatile int*                 quit_flag)
{
    if (!file_readable(path)) return false;
    if (!device)              return false;

    AVFormatContext* rawFmt = nullptr;
    if (avformat_open_input(&rawFmt, path, nullptr, nullptr) != 0) return false;
    AvFormatCtxPtr fmt(rawFmt);
    if (avformat_find_stream_info(fmt.get(), nullptr) < 0) return false;

    int videoIdx = -1;
    std::vector<int> audioStreams;
    for (unsigned i = 0; i < fmt->nb_streams; ++i)
    {
        AVMediaType t = fmt->streams[i]->codecpar->codec_type;
        if      (t == AVMEDIA_TYPE_VIDEO && videoIdx < 0) videoIdx = (int)i;
        else if (t == AVMEDIA_TYPE_AUDIO)                 audioStreams.push_back((int)i);
    }
    if (videoIdx < 0) return false;

    AvCodecCtxPtr vctx = open_decoder(fmt.get(), videoIdx);
    if (!vctx) return false;

    const int videoW = vctx->width;
    const int videoH = vctx->height;
    if (videoW <= 0 || videoH <= 0) return false;

    D3D9Presenter present;
    if (!present.init(device, videoW, videoH)) return false;

    SwsCtxPtr sws(sws_getContext(
        videoW, videoH, vctx->pix_fmt,
        videoW, videoH, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!sws) return false;

    std::vector<unsigned char> bgra((std::size_t)videoW * (std::size_t)videoH * 4u);
    const int bgraPitch = videoW * 4;

    // ---- Audio channels ----
    // TGQ files carry parallel audio streams — typically background
    // music plus one or more voiceovers. The 1998 EA uV player picked
    // ONE via uV_SelectAudioTrack (1-based), which is why call sites
    // still pass `audio_track` (English=0=default, German=2, French=3,
    // Dutch=4, etc.); on TGQs that bundle music + voiceover we have to
    // play every audio stream concurrently or the voiceover goes
    // missing. `audio_track` kept for future use; if a multi-language
    // file produces layered voiceovers we can narrow this back down.
    (void)audio_track;

    struct AudioChannel
    {
        int                              streamIdx     = -1;
        AvCodecCtxPtr                    ctx;
        SwrCtxPtr                        swr;
        std::unique_ptr<FmvAudioStream>  sfml;
        int                              outChannels   = 0;
        int                              outSampleRate = 0;
    };
    std::vector<AudioChannel> audioChannels;

    const float kVolume0_100 =
        (float)std::max(0, std::min(255, audio_volume_0_255)) * (100.0f / 255.0f);

    for (int ai : audioStreams)
    {
        AudioChannel ch;
        ch.streamIdx = ai;
        ch.ctx = open_decoder(fmt.get(), ai);
        if (!ch.ctx) continue;

        ch.outChannels   = (ch.ctx->ch_layout.nb_channels >= 2) ? 2 : 1;
        ch.outSampleRate = ch.ctx->sample_rate > 0 ? ch.ctx->sample_rate : 44100;

        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, ch.outChannels);

        SwrContext* rawSwr = nullptr;
        bool ok = false;
        if (swr_alloc_set_opts2(&rawSwr,
                                &outLayout, AV_SAMPLE_FMT_S16, ch.outSampleRate,
                                &ch.ctx->ch_layout, ch.ctx->sample_fmt, ch.ctx->sample_rate,
                                0, nullptr) == 0 && rawSwr)
        {
            if (swr_init(rawSwr) >= 0)
            {
                ch.swr.reset(rawSwr);
                ch.sfml = std::make_unique<FmvAudioStream>();
                ch.sfml->start((unsigned)ch.outChannels, (unsigned)ch.outSampleRate);
                ch.sfml->setVolume(kVolume0_100);
                ok = true;
            }
            else
            {
                swr_free(&rawSwr);
            }
        }
        av_channel_layout_uninit(&outLayout);
        if (ok)
            audioChannels.push_back(std::move(ch));
    }

    const bool audioReady = !audioChannels.empty();

    AvPacketPtr pkt(av_packet_alloc());
    AvFramePtr  vframe(av_frame_alloc());
    AvFramePtr  aframe(av_frame_alloc());
    if (!pkt || !vframe || !aframe) return false;

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    const AVRational vtb = fmt->streams[videoIdx]->time_base;

    if (quit_flag) *quit_flag = 0;
    bool firstFramePresented = false;
    bool eofReached = false;


    // -----------------------------------------------------------------
    // Helpers (lambdas capture local FFmpeg state).
    //
    //   read_and_dispatch() reads one packet from the demuxer and sends
    //   it to the matching decoder; returns 1 on success, 0 on EOF
    //   (decoders flushed with a null packet), -1 on hard error.
    //
    //   drain_audio() pulls every available decoded audio frame, resamples
    //   to s16 stereo/mono, and pushes into the SFML queue. Cheap when
    //   the decoder is empty (single AVERROR(EAGAIN) check).
    // -----------------------------------------------------------------
    auto read_and_dispatch = [&]() -> int
    {
        int rr = av_read_frame(fmt.get(), pkt.get());
        if (rr < 0)
        {
            avcodec_send_packet(vctx.get(), nullptr);
            for (auto& ch : audioChannels)
                avcodec_send_packet(ch.ctx.get(), nullptr);
            return 0;
        }
        if (pkt->stream_index == videoIdx)
        {
            avcodec_send_packet(vctx.get(), pkt.get());
        }
        else
        {
            for (auto& ch : audioChannels)
            {
                if (pkt->stream_index == ch.streamIdx)
                {
                    avcodec_send_packet(ch.ctx.get(), pkt.get());
                    break;
                }
            }
        }
        av_packet_unref(pkt.get());
        return 1;
    };

    auto drain_audio = [&]()
    {
        for (auto& ch : audioChannels)
        {
            while (avcodec_receive_frame(ch.ctx.get(), aframe.get()) >= 0)
            {
                const int maxOut = (int)av_rescale_rnd(
                    swr_get_delay(ch.swr.get(), ch.ctx->sample_rate) + aframe->nb_samples,
                    ch.outSampleRate, ch.ctx->sample_rate, AV_ROUND_UP);
                std::vector<std::int16_t> tmp((std::size_t)maxOut * ch.outChannels);
                std::int16_t* outBufs[1] = { tmp.data() };
                int outSamples = swr_convert(
                    ch.swr.get(),
                    reinterpret_cast<uint8_t**>(outBufs), maxOut,
                    const_cast<const uint8_t**>(aframe->extended_data),
                    aframe->nb_samples);
                if (outSamples > 0)
                    ch.sfml->push(tmp.data(),
                                  (std::size_t)outSamples * ch.outChannels);
                av_frame_unref(aframe.get());
            }
        }
    };

    // Per-channel low-water (~250 ms): if any channel is below, prefer
    // reading a packet over sleeping. Per-channel prebuffer (~500 ms):
    // initial warmup before the first video frame goes up.
    auto any_channel_below_lowwater = [&]() -> bool
    {
        for (auto& ch : audioChannels)
        {
            const std::size_t lw =
                (std::size_t)ch.outSampleRate * (std::size_t)ch.outChannels / 4u;
            if (ch.sfml->queued() < lw) return true;
        }
        return false;
    };

    auto any_channel_below_prebuffer = [&]() -> bool
    {
        for (auto& ch : audioChannels)
        {
            const std::size_t pb =
                (std::size_t)ch.outSampleRate * (std::size_t)ch.outChannels / 2u;
            if (ch.sfml->queued() < pb) return true;
        }
        return false;
    };

    // Prebuffer ~500 ms of audio on every channel before the first
    // video frame goes up so SFML's streaming thread has runway and
    // doesn't start with an underrun. Skipped when there are no audio
    // streams in the file.
    if (audioReady)
    {
        while (any_channel_below_prebuffer())
        {
            int r = read_and_dispatch();
            drain_audio();
            if (r <= 0) { if (r == 0) eofReached = true; break; }
            if (pump_and_check_quit(idt, quit_flag)) { if (quit_flag) *quit_flag = 0; goto done; }
        }
    }

    for (;;)
    {
        if (pump_and_check_quit(idt, quit_flag)) { if (quit_flag) *quit_flag = 0; break; }

        // Receive the next decoded video frame, demuxing more packets on
        // demand. The decoder returns EAGAIN until it has enough input
        // buffered; we keep feeding packets (and draining audio as a side
        // effect) until either a frame pops out or the file hits EOF.
        int rr;
        while ((rr = avcodec_receive_frame(vctx.get(), vframe.get())) == AVERROR(EAGAIN))
        {
            if (eofReached) break;
            int r = read_and_dispatch();
            if (r == 0) eofReached = true;
            drain_audio();
            if (pump_and_check_quit(idt, quit_flag)) { if (quit_flag) *quit_flag = 0; goto done; }
        }
        if (rr < 0) break;   // genuine EOF or decoder error — we are done

        int64_t pts = vframe->best_effort_timestamp;
        if (pts == AV_NOPTS_VALUE) pts = vframe->pts;
        double ptsSec = (pts == AV_NOPTS_VALUE)
                            ? 0.0
                            : (double)pts * (double)vtb.num / (double)vtb.den;

        // Wait until the frame's presentation time. While we wait, keep
        // window messages flowing AND keep audio fed — underrun in this
        // window is the classic source of cinematic-audio glitching.
        for (;;)
        {
            if (pump_and_check_quit(idt, quit_flag)) { if (quit_flag) *quit_flag = 0; goto done; }
            const double elapsed = std::chrono::duration<double>(clock::now() - t0).count();
            if (elapsed + 0.001 >= ptsSec) break;

            if (audioReady && !eofReached && any_channel_below_lowwater())
            {
                int r = read_and_dispatch();
                if (r == 0) eofReached = true;
                drain_audio();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        // Convert YUV → BGRA and present.
        uint8_t* dstSlices[4] = { bgra.data(), nullptr, nullptr, nullptr };
        int      dstStride[4] = { bgraPitch,  0,       0,       0       };
        sws_scale(sws.get(),
                  const_cast<const uint8_t* const*>(vframe->data),
                  vframe->linesize,
                  0, videoH,
                  dstSlices, dstStride);

        present.uploadBGRA(bgra.data(), bgraPitch);
        if (!present.presentFrame()) goto done;
        firstFramePresented = true;
        av_frame_unref(vframe.get());
    }

done:
    if (audioReady)
    {
        for (auto& ch : audioChannels) ch.sfml->markEof();

        const bool wasSkipped = (quit_flag && *quit_flag);
        if (!wasSkipped)
        {
            // Brief drain so dialogue / music tails don't get clipped on
            // a clean ending; skip path stops audio immediately.
            const auto drainStart = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - drainStart < std::chrono::milliseconds(300))
            {
                bool anyPlaying = false;
                for (auto& ch : audioChannels)
                    if (ch.sfml->getStatus() == sf::SoundStream::Status::Playing)
                        anyPlaying = true;
                if (!anyPlaying) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        for (auto& ch : audioChannels) ch.sfml->stop();
    }
    present.shutdown();
    return firstFramePresented;
}

} // namespace Pop3MoviePlayer                                                                                                                                                                                                                                                                                                                                                                      