#version 450 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D img_TrailMap;

void main() {
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    vec4 pixel = 0.5 *   imageLoad(img_TrailMap, pixel_coords);
    pixel     += 0.062 * imageLoad(img_TrailMap, pixel_coords + ivec2(1, 0));
    pixel     += 0.063 * imageLoad(img_TrailMap, pixel_coords + ivec2(-1, 0));
    pixel     += 0.062 * imageLoad(img_TrailMap, pixel_coords + ivec2(0, 1));
    pixel     += 0.063 * imageLoad(img_TrailMap, pixel_coords + ivec2(0, -1));
    pixel     += 0.062 * imageLoad(img_TrailMap, pixel_coords + ivec2(1, 1));
    pixel     += 0.063 * imageLoad(img_TrailMap, pixel_coords + ivec2(-1, -1));
    pixel     += 0.062 * imageLoad(img_TrailMap, pixel_coords + ivec2(-1, 1));
    pixel     += 0.063 * imageLoad(img_TrailMap, pixel_coords + ivec2(1, -1));

    //pixel /= 9;
    pixel *= 0.999;
    imageStore(img_TrailMap, pixel_coords, pixel);
}