// Orbital Dock 3D - CPU particle system for RCS thruster and main-engine exhaust.
// Particles live in WORLD space, so exhaust stays behind where it was emitted (inertia!).
// Author: Sami Khan
#pragma once
#include <vector>
#include <cstdlib>
#include "spacecraft.h"

struct Particle { Vec3 p, v; float life = 0, maxLife = 1, size = 1; Vec3 col; };

class ParticleSystem {
public:
    std::vector<Particle> list;
    ParticleSystem() { list.reserve(kMax); }

    void clear() { list.clear(); }
    void update(float dt) {
        for (size_t i = 0; i < list.size();) {
            Particle& p = list[i];
            p.life -= dt;
            if (p.life <= 0) { list[i] = list.back(); list.pop_back(); continue; }
            p.p += p.v * dt;
            p.v *= (1.0f - 0.6f * dt);
            ++i;
        }
    }
    void emit(const Vec3& pos, const Vec3& vel, float life, float size, const Vec3& col) {
        if (list.size() >= kMax) return;
        Particle p; p.p = pos; p.v = vel; p.life = p.maxLife = life; p.size = size; p.col = col;
        list.push_back(p);
    }

    // Fire the correct nozzles for this frame's commands. Thrust inputs: x right, y up, z forward; rotation: pitch, yaw, roll.
    void emitForShip(const Spacecraft& s, const Vec3& thrust, const Vec3& rot, bool brake, float dt) {
        acc_ += dt;
        const float step = 1.0f / 90.0f;
        while (acc_ >= step) { acc_ -= step; fireOnce(s, thrust, rot, brake); }
    }

private:
    static constexpr size_t kMax = 2200;
    float acc_ = 0;
    static float rnd() { return (float)std::rand() / RAND_MAX * 2.0f - 1.0f; }

    // nozzle at local position `lp`, exhaust direction `ld` (local), strength 0..1
    void nozzle(const Spacecraft& s, const Vec3& lp, const Vec3& ld, float strength, float speed, const Vec3& col, float size) {
        if (strength < 0.02f) return;
        Vec3 wp = s.pos + s.q.rotate(lp);
        Vec3 wd = s.q.rotate(ld);
        Vec3 v = s.vel + wd * (speed * (0.8f + 0.4f * (rnd() * 0.5f + 0.5f))) + Vec3(rnd(), rnd(), rnd()) * (0.35f * speed * 0.2f);
        emit(wp + wd * 0.1f, v, 0.35f + 0.45f * (rnd() * 0.5f + 0.5f), size, col * strength);
    }
    void fireOnce(const Spacecraft& s, const Vec3& t, const Vec3& r, bool brake) {
        const Vec3 rcs(0.55f, 0.8f, 1.0f);            // cool white-blue RCS puffs
        const Vec3 eng(0.8f, 0.45f, 0.18f);           // warm main engine
        const float sp = 7.0f;
        // main engine: forward thrust -> exhaust backward (+Z local)
        if (t.z > 0.02f && s.fuel > 0) { nozzle(s, {0, 0, 2.4f}, {0, 0, 1}, t.z, 14.0f, eng, 0.55f); nozzle(s, {0, 0, 2.4f}, {0, 0, 1}, t.z, 10.0f, eng * 0.7f, 0.40f); }
        if (s.fuel <= 0) return;
        // reverse thrust: nose-side nozzles fire forward (-Z)
        if (t.z < -0.02f) { float k = -t.z; nozzle(s, {0.6f, 0, -1.5f}, {0, 0, -1}, k, sp, rcs, 0.30f); nozzle(s, {-0.6f, 0, -1.5f}, {0, 0, -1}, k, sp, rcs, 0.30f); }
        // sideways: thrust +X (right) fires the LEFT nozzle toward -X, etc.
        if (t.x > 0.02f)  nozzle(s, {-0.75f, 0, 0}, {-1, 0, 0}, t.x, sp, rcs, 0.30f);
        if (t.x < -0.02f) nozzle(s, {0.75f, 0, 0}, {1, 0, 0}, -t.x, sp, rcs, 0.30f);
        if (t.y > 0.02f)  nozzle(s, {0, -0.75f, 0}, {0, -1, 0}, t.y, sp, rcs, 0.30f);
        if (t.y < -0.02f) nozzle(s, {0, 0.75f, 0}, {0, 1, 0}, -t.y, sp, rcs, 0.30f);
        // rotation: couples of nozzles at the nose and tail
        if (r.x > 0.02f)  { nozzle(s, {0, -0.6f, -1.6f}, {0, -1, 0}, r.x, sp, rcs, 0.24f); nozzle(s, {0, 0.6f, 1.6f}, {0, 1, 0}, r.x, sp, rcs, 0.24f); }
        if (r.x < -0.02f) { nozzle(s, {0, 0.6f, -1.6f}, {0, 1, 0}, -r.x, sp, rcs, 0.24f); nozzle(s, {0, -0.6f, 1.6f}, {0, -1, 0}, -r.x, sp, rcs, 0.24f); }
        if (r.y > 0.02f)  { nozzle(s, {0.6f, 0, -1.6f}, {1, 0, 0}, r.y, sp, rcs, 0.24f); nozzle(s, {-0.6f, 0, 1.6f}, {-1, 0, 0}, r.y, sp, rcs, 0.24f); }
        if (r.y < -0.02f) { nozzle(s, {-0.6f, 0, -1.6f}, {-1, 0, 0}, -r.y, sp, rcs, 0.24f); nozzle(s, {0.6f, 0, 1.6f}, {1, 0, 0}, -r.y, sp, rcs, 0.24f); }
        if (r.z > 0.02f)  { nozzle(s, {3.0f, 0, 1.1f}, {0, -1, 0}, r.z, sp, rcs, 0.24f); nozzle(s, {-3.0f, 0, 1.1f}, {0, 1, 0}, r.z, sp, rcs, 0.24f); }
        if (r.z < -0.02f) { nozzle(s, {3.0f, 0, 1.1f}, {0, 1, 0}, -r.z, sp, rcs, 0.24f); nozzle(s, {-3.0f, 0, 1.1f}, {0, -1, 0}, -r.z, sp, rcs, 0.24f); }
        // brake: exhaust along the direction of travel from the nose
        if (brake && length(s.vel) > 0.05f) {
            Vec3 d = normalize(s.vel);
            Vec3 wp = s.pos + d * 0.5f;
            emit(wp, s.vel + d * 6.0f, 0.5f, 0.3f, rcs);
        }
    }
};
