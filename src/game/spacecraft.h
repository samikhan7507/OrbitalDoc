// Orbital Dock 3D - Spacecraft: Newtonian 6-DOF flight model.
// Ship local axes:  +X right,  +Y up,  -Z forward  (OpenGL convention).
// Author: Sami Khan
#pragma once
#include "../math/quat.h"
#include "euler.h"

struct Spacecraft {
    Vec3 pos{0, 10, 150};
    Vec3 vel{0, 0, 0};          // world-space linear velocity (m/s)
    Quat q;                     // orientation (local -> world)
    Vec3 w{0, 0, 0};            // angular velocity in the SHIP's local frame (rad/s)
    float fuel = 100.0f;        // percent

    // inputs for this frame, all in -1..1
    Vec3 thrustIn{0, 0, 0};     // x: right, y: up, z: forward
    Vec3 rotIn{0, 0, 0};        // x: pitch up, y: yaw right, z: roll right
    bool brake = false, killRot = false, assist = true;

    // ---- rotation mode: quaternion (default) or Euler angles (gimbal-lock demo) ----
    bool eulerMode = false;
    Euler eul;                   // yaw/pitch/roll state used only in Euler mode
    Vec3 eulRate{0, 0, 0};       // (yawRate, pitchRate, rollRate) rad/s used only in Euler mode
    Vec3 refW{0, 0, 0};          // reference angular velocity (ship-local) that assist damps TOWARD (station spin when docking)

    float engineGlow = 0;       // smoothed main-engine intensity (for the flame scale)
    Vec3 rcsGlow{0, 0, 0};      // smoothed per-axis RCS activity (used by particles in Phase 2)

    Vec3 forward() const { return q.rotate({0, 0, -1}); }
    void setEulerMode(bool on) {
        if (on == eulerMode) return;
        eulerMode = on;
        if (on) { eul = quatToEuler(q); eulRate = Vec3(); w = Vec3(); }
        else { eul = quatToEuler(q); }
    }
    Vec3 up() const { return q.rotate({0, 1, 0}); }
    Vec3 right() const { return q.rotate({1, 0, 0}); }

    void update(float dt) {
        const float accel = 3.0f;      // m/s^2 translation
        const float brakeAccel = 5.0f;
        const float torque = 1.5f;     // rad/s^2
        const float maxW = 2.0f;       // rad/s
        float used = 0.0f;
        bool hasFuel = fuel > 0.0f;

        // ---- translation: thrust in local axes, rotated into world space (local vs world space!) ----
        Vec3 t = thrustIn;
        if (!hasFuel) t = Vec3();
        Vec3 aLocal(t.x * accel, t.y * accel, -t.z * accel);
        vel += q.rotate(aLocal) * dt;
        used += (std::fabs(t.x) + std::fabs(t.y) + std::fabs(t.z)) * 0.5f;

        if (brake && hasFuel) {
            float sp = length(vel);
            if (sp > 1e-4f) {
                float dv = clampf(brakeAccel * dt, 0.0f, sp);
                vel -= normalize(vel) * dv;
                used += 1.2f;
            }
        }

        // ---- rotation ----
        Vec3 r = hasFuel ? rotIn : Vec3();
        auto damp = [&](float& c, float in, float target, float rate) {
            if (std::fabs(in) < 0.01f) {
                float d = rate * dt, diff = c - target;
                c = std::fabs(diff) <= d ? target : c - (diff > 0 ? d : -d);
            }
        };
        if (!eulerMode) {
            // Quaternion mode: torque in the local frame (pitch +X, yaw about +Y = turn left, roll about +Z = roll left)
            Vec3 wdot(r.x * torque, -r.y * torque, -r.z * torque);
            w += wdot * dt;
            used += (std::fabs(r.x) + std::fabs(r.y) + std::fabs(r.z)) * 0.15f;
            if ((assist || killRot) && hasFuel) {
                float rate = killRot ? 2.5f : 1.2f;
                float before = length(w - refW);
                damp(w.x, rotIn.x, refW.x, rate); damp(w.y, rotIn.y, refW.y, rate); damp(w.z, rotIn.z, refW.z, rate);
                if (before - length(w - refW) > 1e-5f) used += 0.1f;
            }
            float wl = length(w);
            if (wl > maxW) w = w * (maxW / wl);
            wl = length(w);
            // integrate orientation with a quaternion (no gimbal lock)
            if (wl > 1e-6f) q = (q * Quat::fromAxisAngle(w, wl * dt)).normalized();
        } else {
            // Euler mode: the SAME controls now drive three angles directly -> gimbal lock at pitch = +-90 deg
            eulRate.y += r.x * torque * dt;            // pitch
            eulRate.x += -r.y * torque * dt;           // yaw
            eulRate.z += -r.z * torque * dt;           // roll
            used += (std::fabs(r.x) + std::fabs(r.y) + std::fabs(r.z)) * 0.15f;
            if ((assist || killRot) && hasFuel) {
                float rate = killRot ? 2.5f : 1.2f;
                damp(eulRate.y, rotIn.x, 0.0f, rate); damp(eulRate.x, rotIn.y, 0.0f, rate); damp(eulRate.z, rotIn.z, 0.0f, rate);
            }
            for (int k = 0; k < 3; ++k) (&eulRate.x)[k] = clampf((&eulRate.x)[k], -maxW, maxW);
            eul.yaw += eulRate.x * dt; eul.pitch += eulRate.y * dt; eul.roll += eulRate.z * dt;
            Quat prev = q;
            q = eulerToQuat(eul);
            // measured local angular velocity (so docking rate checks keep working)
            Quat d = prev.conjugate() * q;
            if (d.w < 0) d = {-d.w, -d.x, -d.y, -d.z};
            w = Vec3(d.x, d.y, d.z) * (2.0f / (dt > 1e-6f ? dt : 1e-6f));
        }
        pos += vel * dt;

        used += 0.0f;
        fuel = clampf(fuel - used * 1.2f * dt, 0.0f, 100.0f);   // ~0.6 %/s on one thruster axis

        float target = t.z > 0.01f ? t.z : 0.0f;
        engineGlow += (target - engineGlow) * clampf(dt * 8.0f, 0, 1);
        rcsGlow.x += (std::fabs(t.x) - rcsGlow.x) * clampf(dt * 10.0f, 0, 1);
        rcsGlow.y += (std::fabs(t.y) - rcsGlow.y) * clampf(dt * 10.0f, 0, 1);
        rcsGlow.z += (std::fabs(r.x) + std::fabs(r.y) + std::fabs(r.z) - rcsGlow.z) * clampf(dt * 10.0f, 0, 1);
    }
};
