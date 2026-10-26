// Orbital Dock 3D - Euler angles (yaw-pitch-roll) for the GIMBAL-LOCK demonstration.
// Convention: R = Ry(yaw) * Rx(pitch) * Rz(roll).  At pitch = +-90 deg the yaw axis and the roll axis
// become parallel, one degree of freedom is lost, and the two controls do the same thing.
// Author: Sami Khan
#pragma once
#include "../math/quat.h"

struct Euler { float yaw = 0, pitch = 0, roll = 0; };   // radians

inline Quat eulerToQuat(const Euler& e) {
    Quat qy = Quat::fromAxisAngle({0, 1, 0}, e.yaw);
    Quat qx = Quat::fromAxisAngle({1, 0, 0}, e.pitch);
    Quat qz = Quat::fromAxisAngle({0, 0, 1}, e.roll);
    return (qy * qx * qz).normalized();
}

inline Euler quatToEuler(const Quat& q) {
    Mat4 M = q.toMat4();
    auto R = [&](int r, int c) { return M.m[c * 4 + r]; };      // R(row, col)
    Euler e;
    e.pitch = std::asin(clampf(-R(1, 2), -1.0f, 1.0f));
    if (std::fabs(R(1, 2)) < 0.99999f) {
        e.yaw = std::atan2(R(0, 2), R(2, 2));
        e.roll = std::atan2(R(1, 0), R(1, 1));
    } else {                                                    // gimbal lock: yaw and roll are indistinguishable
        e.yaw = std::atan2(-R(2, 0), R(0, 0));
        e.roll = 0.0f;
    }
    return e;
}
