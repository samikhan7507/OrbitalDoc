// Orbital Dock 3D - Mat4: column-major 4x4 matrix (same memory layout OpenGL expects).
// Element (row r, column c) lives at m[c*4 + r].
// Author: Sami Khan
#pragma once
#include "vec3.h"

struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    // ---- Translation: moves a point by t. Last column holds the offset. ----
    static Mat4 translate(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    // ---- Scaling: stretches each axis independently along the diagonal. ----
    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }
    // ---- Rotation about an arbitrary axis (Rodrigues' formula as a matrix). ----
    static Mat4 rotate(const Vec3& axis, float rad) {
        Vec3 a = normalize(axis);
        float c = std::cos(rad), s = std::sin(rad), t = 1.0f - c;
        Mat4 r = identity();
        r.m[0] = t * a.x * a.x + c;       r.m[4] = t * a.x * a.y - s * a.z; r.m[8]  = t * a.x * a.z + s * a.y;
        r.m[1] = t * a.x * a.y + s * a.z; r.m[5] = t * a.y * a.y + c;       r.m[9]  = t * a.y * a.z - s * a.x;
        r.m[2] = t * a.x * a.z - s * a.y; r.m[6] = t * a.y * a.z + s * a.x; r.m[10] = t * a.z * a.z + c;
        return r;
    }
    static Mat4 rotateX(float r) { return rotate({1, 0, 0}, r); }
    static Mat4 rotateY(float r) { return rotate({0, 1, 0}, r); }
    static Mat4 rotateZ(float r) { return rotate({0, 0, 1}, r); }

    // ---- Perspective projection (view space -> clip space), OpenGL depth range. ----
    static Mat4 perspective(float fovy, float aspect, float zn, float zf) {
        float f = 1.0f / std::tan(fovy * 0.5f);
        Mat4 r{};
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zf * zn) / (zn - zf);
        return r;
    }
    // ---- Orthographic projection (used for the Sun's shadow map: parallel rays, no perspective). ----
    static Mat4 ortho(float l, float r, float b, float t, float zn, float zf) {
        Mat4 m = identity();
        m.m[0] = 2.0f / (r - l);  m.m[5] = 2.0f / (t - b);  m.m[10] = -2.0f / (zf - zn);
        m.m[12] = -(r + l) / (r - l); m.m[13] = -(t + b) / (t - b); m.m[14] = -(zf + zn) / (zf - zn);
        return m;
    }
    // ---- Camera (view) matrix: builds an orthonormal basis then inverts the camera pose. ----
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = normalize(target - eye);       // forward
        Vec3 s = normalize(cross(f, up));       // right
        Vec3 u = cross(s, f);                   // true up
        Mat4 r = identity();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
        return r;
    }

    // result = (*this) * b  -> b is applied first, then *this.
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + row] * b.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }
        return r;
    }
    Vec3 transformPoint(const Vec3& p) const {
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        return {(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / w,
                (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / w,
                (m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]) / w};
    }
    Vec3 transformDir(const Vec3& d) const {
        return {m[0] * d.x + m[4] * d.y + m[8] * d.z,
                m[1] * d.x + m[5] * d.y + m[9] * d.z,
                m[2] * d.x + m[6] * d.y + m[10] * d.z};
    }
    Mat4 transposed() const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) r.m[c * 4 + row] = m[row * 4 + c];
        return r;
    }
    // General 4x4 inverse by cofactors (used for the normal matrix and the sky shader).
    Mat4 inverse() const {
        const float* a = m;
        float inv[16];
        inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
        inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
        inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
        inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
        inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
        inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
        inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11] - a[4]*a[3]*a[9]  - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
        inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10] + a[4]*a[2]*a[9]  + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];
        float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
        Mat4 r = identity();
        if (std::fabs(det) < 1e-20f) return r;
        det = 1.0f / det;
        for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * det;
        return r;
    }
    // Normal matrix = transpose(inverse(upper-left 3x3)). Keeps lighting correct under non-uniform scale.
    void normalMatrix3(float out[9]) const {
        Mat4 inv = inverse();
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r) out[c * 3 + r] = inv.m[r * 4 + c];
    }
    // Remove translation (used by the sky shader so stars never move with the camera).
    Mat4 rotationOnly() const {
        Mat4 r = *this;
        r.m[12] = r.m[13] = r.m[14] = 0.0f;
        return r;
    }
};
