// ============================================================================
//  ORBITAL DOCK 3D  -  Space Station Docking Simulator
//  Author : Sami Khan  (https://www.linkedin.com/in/samikhan07 | https://github.com/samikhan7507)
//  Engine : C++17 + OpenGL ES 3.0 / GLSL, own Vec3/Mat4/Quat math (no GLM)
// ============================================================================
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine/gfx.h"
#include "engine/renderer.h"
#include "game/game.h"
#include "hud.h"
#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #include <emscripten/html5.h>
#endif

// ------------------------------------------------------------------ global state
static GLFWwindow* gWindow = nullptr;
static Renderer gRenderer;
static Game gGame;
static int gWidth = 1280, gHeight = 720;
static int gShadowSize = 2048;
static bool gInvertPitch = false;
static double gLastTime = 0, gFps = 60;
static double gMouseX = 0, gMouseY = 0;
static bool gMouseDown = false;

// remote inputs (touch / gamepad written from JavaScript)
static float gTouch[7] = {0, 0, 0, 0, 0, 0, 0};   // tx, ty, tz, pitch, yaw, roll, brake
static bool gKillTouch = false;

// native test hooks (--shot ...)
static const char* gShotPath = nullptr;
static int gShotFrames = 30;

enum UiAction {
    UI_CAMERA = 1, UI_ASSIST = 2, UI_KILLROT = 3, UI_RESET = 4, UI_INVERT = 5, UI_AUTOPILOT = 6, UI_EULER = 7,
    UI_GIMBAL = 8, UI_REPLAY = 9, UI_KILLROT_OFF = 10, UI_BLOOM = 11, UI_GHOST = 12, UI_SHADOW = 13, UI_FXAA = 14,
    UI_LEVEL1 = 20
};

static void doAction(int a) {
    switch (a) {
    case UI_CAMERA: gGame.cam.mode = (gGame.cam.mode + 1) % CAM_COUNT; break;
    case UI_ASSIST: gGame.ship.assist = !gGame.ship.assist; break;
    case UI_RESET: gGame.reset(); break;
    case UI_INVERT: gInvertPitch = !gInvertPitch; break;
    case UI_AUTOPILOT: gGame.toggleAutopilot(); break;
    case UI_EULER: gGame.toggleEuler(); break;
    case UI_GIMBAL: gGame.gimbalDemo(); break;
    case UI_REPLAY: gGame.startReplay(); break;
    case UI_BLOOM: gRenderer.bloomOn = !gRenderer.bloomOn; break;
    case UI_GHOST: gGame.toggleGhost(); break;
    case UI_SHADOW: gRenderer.shadowOn = !gRenderer.shadowOn; break;
    case UI_FXAA: gRenderer.fxaaOn = !gRenderer.fxaaOn; break;
    default:
        if (a >= UI_LEVEL1 && a < UI_LEVEL1 + 4) gGame.setLevel(a - UI_LEVEL1);
        break;
    }
}

#ifdef __EMSCRIPTEN__
extern "C" {
EMSCRIPTEN_KEEPALIVE void set_touch_input(float tx, float ty, float tz, float pit, float yaw, float roll, int brake) {
    gTouch[0] = tx; gTouch[1] = ty; gTouch[2] = tz; gTouch[3] = pit; gTouch[4] = yaw; gTouch[5] = roll; gTouch[6] = (float)brake;
}
EMSCRIPTEN_KEEPALIVE void ui_action(int id) {
    if (id == UI_KILLROT) gKillTouch = true;
    else if (id == UI_KILLROT_OFF) gKillTouch = false;
    else doAction(id);
}
EMSCRIPTEN_KEEPALIVE void orbit_drag(float dx, float dy) {
    gGame.cam.orbitYaw -= dx * 0.006f;
    gGame.cam.orbitPitch = clampf(gGame.cam.orbitPitch + dy * 0.006f, -1.4f, 1.4f);
}
EMSCRIPTEN_KEEPALIVE void orbit_zoom(float d) {
    gGame.cam.orbitDist = clampf(gGame.cam.orbitDist * (1.0f + d), 5.0f, 60.0f);
}
}
#endif

