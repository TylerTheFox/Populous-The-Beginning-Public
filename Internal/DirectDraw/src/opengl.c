#include <windows.h>
#include <stdio.h>
#include "opengl.h"

PFNWGLCREATECONTEXTPROC xwglCreateContext;
PFNWGLDELETECONTEXTPROC xwglDeleteContext;
PFNWGLGETPROCADDRESSPROC xwglGetProcAddress;
PFNWGLMAKECURRENTPROC xwglMakeCurrent;

PFNGLVIEWPORTPROC glViewport;
PFNGLBINDTEXTUREPROC glBindTexture;
PFNGLGENTEXTURESPROC glGenTextures;
PFNGLTEXPARAMETERIPROC glTexParameteri;
PFNGLDELETETEXTURESPROC glDeleteTextures;
PFNGLTEXIMAGE2DPROC glTexImage2D;
PFNGLDRAWELEMENTSPROC glDrawElements;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
PFNGLGETERRORPROC glGetError;
PFNGLGETSTRINGPROC glGetString;
PFNGLGETTEXIMAGEPROC glGetTexImage;
PFNGLPIXELSTOREIPROC glPixelStorei;
PFNGLENABLEPROC glEnable;
PFNGLCLEARPROC glClear;

PFNGLBEGINPROC glBegin;
PFNGLENDPROC glEnd;
PFNGLTEXCOORD2FPROC glTexCoord2f;
PFNGLVERTEX2FPROC glVertex2f;

// Program
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORM1IVPROC glUniform1iv;
PFNGLUNIFORM2IVPROC glUniform2iv;
PFNGLUNIFORM3IVPROC glUniform3iv;
PFNGLUNIFORM4IVPROC glUniform4iv;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1FVPROC glUniform1fv;
PFNGLUNIFORM2FVPROC glUniform2fv;
PFNGLUNIFORM3FVPROC glUniform3fv;
PFNGLUNIFORM4FVPROC glUniform4fv;
PFNGLUNIFORM4FPROC glUniform4f;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;

PFNGLCREATESHADERPROC glCreateShader;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;

PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLMAPBUFFERPROC glMapBuffer;
PFNGLUNMAPBUFFERPROC glUnmapBuffer;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

PFNGLACTIVETEXTUREPROC glActiveTexture;

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
PFNGLDRAWBUFFERSPROC glDrawBuffers;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;

PFNGLTEXBUFFERPROC glTexBuffer;

HMODULE OpenGL_hModule;

BOOL OpenGL_GotVersion2;
BOOL OpenGL_GotVersion3;

char OpenGL_Version[128];

BOOL OpenGL_LoadDll()
{
    if (!OpenGL_hModule)
        OpenGL_hModule = LoadLibrary("opengl32.dll");

    if (OpenGL_hModule)
    {
        xwglCreateContext = (PFNWGLCREATECONTEXTPROC)GetProcAddress(OpenGL_hModule, "wglCreateContext");
        xwglDeleteContext = (PFNWGLDELETECONTEXTPROC)GetProcAddress(OpenGL_hModule, "wglDeleteContext");
        xwglGetProcAddress = (PFNWGLGETPROCADDRESSPROC)GetProcAddress(OpenGL_hModule, "wglGetProcAddress");
        xwglMakeCurrent = (PFNWGLMAKECURRENTPROC)GetProcAddress(OpenGL_hModule, "wglMakeCurrent");

        glViewport = (PFNGLVIEWPORTPROC)GetProcAddress(OpenGL_hModule, "glViewport");
        glBindTexture = (PFNGLBINDTEXTUREPROC)GetProcAddress(OpenGL_hModule, "glBindTexture");
        glGenTextures = (PFNGLGENTEXTURESPROC)GetProcAddress(OpenGL_hModule, "glGenTextures");
        glTexParameteri = (PFNGLTEXPARAMETERIPROC)GetProcAddress(OpenGL_hModule, "glTexParameteri");
        glDeleteTextures = (PFNGLDELETETEXTURESPROC)GetProcAddress(OpenGL_hModule, "glDeleteTextures");
        glTexImage2D = (PFNGLTEXIMAGE2DPROC)GetProcAddress(OpenGL_hModule, "glTexImage2D");
        glDrawElements = (PFNGLDRAWELEMENTSPROC)GetProcAddress(OpenGL_hModule, "glDrawElements");
        glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)GetProcAddress(OpenGL_hModule, "glTexSubImage2D");
        glGetError = (PFNGLGETERRORPROC)GetProcAddress(OpenGL_hModule, "glGetError");
        glGetString = (PFNGLGETSTRINGPROC)GetProcAddress(OpenGL_hModule, "glGetString");
        glGetTexImage = (PFNGLGETTEXIMAGEPROC)GetProcAddress(OpenGL_hModule, "glGetTexImage");
        glPixelStorei = (PFNGLPIXELSTOREIPROC)GetProcAddress(OpenGL_hModule, "glPixelStorei");
        glEnable = (PFNGLENABLEPROC)GetProcAddress(OpenGL_hModule, "glEnable");
        glClear = (PFNGLCLEARPROC)GetProcAddress(OpenGL_hModule, "glClear");

        glBegin = (PFNGLBEGINPROC)GetProcAddress(OpenGL_hModule, "glBegin");
        glEnd = (PFNGLENDPROC)GetProcAddress(OpenGL_hModule, "glEnd");
        glTexCoord2f = (PFNGLTEXCOORD2FPROC)GetProcAddress(OpenGL_hModule, "glTexCoord2f");
        glVertex2f = (PFNGLVERTEX2FPROC)GetProcAddress(OpenGL_hModule, "glVertex2f");
    }

    return xwglCreateContext && xwglDeleteContext && xwglGetProcAddress && xwglMakeCurrent && glViewport &&
        glBindTexture && glGenTextures && glTexParameteri && glDeleteTextures && glTexImage2D &&
        glDrawElements && glTexSubImage2D && glGetError && glGetString && glGetTexImage && glPixelStorei &&
        glEnable && glClear && glBegin && glEnd && glTexCoord2f && glVertex2f;
}

