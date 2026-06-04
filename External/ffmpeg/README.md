# FFmpeg vendor drop-in

PopTB's FMV playback (intros, splash, outro) is backed by FFmpeg, hidden
behind PopTB Platform's `Pop3MoviePlayer` facade. The pop3 game project
never sees these headers; only `PopTB Platform.vcxproj` includes them.

We link against the **LGPL shared build** so the DLLs stay redistributable.

This directory is committed empty (apart from this README). The build looks
for headers under `inc/` and import libraries under `bin/`. To make
Pop3MoviePlayer compile and link, populate this directory once.

## What to drop in

Download a Windows x86 **shared** LGPL build of FFmpeg 6.x or newer
(gyan.dev or BtbN release builds work). You want the package that
includes the `dev` libs alongside the runtime DLLs.

Copy from the dev package into `inc/`:

```
inc/
  libavcodec/avcodec.h, ...
  libavformat/avformat.h, ...
  libavutil/avutil.h, imgutils.h, opt.h, channel_layout.h, ...
  libswscale/swscale.h
  libswresample/swresample.h
```

Copy from the dev package into `bin/`:

```
bin/
  avcodec.lib
  avformat.lib
  avutil.lib
  swscale.lib
  swresample.lib
```

Copy from the shared package next to the built executable
(typically `Source/Debug/` and `Source/Release/`):

```
avcodec-*.dll
avformat-*.dll
avutil-*.dll
swscale-*.dll
swresample-*.dll
```

## Why shared not static

LGPL allows linking the shared build without infecting our own code.
Static linking would either pull in GPL-only components or trigger the
LGPL's relinkable-object obligation.

## Versions tested

Built against gyan.dev "ffmpeg-7.0-full_build-shared" (June 2024).
FFmpeg ABI is stable within a major; later 7.x point releases drop in
cleanly.
