#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#define ASSERT(x) if (!(x)) __debugbreak();
#define GLCall(x) GLClearError();\
    x;\
    ASSERT(GLLogCall(#x, __FILE__, __LINE__))
#define LOG_ERROR() logError(__FILE__, __FUNCTION__, __LINE__)

#define PI 3.14159265358979f

#define AGENT_COUNT 65536

static void GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

static bool GLLogCall(const char* function, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        std::cout << "[OpenGL Error] (" << error << "): " << function << " " << file << ":" << line << std::endl;
        return false;
    }
    return true;
}

inline void logError(const char* file, const char* func, int line)
{
    std::cout << "Error: " << func << " " << file << ":" << line << std::endl;
}

struct ShaderProgramSource {
    std::string VertexSource;
    std::string FragmentSource;
};

static ShaderProgramSource ParseShader(const std::string& filepath)
{
    std::ifstream stream(filepath);

    enum class ShaderType {
        NONE = -1, VERTEX = 0, FRAGMENT = 1
    };

    std::string line;
    std::stringstream ss[2];
    ShaderType type = ShaderType::NONE;
    while (getline(stream, line))
    {
        if (line.find("#shader") != std::string::npos)
        {
            if (line.find("vertex") != std::string::npos)
            {
                type = ShaderType::VERTEX;
            }
            else if (line.find("fragment") != std::string::npos)
            {
                type = ShaderType::FRAGMENT;
            }
        }
        else
        {
            if (type != ShaderType::NONE) {
                ss[static_cast<int>(type)] << line << "\n";
            }
        }
    }

    return { ss[0].str(), ss[1].str() };
}

static std::string ReadFile(const std::string& filepath) {
    std::ifstream stream(filepath);

    if (!stream.good())
    {
        std::cout << "File " << filepath << " does not exists" << std::endl;
    }

    std::string line;
    std::stringstream ss;
    while (getline(stream, line)) {
        ss << line << "\n";
    }

    return ss.str();
}

static unsigned int CompileShader(unsigned int type, const std::string& source)
{
    GLCall(unsigned int id = glCreateShader(type));
    const char* src = source.c_str();
    GLCall(glShaderSource(id, 1, &src, nullptr));
    GLCall(glCompileShader(id));

    int result;
    GLCall(glGetShaderiv(id, GL_COMPILE_STATUS, &result));
    if (result == GL_FALSE)
    {
        int length;
        GLCall(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
        char* message = (char*)_malloca(length * sizeof(char));
        GLCall(glGetShaderInfoLog(id, length, &length, message));
        std::cout << "Failed to compile " <<
            (type == GL_VERTEX_SHADER ? "vertex" : "fragment") <<
            " shader" << std::endl;
        std::cout << message << std::endl;
        GLCall(glDeleteShader(id));
        return 0;
    }

    return id;
}

static unsigned int CompileComputeShader(const std::string& source) {
    GLCall(unsigned int id = glCreateShader(GL_COMPUTE_SHADER));
    const char* src = source.c_str();
    GLCall(glShaderSource(id, 1, &src, nullptr));
    GLCall(glCompileShader(id));

    int result;
    GLCall(glGetShaderiv(id, GL_COMPILE_STATUS, &result));
    if (result == GL_FALSE) {
        int length;
        GLCall(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
        char* message = (char*)_malloca(length * sizeof(char));
        GLCall(glGetShaderInfoLog(id, length, &length, message));
        std::cout << "Failed to compile compute shader" << std::endl;
        std::cout << message << std::endl;
        GLCall(glDeleteShader(id));
        return 0;
    }

    return id;
}

static unsigned int CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
{
    GLCall(unsigned int program = glCreateProgram());
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

    GLCall(glAttachShader(program, vs));
    GLCall(glAttachShader(program, fs));
    GLCall(glLinkProgram(program));
    GLCall(glValidateProgram(program));

    GLCall(glDeleteShader(vs));
    GLCall(glDeleteShader(fs));

    return program;
}

struct Agent {
    float position[2];
    float angle;
    float angleVelocity;
    int hasFood;
    float padding[3];
};

Agent agents[AGENT_COUNT];

int main(void)
{
    time_t randomSeed = std::time(NULL);
    std::cout << "Using random seed: " << randomSeed << std::endl;
    std::srand(randomSeed);

    float currentTime = 0.0f;
    float lastTime = 0.0f;
    float deltaTime;

    int screenWidth = 2560;
    int screenHeight = 1440;

    int textureWidth = 2560;
    int textureHeight = 1440;

    for (unsigned int i = 0; i < AGENT_COUNT; i++)
    {
        //float x = static_cast<float>(std::rand() % (textureWidth * 100)) / 100.0f;
        //float y = static_cast<float>(std::rand() % (textureHeight * 100)) / 100.0f;
        //x = textureWidth / 2.0f;
        //y = textureHeight / 2.0f;
        float distance = std::sqrt(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * static_cast<float>(std::min(textureWidth, textureHeight) / 2);
        float angle = static_cast<float>(std::rand() % static_cast<int>(2.0f * PI * 1000.0f)) / 1000.0f;
        float x = std::cos(angle) * distance + static_cast<float>(textureWidth) / 2.0f;
        float y = std::sin(angle) * distance + static_cast<float>(textureHeight) / 2.0f;
        agents[i] = { { textureWidth / 2.0f, textureHeight / 2.0f }, angle + 1.0f * PI / 3.0f, 0.0f, 0 };
        //agents[i] = { { x, y }, static_cast<float>(std::rand() % static_cast<int>(2.0f * PI * 1000.0f)) / 1000.0f, 0.0f, 0 };
    }

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        LOG_ERROR();
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(screenWidth, screenHeight, "Ants", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        LOG_ERROR();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
        LOG_ERROR();
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Device Hint: " << glGetString(GL_RENDERER) << std::endl;

    // Load map

    stbi_set_flip_vertically_on_load(true);

    int mapWidth, mapHeight, mapNrChannels;
    unsigned char* data = stbi_load("res/textures/Map_food_smiley.png", &mapWidth, &mapHeight, &mapNrChannels, STBI_rgb_alpha);

    if (!data) {
        std::cout << "Failed to load map texture" << std::endl;
    }

    ASSERT(mapWidth == textureWidth);
    ASSERT(mapHeight == textureHeight);

    // Texture

    unsigned int tex_Map;
    GLCall(glGenTextures(1, &tex_Map));
    GLCall(glActiveTexture(GL_TEXTURE2));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_Map));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, mapWidth, mapHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));

    GLCall(glBindImageTexture(2, tex_Map, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

    stbi_image_free(data);

    unsigned int tex_TrailMap;
    GLCall(glGenTextures(1, &tex_TrailMap));
    GLCall(glActiveTexture(GL_TEXTURE0));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_TrailMap));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureWidth, textureHeight, 0, GL_RGBA, GL_FLOAT, nullptr));

    GLCall(glBindImageTexture(0, tex_TrailMap, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

    unsigned int tex_Agents;
    GLCall(glGenTextures(1, &tex_Agents));
    GLCall(glActiveTexture(GL_TEXTURE1));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_Agents));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureWidth, textureHeight, 0, GL_RGBA, GL_FLOAT, nullptr));

    GLCall(glBindImageTexture(1, tex_Agents, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

    unsigned int ssbo;
    GLCall(glGenBuffers(1, &ssbo));
    GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
    GLCall(glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(agents), agents, GL_DYNAMIC_DRAW));
    GLCall(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo));
    GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));

    int work_grp_cnt[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

    printf("max global (total) work group counts: x:%i y:%i z:%i\n", work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);

    int work_grp_size[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

    printf("max local (in one shader) work group counts: x:%i y:%i z:%i\n", work_grp_size[0], work_grp_size[1], work_grp_size[2]);

    int work_grp_inv;

    GLCall(glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv));

    printf("max local work group invocations %i\n", work_grp_inv);
    
    float vertices[] = {
        // positions     // texture coords
         1.0f,  1.0f,    1.0f, 1.0f, // top right
         1.0f, -1.0f,    1.0f, 0.0f, // bottom right
        -1.0f, -1.0f,    0.0f, 0.0f, // bottom left
        -1.0f,  1.0f,    0.0f, 1.0f  // top lef
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    unsigned int vao;
    GLCall(glGenVertexArrays(1, &vao));
    GLCall(glBindVertexArray(vao));
    
    unsigned int vbo;
    GLCall(glGenBuffers(1, &vbo));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GLCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

    GLCall(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr));
    GLCall(glEnableVertexAttribArray(0));

    GLCall(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))));
    GLCall(glEnableVertexAttribArray(1));

    unsigned int ebo;
    GLCall(glGenBuffers(1, &ebo));
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));


    std::string computeSource = ReadFile("res/shaders/Compute.shader");
    unsigned int computeShader = CompileComputeShader(computeSource);

    GLCall(unsigned int computeProgram = glCreateProgram());
    GLCall(glAttachShader(computeProgram, computeShader));
    GLCall(glLinkProgram(computeProgram));
    GLCall(glValidateProgram(computeProgram));
    GLCall(glDeleteShader(computeShader));

    GLCall(glUseProgram(computeProgram));

    GLCall(int timeLocation = glGetUniformLocation(computeProgram, "u_Time"));
    //ASSERT(timeLocation != -1);
    
    GLCall(int computeTextureSizeLocation = glGetUniformLocation(computeProgram, "u_TextureSize"));
    ASSERT(computeTextureSizeLocation != -1);
    GLCall(glUniform2f(computeTextureSizeLocation, static_cast<float>(textureWidth), static_cast<float>(textureHeight)));

    GLCall(int arrayOffsetLocation = glGetUniformLocation(computeProgram, "u_ArrayOffset"));
    ASSERT(arrayOffsetLocation != -1);
    GLCall(glUniform1i(arrayOffsetLocation, 0));

    std::string fadeSource = ReadFile("res/shaders/Fade.shader");
    unsigned int fadeShader = CompileComputeShader(fadeSource);

    GLCall(unsigned int fadeProgram = glCreateProgram());
    GLCall(glAttachShader(fadeProgram, fadeShader));
    GLCall(glLinkProgram(fadeProgram));
    GLCall(glValidateProgram(fadeProgram));
    GLCall(glDeleteShader(fadeShader));
    GLCall(glUseProgram(fadeProgram));

    std::string clearSource = ReadFile("res/shaders/Clear.shader");
    unsigned int clearShader = CompileComputeShader(clearSource);

    GLCall(unsigned int clearProgram = glCreateProgram());
    GLCall(glAttachShader(clearProgram, clearShader));
    GLCall(glLinkProgram(clearProgram));
    GLCall(glValidateProgram(clearProgram));
    GLCall(glDeleteShader(clearShader));
    GLCall(glUseProgram(clearProgram));
    
    ShaderProgramSource quadSource = ParseShader("res/shaders/Basic.shader");
    unsigned int quadProgram = CreateShader(quadSource.VertexSource, quadSource.FragmentSource);
    GLCall(glUseProgram(quadProgram));

    GLCall(int agentTextureLocation = glGetUniformLocation(quadProgram, "u_AgentTexture"));
    ASSERT(agentTextureLocation != -1);
    GLCall(glUniform1i(agentTextureLocation, 0));

    GLCall(int mapTextureLocation = glGetUniformLocation(quadProgram, "u_MapTexture"));
    ASSERT(mapTextureLocation != -1);
    GLCall(glUniform1i(mapTextureLocation, 2));

    GLCall(int screenSizeLocation = glGetUniformLocation(quadProgram, "u_ScreenSize"));
    ASSERT(screenSizeLocation != -1);
    GLCall(glUniform2f(screenSizeLocation, static_cast<float>(screenWidth), static_cast<float>(screenHeight)));

    GLCall(int textureSizeLocation = glGetUniformLocation(quadProgram, "u_TextureSize"));
    ASSERT(textureSizeLocation != -1);
    GLCall(glUniform2f(textureSizeLocation, static_cast<float>(textureWidth), static_cast<float>(textureHeight)));


    while (!glfwWindowShouldClose(window))
    {
        currentTime = static_cast<float>(glfwGetTime());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        glfwGetWindowSize(window, &screenWidth, &screenHeight);
        GLCall(glViewport(0, 0, screenWidth, screenHeight));
        
        GLCall(glClear(GL_COLOR_BUFFER_BIT));

        for (int i = 0; i < 16; i++) {
            {
                GLCall(glUseProgram(fadeProgram));
                GLCall(glDispatchCompute(textureWidth / 16, textureHeight / 16, 1));
            }

            GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

            {
                GLCall(glUseProgram(clearProgram));
                GLCall(glDispatchCompute(textureWidth / 16, textureHeight / 16, 1));
            }

            GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

            {
                for (int round = 0; round < (AGENT_COUNT / 1024 / work_grp_cnt[0]) + 1; round++)
                {
                    GLCall(glUseProgram(computeProgram));
                    GLCall(glUniform1f(timeLocation, currentTime));
                    GLCall(glUniform1i(arrayOffsetLocation, round * work_grp_cnt[0]));
                    GLCall(glDispatchCompute(std::min(AGENT_COUNT / 1024 - work_grp_cnt[0] * round, work_grp_cnt[0]), 1, 1));
                }
            }
        }

        GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

        {
            GLCall(glUseProgram(quadProgram));
            GLCall(glUniform2f(screenSizeLocation, static_cast<float>(screenWidth), static_cast<float>(screenHeight)));
            GLCall(glBindVertexArray(vao));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            GLCall(glDrawArrays(GL_TRIANGLES, 0, 3));
        }

        glfwPollEvents();
        
        if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_ESCAPE))
        {
            glfwSetWindowShouldClose(window, 1);
        }

        /* Swap front and back buffers */
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}