// ------------------------------------------------------------------ input
static bool key(int k) { return glfwGetKey(gWindow, k) == GLFW_PRESS; }

static void onKey(GLFWwindow*, int k, int, int action, int) {
    if (action != GLFW_PRESS) return;
    switch (k) {
    case GLFW_KEY_C: doAction(UI_CAMERA); break;
    case GLFW_KEY_V: doAction(UI_ASSIST); break;
    case GLFW_KEY_N: doAction(UI_RESET); break;
    case GLFW_KEY_I: doAction(UI_INVERT); break;
    case GLFW_KEY_P: doAction(UI_AUTOPILOT); break;
    case GLFW_KEY_G: doAction(UI_EULER); break;
    case GLFW_KEY_L: doAction(UI_GIMBAL); break;
    case GLFW_KEY_Y: doAction(UI_REPLAY); break;
    case GLFW_KEY_B: doAction(UI_BLOOM); break;
    case GLFW_KEY_O: doAction(UI_GHOST); break;
    case GLFW_KEY_1: case GLFW_KEY_2: case GLFW_KEY_3: case GLFW_KEY_4: doAction(UI_LEVEL1 + (k - GLFW_KEY_1)); break;
    default: break;
    }
}
static void onMouseButton(GLFWwindow* w, int b, int action, int) {
    if (b == GLFW_MOUSE_BUTTON_LEFT) {
        gMouseDown = action == GLFW_PRESS;
        glfwGetCursorPos(w, &gMouseX, &gMouseY);
    }
}
static void onCursor(GLFWwindow*, double x, double y) {
    double dx = x - gMouseX, dy = y - gMouseY;
    gMouseX = x; gMouseY = y;
    if (!gMouseDown) return;
    if (gGame.cam.mode == CAM_ORBIT) {
        gGame.cam.orbitYaw -= (float)dx * 0.006f;
        gGame.cam.orbitPitch = clampf(gGame.cam.orbitPitch + (float)dy * 0.006f, -1.4f, 1.4f);
    } else if (gGame.state == STATE_FLYING) {
        float s = gInvertPitch ? 1.0f : -1.0f;
        Spacecraft& sh = gGame.ship;
        if (sh.eulerMode) { sh.eulRate.y += s * (float)dy * 0.0035f; sh.eulRate.x -= (float)dx * 0.0035f; }
        else { sh.w.x += s * (float)dy * 0.0035f; sh.w.y -= (float)dx * 0.0035f; }
    }
}
static void onScroll(GLFWwindow*, double, double yoff) {
    gGame.cam.orbitDist = clampf(gGame.cam.orbitDist * (1.0f - 0.08f * (float)yoff), 5.0f, 60.0f);
}

