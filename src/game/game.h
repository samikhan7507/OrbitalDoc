// Orbital Dock 3D - game state: ties spacecraft, station, camera, docking rules, levels, scoring,
// debris, particles, autopilot, replay and ghost together.
// Author: Sami Khan
#pragma once
#include <cstdlib>
#include <vector>
#include "spacecraft.h"
#include "station.h"
#include "camera.h"
#include "particles.h"
#include "autopilot.h"

enum GameState { STATE_FLYING = 0, STATE_DOCKED = 1, STATE_CRASHED = 2, STATE_REPLAY = 3 };
enum CrashReason { CRASH_NONE = 0, CRASH_HARD_CONTACT = 1, CRASH_COLLISION = 2, CRASH_DEBRIS = 3 };

struct LevelDef { const char* name; float spin, tiltAmp, tiltPeriod, ringRate, startDist, mul; int debris; };
static const LevelDef LEVELS[4] = {
    {"EASY",   0.040f, 0.00f, 45.0f, 0.12f,  90.0f, 1.0f, 0},
    {"NORMAL", 0.100f, 0.00f, 45.0f, 0.20f, 140.0f, 1.5f, 0},
    {"HARD",   0.220f, 0.17f, 40.0f, 0.30f, 150.0f, 2.2f, 0},   // fast spin + wobbling axis (tumble)
    {"DEBRIS", 0.120f, 0.05f, 50.0f, 0.20f, 150.0f, 2.8f, 26},  // asteroid field in the approach corridor
};

// A drifting rock. Position and angle are pure functions of time, so replays are exact.
struct Debris {
    Vec3 p0, v, axis; float spin, angle0, radius; Vec3 stretch;
    Vec3 posAt(float t) const { return p0 + v * t; }
    float angleAt(float t) const { return angle0 + spin * t; }
};

struct Sample { Vec3 p; Quat q; Vec3 th, ro; };      // one recorded frame (20 Hz)
struct Recording { std::vector<Sample> s; float duration = 0; };

struct Score { int total = 0, base = 0, timeBonus = 0, fuelBonus = 0, precision = 0, grade = 3; bool assisted = false; };

struct Game {
    Spacecraft ship;
    Station station;
    Camera cam;
    DockingState dock;
    Autopilot autopilot;
    ParticleSystem particles;
    std::vector<Debris> debris;
    int state = STATE_FLYING, crashReason = CRASH_NONE, level = 1;
    float time = 0, runTime = 0, stateTime = 0;
    float nearDebris = 999.0f;
    Score score; int best[4] = {0, 0, 0, 0}; bool newBest = false;
    bool ghostOn = true;
    Recording rec, lastRun, ghost[4];
    int lastRunState = STATE_CRASHED;
    float replayT = 0; int replayReturn = STATE_CRASHED;
    float recAcc = 0;
    Vec3 dockRelPos; Quat dockRelQ; Vec3 dockStartPos; Quat dockStartQ; float dockBlend = 0;

    Game() { reset(); }

    // ----------------------------------------------------------------- setup
    void setLevel(int l) { level = l < 0 ? 0 : (l > 3 ? 3 : l); reset(); }

    void reset() {
        const LevelDef& L = LEVELS[level];
        auto rnd = [](float a) { return ((float)std::rand() / RAND_MAX * 2.0f - 1.0f) * a; };
        station = Station();
        station.spinRate = L.spin; station.ringRate = L.ringRate; station.tiltAmp = L.tiltAmp; station.tiltPeriod = L.tiltPeriod;
        station.setTime(0.0f);
        ship = Spacecraft();
        float off = level == 0 ? 8.0f : 16.0f;
        ship.pos = Vec3(rnd(off), rnd(off), L.startDist);
        Vec3 toPort = normalize(station.portWorld() - ship.pos);
        ship.q = Quat::fromTo(Vec3(0, 0, -1), toPort);
        ship.q = (ship.q * Quat::fromAxisAngle({0, 0, 1}, rnd(0.6f))).normalized();
        ship.vel = Vec3(0, 0, -3.0f);
        ship.assist = true;
        autopilot = Autopilot();
        particles.clear();
        state = STATE_FLYING; crashReason = CRASH_NONE; stateTime = 0; runTime = 0; recAcc = 0;
        newBest = false; score = Score();
        rec = Recording(); rec.s.reserve(2000);
        makeDebris();
        cam.snap(ship);
        dock = station.evaluate(ship);
        nearDebris = 999.0f;
    }

