// Author: Sami Khan
#include "hud.h"
#include <cstdio>
#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
EM_JS(void, js_hud, (float* p, int n), {
    if (window.updateHUD) window.updateHUD(HEAPF32.subarray(p >> 2, (p >> 2) + n));
});
#else
  #define GLFW_INCLUDE_NONE
  #include <GLFW/glfw3.h>
#endif

void hudUpdate(const Game& g, const Renderer& r, float fps, bool invertPitch) {
    static float h[H_COUNT];
    const DockingState& d = g.dock;
    h[H_SPEED] = length(g.ship.vel);
    h[H_DIST] = d.dist; h[H_AXIAL] = d.axial; h[H_LATERAL] = d.lateral;
    h[H_CLOSING] = d.closing; h[H_LATSPEED] = d.lateralSpeed;
    h[H_TILT] = d.tiltDeg; h[H_ROLL] = d.rollDeg; h[H_ROLLRATE] = d.rollRateDeg;
    h[H_FUEL] = g.ship.fuel; h[H_STATE] = (float)g.state; h[H_CAM] = (float)g.cam.mode;
    h[H_ASSIST] = g.ship.assist ? 1.0f : 0.0f;
    float nx = 0, ny = 0;
    bool vis = r.projectToScreen(g.station.portWorld(), nx, ny);
    h[H_PORTX] = nx * 0.5f + 0.5f; h[H_PORTY] = 1.0f - (ny * 0.5f + 0.5f); h[H_PORTVIS] = vis ? 1.0f : 0.0f;
    h[H_FPS] = fps; h[H_INVERT] = invertPitch ? 1.0f : 0.0f;
    h[H_OKALIGN] = d.okAlign; h[H_OKSPEED] = d.okSpeed; h[H_OKATT] = d.okAtt; h[H_OKROLL] = d.okRoll;
    h[H_TIME] = g.runTime;
    h[H_POS] = g.ship.pos.x; h[H_POS + 1] = g.ship.pos.y; h[H_POS + 2] = g.ship.pos.z;
    h[H_QUAT] = g.ship.q.w; h[H_QUAT + 1] = g.ship.q.x; h[H_QUAT + 2] = g.ship.q.y; h[H_QUAT + 3] = g.ship.q.z;
    Mat4 M = Mat4::translate(g.ship.pos) * g.ship.q.toMat4();
    for (int i = 0; i < 16; ++i) h[H_MAT + i] = M.m[i];
    h[H_CRASH] = (float)g.crashReason;
    Euler e = g.ship.eulerMode ? g.ship.eul : quatToEuler(g.ship.q);
    h[H_EULERMODE] = g.ship.eulerMode ? 1.0f : 0.0f;
    h[H_YAW] = degrees(e.yaw); h[H_PITCH] = degrees(e.pitch); h[H_EROLL] = degrees(e.roll);
    h[H_AUTOPILOT] = g.autopilot.active ? 1.0f : 0.0f;
    h[H_LEVEL] = (float)g.level;
    h[H_SCORE] = (float)g.score.total; h[H_SC_TIME] = (float)g.score.timeBonus; h[H_SC_FUEL] = (float)g.score.fuelBonus;
    h[H_SC_PREC] = (float)g.score.precision; h[H_SC_BASE] = (float)g.score.base;
    h[H_GHOST] = g.ghost[g.level].s.size() > 2 ? 1.0f : 0.0f;
    h[H_REPLAYPROG] = (g.state == STATE_REPLAY && g.lastRun.duration > 0) ? g.replayT / g.lastRun.duration : 0.0f;
    h[H_ENGINE] = g.ship.engineGlow;
    float rc = g.ship.rcsGlow.x + g.ship.rcsGlow.y + g.ship.rcsGlow.z;
    h[H_RCS] = rc > 1.0f ? 1.0f : rc;
    h[H_NEARDEBRIS] = g.nearDebris;
    h[H_BRAKE] = g.ship.brake ? 1.0f : 0.0f;
    h[H_GRADE] = (float)g.score.grade; h[H_BEST] = (float)g.best[g.level]; h[H_NEWBEST] = g.newBest ? 1.0f : 0.0f;
    h[H_ASSISTED] = g.score.assisted ? 1.0f : 0.0f;
    h[H_BLOOM] = r.bloomOn ? 1.0f : 0.0f; h[H_GHOSTON] = g.ghostOn ? 1.0f : 0.0f;
#ifdef __EMSCRIPTEN__
    js_hud(h, H_COUNT);
#else
    static double last = 0;
    double now = glfwGetTime();
    if (now - last > 0.25) {
        last = now;
        char t[300];
        const char* st = g.state == STATE_DOCKED ? "DOCKED" : (g.state == STATE_CRASHED ? "CRASHED" : (g.state == STATE_REPLAY ? "REPLAY" : "FLYING"));
        std::snprintf(t, sizeof t, "Orbital Dock 3D | %s L%d%s%s | dist %.1f m | closing %.2f m/s | tilt %.1f | roll %.1f | fuel %.0f%% | score %d | %.0f fps",
                      st, g.level + 1, g.autopilot.active ? " AUTOPILOT" : "", g.ship.eulerMode ? " EULER" : "", d.dist, d.closing, d.tiltDeg, d.rollDeg, g.ship.fuel, g.score.total, fps);
        if (GLFWwindow* w = glfwGetCurrentContext()) glfwSetWindowTitle(w, t);
    }
#endif
}
