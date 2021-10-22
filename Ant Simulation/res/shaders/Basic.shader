#shader vertex
#version 450 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoords;

out vec2 TexCoord;

uniform vec2 u_TextureSize;
uniform vec2 u_ScreenSize;

void main()
{
    vec4 newPosition = position;

    float textureAspect = u_TextureSize.x / u_TextureSize.y;
    float screenAspect = u_ScreenSize.x / u_ScreenSize.y;

    if (textureAspect > screenAspect)
    {
        newPosition.y *= screenAspect / textureAspect;
    }
    else 
    {
        newPosition.x *= textureAspect / screenAspect;
    }

    gl_Position = newPosition;
    TexCoord = textureCoords;
};

#shader fragment
#version 450 core

layout(location = 0) out vec4 color;

in vec2 TexCoord;

uniform sampler2D u_TrailTexture;
uniform sampler2D u_AgentTexture;
uniform sampler2D u_MapTexture;

uniform vec2 u_TextureSize;

void main()
{
    vec4 agentColor = texture(u_AgentTexture, TexCoord);
    vec4 trailColor = texture(u_TrailTexture, TexCoord);
    vec4 mapColor = texture(u_MapTexture, TexCoord);

    vec3 added = mapColor.rgb * mapColor.a + trailColor.rgb * trailColor.a + agentColor.rgb * agentColor.a;
    added.r = clamp(added.r, 0.0, 1.0);
    added.g = clamp(added.g, 0.0, 1.0);
    added.b = clamp(added.b, 0.0, 1.0);
    
    color = vec4(added, 1.0);
};

