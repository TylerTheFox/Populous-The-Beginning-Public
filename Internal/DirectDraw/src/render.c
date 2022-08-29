/*
* Copyright (c) 2010 Toni Spets <toni.spets@iki.fi>
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <windows.h>
#include <stdio.h>
#include "opengl.h"
#include "main.h"
#include "surface.h"
#include "openglshader.h"

#define TEXTURE_COUNT 4

static HGLRC OpenGLContext;
static GLuint MainProgram;
static GLuint ScaleProgram;
static BOOL GotError;
static int SurfaceTexWidth;
static int SurfaceTexHeight;
static int *SurfaceTex;
static GLenum SurfaceFormat = GL_LUMINANCE;
static GLenum SurfaceType = GL_UNSIGNED_BYTE;
static GLuint SurfaceTexIds[TEXTURE_COUNT];
static GLuint PaletteTexIds[TEXTURE_COUNT];
static float ScaleW;
static float ScaleH;
static GLint MainTexCoordAttrLoc = -1;
static GLint MainVertexCoordAttrLoc = -1;
static GLuint MainVBOs[3], MainVAO;
static GLint FrameCountUniLoc = -1;
static GLuint FrameBufferId;
static GLuint FrameBufferTexId;
static GLuint ScaleVBOs[3], ScaleVAO;
static BOOL UseOpenGL;
static BOOL AdjustAlignment;
static BOOL FilterBilinear;

static HGLRC CreateContext(HDC hdc);
static void SetMaxFPS();
static void BuildPrograms();
static void CreateTextures(int width, int height);
static void InitMainProgram();
static void InitScaleProgram();
static void Render();
static void DeleteContext(HGLRC context);
static BOOL TextureUploadTest();
static BOOL ShaderTest();

DWORD WINAPI render_main(void)
{
    static int first_run = 1;

    if (first_run || !UseOpenGL)
    {
        GotError = UseOpenGL = FALSE;
        OpenGLContext = CreateContext(ddraw->render.hDC);

        if (OpenGLContext)
        {
            OpenGL_Init();
            SetMaxFPS();
            BuildPrograms();
            CreateTextures(ddraw->width, ddraw->height);
            InitMainProgram();
            InitScaleProgram();

            GotError = GotError || !TextureUploadTest();
            GotError = GotError || !ShaderTest();
            GotError = GotError || glGetError() != GL_NO_ERROR;
            UseOpenGL = (MainProgram || ddraw->bpp == 16) && !GotError;
        }

        if (!UseOpenGL)
        {
            ShowDriverWarning = TRUE;
            ddraw->renderer = render_soft_main;
            render_soft_main();
            return 0;
        }
        first_run = 0;
    }

    if (ddraw->renderer == render_soft_main)
    {
        render_soft_main();
        return 0;
    }

    if (Am_I_Beta() || Am_I_Multiverse())
    {
        Render();
        return 0;
    }
    else
    {
        Render();
        DeleteContext(OpenGLContext);
    }

    return 0;
}

static HGLRC CreateContext(HDC hdc)
{
    HGLRC context = xwglCreateContext(hdc);
    BOOL madeCurrent = context && xwglMakeCurrent(hdc, context);

    if (!madeCurrent || glGetError() != GL_NO_ERROR)
    {
        if (madeCurrent)
        {
            xwglMakeCurrent(NULL, NULL);
            xwglDeleteContext(context);
        }

        context = 0;
    }

    return context;
}

static void SetMaxFPS()
{
    int maxFPS = ddraw->render.maxfps;
    ddraw->fpsLimiter.tickLengthNs = 0;
    ddraw->fpsLimiter.ticklength = 0;

    /*
    if (OpenGL_ExtExists("WGL_EXT_swap_control_tear", ddraw->render.hDC))
    {
        if (wglSwapIntervalEXT)
        {
            if (ddraw->vsync)
            {
                wglSwapIntervalEXT(-1);
                maxFPS = ddraw->mode.dmDisplayFrequency;
            }
            else
                wglSwapIntervalEXT(0);
        }
    }
    else */
    if (OpenGL_ExtExists("WGL_EXT_swap_control", ddraw->render.hDC))
    {
        if (wglSwapIntervalEXT)
        {
            if (ddraw->vsync)
            {
                wglSwapIntervalEXT(1);
                maxFPS = ddraw->mode.dmDisplayFrequency;
            }
            else
                wglSwapIntervalEXT(0);
        }
    }

    if (maxFPS < 0)
        maxFPS = ddraw->mode.dmDisplayFrequency;

    if (maxFPS > 1000)
        maxFPS = 0;

    if (maxFPS > 0)
    {
        float len = 1000.0f / maxFPS;
        ddraw->fpsLimiter.tickLengthNs = len * 10000;
        ddraw->fpsLimiter.ticklength = len;// + 0.5f;
    }
}

