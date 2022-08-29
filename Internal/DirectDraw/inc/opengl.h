#pragma once
#include "glcorearb.h"

// wgl
typedef HGLRC (APIENTRYP PFNWGLCREATECONTEXTPROC)(HDC);
typedef BOOL (APIENTRYP PFNWGLDELETECONTEXTPROC)(HGLRC);
typedef PROC (APIENTRYP PFNWGLGETPROCADDRESSPROC)(LPCSTR);
typedef BOOL (APIENTRYP PFNWGLMAKECURRENTPROC)(HDC, HGLRC);
typedef void (APIENTRYP PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef const char* (APIENTRYP PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);

extern PFNWGLCREATECONTEXTPROC xwglCreateContext;
extern PFNWGLDELETECONTEXTPROC xwglDeleteContext;
extern PFNWGLGETPROCADDRESSPROC xwglGetProcAddress;
extern PFNWGLMAKECURRENTPROC xwglMakeCurrent;
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;


//compat profile only --->
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE8                     0x8040
typedef void (APIENTRYP PFNGLBEGINPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLENDPROC)(void);
typedef void (APIENTRYP PFNGLTEXCOORD2FPROC)(GLfloat s, GLfloat t);
typedef void (APIENTRYP PFNGLVERTEX2FPROC)(GLfloat x, GLfloat y);

extern PFNGLBEGINPROC glBegin;
extern PFNGLENDPROC glEnd;
extern PFNGLTEXCOORD2FPROC glTexCoord2f;
extern PFNGLVERTEX2FPROC glVertex2f;
// <--- compat profile only


BOOL OpenGL_LoadDll();
void OpenGL_Init();
BOOL OpenGL_ExtExists(char *ext, HDC hdc);
GLuint OpenGL_BuildProgram(const GLchar *vertSource, const GLchar *fragSource);
GLuint OpenGL_BuildProgramFromFile(const char *filePath);

extern PFNGLVIEWPORTPROC glViewport;
extern PFNGLBINDTEXTUREPROC glBindTexture;
extern PFNGLGENTEXTURESPROC glGenTextures;
extern PFNGLTEXPARAMETERIPROC glTexParameteri;
extern PFNGLDELETETEXTURESPROC glDeleteTextures;
extern PFNGLTEXIMAGE2DPROC glTexImage2D;
extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
extern PFNGLGETERRORPROC glGetError;
extern PFNGLGETSTRINGPROC glGetString;
extern PFNGLGETTEXIMAGEPROC glGetTexImage;
extern PFNGLPIXELSTOREIPROC glPixelStorei;
extern PFNGLENABLEPROC glEnable;
extern PFNGLCLEARPROC glClear;

extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLDETACHSHADERPROC glDetachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1IVPROC glUniform1iv;
extern PFNGLUNIFORM2IVPROC glUniform2iv;
extern PFNGLUNIFORM3IVPROC glUniform3iv;
extern PFNGLUNIFORM4IVPROC glUniform4iv;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM1FVPROC glUniform1fv;
extern PFNGLUNIFORM2FVPROC glUniform2fv;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORM4FVPROC glUniform4fv;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
extern PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
extern PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
extern PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
extern PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;

extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;

extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLBINDBUFFERPROC	glBindBuffer;
extern PFNGLBUFFERDATAPROC	glBufferData;
extern PFNGLMAPBUFFERPROC glMapBuffer;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

extern PFNGLACTIVETEXTUREPROC glActiveTexture;

extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLDRAWBUFFERSPROC glDrawBuffers;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;

extern PFNGLTEXBUFFERPROC glTexBuffer;

extern HMODULE OpenGL_hModule;

extern BOOL OpenGL_GotVersion2;
extern BOOL OpenGL_GotVersion3;

extern char OpenGL_Version[];
