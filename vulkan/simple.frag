#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(1.0, 0.0, 0.0, 1.0);  
}