#pragma once
#include <algorithm>
#include <cmath>
namespace loblox::game {
// Presentation state belongs to one Play session, independent of both UI hosts.
struct HealthHud {
    bool visible=true;
    mutable float shown=-1;
    float sample(float health,float maximum,float dt) const {
        const float target=std::clamp(health/std::max(1.f,maximum),0.f,1.f);
        if(shown<0||target==0||target==1)shown=target;
        else shown+=(target-shown)*(1-std::exp(-10*std::clamp(dt,0.f,.1f)));
        return std::clamp(shown,0.f,1.f);
    }
};
}