static void BuildPrograms()
{
    MainProgram = ScaleProgram = 0;

    if (OpenGL_GotVersion3)
    {
        if (ddraw->bpp == 8)
            MainProgram = OpenGL_BuildProgram(PassthroughVertShaderSrc, PaletteFragShaderSrc);
        else if (ddraw->bpp == 16)
            MainProgram = OpenGL_BuildProgram(PassthroughVertShaderSrc, PassthroughFragShaderSrc);

        if (MainProgram)
            ScaleProgram = OpenGL_BuildProgramFromFile(ddraw->shader);
        else
            OpenGL_GotVersion3 = FALSE;

        FilterBilinear = strstr(ddraw->shader, "bilinear.glsl") != 0;
    }

    if (OpenGL_GotVersion2 && !MainProgram)
    {
        if (ddraw->bpp == 8)
            MainProgram = OpenGL_BuildProgram(PassthroughVertShader110Src, PaletteFragShader110Src);
        else if (ddraw->bpp == 16)
            MainProgram = OpenGL_BuildProgram(PassthroughVertShader110Src, PassthroughFragShader110Src);
    }
}

static void CreateTextures(int width, int height)
{
    SurfaceTexWidth =
        width <= 1024 ? 1024 : width <= 2048 ? 2048 : width <= 4096 ? 4096 : width;

    SurfaceTexHeight =
        height <= 512 ? 512 : height <= 1024 ? 1024 : height <= 2048 ? 2048 : height <= 4096 ? 4096 : height;

    SurfaceTex = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SurfaceTexWidth * SurfaceTexHeight * sizeof(int));

    AdjustAlignment = (width % 4) != 0;

    ScaleW = (float)width / SurfaceTexWidth;
    ScaleH = (float)height / SurfaceTexHeight;

    glGenTextures(TEXTURE_COUNT, SurfaceTexIds);

    int i;
    for (i = 0; i < TEXTURE_COUNT; i++)
    {
        glBindTexture(GL_TEXTURE_2D, SurfaceTexIds[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        GotError = GotError || glGetError() != GL_NO_ERROR;

        while (glGetError() != GL_NO_ERROR);

        if (ddraw->bpp == 16)
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB565,
                SurfaceTexWidth,
                SurfaceTexHeight,
                0,
                SurfaceFormat = GL_RGB,
                SurfaceType = GL_UNSIGNED_SHORT_5_6_5,
                0);


            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGB5,
                    SurfaceTexWidth,
                    SurfaceTexHeight,
                    0,
                    SurfaceFormat = GL_RGB,
                    SurfaceType = GL_UNSIGNED_SHORT_5_6_5,
                    0);
            }
        }
        else if (ddraw->bpp == 8)
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_LUMINANCE8,
                SurfaceTexWidth,
                SurfaceTexHeight,
                0,
                SurfaceFormat = GL_LUMINANCE,
                SurfaceType = GL_UNSIGNED_BYTE,
                0);


            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_R8,
                    SurfaceTexWidth,
                    SurfaceTexHeight,
                    0,
                    SurfaceFormat = GL_RED,
                    SurfaceType = GL_UNSIGNED_BYTE,
                    0);
            }

            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RED,
                    SurfaceTexWidth,
                    SurfaceTexHeight,
                    0,
                    SurfaceFormat = GL_RED,
                    SurfaceType = GL_UNSIGNED_BYTE,
                    0);
            }
        }
    }

    if (ddraw->bpp == 8)
    {
        glGenTextures(TEXTURE_COUNT, PaletteTexIds);

        for (i = 0; i < TEXTURE_COUNT; i++)
        {
            glBindTexture(GL_TEXTURE_2D, PaletteTexIds[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        }
    }
}

static void InitMainProgram()
{
    if (!MainProgram)
        return;

    glUseProgram(MainProgram);

    glUniform1i(glGetUniformLocation(MainProgram, "SurfaceTex"), 0);
    
    if (ddraw->bpp == 8)
        glUniform1i(glGetUniformLocation(MainProgram, "PaletteTex"), 1);

    if (OpenGL_GotVersion3)
    {
        MainVertexCoordAttrLoc = glGetAttribLocation(MainProgram, "VertexCoord");
        MainTexCoordAttrLoc = glGetAttribLocation(MainProgram, "TexCoord");

        glGenBuffers(3, MainVBOs);

        if (ScaleProgram)
        {
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[0]);
            static const GLfloat vertexCoord[] = {
                -1.0f,-1.0f,
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
            GLfloat texCoord[] = {
                0.0f,   0.0f,
                0.0f,   ScaleH,
                ScaleW, ScaleH,
                ScaleW, 0.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[0]);
            static const GLfloat vertexCoord[] = {
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
                -1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
            GLfloat texCoord[] = {
                0.0f,   0.0f,
                ScaleW, 0.0f,
                ScaleW, ScaleH,
                0.0f,   ScaleH,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        glGenVertexArrays(1, &MainVAO);
        glBindVertexArray(MainVAO);

        glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[0]);
        glVertexAttribPointer(MainVertexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(MainVertexCoordAttrLoc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
        glVertexAttribPointer(MainTexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(MainTexCoordAttrLoc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, MainVBOs[2]);
        static const GLushort indices[] =
        {
            0, 1, 2,
            0, 2, 3,
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindVertexArray(0);

        const float mvpMatrix[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1,
        };
        glUniformMatrix4fv(glGetUniformLocation(MainProgram, "MVPMatrix"), 1, GL_FALSE, mvpMatrix);

    }
}

static void InitScaleProgram()
{
    if (!ScaleProgram)
        return;

    glUseProgram(ScaleProgram);

    GLint vertexCoordAttrLoc = glGetAttribLocation(ScaleProgram, "VertexCoord");
    GLint texCoordAttrLoc = glGetAttribLocation(ScaleProgram, "TexCoord");
    FrameCountUniLoc = glGetUniformLocation(ScaleProgram, "FrameCount");

    glGenBuffers(3, ScaleVBOs);

    glBindBuffer(GL_ARRAY_BUFFER, ScaleVBOs[0]);
    static const GLfloat vertexCoord[] = {
        -1.0f, 1.0f,
         1.0f, 1.0f,
         1.0f,-1.0f,
        -1.0f,-1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, ScaleVBOs[1]);
    GLfloat texCoord[] = {
        0.0f,    0.0f,
        ScaleW,  0.0f,
        ScaleW,  ScaleH,
        0.0f,    ScaleH,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &ScaleVAO);
    glBindVertexArray(ScaleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, ScaleVBOs[0]);
    glVertexAttribPointer(vertexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vertexCoordAttrLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, ScaleVBOs[1]);
    glVertexAttribPointer(texCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(texCoordAttrLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ScaleVBOs[2]);
    static const GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3,
    };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    float inputSize[2], outputSize[2], textureSize[2];

    inputSize[0] = ddraw->width;
    inputSize[1] = ddraw->height;
    textureSize[0] = SurfaceTexWidth;
    textureSize[1] = SurfaceTexHeight;
    outputSize[0] = ddraw->render.viewport.width;
    outputSize[1] = ddraw->render.viewport.height;

    glUniform2fv(glGetUniformLocation(ScaleProgram, "OutputSize"), 1, outputSize);
    glUniform2fv(glGetUniformLocation(ScaleProgram, "TextureSize"), 1, textureSize);
    glUniform2fv(glGetUniformLocation(ScaleProgram, "InputSize"), 1, inputSize);
    glUniform1i(glGetUniformLocation(ScaleProgram, "FrameDirection"), 1);
    glUniform1i(glGetUniformLocation(ScaleProgram, "Texture"), 0);

    const float mvpMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
    glUniformMatrix4fv(glGetUniformLocation(ScaleProgram, "MVPMatrix"), 1, GL_FALSE, mvpMatrix);

    glGenFramebuffers(1, &FrameBufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);

    glGenTextures(1, &FrameBufferTexId);
    glBindTexture(GL_TEXTURE_2D, FrameBufferTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FilterBilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FilterBilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SurfaceTexWidth, SurfaceTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FrameBufferTexId, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteTextures(1, &FrameBufferTexId);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &FrameBufferId);

        if (glDeleteProgram)
            glDeleteProgram(ScaleProgram);

        ScaleProgram = 0;

        if (glDeleteBuffers)
            glDeleteBuffers(3, ScaleVBOs);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &ScaleVAO);

        if (MainProgram)
        {
            glBindVertexArray(MainVAO);
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[0]);
            static const GLfloat vertexCoordPal[] = {
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
                -1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoordPal), vertexCoordPal, GL_STATIC_DRAW);
            glVertexAttribPointer(MainVertexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(MainVertexCoordAttrLoc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            glBindVertexArray(MainVAO);
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
            GLfloat texCoordPal[] = {
                0.0f,   0.0f,
                ScaleW, 0.0f,
                ScaleW, ScaleH,
                0.0f,   ScaleH,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(texCoordPal), texCoordPal, GL_STATIC_DRAW);
            glVertexAttribPointer(MainTexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(MainTexCoordAttrLoc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void Renderer_Impl()
{
    static DWORD tickStart = 0;
    static DWORD tickEnd = 0;
    static BOOL needsUpdate = FALSE;
#if _DEBUG
    DrawFrameInfoStart();
#endif

    ScaleW = (float)ddraw->width / SurfaceTexWidth;
    ScaleH = (float)ddraw->height / SurfaceTexHeight;

    static int texIndex = 0, palIndex = 0;

    BOOL scaleChanged = FALSE;

    if (ddraw->fpsLimiter.ticklength > 0)
        tickStart = timeGetTime();

    EnterCriticalSection(&ddraw->cs);

    if (ddraw->primary && (ddraw->bpp == 16 || ddraw->primary->palette))
    {
        if (ddraw->vhack)
        {
            if (detect_cutscene())
            {
                ScaleW *= (float)CUTSCENE_WIDTH / ddraw->width;
                ScaleH *= (float)CUTSCENE_HEIGHT / ddraw->height;

                if (!InterlockedExchange(&ddraw->incutscene, TRUE))
                    scaleChanged = TRUE;
            }
            else
            {
                if (InterlockedExchange(&ddraw->incutscene, FALSE))
                    scaleChanged = TRUE;
            }
        }

        if (ddraw->bpp == 8 && InterlockedExchange(&ddraw->render.paletteUpdated, FALSE))
        {
            if (++palIndex >= TEXTURE_COUNT)
                palIndex = 0;

            glBindTexture(GL_TEXTURE_2D, PaletteTexIds[palIndex]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                256,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                ddraw->primary->palette->data_bgr);
        }

        if (InterlockedExchange(&ddraw->render.surfaceUpdated, FALSE))
        {
            if (++texIndex >= TEXTURE_COUNT)
                texIndex = 0;

            glBindTexture(GL_TEXTURE_2D, SurfaceTexIds[texIndex]);

            if (AdjustAlignment)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                ddraw->width,
                ddraw->height,
                SurfaceFormat,
                SurfaceType,
                ddraw->primary->surface);

            if (AdjustAlignment)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }

        static int errorCheckCount = 0;
        if (errorCheckCount < 20)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            errorCheckCount++;

            if (glGetError() != GL_NO_ERROR)
                UseOpenGL = FALSE;
        }

        if (!ddraw->handlemouse)
        {
            ChildWindowExists = FALSE;
            EnumChildWindows(ddraw->hWnd, EnumChildProc, (LPARAM)ddraw->primary);

            if (ddraw->render.width != ddraw->width || ddraw->render.height != ddraw->height)
            {
                if (ChildWindowExists)
                {
                    glClear(GL_COLOR_BUFFER_BIT);

                    if (!needsUpdate)
                    {
                        glViewport(0, ddraw->render.height - ddraw->height, ddraw->width, ddraw->height);
                        needsUpdate = TRUE;
                    }
                }
                else if (needsUpdate)
                {
                    glViewport(
                        ddraw->render.viewport.x, ddraw->render.viewport.y,
                        ddraw->render.viewport.width, ddraw->render.viewport.height);

                    needsUpdate = FALSE;
                }
            }
        }
    }

    LeaveCriticalSection(&ddraw->cs);

    if (scaleChanged)
    {
        if (ScaleProgram && MainProgram)
        {
            glBindVertexArray(MainVAO);
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
            GLfloat texCoord[] = {
                0.0f,   0.0f,
                0.0f,   ScaleH,
                ScaleW, ScaleH,
                ScaleW, 0.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
            glVertexAttribPointer(MainTexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(MainTexCoordAttrLoc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        else if (OpenGL_GotVersion3 && MainProgram)
        {
            glBindVertexArray(MainVAO);
            glBindBuffer(GL_ARRAY_BUFFER, MainVBOs[1]);
            GLfloat texCoord[] = {
                0.0f,    0.0f,
                ScaleW,  0.0f,
                ScaleW,  ScaleH,
                0.0f,    ScaleH,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
            glVertexAttribPointer(MainTexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(MainTexCoordAttrLoc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, SurfaceTexIds[texIndex]);

    if (ddraw->bpp == 8)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, PaletteTexIds[palIndex]);

        glActiveTexture(GL_TEXTURE0);
    }

    if (ScaleProgram && MainProgram)
    {
        // draw surface into framebuffer
        glUseProgram(MainProgram);

        glViewport(0, 0, ddraw->width, ddraw->height);

        glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);

        glBindVertexArray(MainVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (ChildWindowExists)
            glViewport(0, ddraw->render.height - ddraw->height, ddraw->width, ddraw->height);
        else
            glViewport(
                ddraw->render.viewport.x, ddraw->render.viewport.y,
                ddraw->render.viewport.width, ddraw->render.viewport.height);

        // apply filter

        glUseProgram(ScaleProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, FrameBufferTexId);

        static int frames = 1;
        if (FrameCountUniLoc != -1)
            glUniform1i(FrameCountUniLoc, frames++);

        glBindVertexArray(ScaleVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);
    }
    else if (OpenGL_GotVersion3 && MainProgram)
    {
        glBindVertexArray(MainVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);
    }
    else
    {
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 0);             glVertex2f(-1, 1);
        glTexCoord2f(ScaleW, 0);        glVertex2f(1, 1);
        glTexCoord2f(ScaleW, ScaleH);   glVertex2f(1, -1);
        glTexCoord2f(0, ScaleH);        glVertex2f(-1, -1);
        glEnd();
    }

    if (Am_I_Beta() || Am_I_Multiverse())
        (*poptb_callback_func)();

    if (ddraw->bnetActive)
        glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers(ddraw->render.hDC);

#if _DEBUG
    DrawFrameInfoEnd();
#endif

    if (ddraw->fpsLimiter.ticklength > 0)
    {
        if (ddraw->fpsLimiter.hTimer)
        {
            if (ddraw->vsync)
            {
                WaitForSingleObject(ddraw->fpsLimiter.hTimer, ddraw->fpsLimiter.ticklength * 2);
                LARGE_INTEGER liDueTime = { .QuadPart = -ddraw->fpsLimiter.tickLengthNs };
                SetWaitableTimer(ddraw->fpsLimiter.hTimer, &liDueTime, 0, NULL, NULL, FALSE);
            }
            else
            {
                FILETIME ft = { 0 };
                GetSystemTimeAsFileTime(&ft);

                if (CompareFileTime((FILETIME *)&ddraw->fpsLimiter.dueTime, &ft) == -1)
                {
                    memcpy(&ddraw->fpsLimiter.dueTime, &ft, sizeof(LARGE_INTEGER));
                }
                else
                {
                    WaitForSingleObject(ddraw->fpsLimiter.hTimer, ddraw->fpsLimiter.ticklength * 2);
                }

                ddraw->fpsLimiter.dueTime.QuadPart += ddraw->fpsLimiter.tickLengthNs;
                SetWaitableTimer(ddraw->fpsLimiter.hTimer, &ddraw->fpsLimiter.dueTime, 0, NULL, NULL, FALSE);
            }
        }
        else
        {
            if (!Am_I_Beta())
            {
                tickEnd = timeGetTime();

                if (tickEnd - tickStart < ddraw->fpsLimiter.ticklength)
                    Sleep(ddraw->fpsLimiter.ticklength - (tickEnd - tickStart));
            }
        }
    }
}

static void Render()
{
    static int first_init = 1;

    if (first_init)
    {
        glViewport(ddraw->render.viewport.x, ddraw->render.viewport.y, ddraw->render.viewport.width, ddraw->render.viewport.height);

        if (MainProgram)
            glUseProgram(MainProgram);
        else if (ddraw->bpp == 16)
            glEnable(GL_TEXTURE_2D);

        first_init = 0;
    }

    if (Am_I_Beta() || Am_I_Multiverse())
    {
        Renderer_Impl();
        return;
    }
    else
    {
        while (UseOpenGL && ddraw->render.run && WaitForSingleObject(ddraw->render.sem, INFINITE) != WAIT_FAILED)
        {
            Renderer_Impl();
        }
    }
}

static void DeleteContext(HGLRC context)
{
    HeapFree(GetProcessHeap(), 0, SurfaceTex);
    glDeleteTextures(TEXTURE_COUNT, SurfaceTexIds);

    if (ddraw->bpp == 8)
        glDeleteTextures(TEXTURE_COUNT, PaletteTexIds);

    if (glUseProgram)
        glUseProgram(0);

    if (ScaleProgram)
    {
        glDeleteTextures(1, &FrameBufferTexId);

        if (glDeleteBuffers)
            glDeleteBuffers(3, ScaleVBOs);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &FrameBufferId);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &ScaleVAO);
    }

    if (glDeleteProgram)
    {
        if (MainProgram)
            glDeleteProgram(MainProgram);

        if (ScaleProgram)
            glDeleteProgram(ScaleProgram);
    }

    if (OpenGL_GotVersion3)
    {
        if (MainProgram)
        {
            if (glDeleteBuffers)
                glDeleteBuffers(3, MainVBOs);

            if (glDeleteVertexArrays)
                glDeleteVertexArrays(1, &MainVAO);
        }
    }

    xwglMakeCurrent(NULL, NULL);
    xwglDeleteContext(context);
}

static BOOL TextureUploadTest()
{
    static char testData[] = { 0,1,2,0,0,2,3,0,0,4,5,0,0,6,7,0,0,8,9,0 };

    int i;
    for (i = 0; i < TEXTURE_COUNT; i++)
    {
        memcpy(SurfaceTex, testData, sizeof(testData));

        glBindTexture(GL_TEXTURE_2D, SurfaceTexIds[i]);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            ddraw->width,
            ddraw->height,
            SurfaceFormat,
            SurfaceType,
            SurfaceTex);

        memset(SurfaceTex, 0, sizeof(testData));

        glGetTexImage(GL_TEXTURE_2D, 0, SurfaceFormat, SurfaceType, SurfaceTex);

        if (memcmp(SurfaceTex, testData, sizeof(testData)) != 0)
            return FALSE;
    }

    if (ddraw->bpp == 8)
    {
        for (i = 0; i < TEXTURE_COUNT; i++)
        {
            glBindTexture(GL_TEXTURE_2D, PaletteTexIds[i]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                256,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                SurfaceTex);

            memset(SurfaceTex, 0, sizeof(testData));

            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, SurfaceTex);

            if (memcmp(SurfaceTex, testData, sizeof(testData)) != 0)
                return FALSE;
        }
    }
    return TRUE;
}

static BOOL ShaderTest()
{
    BOOL result = TRUE;

    if (ddraw->bpp != 8)
        return result;

    if (OpenGL_GotVersion3 && MainProgram)
    {
        memset(SurfaceTex, 0, SurfaceTexHeight * SurfaceTexWidth * sizeof(int));

        GLuint fboId = 0;
        glGenFramebuffers(1, &fboId);

        glBindFramebuffer(GL_FRAMEBUFFER, fboId);

        GLuint fboTexId = 0;
        glGenTextures(1, &fboTexId);
        glBindTexture(GL_TEXTURE_2D, fboTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SurfaceTexWidth, SurfaceTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, SurfaceTex);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexId, 0);

        GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, drawBuffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            static char gray0pal[] = { 128,128,128,128 };

            glBindTexture(GL_TEXTURE_2D, PaletteTexIds[0]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                1,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                gray0pal);

            glBindTexture(GL_TEXTURE_2D, SurfaceTexIds[0]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                SurfaceTexWidth,
                SurfaceTexHeight,
                SurfaceFormat,
                GL_UNSIGNED_BYTE,
                SurfaceTex);

            glViewport(0, 0, SurfaceTexWidth, SurfaceTexHeight);

            glUseProgram(MainProgram);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, PaletteTexIds[0]);
            glActiveTexture(GL_TEXTURE0);

            glBindVertexArray(MainVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, fboTexId);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, SurfaceTex);

            int i;
            for (i = 0; i < SurfaceTexHeight * SurfaceTexWidth; i++)
            {
                if (SurfaceTex[i] != 0x80808080)
                {
                    result = FALSE;
                    break;
                }  
            }
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &fboId);

        glDeleteTextures(1, &fboTexId);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    return result;
}