#ifndef __EMSCRIPTEN__
// Native gamepad (any controller GLFW has a mapping for). Web uses the browser Gamepad API in shell.html.
static void pollGamepad(float& tx, float& ty, float& tz, float& pitch, float& yaw, float& roll, bool& brake) {
    static bool prev[16] = {false};
    GLFWgamepadstate gs;
    if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &gs)) return;
    auto dz = [](float v) { return std::fabs(v) < 0.15f ? 0.0f : v; };
    tx += dz(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    tz -= dz(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    yaw += dz(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
    pitch -= dz(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
    if (gs.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) ty += 1; if (gs.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) ty -= 1;
    if (gs.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]) roll -= 1; if (gs.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) roll += 1;
    brake = brake || gs.buttons[GLFW_GAMEPAD_BUTTON_A];
    struct M { int b, a; } map[] = {{GLFW_GAMEPAD_BUTTON_Y, UI_CAMERA}, {GLFW_GAMEPAD_BUTTON_B, UI_ASSIST}, {GLFW_GAMEPAD_BUTTON_START, UI_RESET}, {GLFW_GAMEPAD_BUTTON_BACK, UI_AUTOPILOT}};
    for (int i = 0; i < 4; ++i) {
        bool d = gs.buttons[map[i].b] == GLFW_PRESS;
        if (d && !prev[i]) doAction(map[i].a);
        prev[i] = d;
    }
}
#endif

static void pollInput() {
    Spacecraft& s = gGame.ship;
    auto ax = [](int pos, int neg) { return (key(pos) ? 1.0f : 0.0f) - (key(neg) ? 1.0f : 0.0f); };
    float tx = ax(GLFW_KEY_D, GLFW_KEY_A) + gTouch[0];
    float ty = ax(GLFW_KEY_R, GLFW_KEY_F) + gTouch[1];
    float tz = ax(GLFW_KEY_W, GLFW_KEY_S) + gTouch[2];
    float pitch = ax(GLFW_KEY_UP, GLFW_KEY_DOWN) * (gInvertPitch ? -1.0f : 1.0f) + gTouch[3];
    float yaw = ax(GLFW_KEY_RIGHT, GLFW_KEY_LEFT) + gTouch[4];
    float roll = ax(GLFW_KEY_E, GLFW_KEY_Q) + gTouch[5];
    bool brake = key(GLFW_KEY_Z) || key(GLFW_KEY_SPACE) || gTouch[6] > 0.5f;
#ifndef __EMSCRIPTEN__
    pollGamepad(tx, ty, tz, pitch, yaw, roll, brake);
#endif
    s.thrustIn = Vec3(clampf(tx, -1, 1), clampf(ty, -1, 1), clampf(tz, -1, 1));
    s.rotIn = Vec3(clampf(pitch, -1, 1), clampf(yaw, -1, 1), clampf(roll, -1, 1));
    s.brake = brake;
    s.killRot = key(GLFW_KEY_X) || gKillTouch;
    if (gGame.state != STATE_FLYING) { s.thrustIn = Vec3(); s.rotIn = Vec3(); s.brake = false; }
}

// ------------------------------------------------------------------ frame
static void updateSize() {
#ifdef __EMSCRIPTEN__
    double cw, ch;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    double dpr = emscripten_get_device_pixel_ratio();
    static double cap = EM_ASM_INT({ return ('ontouchstart' in window || navigator.maxTouchPoints > 0) ? 1 : 0; }) ? 1.25 : 2.0;
    if (dpr > cap) dpr = cap;
    int w = (int)(cw * dpr), h = (int)(ch * dpr);
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    if (w != gWidth || h != gHeight) {
        gWidth = w; gHeight = h;
        emscripten_set_canvas_element_size("#canvas", w, h);
    }
#else
    glfwGetFramebufferSize(gWindow, &gWidth, &gHeight);
#endif
}

static void frame() {
    double now = glfwGetTime();
    float dt = (float)(now - gLastTime);
    gLastTime = now;
    if (dt > 0.05f) dt = 0.05f;
    if (dt > 0) gFps += (1.0 / dt - gFps) * 0.05;
    glfwPollEvents();
    updateSize();
    pollInput();
    gGame.update(dt);
    gRenderer.render(gGame, gWidth, gHeight);
    hudUpdate(gGame, gRenderer, (float)gFps, gInvertPitch);
    glfwSwapBuffers(gWindow);
}

int main(int argc, char** argv) {
    bool poseSet = false, autopilot = false, gimbal = false;
    Vec3 poseP, poseLook, shotThrust, shotRot;
    int camMode = -1, level = -1;
    float simSeconds = 0.0f;
    for (int i = 1; i < argc; ++i) {
        auto num = [&](int k) { return (float)std::atof(argv[i + k]); };
        if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) gShotPath = argv[++i];
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) gShotFrames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--cam") && i + 1 < argc) camMode = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--level") && i + 1 < argc) level = std::atoi(argv[++i]) - 1;
        else if (!std::strcmp(argv[i], "--sim") && i + 1 < argc) simSeconds = num(1), ++i;
        else if (!std::strcmp(argv[i], "--autopilot")) autopilot = true;
        else if (!std::strcmp(argv[i], "--gimbal")) gimbal = true;
        else if (!std::strcmp(argv[i], "--thrust") && i + 6 < argc) { shotThrust = Vec3(num(1), num(2), num(3)); shotRot = Vec3(num(4), num(5), num(6)); i += 6; }
        else if (!std::strcmp(argv[i], "--pose") && i + 6 < argc) { poseP = Vec3(num(1), num(2), num(3)); poseLook = Vec3(num(4), num(5), num(6)); poseSet = true; i += 6; }
    }
    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return 1; }
