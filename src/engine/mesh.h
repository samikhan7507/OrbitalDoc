// Orbital Dock 3D - procedural meshes (sphere, box, frustum/cylinder, torus).
// Everything is generated in code, so there are no model files or licences to worry about.
// Author: Sami Khan
#pragma once
#include <vector>
#include <cstdint>
#include "gfx.h"
#include "../math/vec3.h"

struct Vertex { float p[3], n[3], uv[2]; };

class Mesh {
public:
    GLuint vao = 0, vbo = 0, ibo = 0;
    GLsizei count = 0;

    void upload(const std::vector<Vertex>& v, const std::vector<uint32_t>& idx) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(Vertex)), v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
        glBindVertexArray(0);
        count = (GLsizei)idx.size();
    }
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }
};

namespace MeshGen {

// Unit sphere (radius 1), y-up poles, uv = (longitude, latitude).
inline Mesh sphere(int seg, int rings) {
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    for (int j = 0; j <= rings; ++j) {
        float th = PI * j / rings;
        for (int i = 0; i <= seg; ++i) {
            float ph = 2 * PI * i / seg;
            float x = std::sin(th) * std::cos(ph), y = std::cos(th), z = std::sin(th) * std::sin(ph);
            v.push_back({{x, y, z}, {x, y, z}, {(float)i / seg, (float)j / rings}});
        }
    }
    for (int j = 0; j < rings; ++j)
        for (int i = 0; i < seg; ++i) {
            uint32_t a = j * (seg + 1) + i, b = a + seg + 1;
            idx.insert(idx.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    Mesh m; m.upload(v, idx); return m;
}

// Unit cube centred on the origin (side 1), per-face uv 0..1.
inline Mesh box() {
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    const Vec3 N[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int f = 0; f < 6; ++f) {
        Vec3 n = N[f];
        Vec3 t = std::fabs(n.y) > 0.5f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        Vec3 u = normalize(cross(n, t)), w = cross(n, u);
        uint32_t base = (uint32_t)v.size();
        const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
        for (int k = 0; k < 4; ++k) {
            Vec3 p = n * 0.5f + u * (sx[k] * 0.5f) + w * (sy[k] * 0.5f);
            v.push_back({{p.x, p.y, p.z}, {n.x, n.y, n.z}, {(sx[k] + 1) * 0.5f, (sy[k] + 1) * 0.5f}});
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    // fix winding so every face is counter-clockwise when seen from outside
    for (size_t i = 0; i < idx.size(); i += 3) {
        Vec3 a(v[idx[i]].p[0], v[idx[i]].p[1], v[idx[i]].p[2]);
        Vec3 b(v[idx[i+1]].p[0], v[idx[i+1]].p[1], v[idx[i+1]].p[2]);
        Vec3 c(v[idx[i+2]].p[0], v[idx[i+2]].p[1], v[idx[i+2]].p[2]);
        Vec3 n(v[idx[i]].n[0], v[idx[i]].n[1], v[idx[i]].n[2]);
        if (dot(cross(b - a, c - a), n) < 0) std::swap(idx[i + 1], idx[i + 2]);
    }
    Mesh m; m.upload(v, idx); return m;
}

// Frustum/cylinder along Z from z=-0.5 (radius rb) to z=+0.5 (radius rt), optional end caps.
inline Mesh frustum(int seg, float rb, float rt, bool caps = true) {
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    float slope = rb - rt;
    for (int i = 0; i <= seg; ++i) {
        float a = 2 * PI * i / seg, c = std::cos(a), s = std::sin(a);
        Vec3 n = normalize(Vec3(c, s, slope));
        v.push_back({{rb * c, rb * s, -0.5f}, {n.x, n.y, n.z}, {(float)i / seg, 0}});
        v.push_back({{rt * c, rt * s,  0.5f}, {n.x, n.y, n.z}, {(float)i / seg, 1}});
    }
    for (int i = 0; i < seg; ++i) {
        uint32_t a = 2 * i;
        idx.insert(idx.end(), {a, a + 2, a + 1, a + 1, a + 2, a + 3});
    }
    if (caps) {
        for (int e = 0; e < 2; ++e) {
            float z = e ? 0.5f : -0.5f, r = e ? rt : rb, nz = e ? 1.0f : -1.0f;
            uint32_t c0 = (uint32_t)v.size();
            v.push_back({{0, 0, z}, {0, 0, nz}, {0.5f, 0.5f}});
            for (int i = 0; i <= seg; ++i) {
                float a = 2 * PI * i / seg;
                v.push_back({{r * std::cos(a), r * std::sin(a), z}, {0, 0, nz}, {0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)}});
            }
            for (int i = 0; i < seg; ++i) {
                if (e) idx.insert(idx.end(), {c0, c0 + 1 + i, c0 + 2 + i});
                else   idx.insert(idx.end(), {c0, c0 + 2 + i, c0 + 1 + i});
            }
        }
    }
    Mesh m; m.upload(v, idx); return m;
}

// Torus around the Z axis. uv.x runs around the big circle, uv.y around the tube.
inline Mesh torus(float R, float r, int seg, int sides) {
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    for (int i = 0; i <= seg; ++i) {
        float u = 2 * PI * i / seg, cu = std::cos(u), su = std::sin(u);
        for (int j = 0; j <= sides; ++j) {
            float w = 2 * PI * j / sides, cw = std::cos(w), sw = std::sin(w);
            v.push_back({{(R + r * cw) * cu, (R + r * cw) * su, r * sw}, {cw * cu, cw * su, sw}, {(float)i / seg, (float)j / sides}});
        }
    }
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < sides; ++j) {
            uint32_t a = i * (sides + 1) + j, b = a + sides + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    Mesh m; m.upload(v, idx); return m;
}

} // namespace MeshGen
