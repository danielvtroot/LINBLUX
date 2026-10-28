#pragma once
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include "raylib.h"
#include "rlgl.h"
namespace loblox {
// Snapshot for nested offscreen passes. Never assume the destination is FBO 0.
struct FrameState {
    GLint read=0,draw=0,viewport[4]{};
    GLboolean depthMask=GL_TRUE;
    bool depth=false,cull=false,scissor=false,srgb=false;
    Matrix projection{},modelview{};
    FrameState() {
        rlDrawRenderBatchActive();glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
        projection=rlGetMatrixProjection();modelview=rlGetMatrixModelview();
    }
    void restore() const {
        rlDrawRenderBatchActive();glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
        rlViewport(viewport[0],viewport[1],viewport[2],viewport[3]);rlSetMatrixProjection(projection);rlSetMatrixModelview(modelview);
        if(depth)rlEnableDepthTest();else rlDisableDepthTest();
        if(depthMask)rlEnableDepthMask();else rlDisableDepthMask();
        if(cull)rlEnableBackfaceCulling();else rlDisableBackfaceCulling();
        if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);else glDisable(GL_FRAMEBUFFER_SRGB);
    }
};
}
