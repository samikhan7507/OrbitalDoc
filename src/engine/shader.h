// Orbital Dock 3D - GLSL program wrapper with uniform-location cache.
// Author: Sami Khan
#pragma once
#include <string>
#include <unordered_map>
#include <cstdio>
#include <vector>
#include "gfx.h"
#include "../math/quat.h"

class Shader {
public:
    GLuint prog = 0;

    bool build(const char* vsSrc, const char* fsSrc, const char* name) {
#ifdef __EMSCRIPTEN__
        const char* prefix = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#else
        const char* prefix = "#version 330 core\n";
#endif
        GLuint vs = compile(GL_VERTEX_SHADER, prefix, vsSrc, name);
        GLuint fs = compile(GL_FRAGMENT_SHADER, prefix, fsSrc, name);
        if (!vs || !fs) return false;
        prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(prog, 2048, nullptr, log);
            std::fprintf(stderr, "[shader:%s] link error:\n%s\n", name, log);
            return false;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return true;
    }
    void use() const { glUseProgram(prog); }
    void setF(const char* n, float v) { glUniform1f(loc(n), v); }
    void setI(const char* n, int v) { glUniform1i(loc(n), v); }
    void setF2(const char* n, float a, float b) { glUniform2f(loc(n), a, b); }
    void setV3(const char* n, const Vec3& v) { glUniform3f(loc(n), v.x, v.y, v.z); }
    void setM4(const char* n, const Mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, m.m); }
    void setM3(const char* n, const float* m) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, m); }

private:
    std::unordered_map<std::string, GLint> cache_;
    GLint loc(const char* n) {
        auto it = cache_.find(n);
        if (it != cache_.end()) return it->second;
        GLint l = glGetUniformLocation(prog, n);
        cache_[n] = l;
        return l;
    }
    static GLuint compile(GLenum type, const char* prefix, const char* src, const char* name) {
        GLuint s = glCreateShader(type);
        const char* parts[2] = {prefix, src};
        glShaderSource(s, 2, parts, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetShaderInfoLog(s, 4096, nullptr, log);
            std::fprintf(stderr, "[shader:%s] %s compile error:\n%s\n", name, type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
            return 0;
        }
        return s;
    }
};