    // ----------------------------------------------------------------- actions
    void toggleAutopilot() {
        if (state != STATE_FLYING) return;
        if (autopilot.active) autopilot.disengage();
        else { ship.setEulerMode(false); autopilot.engage(ship); score.assisted = true; }
    }
    void toggleEuler() { if (state == STATE_FLYING && !autopilot.active) ship.setEulerMode(!ship.eulerMode); }
    // Jump to a pose right next to gimbal lock (pitch 88 deg) so the effect can be demonstrated instantly.
    void gimbalDemo() {
        if (state != STATE_FLYING) return;
        autopilot.disengage();
        ship.eulerMode = false; ship.setEulerMode(true);
        ship.eul.pitch = radians(88.0f); ship.eulRate = Vec3(); ship.q = eulerToQuat(ship.eul);
        cam.snap(ship);
    }
    void startReplay() {
        if (lastRun.s.size() < 3 || state == STATE_REPLAY || state == STATE_FLYING) return;
        replayReturn = state; state = STATE_REPLAY; replayT = 0; particles.clear();
    }
    void toggleGhost() { ghostOn = !ghostOn; }

    bool ghostPose(Vec3& p, Quat& q) const {
        const Recording& g = ghost[level];
        if (!ghostOn || state != STATE_FLYING || g.s.size() < 2) return false;
        float t = runTime > g.duration ? g.duration : runTime;
        sampleAt(g, t, p, q);
        return true;
    }

