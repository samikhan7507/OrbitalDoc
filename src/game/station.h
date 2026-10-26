// Orbital Dock 3D - Space station: hierarchy, solar tracking, docking evaluation, collisions.
// Station local frame: spin axis = Z, docking port on the +Z end.
// Author: Sami Khan
#pragma once
#include "spacecraft.h"

struct DockingState {
    float dist = 0;          // distance nose -> port
    float axial = 0;         // along station Z, 0 = touching the port plane
    float lateral = 0;       // offset from the spin axis
    float closing = 0;       // approach speed (+ = moving toward the port)
    float lateralSpeed = 0;
    float tiltDeg = 0;       // angle between ship axis and docking axis
    float rollDeg = 0;       // roll error about the docking axis, -180..180
    float rollRateDeg = 0;   // relative spin rate about the docking axis
    float otherRateDeg = 0;  // relative pitch/yaw rate
    bool okAlign = false, okSpeed = false, okAtt = false, okRoll = false;
    bool allOk() const { return okAlign && okSpeed && okAtt && okRoll; }
};

struct Station {
    // Every animated angle is a pure function of the run time t, so a replay can jump to any moment.
    float t = 0.0f;
    float spinAngle = 0.0f, spinRate = 0.10f;   // whole station rotates about its Z axis
    float ringAngle = 0.0f, ringRate = 0.20f;   // habitat ring rotates relative to the hub (child transform)
    float tiltAmp = 0.0f, tiltPeriod = 45.0f;   // optional wobble ("tumble") of the spin axis, radians / seconds
    Vec3 portLocal{0, 0, 21.0f};
    float noseOffset = 3.3f;                    // ship centre -> docking probe
    Vec3 omega{0, 0, 0};                        // station angular velocity in WORLD space (rad/s), derived from orient(t)

    Quat orientAt(float tt, float spin) const {
        Quat q = Quat::fromAxisAngle({0, 0, 1}, spin);
        if (tiltAmp > 0.0f) {
            float a = tiltAmp * std::sin(2.0f * PI * tt / tiltPeriod);
            q = Quat::fromAxisAngle({1.0f, 0.45f, 0.0f}, a) * q;
        }
        return q;
    }
    Quat orient() const { return orientAt(t, spinAngle); }
    Mat4 model() const { return orient().toMat4(); }
    Vec3 portWorld() const { return orient().rotate(portLocal); }

    void setTime(float tt) {
        t = tt;
        spinAngle = std::fmod(spinRate * t, 2.0f * PI);
        ringAngle = std::fmod(ringRate * t, 2.0f * PI);
        const float e = 0.01f;                   // numerical angular velocity: dq = q(t+e) * q(t)^-1
        Quat q1 = orient(), q2 = orientAt(t + e, spinRate * (t + e));
        Quat d = q2 * q1.conjugate();
        if (d.w < 0) d = {-d.w, -d.x, -d.y, -d.z};
        omega = Vec3(d.x, d.y, d.z) * (2.0f / e);
    }
    void update(float dt) { setTime(t + dt); }

    DockingState evaluate(const Spacecraft& s) const {
        DockingState d;
        Quat qs = orient(), qi = qs.conjugate();
        Vec3 fwd = s.forward();
        Vec3 nose = s.pos + fwd * noseOffset;
        Vec3 P = qi.rotate(nose);                       // nose position in the station frame
        d.axial = P.z - portLocal.z;
        d.lateral = std::sqrt(P.x * P.x + P.y * P.y);
        d.dist = length(P - portLocal);
        Vec3 omegaS = omega;                            // station angular velocity (world space)
        Vec3 vRel = qi.rotate(s.vel - cross(omegaS, nose));
        d.closing = -vRel.z;
        d.lateralSpeed = std::sqrt(vRel.x * vRel.x + vRel.y * vRel.y);
        Vec3 fL = qi.rotate(fwd);
        d.tiltDeg = degrees(std::acos(clampf(-fL.z, -1.0f, 1.0f)));
        Quat qr = qi * s.q;                             // ship orientation relative to the station
        if (qr.w < 0) qr = {-qr.w, -qr.x, -qr.y, -qr.z};
        float twist = 2.0f * std::atan2(qr.z, qr.w);    // swing-twist: twist about the docking axis
        d.rollDeg = degrees(twist);
        Vec3 wRel = s.q.rotate(s.w) - omegaS;
        Vec3 wRelL = qi.rotate(wRel);
        d.rollRateDeg = degrees(std::fabs(wRelL.z));
        d.otherRateDeg = degrees(std::sqrt(wRelL.x * wRelL.x + wRelL.y * wRelL.y));
        d.okAlign = d.lateral < 1.0f;
        d.okSpeed = d.closing > 0.0f && d.closing < 1.5f && d.lateralSpeed < 0.5f;
        d.okAtt = d.tiltDeg < 8.0f && d.otherRateDeg < 14.0f;
        d.okRoll = std::fabs(d.rollDeg) < 10.0f && d.rollRateDeg < 7.0f;
        return d;
    }

    // Solid-body collision against hub capsule, rotating ring + spokes, modules and solar truss.
    bool collidesPoint(const Vec3& worldPos, float r) const {
        Vec3 p = orient().conjugate().rotate(worldPos);
        float rho = std::sqrt(p.x * p.x + p.y * p.y);
        // hub: capsule along Z from -15 to 17, radius 3.2
        float dz = p.z < -15 ? -15 - p.z : (p.z > 17 ? p.z - 17 : 0.0f);
        if (rho * rho + dz * dz < (3.2f + r) * (3.2f + r)) return true;
        // ring: torus R=24, r=2.2
        float dr = rho - 24.0f;
        if (dr * dr + p.z * p.z < (2.2f + r) * (2.2f + r)) return true;
        // spokes (rotate into ring frame)
        Quat rq = Quat::fromAxisAngle({0, 0, 1}, -ringAngle);
        Vec3 pr = rq.rotate(p);
        float hw = 0.8f + r;
        if (std::fabs(pr.z) < hw && ((std::fabs(pr.x) < 24 && std::fabs(pr.y) < hw) || (std::fabs(pr.y) < 24 && std::fabs(pr.x) < hw))) return true;
        // modules (two crossing arms at z=-6)
        float mz = std::fabs(p.z + 6.0f), mh = 2.2f + r;
        if (mz < mh && ((std::fabs(p.x) < 13 + r && std::fabs(p.y) < mh) || (std::fabs(p.y) < 11 + r && std::fabs(p.x) < mh))) return true;
        // solar truss + panels (panel plane rotates, so use a loose box)
        if (std::fabs(p.x) < 35.0f + r && std::fabs(p.y) < 6.2f + r && std::fabs(p.z + 14.0f) < 6.2f + r) return true;
        return false;
    }
    bool collides(const Spacecraft& s) const {
        return collidesPoint(s.pos, 1.5f) || collidesPoint(s.pos + s.forward() * 2.0f, 0.7f);
    }
};
