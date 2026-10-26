// Headless self-test of math + physics + docking rules (no GPU needed).
// Build: g++ -std=c++17 -Isrc tools/test_physics.cpp -o test_physics && ./test_physics
// Author: Sami Khan
#include <cstdio>
#include "game/game.h"

static int fails = 0;
#define CHECK(cond, msg) do { bool ok_ = (cond); std::printf("[%s] %s\n", ok_ ? "PASS" : "FAIL", msg); if (!ok_) ++fails; } while (0)
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

int main() {
    // ---- math ----
    Mat4 a = Mat4::translate({1, 2, 3}) * Mat4::rotateY(0.7f) * Mat4::scale({2, 3, 4});
    Mat4 id = a * a.inverse();
    bool isId = true;
    for (int i = 0; i < 16; ++i) isId &= near(id.m[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-4f);
    CHECK(isId, "Mat4 inverse: M * M^-1 = I");
    Quat q = Quat::fromAxisAngle({0, 1, 0}, PI / 2);
    Vec3 r = q.rotate({1, 0, 0});
    CHECK(near(r.x, 0) && near(r.z, -1), "Quat: 90deg about Y maps +X to -Z");
    Vec3 viaMat = q.toMat4().transformDir({1, 0, 0});
    CHECK(near(viaMat.x, r.x) && near(viaMat.z, r.z), "Quat.toMat4 agrees with Quat.rotate");
    Quat s = slerp(Quat(), q, 0.5f);
    CHECK(near(2 * std::acos(s.w), PI / 4, 1e-3f), "SLERP halfway = 45deg");
    // matrix multiplication order matters
    Mat4 TR = Mat4::translate({5, 0, 0}) * Mat4::rotateZ(PI / 2);
    Mat4 RT = Mat4::rotateZ(PI / 2) * Mat4::translate({5, 0, 0});
    Vec3 p1 = TR.transformPoint({1, 0, 0}), p2 = RT.transformPoint({1, 0, 0});
    CHECK(near(p1.x, 5) && near(p1.y, 1) && near(p2.x, 0) && near(p2.y, 6), "T*R != R*T (order matters)");

    // ---- flight model ----
    Spacecraft ship;
    ship.pos = {0, 0, 0}; ship.vel = {0, 0, 0};
    ship.thrustIn = {0, 0, 1};
    for (int i = 0; i < 60; ++i) ship.update(1.0f / 60);
    CHECK(near(ship.vel.z, -3.0f, 0.05f), "1 s forward thrust -> 3 m/s toward -Z");
    ship.thrustIn = {0, 0, 0};
    ship.rotIn = {0, 0, 0}; ship.w = {0, 1.0f, 0}; ship.assist = false;
    for (int i = 0; i < 60; ++i) ship.update(1.0f / 60);
    CHECK(near(length(ship.w), 1.0f, 1e-3f), "No assist: angular velocity persists (Newtonian)");
    Vec3 f = ship.forward();
    CHECK(near(length(f), 1.0f, 1e-3f), "Orientation stays normalised");
    ship.assist = true;
    for (int i = 0; i < 180; ++i) ship.update(1.0f / 60);
    CHECK(length(ship.w) < 1e-3f, "Assist damps rotation to zero");

    // ---- docking ----
    auto setup = [](Game& g, Vec3 pos, float closing, float rollRad, float tilt) {
        g.state = STATE_FLYING;
        g.station.spinAngle = 0.0f;
        g.ship = Spacecraft();
        g.ship.pos = pos;
        g.ship.q = Quat::fromAxisAngle({0, 0, 1}, rollRad) ;
        g.ship.q = (Quat::fromAxisAngle({1, 0, 0}, tilt) * g.ship.q).normalized();
        g.ship.vel = Vec3(0, 0, -closing);
        g.ship.assist = false;
        g.ship.w = Vec3(0, 0, g.station.spinRate);   // pilot matches the station's roll rate (about the shared Z axis)
    };
    {
        Game g; setup(g, {0, 0, 21.0f + 3.3f + 6.0f}, 1.0f, 0, 0);
        for (int i = 0; i < 600 && g.state == STATE_FLYING; ++i) g.update(1.0f / 60);
        CHECK(g.state == STATE_DOCKED, "Aligned, slow approach -> DOCKED");
        // then follows station rotation
        float a0 = g.station.spinAngle;
        for (int i = 0; i < 120; ++i) g.update(1.0f / 60);
        Vec3 up = g.ship.up();
        Vec3 expect = g.station.orient().rotate({0, 1, 0});
        CHECK(near(dot(up, expect), 1.0f, 1e-3f) && g.station.spinAngle != a0, "Docked ship rotates with the station (child transform)");
    }
    {
        Game g; setup(g, {0, 0, 21.0f + 3.3f + 6.0f}, 6.0f, 0, 0);
        for (int i = 0; i < 600 && g.state == STATE_FLYING; ++i) g.update(1.0f / 60);
        CHECK(g.state == STATE_CRASHED && g.crashReason == CRASH_HARD_CONTACT, "Too fast -> hard-contact crash");
    }
    {
        Game g; setup(g, {0, 0, 21.0f + 3.3f + 6.0f}, 1.0f, radians(35.0f), 0);
        for (int i = 0; i < 600 && g.state == STATE_FLYING; ++i) g.update(1.0f / 60);
        CHECK(g.state == STATE_CRASHED, "35 deg roll error -> crash");
    }
    {
        Game g; setup(g, {10.0f, 0, 21.0f + 3.3f + 6.0f}, 1.0f, 0, 0);
        for (int i = 0; i < 4000 && g.state == STATE_FLYING; ++i) g.update(1.0f / 60);
        CHECK(g.state == STATE_CRASHED && g.crashReason == CRASH_COLLISION, "Off-axis -> hits the station hull");
    }
    {
        Game g; g.reset();
        CHECK(g.state == STATE_FLYING && g.dock.dist > 100.0f, "Reset places ship ~150 m out");
    }
    // ---- Euler angles / gimbal lock ----
    {
        Euler e; e.pitch = radians(90.0f); e.yaw = 0.3f; e.roll = 0.0f;
        Quat qa = eulerToQuat(e);
        Euler e2; e2.pitch = radians(90.0f); e2.yaw = 0.0f; e2.roll = -0.3f;   // roll instead of yaw
        Quat qb = eulerToQuat(e2);
        float d = std::fabs(qa.w * qb.w + qa.x * qb.x + qa.y * qb.y + qa.z * qb.z);
        CHECK(d > 0.9999f, "GIMBAL LOCK: at pitch 90 deg, yaw and roll produce the same rotation");
        Euler f; f.pitch = radians(30.0f); f.yaw = 0.3f; Euler f2; f2.pitch = radians(30.0f); f2.roll = -0.3f;
        Quat qc = eulerToQuat(f), qd = eulerToQuat(f2);
        float d2 = std::fabs(qc.w * qd.w + qc.x * qd.x + qc.y * qd.y + qc.z * qd.z);
        CHECK(d2 < 0.99f, "Away from the lock, yaw and roll are different rotations");
        Euler r = quatToEuler(eulerToQuat(Euler{0.5f, 0.4f, -0.7f}));
        CHECK(near(r.yaw, 0.5f, 1e-3f) && near(r.pitch, 0.4f, 1e-3f) && near(r.roll, -0.7f, 1e-3f), "quatToEuler inverts eulerToQuat");
        // quaternion mode keeps all three axes alive when pointing straight up
        Spacecraft sh; sh.assist = false; sh.q = Quat::fromAxisAngle({1, 0, 0}, radians(90.0f));
        sh.rotIn = {0, 0, 1};
        for (int i = 0; i < 60; ++i) sh.update(1.0f / 60);
        Quat qq = sh.q;
        CHECK(std::fabs(qq.w) < 0.999f && length(sh.w) > 0.1f, "Quaternion mode: roll still works when nose points straight up");
    }

    // ---- scoring, levels, autopilot, replay, ghost ----
    for (int lvl = 0; lvl < 4; ++lvl) {
        int okRuns = 0, total = 5;
        for (int seed = 1; seed <= total; ++seed) {
            std::srand(seed * 977 + lvl);
            Game g; g.setLevel(lvl);
            g.toggleAutopilot();
            for (int i = 0; i < 60 * 420 && g.state == STATE_FLYING; ++i) { g.ship.thrustIn = Vec3(); g.ship.rotIn = Vec3(); g.update(1.0f / 60); }
            if (g.state == STATE_DOCKED) ++okRuns;
            else std::printf("   (autopilot level %d seed %d ended in state %d reason %d, dist %.1f)\n", lvl, seed, g.state, g.crashReason, g.dock.dist);
        }
        char msg[96]; std::snprintf(msg, sizeof msg, "Autopilot docks on level %d (%s): %d/%d runs", lvl + 1, LEVELS[lvl].name, okRuns, total);
        CHECK(okRuns == total, msg);
    }
    {
        std::srand(4242);
        Game g; g.setLevel(0); g.toggleAutopilot();
        for (int i = 0; i < 60 * 300 && g.state == STATE_FLYING; ++i) { g.ship.thrustIn = Vec3(); g.ship.rotIn = Vec3(); g.update(1.0f / 60); }
        CHECK(g.state == STATE_DOCKED && g.score.total > 0 && g.score.assisted, "Autopilot dock gives a (halved) score");
        CHECK(g.best[0] == g.score.total && g.ghost[0].s.size() > 10, "New best stores a ghost recording");
        g.startReplay();
        CHECK(g.state == STATE_REPLAY, "Replay starts after a run");
        for (int i = 0; i < 60 * 400 && g.state == STATE_REPLAY; ++i) g.update(1.0f / 60);
        CHECK(g.state == STATE_DOCKED, "Replay finishes and returns to the result screen");
        g.reset();
        Vec3 gp; Quat gq;
        CHECK(g.state == STATE_FLYING && g.ghostPose(gp, gq), "Ghost of the best run is available on the next attempt");
    }
    {
        // manual input takes the controls back from the autopilot
        std::srand(7);
        Game g; g.setLevel(1); g.toggleAutopilot();
        for (int i = 0; i < 120; ++i) { g.ship.thrustIn = Vec3(); g.ship.rotIn = Vec3(); g.update(1.0f / 60); }
        g.ship.thrustIn = {1, 0, 0};
        g.update(1.0f / 60);
        CHECK(!g.autopilot.active, "Pilot input disengages the autopilot");
    }
    {
        std::srand(9);
        Game g; g.setLevel(3);
        CHECK(g.debris.size() == 26, "Debris level spawns 26 rocks");
        g.ship.pos = g.debris[0].posAt(0.0f); g.ship.vel = Vec3();
        g.update(1.0f / 60);
        CHECK(g.state == STATE_CRASHED && g.crashReason == CRASH_DEBRIS, "Flying into a rock is a debris crash");
    }
    std::printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
