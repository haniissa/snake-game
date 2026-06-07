#version 450

layout(push_constant) uniform PC {
    vec2  pos;
    vec2  size;
    vec4  color;
    float time;
    int   mode;
    vec2  _pad;
} pc;


layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat int fragMode;
layout(location = 0) out vec4 outColor;


//-------Noise helpers----------------
float hash(vec2 p){
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.31);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x),
        f.y
    );
}

//Fractional Brownian Motion - layered noise
float fbm(vec2 p){
    float v = 0.0, a = 0.5;
    for(int i=0; i < 5; i++){
        v += a * noise(p);
        p *= 2.1;
        a *= 0.5;
    }
    return v;
}

//----Mode 0 . Solid rounded rectangle ---------------
vec4 drawSolid(vec2 uv, vec4 color){
    //Signed-distance rounding: squeeze UV to [-0.5, 0.5], chamfer corners
    vec2 c = uv - 0.5;
    vec2 q = abs(c) - 0.4;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.06; // r = 0.06
    float a = (1.0 - smoothstep(-0.01, 0.02, d)) * color.a;
    return vec4(color.rgb, a);
}

//============Mode 1 Fire (additive pipeline) ------------------
// Flame rises upward; hottest at center-bottom

vec4 drawFire(vec2 uv, float time) {
    //Animate upward + slight horizontal sway
    vec2 p = uv + vec2(sin(time * 1.3 + uv.y * 4.0) * 0.04, -time * 0.9);

    float n = fbm(p * 2.8);
    float n2 = fbm(p * 6.0 + vec2(time * 0.4, 0.0)) * 0.4;
    float f  = clamp(n + n2, 0.0, 1.0);


    //Height mask: fire rises (bottom-hot, top-fades)
    float rise = pow(1.0 - uv.y, 1.6);
    //horizontal soft edges
    float hEdge = smoothstep(0.0, 0.18, uv.x) * smoothstep(1.0, 0.82, uv.x);
    float fire = smoothstep(0.05, 0.95,  f * rise * 2.4) * hEdge;

    //Colour ramp: red -> orange -> yellow -> white-hot core
    vec3 cool = vec3(0.85, 0.05, 0.0);
    vec3 mid = vec3(1.00, 0.40, 0.0);
    vec3 hot = vec3(1.00, 0.90, 0.4);
    vec3 core = vec3(1.00, 1.00, 0.9);

    vec3 col = mix(cool, mid, smoothstep(0.0 , 0.35 , fire));
         col = mix(col, hot,  smoothstep(0.35 , 0.65 , fire));
         col = mix(col, core, smoothstep(0.65 , 0.90 , fire));


    //Additive: pre-multiply by alpha for additive blend
    return vec4(col * fire, fire);
}


//-----Mode 2 * Soft drop shadow
vec4 drawShadow(vec2 uv) {
    vec2 c = uv - 0.5;
    float dist = length(c * vec2(1.0, 0.75)); //slightly squashed
    float a    = (1.0 - smoothstep(0.18, 0.50, dist)) * 0.55;
    return vec4(0.0, 0.0, 0.0, a);
}

//-----Mode 3 * Pulsing circle (food) ------
vec4 drawCircle(vec2 uv, vec4 color, float time){
    vec2 c = uv - 0.5;
    float dist = length(c);

    //Glow ring pulsing with time
    float pulse = 0.5 + 0.5 * sin(time * 4.0);
    float glow = (1.0 - smoothstep(0.38 + pulse * 0.04, 0.48 + pulse * 0.04, dist));
    float solid = 1.0 - smoothstep(0.36, 0.42, dist);


    vec3 col = color.rgb + glow * vec3(1.0, 0.6, 0.3);
    float a = solid * color.a;

    //Inner highlight (specular)
    float spec = 1.0 - smoothstep(0.0, 0.15, length(c - vec2(-0.08, -0.10)));
    col += spec * 0.35;

    return vec4(col, a);
}

//---Main
void main() {
    switch (fragMode){
        case 1: outColor = drawFire(fragUV, pc.time); break;
        case 2: outColor = drawShadow(fragUV); break;
        case 3: outColor = drawCircle(fragUV, pc.color, pc.time); break;
        default: outColor = drawSolid(fragUV, pc.color); break;
    }
}
