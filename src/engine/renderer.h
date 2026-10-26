// Orbital Dock 3D - renderer.
// Frame layout:  1) Sun shadow map  2) far pass (sky/Earth/Moon)  3) near pass (station, ship, rocks, particles, ghost)
//                4) post: bright pass -> 2-level blur -> composite (bloom + FXAA + vignette)
// Splitting the depth range in two passes gives full precision both at 0.1 m and at 6000 m.
// Author: Sami Khan
#pragma once
#include "mesh.h"
#include "shader.h"
#include "../game/game.h"

struct RenderTarget { GLuint fbo = 0, tex = 0, depth = 0; int w = 0, h = 0; };

class Renderer {
public:
    bool init(int shadowSize);
    void render(const Game& g, int w, int h);

    bool bloomOn = true, shadowOn = true, fxaaOn = true;

    // Scene constants (world units ~ metres)
    Vec3 sunDir = normalize(Vec3(0.72f, 0.28f, 0.30f));
    Vec3 earthCenter{700.0f, -4950.0f, -2300.0f};
    float earthRadius = 4500.0f;
    Vec3 moonCenter{-2900.0f, 1500.0f, -8200.0f};
    float moonRadius = 1050.0f;

    // matrices of the last frame (the HUD uses them to project the docking port to the screen)
    Mat4 lastView, lastNearProj;
    float lastFovY = 1.0f;
    bool projectToScreen(const Vec3& world, float& nx, float& ny) const;

private:
    Shader sky_, earth_, atmo_, moon_, lit_, depth_, part_, bright_, blur_, comp_;
    Mesh sphereHi_, sphereMid_, sphereLo_, box_, cyl_, cone_, noseCone_, flame_, bigRing_, collar_;
    GLuint emptyVao_ = 0;
    GLuint partVao_ = 0, partVbo_ = 0;
    GLuint shadowTex_ = 0, shadowFbo_ = 0; int shadowSize_ = 2048;
    RenderTarget scene_, half_[2], quarter_[2];
    Mat4 lightVP_;

    struct LitArgs { Vec3 color{0.8f, 0.8f, 0.8f}; Vec3 emis{0, 0, 0}; float metal = 0.3f, rough = 0.5f; int pattern = 0; bool unlit = false; };
    void drawLit(const Mesh& m, const Mat4& model, const Vec3& size, const LitArgs& a);
    void drawStation(const Game& g);
    void drawShip(const Mat4& M, float engine, bool ghost);
    void drawDebris(const Game& g);
    void drawParticles(const Game& g, const Mat4& vp, int h);
    void shadowPass(const Game& g);
    void postPass(int w, int h);
    void makeTarget(RenderTarget& t, int w, int h, bool depth);
    void freeTarget(RenderTarget& t);
    void fullscreen() { glBindVertexArray(emptyVao_); glDrawArrays(GL_TRIANGLES, 0, 3); }

    bool shadowPass_ = false, ghostPass_ = false;
    Vec3 camPos_{0, 0, 0};
    float time_ = 0;
};
