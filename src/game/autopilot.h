// Orbital Dock 3D - Docking autopilot.
// Attitude: a commanded orientation is SLERPed from the current attitude to the docking attitude at a constant
//           angular speed, and a PD controller makes the ship follow it (feed-forward = station spin).
// Position: a velocity funnel in the STATION frame (works while the station spins or wobbles).
// Author: Sami Khan
#pragma once
#include <vector>
#include "station.h"

struct Debris;                       // defined in game.h


struct Autopilot {
    bool active = false;
    Quat qCmd;               // commanded attitude (SLERP trajectory)
    float phaseT = 0;

    void engage(const Spacecraft& s) { active = true; qCmd = s.q; phaseT = 0; }
    void disengage() { active = false; }

    // Fills ship.thrustIn / ship.rotIn. Returns true while it is flying the ship.
    void control(Spacecraft& s, const Station& st, float dt, const std::vector<Vec3>* rocks = nullptr, const std::vector<float>* rockR = nullptr) {
        if (!active) return;
        phaseT += dt;
        Quat qs = st.orient(), qi = qs.conjugate();

        // ---------- attitude: SLERP toward "same attitude as the station" (ship -Z == station -Z, roll matched)
        const Quat target = qs;
        float cosHalf = clampf(std::fabs(qCmd.w * target.w + qCmd.x * target.x + qCmd.y * target.y + qCmd.z * target.z), 0.0f, 1.0f);
        float ang = 2.0f * std::acos(cosHalf);
        const float slewRate = 0.45f;                                    // rad/s along the great-circle arc
        float tStep = ang > 1e-4f ? clampf(slewRate * dt / ang, 0.0f, 1.0f) : 1.0f;
        qCmd = slerp(qCmd, target, tStep);

        // PD on the error between the ship and the commanded attitude (expressed in the ship's local frame)
        Quat qe = s.q.conjugate() * qCmd;
        if (qe.w < 0) qe = {-qe.w, -qe.x, -qe.y, -qe.z};
        Vec3 err(qe.x * 2.0f, qe.y * 2.0f, qe.z * 2.0f);
        Vec3 wFF = s.q.conjugate().rotate(st.omega);                      // station spin seen from the ship
        Vec3 wDes = err * 1.6f + wFF;
        Vec3 aDes = (wDes - s.w) * 3.0f;                                  // desired angular acceleration (local)
        const float torque = 1.5f;
        s.rotIn = Vec3(clampf(aDes.x / torque, -1, 1), clampf(-aDes.y / torque, -1, 1), clampf(-aDes.z / torque, -1, 1));
        s.assist = false;

        // ---------- translation: funnel in the station frame, controlling the docking probe (nose) position
        Vec3 nose = s.pos + s.forward() * st.noseOffset;
        Vec3 P = qi.rotate(nose);
        float axial = P.z - st.portLocal.z;
        float lat = std::sqrt(P.x * P.x + P.y * P.y);
        float funnel = 1.0f - clampf(lat / (0.6f + 0.18f * std::fabs(axial)), 0.0f, 1.0f);   // only descend when centred
        bool attitudeOk = std::fabs(ang) < 0.35f || axial > 40.0f;
        float vClose = clampf(0.22f * axial, 0.45f, 7.0f) * (axial > 8.0f ? (0.25f + 0.75f * funnel) : (0.45f + 0.55f * funnel));
        if (!attitudeOk) vClose = 0.0f;
        if (axial < 0.0f) vClose = 0.5f;
        Vec3 vLoc(clampf(-0.9f * P.x, -2.5f, 2.5f), clampf(-0.9f * P.y, -2.5f, 2.5f), -vClose);
        Vec3 vDesWorld = qs.rotate(vLoc) + cross(st.omega, nose);
        // debris avoidance: push away from any rock that is close to the ship (potential field)
        if (rocks) for (size_t i = 0; i < rocks->size(); ++i) {
            Vec3 d = s.pos - (*rocks)[i];
            float dist = length(d), clear = (*rockR)[i] + 9.0f;
            if (dist < clear && dist > 1e-3f) vDesWorld += d * ((clear - dist) / dist * 0.9f);
        }
        Vec3 aWorld = (vDesWorld - s.vel) * 1.8f;
        Vec3 aLoc = s.q.conjugate().rotate(aWorld);
        const float accel = 3.0f;
        s.thrustIn = Vec3(clampf(aLoc.x / accel, -1, 1), clampf(aLoc.y / accel, -1, 1), clampf(-aLoc.z / accel, -1, 1));
        s.brake = false;
    }
};
