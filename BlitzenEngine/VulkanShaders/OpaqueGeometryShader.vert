#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

struct Vertex
{
    vec3 pos;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};


//This is where the vertex buffer will be passed, using an SSBO
layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

#include "inputStructures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUvMap;

//The push constants have access to the vertex buffer's address and the model matrix
layout(push_constant) uniform constants
{
    mat4 matrix;
}PushConstants;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = sceneData.projection * sceneData.view * PushConstants.matrix * vec4(currentVertex.pos, 1.0);

    //Send the necessary data to the fragment shader
    outColor = currentVertex.color.xyz;
    outUvMap.x = currentVertex.uv_x;
    outUvMap.y = currentVertex.uv_y;
}