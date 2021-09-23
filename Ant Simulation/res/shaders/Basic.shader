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

    vec4 add = vec4(0.0);
    if (abs(TexCoord.x * 2560 - 2560 / 2) < 20.0 && abs(TexCoord.y * 1440 - 1440 / 2) < 20.0) {
        add = vec4(0.4, 0.4, 0.4, 1.0);
    }
    
    color = vec4(mapColor.rgb * mapColor.a + agentColor.rgb * agentColor.a + add.rgb * add.a, 1.0);
};