void OpenGL_Init()
{
    // Program
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)xwglGetProcAddress("glCreateProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)xwglGetProcAddress("glDeleteProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)xwglGetProcAddress("glUseProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)xwglGetProcAddress("glAttachShader");
    glDetachShader = (PFNGLDETACHSHADERPROC)xwglGetProcAddress("glDetachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)xwglGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)xwglGetProcAddress("glGetProgramiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)xwglGetProcAddress("glGetShaderInfoLog");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)xwglGetProcAddress("glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)xwglGetProcAddress("glUniform1i");
    glUniform1iv = (PFNGLUNIFORM1IVPROC)xwglGetProcAddress("glUniform1iv");
    glUniform2iv = (PFNGLUNIFORM2IVPROC)xwglGetProcAddress("glUniform2iv");
    glUniform3iv = (PFNGLUNIFORM3IVPROC)xwglGetProcAddress("glUniform3iv");
    glUniform4iv = (PFNGLUNIFORM4IVPROC)xwglGetProcAddress("glUniform4iv");
    glUniform1f = (PFNGLUNIFORM1FPROC)xwglGetProcAddress("glUniform1f");
    glUniform1fv = (PFNGLUNIFORM1FVPROC)xwglGetProcAddress("glUniform1fv");
    glUniform2fv = (PFNGLUNIFORM2FVPROC)xwglGetProcAddress("glUniform2fv");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)xwglGetProcAddress("glUniform3fv");
    glUniform4fv = (PFNGLUNIFORM4FVPROC)xwglGetProcAddress("glUniform4fv");
    glUniform4f = (PFNGLUNIFORM4FPROC)xwglGetProcAddress("glUniform4f");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)xwglGetProcAddress("glUniformMatrix4fv");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)xwglGetProcAddress("glGetAttribLocation");
    glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)xwglGetProcAddress("glVertexAttrib1f");
    glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)xwglGetProcAddress("glVertexAttrib1fv");
    glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)xwglGetProcAddress("glVertexAttrib2fv");
    glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)xwglGetProcAddress("glVertexAttrib3fv");
    glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)xwglGetProcAddress("glVertexAttrib4fv");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)xwglGetProcAddress("glEnableVertexAttribArray");
    glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)xwglGetProcAddress("glBindAttribLocation");

    glCreateShader = (PFNGLCREATESHADERPROC)xwglGetProcAddress("glCreateShader");
    glDeleteShader = (PFNGLDELETESHADERPROC)xwglGetProcAddress("glDeleteShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)xwglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)xwglGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)xwglGetProcAddress("glGetShaderiv");

    glGenBuffers = (PFNGLGENBUFFERSPROC)xwglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)xwglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)xwglGetProcAddress("glBufferData");
    glMapBuffer = (PFNGLMAPBUFFERPROC)xwglGetProcAddress("glMapBuffer");
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)xwglGetProcAddress("glUnmapBuffer");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)xwglGetProcAddress("glBufferSubData");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)xwglGetProcAddress("glVertexAttribPointer");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)xwglGetProcAddress("glDeleteBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)xwglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)xwglGetProcAddress("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)xwglGetProcAddress("glDeleteVertexArrays");

    glActiveTexture = (PFNGLACTIVETEXTUREPROC)xwglGetProcAddress("glActiveTexture");

    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)xwglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)xwglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)xwglGetProcAddress("glFramebufferTexture2D");
    glDrawBuffers = (PFNGLDRAWBUFFERSPROC)xwglGetProcAddress("glDrawBuffers");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)xwglGetProcAddress("glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)xwglGetProcAddress("glDeleteFramebuffers");

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)xwglGetProcAddress("wglSwapIntervalEXT");
    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)xwglGetProcAddress("wglGetExtensionsStringARB");

    glTexBuffer = (PFNGLTEXBUFFERPROC)xwglGetProcAddress("glTexBuffer");

    char *glversion = (char *)glGetString(GL_VERSION);
    if (glversion)
    {
        strncpy(OpenGL_Version, glversion, sizeof(OpenGL_Version));
        const char deli[2] = " ";
        strtok(OpenGL_Version, deli);
    }
    else
        OpenGL_Version[0] = '0';

    OpenGL_GotVersion2 = glGetUniformLocation && glActiveTexture && glUniform1i;

    OpenGL_GotVersion3 = glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glDrawBuffers &&
        glCheckFramebufferStatus && glUniform4f && glActiveTexture && glUniform1i &&
        glGetAttribLocation && glGenBuffers && glBindBuffer && glBufferData && glVertexAttribPointer &&
        glEnableVertexAttribArray && glUniform2fv && glUniformMatrix4fv && glGenVertexArrays && glBindVertexArray &&
        glGetUniformLocation && glversion && glversion[0] != '2';
}

