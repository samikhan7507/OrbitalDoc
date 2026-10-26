// Orbital Dock 3D - HUD data bridge.
// Web build  : packs telemetry into a float array and hands it to the HTML HUD (web/shell.html).
// Native build: shows a one-line summary in the window title.
// The index list below is mirrored by the constant table `I` in web/shell.html.
// Author: Sami Khan
#pragma once
#include "game/game.h"
#include "engine/renderer.h"

enum HudIndex {
    H_SPEED = 0, H_DIST = 1, H_AXIAL = 2, H_LATERAL = 3, H_CLOSING = 4, H_LATSPEED = 5, H_TILT = 6, H_ROLL = 7,
    H_ROLLRATE = 8, H_FUEL = 9, H_STATE = 10, H_CAM = 11, H_ASSIST = 12, H_PORTX = 13, H_PORTY = 14, H_PORTVIS = 15,
    H_FPS = 16, H_INVERT = 17, H_OKALIGN = 18, H_OKSPEED = 19, H_OKATT = 20, H_OKROLL = 21, H_TIME = 22,
    H_POS = 23 /*3*/, H_QUAT = 26 /*4*/, H_MAT = 30 /*16*/, H_CRASH = 46,
    H_EULERMODE = 47, H_YAW = 48, H_PITCH = 49, H_EROLL = 50, H_AUTOPILOT = 51, H_LEVEL = 52,
    H_SCORE = 53, H_SC_TIME = 54, H_SC_FUEL = 55, H_SC_PREC = 56, H_GHOST = 57, H_REPLAYPROG = 58,
    H_ENGINE = 59, H_RCS = 60, H_NEARDEBRIS = 61, H_BRAKE = 62, H_GRADE = 63, H_BEST = 64, H_NEWBEST = 65,
    H_SC_BASE = 66, H_ASSISTED = 67, H_BLOOM = 68, H_GHOSTON = 69, H_COUNT = 70
};

void hudUpdate(const Game& g, const Renderer& r, float fps, bool invertPitch);
