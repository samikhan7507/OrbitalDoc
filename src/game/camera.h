// Orbital Dock 3D - camera system (cockpit, chase with SLERP lag, free orbit, tracking cam).
// Author: Sami Khan
#pragma once
#include "spacecraft.h"

enum CamMode { CAM_COCKPIT = 0, CAM_CHASE = 1, CAM_ORBIT = 2, CAM_TRACK = 3, CAM_COUNT = 4 };

struct Camera {
    int mode = CAM_CHASE;
    Quat chaseQ;
    float orbitYaw = 0.7f, orbitPitch = 0.25f, orbitDist = 12.0f;
    Vec3 eye{0, 0, 0}, target{0, 0, -1}, up{0, 1, 0};
    float fovBoost = 0;

    void snap(const Spacecraft& s) { chaseQ = s.q; }

    void update(const Spacecraft& s, float dt) {
        switch (mode) {
        case CAM_COCKPIT:
            eye = s.pos + s.q.rotate({0, 0.32f, -1.0f});
            target = eye + s.forward();
            up = s.up();
            break;
        case CAM_CHASE: {
            // SLERP makes the camera trail behind the ship's rotation smoothly.
            chaseQ = slerp(chaseQ, s.q, 1.0f - std::exp(-dt * 4.5f));
            eye = s.pos + chaseQ.rotate({0, 2.3f, 9.5f});
            target = s.pos + chaseQ.rotate({0, 0.5f, -6.0f});
            up = chaseQ.rotate({0, 1, 0});
            break;
        }
        case CAM_ORBIT: {
            float cp = std::cos(orbitPitch), sp = std::sin(orbitPitch);
            Vec3 off(std::sin(orbitYaw) * cp, sp, std::cos(orbitYaw) * cp);
            eye = s.pos + off * orbitDist;
            target = s.pos;
            up = {0, 1, 0};
            break;
        }
        case CAM_TRACK:
        default:
            eye = Vec3(26, 14, 34);   // fixed camera near the station, tracking the ship
            target = s.pos;
            up = {0, 1, 0};
            break;
        }
    }
    Mat4 view() const { return Mat4::lookAt(eye, target, up); }
};
