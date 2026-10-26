// Author: Sami Khan
#include "renderer.h"
#include "shaders.h"
#include <string>

static GLuint makeTexture(int w, int h, GLenum internal, GLenum format, GLenum type, GLint filter) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal, w, h, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void Renderer::makeTarget(RenderTarget& t, int w, int h, bool depth) {
    freeTarget(t);
    t.w = w; t.h = h;
    t.tex = makeTexture(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (depth) {
        glGenRenderbuffers(1, &t.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[renderer] framebuffer %dx%d incomplete\n", w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Renderer::freeTarget(RenderTarget& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    if (t.depth) glDeleteRenderbuffers(1, &t.depth);
    t = RenderTarget();
}

bool Renderer::init(int shadowSize) {
    auto fs = [](const char* body) { return std::string(Shaders::COMMON) + body; };
    bool ok = true;
    ok &= sky_.build(Shaders::SKY_VERT, fs(Shaders::SKY_FRAG).c_str(), "sky");
    ok &= earth_.build(Shaders::VERT, fs(Shaders::EARTH_FRAG).c_str(), "earth");
    ok &= atmo_.build(Shaders::VERT, fs(Shaders::ATMO_FRAG).c_str(), "atmosphere");
    ok &= moon_.build(Shaders::VERT, fs(Shaders::MOON_FRAG).c_str(), "moon");
    ok &= lit_.build(Shaders::VERT, fs(Shaders::LIT_FRAG).c_str(), "lit");
    ok &= depth_.build(Shaders::VERT, Shaders::DEPTH_FRAG, "depth");
    ok &= part_.build(Shaders::PART_VERT, Shaders::PART_FRAG, "particles");
    ok &= bright_.build(Shaders::POST_VERT, Shaders::BRIGHT_FRAG, "bright");
    ok &= blur_.build(Shaders::POST_VERT, Shaders::BLUR_FRAG, "blur");
    ok &= comp_.build(Shaders::POST_VERT, Shaders::COMPOSITE_FRAG, "composite");
    if (!ok) return false;

    sphereHi_ = MeshGen::sphere(160, 80);
    sphereMid_ = MeshGen::sphere(48, 24);
    sphereLo_ = MeshGen::sphere(20, 12);
    box_ = MeshGen::box();
    cyl_ = MeshGen::frustum(48, 1.0f, 1.0f, true);
    cone_ = MeshGen::frustum(32, 1.0f, 0.62f, true);       // station hub taper
    noseCone_ = MeshGen::frustum(32, 0.17f, 1.0f, true);   // ship nose
    flame_ = MeshGen::frustum(24, 1.0f, 0.05f, false);     // engine flame
    bigRing_ = MeshGen::torus(24.0f, 2.2f, 160, 24);
    collar_ = MeshGen::torus(2.5f, 0.32f, 48, 12);
    glGenVertexArrays(1, &emptyVao_);

    // particle buffer: pos(3) colour(3) alpha(1) size(1)
    glGenVertexArrays(1, &partVao_);
    glGenBuffers(1, &partVbo_);
    glBindVertexArray(partVao_);
    glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
    glBufferData(GL_ARRAY_BUFFER, 2400 * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(7 * sizeof(float)));
    glBindVertexArray(0);

    // Sun shadow map (depth texture, sampled manually in the lit shader)
    shadowSize_ = shadowSize;
    shadowTex_ = makeTexture(shadowSize_, shadowSize_, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_NEAREST);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[renderer] shadow framebuffer incomplete - shadows disabled\n");
        shadowOn = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
#ifndef __EMSCRIPTEN__
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif
    return true;
}

bool Renderer::projectToScreen(const Vec3& world, float& nx, float& ny) const {
    Mat4 vp = lastNearProj * lastView;
    float x = vp.m[0] * world.x + vp.m[4] * world.y + vp.m[8] * world.z + vp.m[12];
    float y = vp.m[1] * world.x + vp.m[5] * world.y + vp.m[9] * world.z + vp.m[13];
    float w = vp.m[3] * world.x + vp.m[7] * world.y + vp.m[11] * world.z + vp.m[15];
    nx = x / (w == 0 ? 1e-6f : w);
    ny = y / (w == 0 ? 1e-6f : w);
    if (w < 0) { nx = -nx; ny = -ny; }
    return w > 0.0f;
}

void Renderer::drawLit(const Mesh& m, const Mat4& model, const Vec3& size, const LitArgs& a) {
    if (shadowPass_) {                         // depth only: skip purely emissive bits
        if (a.unlit) return;
        depth_.setM4("uModel", model);
        m.draw();
        return;
    }
    LitArgs b = a;
    if (ghostPass_) { b.unlit = true; b.emis = Vec3(0.10f, 0.55f, 0.85f) * 0.9f; b.pattern = 0; }
    float nm[9];
    model.normalMatrix3(nm);
    lit_.setM4("uModel", model);
    lit_.setM3("uNormalMat", nm);
    lit_.setV3("uSize", size);
    lit_.setV3("uColor", b.color);
    lit_.setV3("uEmissive", b.emis);
    lit_.setF("uMetal", b.metal);
    lit_.setF("uRough", b.rough);
    lit_.setF("uUnlit", b.unlit ? 1.0f : 0.0f);
    lit_.setI("uPattern", b.pattern);
    m.draw();
}

void Renderer::drawStation(const Game& g) {
    const Station& st = g.station;
    Mat4 S = st.model();                                   // root of the station hierarchy
    LitArgs hull; hull.color = {0.78f, 0.80f, 0.84f}; hull.metal = 0.55f; hull.rough = 0.45f; hull.pattern = 1;
    LitArgs dark = hull; dark.color = {0.35f, 0.37f, 0.42f};

    drawLit(cyl_, S * Mat4::scale({3.0f, 3.0f, 32.0f}), {3, 3, 32}, hull);
    drawLit(cone_, S * Mat4::translate({0, 0, 18.0f}) * Mat4::scale({3.0f, 3.0f, 4.0f}), {3, 3, 4}, dark);
    drawLit(cyl_, S * Mat4::translate({0, 0, 20.25f}) * Mat4::scale({1.9f, 1.9f, 1.5f}), {1.9f, 1.9f, 1.5f}, hull);

    LitArgs lights; lights.color = {0.1f, 0.12f, 0.15f}; lights.pattern = 4; lights.metal = 0.2f;
    drawLit(collar_, S * Mat4::translate({0, 0, 21.0f}), {1, 1, 1}, lights);
    LitArgs hole; hole.color = {0.01f, 0.012f, 0.02f}; hole.metal = 0.0f; hole.rough = 1.0f;
    drawLit(cyl_, S * Mat4::translate({0, 0, 21.05f}) * Mat4::scale({2.1f, 2.1f, 0.1f}), {2.1f, 2.1f, 0.1f}, hole);
    LitArgs beacon; beacon.unlit = true;
    float blink = 0.5f + 0.5f * std::sin(time_ * 4.0f);
    beacon.emis = Vec3(1.0f, 0.15f, 0.1f) * (1.0f + 6.0f * blink);
    drawLit(sphereLo_, S * Mat4::translate({0, 2.6f, 18.4f}) * Mat4::scale({0.22f, 0.22f, 0.22f}), {1, 1, 1}, beacon);

    Mat4 R = S * Mat4::rotateZ(st.ringAngle);              // habitat ring: child of the station
    LitArgs ring; ring.color = {0.72f, 0.74f, 0.80f}; ring.metal = 0.5f; ring.rough = 0.5f; ring.pattern = 3;
    drawLit(bigRing_, R, {1, 1, 1}, ring);
    for (int k = 0; k < 4; ++k) {
        Mat4 spoke = R * Mat4::rotateZ(k * PI * 0.5f) * Mat4::translate({12.5f, 0, 0}) * Mat4::scale({21.0f, 1.2f, 1.2f});
        drawLit(box_, spoke, {21.0f, 1.2f, 1.2f}, hull);
    }

    LitArgs mod = hull; mod.color = {0.86f, 0.84f, 0.78f};
    drawLit(cyl_, S * Mat4::translate({0, 0, -6.0f}) * Mat4::rotateY(PI * 0.5f) * Mat4::scale({2.2f, 2.2f, 26.0f}), {2.2f, 2.2f, 26.0f}, mod);
    drawLit(cyl_, S * Mat4::translate({0, 0, -6.0f}) * Mat4::rotateX(PI * 0.5f) * Mat4::scale({2.0f, 2.0f, 22.0f}), {2.0f, 2.0f, 22.0f}, mod);
    LitArgs cap; cap.color = {0.2f, 0.2f, 0.25f}; cap.emis = {0.1f, 0.5f, 0.9f}; cap.metal = 0.1f;
    for (int s = -1; s <= 1; s += 2)
        drawLit(sphereLo_, S * Mat4::translate({s * 13.1f, 0, -6.0f}) * Mat4::scale({0.8f, 0.8f, 0.8f}), {1, 1, 1}, cap);

    drawLit(box_, S * Mat4::translate({0, 0, -14.0f}) * Mat4::scale({72.0f, 0.9f, 0.9f}), {72.0f, 0.9f, 0.9f}, dark);
    Vec3 sl = st.orient().conjugate().rotate(sunDir);       // panels rotate to face the Sun
    float panelAngle = std::atan2(sl.z, sl.y);
    LitArgs pan; pan.pattern = 2;
    for (int s = -1; s <= 1; s += 2)
        for (int i = 0; i < 4; ++i) {
            float x = s * (10.0f + 7.0f * i);
            Mat4 pm = S * Mat4::translate({x, 0, -14.0f}) * Mat4::rotateX(panelAngle) * Mat4::scale({6.4f, 0.12f, 12.0f});
            drawLit(box_, pm, {6.4f, 0.12f, 12.0f}, pan);
        }
    drawLit(cyl_, S * Mat4::translate({0, 0, -18.0f}) * Mat4::scale({0.25f, 0.25f, 4.0f}), {0.25f, 0.25f, 4.0f}, dark);
    LitArgs dish; dish.color = {0.9f, 0.9f, 0.92f}; dish.metal = 0.6f; dish.rough = 0.3f;
    drawLit(sphereMid_, S * Mat4::translate({0, 0, -20.2f}) * Mat4::scale({3.2f, 3.2f, 0.7f}), {3.2f, 3.2f, 0.7f}, dish);
}

void Renderer::drawShip(const Mat4& M, float engine, bool ghost) {
    ghostPass_ = ghost;
    LitArgs body; body.color = {0.88f, 0.89f, 0.92f}; body.metal = 0.35f; body.rough = 0.4f; body.pattern = 1;
    LitArgs trim; trim.color = {0.95f, 0.45f, 0.12f}; trim.metal = 0.3f; trim.rough = 0.5f; trim.pattern = 1;
    LitArgs grey; grey.color = {0.30f, 0.32f, 0.36f}; grey.metal = 0.6f; grey.rough = 0.4f;

    drawLit(cyl_, M * Mat4::translate({0, 0, 0.2f}) * Mat4::scale({0.7f, 0.7f, 3.2f}), {0.7f, 0.7f, 3.2f}, body);
    drawLit(noseCone_, M * Mat4::translate({0, 0, -2.2f}) * Mat4::scale({0.7f, 0.7f, 1.6f}), {0.7f, 0.7f, 1.6f}, trim);
    drawLit(cyl_, M * Mat4::translate({0, 0, -3.25f}) * Mat4::scale({0.08f, 0.08f, 0.7f}), {0.08f, 0.08f, 0.7f}, grey);
    for (int s = -1; s <= 1; s += 2) {
        drawLit(box_, M * Mat4::translate({s * 1.8f, 0, 0.9f}) * Mat4::rotateY(s * 0.25f) * Mat4::scale({2.4f, 0.08f, 1.5f}), {2.4f, 0.08f, 1.5f}, body);
        drawLit(box_, M * Mat4::translate({s * 2.9f, 0.0f, 0.62f}) * Mat4::rotateY(s * 0.25f) * Mat4::scale({0.5f, 0.1f, 0.5f}), {0.5f, 0.1f, 0.5f}, trim);
        drawLit(box_, M * Mat4::translate({s * 0.45f, 0.55f, 1.55f}) * Mat4::rotateZ(-s * 0.35f) * Mat4::scale({0.07f, 0.9f, 0.8f}), {0.07f, 0.9f, 0.8f}, trim);
    }
    drawLit(noseCone_, M * Mat4::translate({0, 0, 2.05f}) * Mat4::rotateY(PI) * Mat4::scale({0.5f, 0.5f, 0.5f}), {0.5f, 0.5f, 0.5f}, grey);
    if (engine > 0.02f && !ghost) {
        float len = 0.4f + 3.2f * engine;
        float flick = 0.92f + 0.08f * std::sin(time_ * 60.0f);
        LitArgs fl; fl.unlit = true; fl.emis = Vec3(1.2f, 0.7f, 0.25f) * 4.0f * flick;
        drawLit(flame_, M * Mat4::translate({0, 0, 2.3f + len * 0.5f}) * Mat4::scale({0.42f * flick, 0.42f * flick, len}), {1, 1, 1}, fl);
        LitArgs core; core.unlit = true; core.emis = Vec3(0.7f, 0.9f, 1.6f) * 5.0f;
        drawLit(flame_, M * Mat4::translate({0, 0, 2.3f + len * 0.3f}) * Mat4::scale({0.2f, 0.2f, len * 0.6f}), {1, 1, 1}, core);
    }
    LitArgs glass; glass.color = {0.03f, 0.07f, 0.12f}; glass.emis = {0.02f, 0.06f, 0.12f}; glass.metal = 0.9f; glass.rough = 0.08f;
    drawLit(sphereMid_, M * Mat4::translate({0, 0.42f, -0.9f}) * Mat4::scale({0.5f, 0.36f, 1.0f}), {1, 1, 1}, glass);
    if (!ghost) {
        float bl = 0.5f + 0.5f * std::sin(time_ * 5.0f);
        LitArgs red; red.unlit = true; red.emis = Vec3(1.0f, 0.05f, 0.05f) * (1.0f + 4.0f * bl);
        LitArgs grn; grn.unlit = true; grn.emis = Vec3(0.05f, 1.0f, 0.15f) * (1.0f + 4.0f * (1.0f - bl));
        drawLit(sphereLo_, M * Mat4::translate({-3.0f, 0.05f, 1.1f}) * Mat4::scale({0.1f, 0.1f, 0.1f}), {1, 1, 1}, red);
        drawLit(sphereLo_, M * Mat4::translate({3.0f, 0.05f, 1.1f}) * Mat4::scale({0.1f, 0.1f, 0.1f}), {1, 1, 1}, grn);
    }
    ghostPass_ = false;
}

void Renderer::drawDebris(const Game& g) {
    LitArgs rock; rock.pattern = 5; rock.metal = 0.05f; rock.rough = 0.9f;
    for (const Debris& d : g.debris) {
        Vec3 sz = d.stretch * d.radius;
        Mat4 M = Mat4::translate(d.posAt(g.station.t)) * Mat4::rotate(d.axis, d.angleAt(g.station.t)) * Mat4::scale(sz);
        drawLit(sphereMid_, M, sz, rock);
    }
}

void Renderer::drawParticles(const Game& g, const Mat4& vp, int h) {
    static std::vector<float> buf;
    buf.clear();
    for (const Particle& p : g.particles.list) {
        float k = p.life / p.maxLife;
        float size = p.size * (1.0f + (1.0f - k) * 1.6f);
        buf.insert(buf.end(), {p.p.x, p.p.y, p.p.z, p.col.x, p.col.y, p.col.z, k, size});
    }
    if (buf.empty()) return;
    part_.use();
    part_.setM4("uVP", vp);
    part_.setF("uPxScale", (float)h / (2.0f * std::tan(lastFovY * 0.5f)));
    glBindVertexArray(partVao_);
    glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(buf.size() * sizeof(float)), buf.data());
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS, 0, (GLsizei)(buf.size() / 8));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::shadowPass(const Game& g) {
    // Orthographic projection along the Sun direction, centred on the docking area.
    Vec3 centre(0, 0, 10.0f);
    Mat4 lv = Mat4::lookAt(centre + sunDir * 200.0f, centre, Vec3(0, 1, 0));
    lightVP_ = Mat4::ortho(-60, 60, -60, 60, 20.0f, 400.0f) * lv;
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, shadowSize_, shadowSize_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(2.0f, 4.0f);
    depth_.use();
    depth_.setM4("uVP", lightVP_);
    shadowPass_ = true;
    drawStation(g);
    if (g.cam.mode != CAM_COCKPIT) drawShip(Mat4::translate(g.ship.pos) * g.ship.q.toMat4(), 0.0f, false);
    drawDebris(g);
    shadowPass_ = false;
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::postPass(int w, int h) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    // bright pass: scene -> half resolution
    glBindFramebuffer(GL_FRAMEBUFFER, half_[0].fbo); glViewport(0, 0, half_[0].w, half_[0].h);
    bright_.use(); bright_.setI("uTex", 0); bright_.setF2("uTexel", 1.0f / w, 1.0f / h);
    glBindTexture(GL_TEXTURE_2D, scene_.tex); fullscreen();
    // blur half-res (H then V)
    glBindFramebuffer(GL_FRAMEBUFFER, half_[1].fbo);
    blur_.use(); blur_.setI("uTex", 0); blur_.setF2("uDir", 1.0f / half_[0].w, 0);
    glBindTexture(GL_TEXTURE_2D, half_[0].tex); fullscreen();
    glBindFramebuffer(GL_FRAMEBUFFER, half_[0].fbo);
    blur_.setF2("uDir", 0, 1.0f / half_[0].h);
    glBindTexture(GL_TEXTURE_2D, half_[1].tex); fullscreen();
    // downsample to quarter-res and blur twice for a wide glow
    glBindFramebuffer(GL_FRAMEBUFFER, quarter_[0].fbo); glViewport(0, 0, quarter_[0].w, quarter_[0].h);
    blur_.setF2("uDir", 1.0f / half_[0].w, 0);                 // first tap pattern doubles as a smooth downsample
    glBindTexture(GL_TEXTURE_2D, half_[0].tex); fullscreen();
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, quarter_[1].fbo);
        blur_.setF2("uDir", 1.0f / quarter_[0].w, 0);
        glBindTexture(GL_TEXTURE_2D, quarter_[0].tex); fullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, quarter_[0].fbo);
        blur_.setF2("uDir", 0, 1.0f / quarter_[0].h);
        glBindTexture(GL_TEXTURE_2D, quarter_[1].tex); fullscreen();
    }
    // composite to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, w, h);
    comp_.use();
    comp_.setI("uScene", 0); comp_.setI("uBloom1", 1); comp_.setI("uBloom2", 2);
    comp_.setF2("uTexel", 1.0f / w, 1.0f / h);
    comp_.setF("uBloom", bloomOn ? 1.0f : 0.0f);
    comp_.setF("uFxaa", fxaaOn ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, half_[0].tex);
    glActiveTexture(GL_TEXTURE0 + 2); glBindTexture(GL_TEXTURE_2D, quarter_[0].tex);
    glActiveTexture(GL_TEXTURE0);     glBindTexture(GL_TEXTURE_2D, scene_.tex);
    fullscreen();
    glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 2); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);     glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::render(const Game& g, int w, int h) {
    time_ = g.time;
    if (scene_.w != w || scene_.h != h) {
        makeTarget(scene_, w, h, true);
        int hw = w / 2 < 2 ? 2 : w / 2, hh = h / 2 < 2 ? 2 : h / 2;
        int qw = w / 4 < 2 ? 2 : w / 4, qh = h / 4 < 2 ? 2 : h / 4;
        for (int i = 0; i < 2; ++i) { makeTarget(half_[i], hw, hh, false); makeTarget(quarter_[i], qw, qh, false); }
    }
    float aspect = (float)w / (float)(h > 0 ? h : 1);
    float vt = std::tan(radians(32.0f));
    if (aspect < 1.5f) vt = std::min(vt * 1.5f / aspect, std::tan(radians(62.0f)));
    float fovy = 2.0f * std::atan(vt);
    Mat4 view = g.cam.view();
    camPos_ = g.cam.eye;
    Mat4 farProj = Mat4::perspective(fovy, aspect, 30.0f, 3.0e6f);
    Mat4 nearProj = Mat4::perspective(fovy, aspect, 0.1f, 3000.0f);
    lastView = view; lastNearProj = nearProj; lastFovY = fovy;

    if (shadowOn) shadowPass(g);

    glBindFramebuffer(GL_FRAMEBUFFER, scene_.fbo);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(emptyVao_);

    // ---------- FAR PASS: sky -> Earth -> Moon -> atmosphere
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sky_.use();
    sky_.setM4("uInvVP", (farProj * view.rotationOnly()).inverse());
    sky_.setV3("uSunDir", sunDir);
    sky_.setF("uTime", g.time);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    Mat4 farVP = farProj * view;

    Mat4 eM = Mat4::translate(earthCenter) * Mat4::rotateX(1.2f) * Mat4::rotateZ(0.41f) * Mat4::rotateY(g.time * 0.01f + 2.2f) * Mat4::scale({earthRadius, earthRadius, earthRadius});
    float nm[9];
    eM.normalMatrix3(nm);
    earth_.use();
    earth_.setM4("uModel", eM); earth_.setM4("uVP", farVP); earth_.setM3("uNormalMat", nm);
    earth_.setV3("uSunDir", sunDir); earth_.setV3("uCamPos", camPos_); earth_.setF("uTime", g.time);
    sphereHi_.draw();

    Mat4 mM = Mat4::translate(moonCenter) * Mat4::rotateY(2.35f) * Mat4::rotateX(0.2f) * Mat4::scale({moonRadius, moonRadius, moonRadius});
    mM.normalMatrix3(nm);
    Mat4 mInv = mM.inverse();
    moon_.use();
    moon_.setM4("uModel", mM); moon_.setM4("uVP", farVP); moon_.setM3("uNormalMat", nm);
    moon_.setV3("uSunObj", normalize(mInv.transformDir(sunDir)));
    moon_.setV3("uCamObj", mInv.transformPoint(camPos_));
    moon_.setF("uBump", 0.028f);
    sphereHi_.draw();

    float Rs = earthRadius * 1.045f;
    Mat4 aM = Mat4::translate(earthCenter) * Mat4::scale({Rs, Rs, Rs});
    aM.normalMatrix3(nm);
    atmo_.use();
    atmo_.setM4("uModel", aM); atmo_.setM4("uVP", farVP); atmo_.setM3("uNormalMat", nm);
    atmo_.setV3("uSunDir", sunDir); atmo_.setV3("uCamPos", camPos_); atmo_.setV3("uCenter", earthCenter);
    atmo_.setF("uRp", earthRadius); atmo_.setF("uRs", Rs);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    sphereHi_.draw();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---------- NEAR PASS: station + ship + rocks (fresh depth buffer, tight near/far planes)
    glClear(GL_DEPTH_BUFFER_BIT);
    Mat4 nearVP = nearProj * view;
    lit_.use();
    lit_.setM4("uVP", nearVP);
    lit_.setV3("uSunDir", sunDir);
    lit_.setV3("uCamPos", camPos_);
    lit_.setV3("uEarthDir", normalize(earthCenter));
    lit_.setF("uTime", g.time);
    lit_.setM4("uLightVP", lightVP_);
    lit_.setF("uShadowTexel", 1.0f / (float)shadowSize_);
    lit_.setF("uShadowOn", shadowOn ? 1.0f : 0.0f);
    lit_.setI("uShadow", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    drawStation(g);
    drawDebris(g);
    if (g.cam.mode != CAM_COCKPIT) drawShip(Mat4::translate(g.ship.pos) * g.ship.q.toMat4(), g.ship.engineGlow, false);
    glBindTexture(GL_TEXTURE_2D, 0);

    drawParticles(g, nearVP, h);

    Vec3 gp; Quat gq;                                   // ghost of your best run (additive blue-green hologram)
    if (g.ghostPose(gp, gq)) {
        lit_.use();
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glDepthMask(GL_FALSE);
        drawShip(Mat4::translate(gp) * gq.toMat4(), 0.0f, true);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }

    postPass(w, h);
}
