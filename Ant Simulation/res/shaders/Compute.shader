#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;

struct Agent
{
    vec2 position;
    float angle;
    int hasFood;
    int foodLeftAtHome;
    float timeAtSource;
    float timeAtWallCollision;
    int special;
};

layout(rgba32f, binding = 0) uniform image2D img_TrailMap;
layout(rgba32f, binding = 1) uniform image2D img_Agents;
layout(rgba8ui, binding = 2) uniform uimage2D img_Map;

layout(std430, binding = 3) buffer agentsLayout
{
    Agent agents[];
};

uniform float u_Time;
uniform vec2 u_TextureSize;
uniform int u_ArrayOffset;

const bool USE_WALL_FEROMONE = true;

const float PI = 3.141592653589793238;
const float PHI = 1.61803398874989484820459;
const float TIME_LAYING_TRAIL = 50.0;
const float TIME_ERASING_FEROMONES_AFTER_WALL_COLLISION = 0.10;

const float senceDistance = 10.0;

float rand(vec2 co);
float gold_noise(vec2 xy, float seed, float seed_counter);
vec4 sence(ivec2 pos);

void main() {
    float seed_counter = 0.0;

    uint ind = gl_GlobalInvocationID.x + u_ArrayOffset;
    Agent agent = agents[ind];

    ivec2 pixel_coords = ivec2(agent.position);
    
    // Move by current angle and speed
    agent.position += vec2(cos(agent.angle), sin(agent.angle)) * 90.0 / 60.0;
    ivec2 new_pixel_coords = ivec2(agent.position);

    // Sence feromone concentration
    vec4 left =   sence(ivec2(agent.position + vec2(cos(agent.angle + PI / 3.0), sin(agent.angle + PI / 3.0)) * senceDistance), agent.special);
    vec4 middle = sence(ivec2(agent.position + vec2(cos(agent.angle           ), sin(agent.angle           )) * senceDistance), agent.special);
    vec4 right =  sence(ivec2(agent.position + vec2(cos(agent.angle - PI / 3.0), sin(agent.angle - PI / 3.0)) * senceDistance), agent.special);

    float f_left =   (agent.hasFood == 1 ? left.g   : left.r);
    float f_right =  (agent.hasFood == 1 ? right.g  : right.r);
    float f_middle = (agent.hasFood == 1 ? middle.g : middle.r);
    
    float f_turning = 0.0;

    if (f_middle > f_left && f_middle > f_right) 
    {
        f_turning = 0.0;
    }

    if (f_left > f_middle && f_left > f_right)
    {
        seed_counter++;
        f_turning = gold_noise(agent.position, u_Time, seed_counter) * PI * 0.5;
    }

    if (f_right > f_middle && f_right > f_left)
    {
        seed_counter++;
        f_turning = -gold_noise(agent.position, u_Time, seed_counter) * PI * 0.5;
    }

    // Turn by anglechange
    seed_counter++;
    agent.angle += f_turning * 1.0 + (gold_noise(agent.position, u_Time, seed_counter) - 0.5) * 0.4;
    //agent.angle += gold_noise(agent.position, u_Time) - 0.5;
    

    // Bounce on map borders - horizontal
    if (agent.position.x < 0.0)
    {
         agent.position.x = 0.0;

         seed_counter++;
         agent.angle = (gold_noise(agent.position, u_Time, seed_counter) - 0.5) * PI;
         agent.timeAtWallCollision = u_Time;
    }
    else if (agent.position.x >= u_TextureSize.x)
    {
        agent.position.x = u_TextureSize.x - 1.0;

        seed_counter++;
        agent.angle = (gold_noise(agent.position, u_Time, seed_counter) + 0.5) * PI;
        agent.timeAtWallCollision = u_Time;
    }

    // Bounce on map borders - vertical
    if (agent.position.y < 0.0)
    {
        agent.position.y = 0.0;

        seed_counter++;
        agent.angle = (gold_noise(agent.position, u_Time, seed_counter)) * PI;
        agent.timeAtWallCollision = u_Time;
    }
    else if (agent.position.y >= u_TextureSize.y)
    {
        agent.position.y = u_TextureSize.y - 1.0;

        seed_counter++;
        agent.angle = (gold_noise(agent.position, u_Time, seed_counter) + 1.0) * PI;
        agent.timeAtWallCollision = u_Time;
    }

    // Interactions with map
    float xDiff = new_pixel_coords.x - pixel_coords.x;
    float yDiff = new_pixel_coords.y - pixel_coords.y;
    float maxDiff = max(abs(xDiff), abs(yDiff));
        
    for (float step = 1; step <= maxDiff; step++) {
        ivec2 intermediate_pixel_coords = ivec2(pixel_coords.x + xDiff * step / maxDiff, pixel_coords.y + yDiff * step / maxDiff);

        uvec4 mapColor = imageLoad(img_Map, intermediate_pixel_coords);
        if (mapColor == uvec4(128, 128, 128, 255))  // Wall collision
        {
            agent.position = vec2(pixel_coords);

            seed_counter++;
            agent.angle += PI + ((gold_noise(agent.position, u_Time, seed_counter) - 0.5) * PI);
            agent.timeAtWallCollision = u_Time;
            break;
        }
        else if (mapColor == uvec4(255, 0, 0, 255) && agent.hasFood == 0) // Food collision
        {
            imageStore(img_Map, intermediate_pixel_coords, uvec4(0.0));
            agent.hasFood = 1;
            agent.timeAtSource = u_Time;
            agent.angle += PI;
            break;
        }
        else if (mapColor == uvec4(100, 100, 100, 255)) // Home collision
        {
            agent.timeAtSource = u_Time;
            if (agent.hasFood == 1) {
                agent.foodLeftAtHome++;
                agent.hasFood = 0;
                agent.angle += PI;
            }
            break;
        }

        if (u_Time - agent.timeAtWallCollision < TIME_ERASING_FEROMONES_AFTER_WALL_COLLISION && USE_WALL_FEROMONE)
        {
            imageStore(img_TrailMap, intermediate_pixel_coords, vec4(0.0));
        }
    }

    // Show agent on map

    vec4 pixel = vec4(0.0, 1.0, 0.0, 1.0);
    if (agent.hasFood == 1)
    {
        pixel = vec4(1.0, 0.0, 0.0, 1.0);
    }

    ivec2 final_pixel_coords = ivec2(agent.position);

    // Show on agent map

    imageStore(img_Agents, final_pixel_coords, pixel);
    if (agent.special == 1) 
    {
        for (int x = -2; x <= 2; x++) {
            for (int y = -2; y <= 2; y++) {
                imageStore(img_Agents, final_pixel_coords + ivec2(x, y), vec4(1.0));
            }
        }
    }

    // Add feromone-trail

    vec4 oldValue = imageLoad(img_TrailMap, final_pixel_coords);

    pixel *= max(1 - (u_Time - agent.timeAtSource) / TIME_LAYING_TRAIL, 0);

    vec4 newValue = vec4(clamp(oldValue.rgb + pixel.rgb / 2.0, 0.0, 1.0), 1.0);

    imageStore(img_TrailMap, final_pixel_coords, newValue);
   

    agents[ind] = agent;
}

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float gold_noise(vec2 xy, float seed, float seed_counter)
{
    seed = fract(seed) + seed_counter;
    xy += vec2(1.0);

    return fract(sin(distance(xy * PHI, xy) * seed) * xy.x);
}

vec4 sence(ivec2 pos, int special)
{
    vec4 averageColor = vec4(0.0);
    for (int x = -5; x <= 5; x++)
    {
        for (int y = -5; y <= 5; y++)
        {
            vec4 trail = imageLoad(img_TrailMap, pos + ivec2(x, y));

            uvec4 map = imageLoad(img_Map, pos + ivec2(x, y));
            if (map == uvec4(100, 100, 100, 255)) 
            {
                trail.g = 1.0;
            }
            else if (map == uvec4(255, 0, 0, 255)) 
            {
                trail.r = 100.0;
            }
            averageColor += trail;

            if (special == 1) 
            {
                imageStore(img_Agents, pos + ivec2(x, y), vec4(1.0));
            }
        }
    }
    averageColor /= 25.0;

    return averageColor;
}