BOOL OpenGL_ExtExists(char *ext, HDC hdc)
{
    char *glext = (char *)glGetString(GL_EXTENSIONS);
    if (glext)
    {
        if (strstr(glext, ext))
            return TRUE;
    }

    if (wglGetExtensionsStringARB)
    {
        char *wglext = (char *)wglGetExtensionsStringARB(hdc);
        if (wglext)
        {
            if (strstr(wglext, ext))
                return TRUE;
        }
    }

    return FALSE;
}

GLuint OpenGL_BuildProgram(const GLchar *vertSource, const GLchar *fragSource)
{
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glCreateProgram ||
        !glAttachShader || !glLinkProgram || !glUseProgram || !glDetachShader)
        return 0;

    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!vertShader || !fragShader)
        return 0;

    glShaderSource(vertShader, 1, &vertSource, NULL);
    glShaderSource(fragShader, 1, &fragSource, NULL);

    GLint isCompiled = 0;

    glCompileShader(vertShader);
    if (glGetShaderiv)
    {
        glGetShaderiv(vertShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            if (glDeleteShader)
                glDeleteShader(vertShader);

            return 0;
        }
    }

    glCompileShader(fragShader);
    if (glGetShaderiv)
    {
        glGetShaderiv(fragShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            if (glDeleteShader)
            {
                glDeleteShader(fragShader);
                glDeleteShader(vertShader);
            }

            return 0;
        }
    }

    GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);

        glDetachShader(program, vertShader);
        glDetachShader(program, fragShader);
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        if (glGetProgramiv)
        {
            GLint isLinked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
            if (isLinked == GL_FALSE)
            {
                if (glDeleteProgram)
                    glDeleteProgram(program);

                return 0;
            }
        }
    }

    return program;
}

GLuint OpenGL_BuildProgramFromFile(const char *filePath)
{
    GLuint program = 0;

    FILE *file = fopen(filePath, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *source = fileSize > 0 ? calloc(fileSize + 1, 1) : NULL;
        if (source)
        {
            fread(source, fileSize, 1, file);
            fclose(file);

            char *vertSource = calloc(fileSize + 50, 1);
            char *fragSource = calloc(fileSize + 50, 1);

            if (fragSource && vertSource)
            {
                char *versionStart = strstr(source, "#version");
                if (versionStart)
                {
                    const char deli[2] = "\n";
                    char *version = strtok(versionStart, deli);

                    strcpy(vertSource, source);
                    strcpy(fragSource, source);
                    strcat(vertSource, "\n#define VERTEX\n");
                    strcat(fragSource, "\n#define FRAGMENT\n");
                    strcat(vertSource, version + strlen(version) + 1);
                    strcat(fragSource, version + strlen(version) + 1);

                    program = OpenGL_BuildProgram(vertSource, fragSource);
                }
                else
                {
                    strcpy(vertSource, "#define VERTEX\n");
                    strcpy(fragSource, "#define FRAGMENT\n");
                    strcat(vertSource, source);
                    strcat(fragSource, source);

                    program = OpenGL_BuildProgram(vertSource, fragSource);
                }

                free(vertSource);
                free(fragSource);
            }

            free(source);
        }
    }

    return program;
}