#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    gShadowSize = EM_ASM_INT({ return ('ontouchstart' in window || navigator.maxTouchPoints > 0) ? 1 : 0; }) ? 1024 : 2048;
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    gWindow = glfwCreateWindow(gWidth, gHeight, "Orbital Dock 3D", nullptr, nullptr);
    if (!gWindow) { std::fprintf(stderr, "Window/context creation failed (need OpenGL 3.3)\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);
    if (!loadGL([](const char* n) -> void* { return (void*)glfwGetProcAddress(n); })) {
        std::fprintf(stderr, "Failed to load OpenGL functions\n");
        return 1;
    }
    if (!gRenderer.init(gShadowSize)) { std::fprintf(stderr, "Renderer init failed\n"); return 1; }

    glfwSetKeyCallback(gWindow, onKey);
    glfwSetMouseButtonCallback(gWindow, onMouseButton);
    glfwSetCursorPosCallback(gWindow, onCursor);
    glfwSetScrollCallback(gWindow, onScroll);

    std::srand(12345);
    if (level >= 0) gGame.setLevel(level); else gGame.reset();
    if (camMode >= 0) gGame.cam.mode = camMode;
    if (poseSet) {
        gGame.ship.pos = poseP;
        gGame.ship.q = Quat::fromTo(Vec3(0, 0, -1), normalize(poseLook - poseP));
        gGame.ship.vel = Vec3();
        gGame.cam.snap(gGame.ship);
    }
    if (gimbal) gGame.gimbalDemo();
    if (autopilot) gGame.toggleAutopilot();
    gLastTime = glfwGetTime();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    if (gShotPath) {
        // headless self-test: simulate, then render N frames, dump the last one as PPM, exit
        int simSteps = (int)(simSeconds * 60.0f);
        for (int i = 0; i < simSteps; ++i) { gGame.ship.thrustIn = shotThrust; gGame.ship.rotIn = shotRot; gGame.update(1.0f / 60.0f); }
        for (int i = 0; i < gShotFrames; ++i) {
            gGame.ship.thrustIn = shotThrust; gGame.ship.rotIn = shotRot;
            gGame.update(1.0f / 60.0f);
            gRenderer.render(gGame, gWidth, gHeight);
            glfwSwapBuffers(gWindow);
        }
        std::vector<unsigned char> px((size_t)gWidth * gHeight * 4);
        glReadPixels(0, 0, gWidth, gHeight, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        FILE* f = std::fopen(gShotPath, "wb");
        std::fprintf(f, "P6\n%d %d\n255\n", gWidth, gHeight);
        for (int y = gHeight - 1; y >= 0; --y)
            for (int x = 0; x < gWidth; ++x) std::fwrite(&px[((size_t)y * gWidth + x) * 4], 1, 3, f);
        std::fclose(f);
        std::printf("screenshot written: %s (%dx%d) state=%d dist=%.1f\n", gShotPath, gWidth, gHeight, gGame.state, gGame.dock.dist);
        return 0;
    }
    while (!glfwWindowShouldClose(gWindow)) frame();
    glfwTerminate();
#endif
    return 0;
}
