#version 450 core
layout(local_size_x = 1024, local_size_y = 1) in;

struct Agent
{
    vec2 position;
    float angle;
    float angleVelocity;
    int hasFood;
    vec3 padding;
};

layout(rgba32f, binding = 0) uniform image2D img_TrailMap;
layout(rgba32f, binding = 1) uniform image2D img_Agents;
layout(rgba32f, binding = 2) uniform image2D img_Map;

layout(std430, binding = 3) buffer agentsLayout
{
    Agent agents[];
};
uniform float u_Time;
uniform vec2 u_TextureSize;
uniform int u_ArrayOffset;

const float PI = 3.1415926535;
const float senceDistance = 4.0;

float rand(vec2 co);
vec4 sence(ivec2 pos);

void main() {
    uint ind = gl_GlobalInvocationID.x + u_ArrayOffset;
    ivec2 pixel_coords = ivec2(agents[ind].position);

    agents[ind].position += vec2(cos(agents[ind].angle), sin(agents[ind].angle)) * 60.0 / 60.0;
    ivec2 new_pixel_coords = ivec2(agents[ind].position);

    vec4 left =   sence(ivec2(agents[ind].position + vec2(cos(agents[ind].angle + PI / 6.0), sin(agents[ind].angle + PI / 6.0)) * senceDistance));
    vec4 middle = sence(ivec2(agents[ind].position + vec2(cos(agents[ind].angle           ), sin(agents[ind].angle           )) * senceDistance));
    vec4 right =  sence(ivec2(agents[ind].position + vec2(cos(agents[ind].angle - PI / 6.0), sin(agents[ind].angle - PI / 6.0)) * senceDistance));

    float f_left = (agents[ind].hasFood == 1 ? left.g : left.r);
    float f_right = (agents[ind].hasFood == 1 ? right.g : right.r);
    float f_middle = (agents[ind].hasFood == 1 ? middle.g : middle.r);

    float angleChange = (PI / 6.0) * f_left - (PI / 6.0) * f_right + agents[ind].angleVelocity;
    //agents[ind].angleVelocity += rand(agents[ind].position) * 0.0001;
    //agents[ind].angleVelocity += rand(agents[ind].position) * 0.000001;
    agents[ind].angleVelocity = 0.0;

    if (abs(agents[ind].position.x - u_TextureSize.x / 2) < 20.0 && abs(agents[ind].position.y - u_TextureSize.y / 2) < 20.0) {
        agents[ind].hasFood = 0;
    }

    agents[ind].angle += angleChange * 10.0 + rand(agents[ind].position) * 0.0005;
    //agents[ind].angle += angleChange * 10.0;

    if (agents[ind].position.x < 0.0)
    {
       agents[ind].position.x = 0.0;
       agents[ind].angle = (rand(agents[ind].position) - 0.5) * PI;
    }
    else if (agents[ind].position.x >= u_TextureSize.x)
    {
        agents[ind].position.x = u_TextureSize.x - 1.0;
        agents[ind].angle = (rand(agents[ind].position) + 0.5) * PI;
    }

    if (agents[ind].position.y < 0.0)
    {
        agents[ind].position.y = 0.0;
        agents[ind].angle = (rand(agents[ind].position)) * PI;
    }
    else if (agents[ind].position.y >= u_TextureSize.y)
    {
        agents[ind].position.y = u_TextureSize.y - 1.0;
        agents[ind].angle = (rand(agents[ind].position) + 1.0) * PI;
    }

    vec4 mapColor = imageLoad(img_Map, new_pixel_coords);
    if (mapColor == vec4(128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0, 1.0)) 
    {
        agents[ind].position = vec2(pixel_coords);
        agents[ind].angle += PI + ((rand(agents[ind].position) - 0.5) * PI);
    }
    else if (mapColor == vec4(1.0, 0.0, 0.0, 1.0) && agents[ind].hasFood == 0)
    {
        imageStore(img_Map, new_pixel_coords, vec4(0.0));
        agents[ind].hasFood = 1;
        // TEMP
        agents[ind].angle += PI;
    }

    vec4 pixel = vec4(0.0, 1.0, 0.0, 1.0);
    if (agents[ind].hasFood == 1)
    {
        pixel = vec4(1.0, 0.0, 0.0, 1.0);
    }
    
    imageStore(img_TrailMap, pixel_coords, vec4(pixel.rgb / 2.0, 1.0));
    imageStore(img_Agents, pixel_coords, pixel);
}

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 sence(ivec2 pos)
{
    vec4 averageColor = vec4(0.0);
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            averageColor += imageLoad(img_TrailMap, pos + ivec2(x, y));
        }
    }
    averageColor /= 25.0;

    return averageColor;
}