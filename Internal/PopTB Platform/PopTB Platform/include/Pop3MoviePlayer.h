#pragma once

// Pop3MoviePlayer — TGQ/TGV cinematic playback facade.
//
// Hides FFmpeg (libavformat/avcodec/swscale/swresample) and SFML (audio
// stream) behind the PopTB Platform layer so the game project (pop3.vcxproj)
// only sees this header. Same pattern as Pop3Sound.{h,cpp}.
//
// Replaces the 1998 EA "uV" lib (Martin Griffiths / Patrick Ratto, EA Canada).
// Backed by FFmpeg's `ea` demuxer + `eatgq`/`eatgv`/`eatqi` decoders for
// video and `eaac`/`madmp3` for audio; video is converted to BGRA via
// swscale and presented through the caller-supplied D3D9 device; audio is
// resampled to s16 stereo by swresample and pushed through SFML's
// SoundStream (so it travels the same backend the rest of the engine uses).
//
// Movies block the caller (boot splash runs before the engine loop, intros
// block menu transitions). While the movie plays the player pumps Windows
// messages through `idt` so the window stays responsive and the existing
// any-key/any-click skip handlers (P3KeyHandler3 / P3MouseHandler3 in
// Utils.cpp) fire as normal.

struct IDirect3DDevice9;
struct Pop3InputDispatchTable;

namespace Pop3MoviePlayer
{
    // Returns true if `path` opened and at least one video frame was
    // presented (including when the user skipped). Returns false if the
    // file was missing or the demuxer couldn't open it — caller treats
    // that as a silent no-op (several call sites reference assets that
    // ship only on the CD: blogo.tgq, outro.tgq, pop3mask.tgq, p3intro1
    // .tgq).
    //
    // Parameters:
    //   path                 file system path to the .tgq container.
    //   audio_track          0-based audio stream index for localised
    //                        dialog tracks. Falls back to the first
    //                        stream if the requested index doesn't exist.
    //   audio_volume_0_255   legacy 0..255 volume from the engine; the
    //                        player scales to SFML's 0..100 float.
    //   device               the live D3D9 device (caller obtains via
    //                        Dx9Device::get_d3d9_device()).
    //   idt                  input dispatch table used by ProcessWindows-
    //                        Messages while playing. Pass &IDT3 to keep
    //                        the legacy any-key skip semantics.
    //   quit_flag            polled each iteration; non-zero ends
    //                        playback. The caller's input handlers set
    //                        this (see `quit_intro` in Utils.cpp). The
    //                        player clears it before returning.
    bool play_blocking(const char*                       path,
                       int                               audio_track,
                       int                               audio_volume_0_255,
                       IDirect3DDevice9*                 device,
                       const Pop3InputDispatchTable*     idt,
                       volatile int*                     quit_flag);
}
