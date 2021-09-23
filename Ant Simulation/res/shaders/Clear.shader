#version 450 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 1) uniform image2D img_Agents;

void main() {
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 pixel = vec4(0.0);
    
    imageStore(img_Agents, pixel_coords, pixel);
}