#pragma once
#include <cstdint>

namespace monitor {
// Values match the generated sprite state table. Motion uses integer pixels;
// no interpolation, rotations, canvas buffers or runtime image decoding.
enum class CharacterState { Idle, Caution, Critical, Exhausted, Offline, Stale };
struct CharacterPose {
    int x=0, y=0;
    bool expression=false;
};
inline CharacterPose characterPose(CharacterState state, unsigned provider, uint32_t seed, uint32_t now) {
    // Work in 100 ms ticks. Unsigned wrap is safe, including LVGL's tick rollover.
    const uint32_t t=now/100u+seed%997u+provider*11u;
    const unsigned period=37u+seed%19u+provider*3u;
    const unsigned face=t%period;
    CharacterPose pose;
    pose.expression=face==12u||face==13u; // A short blink/expression, not a pose flicker loop.
    const unsigned breath=(t/(state==CharacterState::Exhausted||state==CharacterState::Offline?6u:3u))%8u;
    constexpr int breathe[8]={0,0,-1,-1,-2,-1,-1,0};
    pose.y=breathe[breath];
    const unsigned gesture=t%(83u+provider*7u+seed%13u);
    switch(state) {
        case CharacterState::Idle:
            // Occasional curious lean / two-step hop, spaced between breaths.
            if(gesture>=22&&gesture<30) {
                constexpr int hop[8]={-1,-2,-3,-3,-2,-1,0,0};
                pose.y=hop[gesture-22]; pose.x=provider%2?-1:1;
            }
            break;
        case CharacterState::Caution:
            if(gesture>=20&&gesture<28) pose.x=gesture<24?-1:1;
            break;
        case CharacterState::Critical:
            if(gesture>=20&&gesture<26) { pose.x=gesture%2?2:-2; pose.expression=true; }
            break;
        case CharacterState::Exhausted:
            pose.y=breath>=3&&breath<=5?-1:0;
            pose.expression=face>=12&&face<18;
            break;
        case CharacterState::Offline:
            pose.y=breath>=3&&breath<=5?-1:0;
            pose.expression=face>=12&&face<16;
            break;
        case CharacterState::Stale:
            if(gesture>=20&&gesture<28) { pose.x=gesture<24?1:-1; pose.expression=true; }
            break;
    }
    return pose;
}
}
