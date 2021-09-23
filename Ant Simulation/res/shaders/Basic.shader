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
#version 430 core

layout(location = 0) out vec4 color;

in vec2 TexCoord;

uniform sampler2D u_AgentTexture;
uniform sampler2D u_MapTexture;

void main()
{
    vec4 agentColor = texture(u_AgentTexture, TexCoord);
    vec4 mapColor = texture(u_MapTexture, TexCoord);
    
    color = vec4(mapColor.rgb * mapColor.a + agentColor.rgb * agentColor.a, 1.0);
};

