#version 450


//Push constants shared with fragment shader
layout(push_constant) uniform PC {
    vec2 pos;   //NDC top-left corner
    vec2 size;  //NDC width x height
    vec4 color;  // base RGBA

    float time;  // seconds (for animation)
    int mode;   // 0=solid 1=fire 2=shadow 3=circle
    vec2 _pad;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat int fragMode;

// Unit quad vertices for TRIANGLE_STRIP
const vec2 QUAD[4] = vec2[](
    vec2(0.0, 0.0),  // top-left
    vec2(1.0, 0.0), // top-right
    vec2(0.0, 1.0), // bottom-left
    vec2(1.0, 1.0) // bottom-right
);

void main(){
    vec2 local = QUAD[gl_VertexIndex];
    vec2 ndc   = pc.pos + local * pc.size;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragUV     = local;
    fragMode = pc.mode;
}
