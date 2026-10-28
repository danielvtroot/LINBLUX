#include "rendering/SceneTarget.h"
#include <cstdlib>
namespace loblox {
namespace {
const char* kFxaaVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFxaaFragmentShader = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 resolution;
out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / resolution;
    vec3 rgbM  = texture(texture0, uv).rgb;
    vec3 rgbNW = texture(texture0, uv + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(texture0, uv + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(texture0, uv + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture(texture0, uv + vec2( 1.0,  1.0) * texel).rgb;

    float lumaM  = luma(rgbM);
    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) {
        finalColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)),
                     ((lumaNW + lumaSW) - (lumaNE + lumaSE)));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.0625, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    vec3 rgbA = 0.5 * (texture(texture0, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(texture0, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 *
        (texture(texture0, uv - dir * 0.5).rgb +
         texture(texture0, uv + dir * 0.5).rgb);
    float lumaB = luma(rgbB);
    finalColor = vec4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, 1.0);
}
)";

}
namespace {
GLint depthFormat(unsigned fbo) {
    GLint type=0,id=0,format=0,old=0;glGetIntegerv(GL_RENDERBUFFER_BINDING,&old);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    if(fbo){glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&id);}
    if(type==GL_RENDERBUFFER){glBindRenderbuffer(GL_RENDERBUFFER,id);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&format);}
    glBindRenderbuffer(GL_RENDERBUFFER,old);return format;
}
}
void SceneTarget::release() {
    if(!multisample_&&!color_&&!depth_&&!resolved_.id)return;
    FrameState state;
    if(state.read==GLint(multisample_)||state.read==GLint(resolved_.id))state.read=0;
    if(state.draw==GLint(multisample_)||state.draw==GLint(resolved_.id))state.draw=0;
    if(multisample_)glDeleteFramebuffers(1,&multisample_);
    if(color_)glDeleteRenderbuffers(1,&color_);if(depth_)glDeleteRenderbuffers(1,&depth_);
    multisample_=color_=depth_=0;
    if(resolved_.id)UnloadRenderTexture(resolved_);resolved_={};
    state.restore();
}
SceneTarget::~SceneTarget(){release();if(fxaa_.id)UnloadShader(fxaa_);}
bool SceneTarget::allocate(int width,int height,bool msaa) {
    if(resolved_.id&&resolved_.texture.width==width&&resolved_.texture.height==height&&bool(multisample_)==msaa)return true;
    release();resolved_=LoadRenderTexture(width,height);if(!IsRenderTextureValid(resolved_)){release();return false;}
    SetTextureFilter(resolved_.texture,TEXTURE_FILTER_BILINEAR);
    if(!msaa)return true;
    GLint samples=0,old=0;glGetIntegerv(GL_MAX_SAMPLES,&samples);glGetIntegerv(GL_RENDERBUFFER_BINDING,&old);
    if(samples<4||std::getenv("LINBLUX_DISABLE_MSAA")){msaaUnavailable_=true;release();return false;}
    const auto format=depthFormat(resolved_.id);
    glGenFramebuffers(1,&multisample_);glBindFramebuffer(GL_FRAMEBUFFER,multisample_);
    glGenRenderbuffers(1,&color_);glBindRenderbuffer(GL_RENDERBUFFER,color_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,GL_RGBA8,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color_);
    glGenRenderbuffers(1,&depth_);glBindRenderbuffer(GL_RENDERBUFFER,depth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,format,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_);
    const bool valid=format&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindRenderbuffer(GL_RENDERBUFFER,old);
    if(!valid){msaaUnavailable_=true;release();}return valid;
}
bool SceneTarget::begin(int width,int height,Antialiasing requested,RenderQuality quality) {
    actual_=requested==Antialiasing::Auto?(quality==RenderQuality::High?Antialiasing::MSAA4:
        quality==RenderQuality::Medium?Antialiasing::FXAA:Antialiasing::None):requested;
    if(actual_==Antialiasing::None){release();return false;}
    saved_=std::make_unique<FrameState>();
    if(actual_==Antialiasing::MSAA4&&(msaaUnavailable_||!allocate(width,height,true)))actual_=Antialiasing::FXAA;
    if(actual_==Antialiasing::FXAA){
        if(!shaderTried_){shaderTried_=true;
            if(!std::getenv("LINBLUX_DISABLE_FXAA"))fxaa_=LoadShaderFromMemory(kFxaaVertexShader,kFxaaFragmentShader);
            if(fxaa_.id==rlGetShaderIdDefault())fxaa_={};}
        if(!fxaa_.id||!allocate(width,height,false))actual_=Antialiasing::None;
    }
    if(actual_==Antialiasing::None){release();saved_->restore();saved_.reset();return false;}
    glBindFramebuffer(GL_FRAMEBUFFER,multisample_?multisample_:resolved_.id);
    if(multisample_)glEnable(GL_MULTISAMPLE);
    rlViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);
    return true;
}
void SceneTarget::finish() {
    if(!saved_)return;
    rlDrawRenderBatchActive();const int w=resolved_.texture.width,h=resolved_.texture.height;
    if(multisample_){glBindFramebuffer(GL_READ_FRAMEBUFFER,multisample_);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resolved_.id);
        glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
    // Keep scene depth for the editor's later F8/hover passes. Only copy when
    // both non-MSAA render targets have matching depth attachment formats.
    const auto sourceFormat=depthFormat(resolved_.id);
    const auto destinationFormat=saved_->draw?depthFormat(saved_->draw):0;
    if(destinationFormat&&sourceFormat==destinationFormat){
        glBindFramebuffer(GL_READ_FRAMEBUFFER,resolved_.id);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,saved_->draw);
        glBlitFramebuffer(0,0,w,h,saved_->viewport[0],saved_->viewport[1],saved_->viewport[0]+w,saved_->viewport[1]+h,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    }
    saved_->restore();glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);rlDisableDepthTest();rlDisableDepthMask();
    if(actual_==Antialiasing::FXAA){BeginShaderMode(fxaa_);const Vector2 resolution{float(w),float(h)};
        SetShaderValue(fxaa_,GetShaderLocation(fxaa_,"resolution"),&resolution.x,SHADER_UNIFORM_VEC2);}
    DrawTextureRec(resolved_.texture,{0,0,float(w),-float(h)},{0,0},WHITE);
    rlDrawRenderBatchActive();if(actual_==Antialiasing::FXAA)EndShaderMode();
    saved_->restore();saved_.reset();
}
}
