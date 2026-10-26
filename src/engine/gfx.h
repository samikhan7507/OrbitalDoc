// Orbital Dock 3D - graphics API layer.
// Web build  : uses the browser's WebGL2 (= OpenGL ES 3.0) through Emscripten.
// Native build: loads OpenGL 3.3 core functions at runtime (tiny built-in loader, no extra libs).
// Author: Sami Khan
#pragma once
#ifdef __EMSCRIPTEN__
  #include <GLES3/gl3.h>
  inline bool loadGL(void* (*)(const char*)) { return true; }
#else
  #include <cstddef>
  #ifdef _WIN32
    #define GLAPI_CALL __stdcall
  #else
    #define GLAPI_CALL
  #endif
  typedef unsigned int GLenum; typedef unsigned char GLboolean; typedef unsigned int GLbitfield;
  typedef int GLint; typedef unsigned int GLuint; typedef int GLsizei; typedef float GLfloat;
  typedef char GLchar; typedef ptrdiff_t GLsizeiptr;

  #define GL_FALSE 0
  #define GL_TRUE 1
  #define GL_COLOR_BUFFER_BIT 0x4000
  #define GL_DEPTH_BUFFER_BIT 0x0100
  #define GL_DEPTH_TEST 0x0B71
  #define GL_CULL_FACE 0x0B44
  #define GL_BLEND 0x0BE2
  #define GL_ONE 1
  #define GL_SRC_ALPHA 0x0302
  #define GL_ONE_MINUS_SRC_ALPHA 0x0303
  #define GL_FRONT 0x0404
  #define GL_BACK 0x0405
  #define GL_LESS 0x0201
  #define GL_LEQUAL 0x0203
  #define GL_TRIANGLES 4
  #define GL_UNSIGNED_BYTE 0x1401
  #define GL_UNSIGNED_INT 0x1405
  #define GL_FLOAT 0x1406
  #define GL_RGBA 0x1908
  #define GL_ARRAY_BUFFER 0x8892
  #define GL_ELEMENT_ARRAY_BUFFER 0x8893
  #define GL_STATIC_DRAW 0x88E4
  #define GL_FRAGMENT_SHADER 0x8B30
  #define GL_VERTEX_SHADER 0x8B31
  #define GL_COMPILE_STATUS 0x8B81
  #define GL_LINK_STATUS 0x8B82
  #define GL_INFO_LOG_LENGTH 0x8B84

  #define GL_POINTS 0
  #define GL_NONE 0
  #define GL_TEXTURE_2D 0x0DE1
  #define GL_TEXTURE0 0x84C0
  #define GL_RGBA8 0x8058
  #define GL_DEPTH_COMPONENT 0x1902
  #define GL_DEPTH_COMPONENT24 0x81A6
  #define GL_DEPTH_COMPONENT16 0x81A5
  #define GL_TEXTURE_MIN_FILTER 0x2801
  #define GL_TEXTURE_MAG_FILTER 0x2800
  #define GL_TEXTURE_WRAP_S 0x2802
  #define GL_TEXTURE_WRAP_T 0x2803
  #define GL_LINEAR 0x2601
  #define GL_NEAREST 0x2600
  #define GL_CLAMP_TO_EDGE 0x812F
  #define GL_FRAMEBUFFER 0x8D40
  #define GL_RENDERBUFFER 0x8D41
  #define GL_COLOR_ATTACHMENT0 0x8CE0
  #define GL_DEPTH_ATTACHMENT 0x8D00
  #define GL_FRAMEBUFFER_COMPLETE 0x8CD5
  #define GL_PROGRAM_POINT_SIZE 0x8642
  #define GL_DYNAMIC_DRAW 0x88E8
  #define GL_UNSIGNED_SHORT 0x1403
  #define GL_POLYGON_OFFSET_FILL 0x8037

  #define GL_FUNCS(X) \
    X(void, glGenTextures, (GLsizei, GLuint*)) \
    X(void, glDeleteTextures, (GLsizei, const GLuint*)) \
    X(void, glBindTexture, (GLenum, GLuint)) \
    X(void, glActiveTexture, (GLenum)) \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    X(void, glGenFramebuffers, (GLsizei, GLuint*)) \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint*)) \
    X(void, glBindFramebuffer, (GLenum, GLuint)) \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
    X(GLenum, glCheckFramebufferStatus, (GLenum)) \
    X(void, glGenRenderbuffers, (GLsizei, GLuint*)) \
    X(void, glDeleteRenderbuffers, (GLsizei, const GLuint*)) \
    X(void, glBindRenderbuffer, (GLenum, GLuint)) \
    X(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
    X(void, glDrawBuffers, (GLsizei, const GLenum*)) \
    X(void, glReadBuffer, (GLenum)) \
    X(void, glBufferSubData, (GLenum, ptrdiff_t, GLsizeiptr, const void*)) \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, glPolygonOffset, (GLfloat, GLfloat)) \
    X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, glClear, (GLbitfield)) \
    X(void, glEnable, (GLenum)) \
    X(void, glDisable, (GLenum)) \
    X(void, glBlendFunc, (GLenum, GLenum)) \
    X(void, glDepthMask, (GLboolean)) \
    X(void, glDepthFunc, (GLenum)) \
    X(void, glCullFace, (GLenum)) \
    X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*)) \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
    X(GLuint, glCreateShader, (GLenum)) \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, glCompileShader, (GLuint)) \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glDeleteShader, (GLuint)) \
    X(GLuint, glCreateProgram, (void)) \
    X(void, glAttachShader, (GLuint, GLuint)) \
    X(void, glLinkProgram, (GLuint)) \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glUseProgram, (GLuint)) \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*)) \
    X(void, glUniform1f, (GLint, GLfloat)) \
    X(void, glUniform1i, (GLint, GLint)) \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat)) \
    X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
    X(void, glUniformMatrix3fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void, glGenBuffers, (GLsizei, GLuint*)) \
    X(void, glBindBuffer, (GLenum, GLuint)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, glGenVertexArrays, (GLsizei, GLuint*)) \
    X(void, glBindVertexArray, (GLuint)) \
    X(void, glEnableVertexAttribArray, (GLuint)) \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))

  #define X(ret, name, args) typedef ret (GLAPI_CALL *name##_t) args; extern name##_t name;
  GL_FUNCS(X)
  #undef X
  bool loadGL(void* (*getProc)(const char*));
#endif