    // ----------------------------------------------------------------- per-frame update
    void update(float dt) {
        time += dt; stateTime += dt;
        if (state == STATE_REPLAY) { updateReplay(dt); particles.update(dt); cam.update(ship, dt); return; }
        station.update(dt);

        if (state == STATE_FLYING) {
            runTime += dt;
            float manual = length(ship.thrustIn) + length(ship.rotIn);
            if (autopilot.active) {
                if (manual > 0.35f) autopilot.disengage();
                else {
                    std::vector<Vec3> rp; std::vector<float> rr;
                    for (auto& d : debris) { rp.push_back(d.posAt(station.t)); rr.push_back(d.radius * std::max(d.stretch.x, std::max(d.stretch.y, d.stretch.z))); }
                    autopilot.control(ship, station, dt, &rp, &rr);
                }
            }
            // sync-assist: near the station the assist matches the station's spin instead of zero
            float dPort = length(ship.pos + ship.forward() * station.noseOffset - station.portWorld());
            ship.refW = dPort < 120.0f ? ship.q.conjugate().rotate(station.omega) : Vec3();
            particles.emitForShip(ship, ship.thrustIn, ship.rotIn, ship.brake, dt);
            ship.update(dt);
            record(dt);
            dock = station.evaluate(ship);
            findNearDebris();
            bool atPort = dock.axial <= 0.25f && dock.axial > -3.0f && dock.lateral < 3.2f;
            if (atPort) {
                if (dock.allOk()) beginDock();
                else crash(CRASH_HARD_CONTACT);
            } else if (station.collides(ship)) crash(CRASH_COLLISION);
            else if (debrisHit()) crash(CRASH_DEBRIS);
        } else if (state == STATE_DOCKED) {
            dockBlend = clampf(dockBlend + dt / 1.2f, 0.0f, 1.0f);
            float e = dockBlend * dockBlend * (3.0f - 2.0f * dockBlend);
            Vec3 rel = lerp(dockStartPos, dockRelPos, e);
            Quat rq = slerp(dockStartQ, dockRelQ, e);
            Quat qs = station.orient();
            ship.pos = qs.rotate(rel);
            ship.q = (qs * rq).normalized();
            ship.vel = Vec3(); ship.w = Vec3();
            dock = station.evaluate(ship);
        } else {
            dock = station.evaluate(ship);
        }
        particles.update(dt);
        cam.update(ship, dt);
    }

private:
    void makeDebris() {
        debris.clear();
        int n = LEVELS[level].debris;
        auto r01 = []() { return (float)std::rand() / RAND_MAX; };
        for (int i = 0; i < n; ++i) {
            Debris d;
            float a = r01() * 2.0f * PI, rad = 9.0f + r01() * 34.0f, z = 28.0f + r01() * 100.0f;
            d.p0 = Vec3(rad * std::cos(a), rad * std::sin(a), z);
            d.v = Vec3(r01() - 0.5f, r01() - 0.5f, r01() - 0.5f) * 0.5f;
            d.axis = normalize(Vec3(r01() - 0.5f, r01() - 0.5f, r01() - 0.5f) + Vec3(0.01f, 0.02f, 0.03f));
            d.spin = (r01() - 0.5f) * 0.8f; d.angle0 = r01() * 6.28f;
            d.radius = 1.0f + r01() * 1.8f;
            d.stretch = Vec3(0.8f + r01() * 0.6f, 0.7f + r01() * 0.5f, 0.8f + r01() * 0.6f);
            debris.push_back(d);
        }
    }
    void findNearDebris() {
        nearDebris = 999.0f;
        for (auto& d : debris) {
            float dd = length(ship.pos - d.posAt(station.t)) - d.radius * 1.3f;
            if (dd < nearDebris) nearDebris = dd;
        }
    }
    bool debrisHit() const {
        Vec3 nose = ship.pos + ship.forward() * 2.0f;
        for (auto& d : debris) {
            Vec3 c = d.posAt(station.t);
            float R = d.radius * 1.15f;
            if (length(ship.pos - c) < R + 1.4f || length(nose - c) < R + 0.7f) return true;
        }
        return false;
    }
    void record(float dt) {
        recAcc += dt;
        while (recAcc >= 0.05f) {
            recAcc -= 0.05f;
            rec.s.push_back({ship.pos, ship.q, ship.thrustIn, ship.rotIn});
        }
        rec.duration = runTime;
    }
    static void sampleAt(const Recording& r, float t, Vec3& p, Quat& q) {
        float f = t / 0.05f;
        size_t i = (size_t)f;
        if (i + 1 >= r.s.size()) { p = r.s.back().p; q = r.s.back().q; return; }
        float k = f - (float)i;
        p = lerp(r.s[i].p, r.s[i + 1].p, k);
        q = slerp(r.s[i].q, r.s[i + 1].q, k);      // SLERP between recorded orientations
    }
    void finishRun(int endState) {
        rec.duration = runTime;
        lastRun = rec; lastRunState = endState;
    }
    void crash(int reason) {
        state = STATE_CRASHED; crashReason = reason; stateTime = 0;
        autopilot.disengage();
        ship.vel = Vec3(); ship.w = Vec3();
        finishRun(STATE_CRASHED);
        // debris explosion
        for (int i = 0; i < 160; ++i) {
            auto r = []() { return (float)std::rand() / RAND_MAX * 2.0f - 1.0f; };
            Vec3 d = normalize(Vec3(r(), r(), r()) + Vec3(0.001f, 0.002f, 0.003f));
            particles.emit(ship.pos + d, d * (3.0f + 8.0f * std::fabs(r())), 0.8f + 1.4f * std::fabs(r()), 1.3f, Vec3(1.0f, 0.55f, 0.2f));
        }
    }
    void beginDock() {
        state = STATE_DOCKED; stateTime = 0; dockBlend = 0;
        autopilot.disengage();
        Quat qi = station.orient().conjugate();
        dockStartPos = qi.rotate(ship.pos);
        dockStartQ = qi * ship.q;
        dockRelPos = Vec3(0, 0, station.portLocal.z + station.noseOffset);
        dockRelQ = Quat();
        // ---- scoring ----
        Score s; s.assisted = score.assisted;
        s.base = 1000;
        s.timeBonus = (int)std::max(0.0f, 600.0f - runTime * 2.5f);
        s.fuelBonus = (int)(ship.fuel * 6.0f);
        auto q01 = [](float v) { return clampf(v, 0.0f, 1.0f); };
        s.precision = (int)(200.0f * q01(1.0f - dock.lateral / 1.0f) + 100.0f * q01(1.0f - std::fabs(dock.closing - 0.5f)) +
                            100.0f * q01(1.0f - dock.tiltDeg / 8.0f) + 100.0f * q01(1.0f - std::fabs(dock.rollDeg) / 10.0f));
        int raw = s.base + s.timeBonus + s.fuelBonus + s.precision;
        s.total = (int)(raw * LEVELS[level].mul * (s.assisted ? 0.5f : 1.0f));
        s.grade = raw >= 2200 ? 0 : (raw >= 1900 ? 1 : (raw >= 1600 ? 2 : 3));
        score = s;
        newBest = s.total > best[level];
        if (newBest) { best[level] = s.total; ghost[level] = rec; }
        finishRun(STATE_DOCKED);
    }
    void updateReplay(float dt) {
        replayT += dt;
        if (replayT >= lastRun.duration) {
            state = replayReturn; stateTime = 0;
            station.setTime(lastRun.duration);
            sampleAt(lastRun, lastRun.duration, ship.pos, ship.q);
            if (replayReturn == STATE_DOCKED) { dockBlend = 1.0f; }
            return;
        }
        station.setTime(replayT);
        sampleAt(lastRun, replayT, ship.pos, ship.q);
        size_t i = (size_t)(replayT / 0.05f);
        if (i < lastRun.s.size()) {
            const Sample& sm = lastRun.s[i];
            ship.thrustIn = sm.th; ship.rotIn = sm.ro;
            ship.engineGlow = sm.th.z > 0 ? sm.th.z : 0;
            particles.emitForShip(ship, sm.th, sm.ro, false, dt);
        }
        ship.vel = Vec3();
    }
};
