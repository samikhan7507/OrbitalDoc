// Orbital Dock 3D - Quaternion: rotation without gimbal lock, plus SLERP.
// Author: Sami Khan
#pragma once
#include "mat4.h"

struct Quat {
    float w, x, y, z;
    constexpr Quat() : w(1), x(0), y(0), z(0) {}
    constexpr Quat(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}

    static Quat fromAxisAngle(const Vec3& axis, float rad) {
        Vec3 a = normalize(axis);
        float s = std::sin(rad * 0.5f);
        return {std::cos(rad * 0.5f), a.x * s, a.y * s, a.z * s};
    }
    // Shortest rotation taking direction a onto direction b.
    static Quat fromTo(const Vec3& a, const Vec3& b) {
        Vec3 u = normalize(a), v = normalize(b);
        float d = dot(u, v);
        if (d > 0.999999f) return Quat();
        if (d < -0.999999f) {
            Vec3 ax = cross(Vec3(1, 0, 0), u);
            if (length(ax) < 1e-4f) ax = cross(Vec3(0, 1, 0), u);
            return fromAxisAngle(ax, PI);
        }
        Vec3 c = cross(u, v);
        Quat q(1.0f + d, c.x, c.y, c.z);
        return q.normalized();
    }
    // Hamilton product: (a*b) applies b first, then a.
    Quat operator*(const Quat& o) const {
        return {w * o.w - x * o.x - y * o.y - z * o.z,
                w * o.x + x * o.w + y * o.z - z * o.y,
                w * o.y - x * o.z + y * o.w + z * o.x,
                w * o.z + x * o.y - y * o.x + z * o.w};
    }
    Quat conjugate() const { return {w, -x, -y, -z}; }
    float norm() const { return std::sqrt(w * w + x * x + y * y + z * z); }
    Quat normalized() const {
        float n = norm();
        return n > 1e-8f ? Quat{w / n, x / n, y / n, z / n} : Quat();
    }
    // Rotate a vector: v' = v + w*t + q.xyz x t, t = 2 (q.xyz x v)
    Vec3 rotate(const Vec3& v) const {
        Vec3 q(x, y, z);
        Vec3 t = cross(q, v) * 2.0f;
        return v + t * w + cross(q, t);
    }
    Mat4 toMat4() const {
        Mat4 r = Mat4::identity();
        float xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
        r.m[0] = 1 - 2 * (yy + zz); r.m[1] = 2 * (xy + wz);     r.m[2]  = 2 * (xz - wy);
        r.m[4] = 2 * (xy - wz);     r.m[5] = 1 - 2 * (xx + zz); r.m[6]  = 2 * (yz + wx);
        r.m[8] = 2 * (xz + wy);     r.m[9] = 2 * (yz - wx);     r.m[10] = 1 - 2 * (xx + yy);
        return r;
    }
};

// Spherical linear interpolation: constant angular speed between two orientations.
inline Quat slerp(Quat a, Quat b, float t) {
    float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0) { b = {-b.w, -b.x, -b.y, -b.z}; d = -d; }   // take the short way round
    if (d > 0.9995f) {                                      // nearly parallel -> normalised lerp
        return Quat{a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}.normalized();
    }
    float th = std::acos(d), s = std::sin(th);
    float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return Quat{a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb};
}
