#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
//#include <chrono>

#define ASSERT(x) if (!(x)) __debugbreak();
#define GLCall(x) GLClearError();\
    x;\
    ASSERT(GLLogCall(#x, __FILE__, __LINE__))
#define LOG_ERROR() logError(__FILE__, __FUNCTION__, __LINE__)

#define PI 3.14159265358979f

int AGENT_COUNT = 3000;

std::string MAP_PATH = "res/textures/testmap_simplemaze_1024x512.png";

// #define SAVE_DATA true
#define EXPORTED_CSVS_FOLDER "res/exported_csvs/"
std::string logFileName = "results.csv";
#define SAVE_DATA_EVERY_N_ROUNDS 240
#define END_SIMULATION_AFTER_N_ROUNDS 163200

#define FOLLOW_GREEN_PHEROMONE "false"
#define FOLLOW_RED_PHEROMONE "false"
#define AVOID_WALLS "true"

bool SAVE_DATA = false;
// If this variable is one the simulation will end when all the food has been collected (or the maximum number of rounds has been reached)
// If this variable is greater than one the simualtion will end according to the END_SIMULATION_AFTER_N_ROUNDS variable (since it is impossible for the ants to collect more food than is on the map)
const double END_SIMULATION_AFTER_FOODP_COLLECTED = 0.995;

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

void findReplaceAll(std::string& data, std::string search, std::string replaceString) {
    while (data.find(search) != std::string::npos) {
        data.replace(data.find(search), search.size(), replaceString);
    }
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
    int hasFood;
    int foodLeftAtHome;
    float timeAtSource;
    float timeAtWallCollision;
    int special;
};

std::vector<Agent> agents;

int main(int argc, char** argv)
{
    time_t randomSeed = std::time(NULL);
    std::cout << "Using random seed: " << randomSeed << std::endl;
    std::srand(randomSeed);

    if (argc > 1)
    {
        if (argc != 4)
        {
            std::cout << "Unexpected number of arguments. Expected 'map path' num_ants export_name";
            return 1;
        }

        SAVE_DATA = true;

        std::string map = argv[1];
        std::cout << map << std::endl;

        int agent_count = std::stoi(argv[2]);

        std::string export_name = argv[3];

        MAP_PATH = map;
        AGENT_COUNT = agent_count;

        logFileName = "result_" + std::to_string(randomSeed) + "_ac" + std::to_string(AGENT_COUNT) + "_" + export_name + ".csv";
        std::cout << logFileName << std::endl;
        std::cout << "Running map: " << MAP_PATH << std::endl << "    with " << AGENT_COUNT << " number of agents." << std::endl;
    }

    agents.reserve(AGENT_COUNT);

    float currentTime = 0.0f;
    float lastTime = 0.0f;
    float deltaTime;

    if (SAVE_DATA) {
        std::ofstream logFile;

        logFile.open(EXPORTED_CSVS_FOLDER + logFileName, std::fstream::app);
        std::time_t now = std::time(0);
        char* dt = std::ctime(&now);
        logFile << "Started at: " << dt << std::endl;
        logFile << "Using random seed: " << randomSeed << std::endl;
        logFile << "Map: " << MAP_PATH << std::endl;
        logFile << "FOLLOW_GREEN_PHEROMONE: " << FOLLOW_GREEN_PHEROMONE << std::endl;
        logFile << "FOLLOW_RED_PHEROMONE: " << FOLLOW_RED_PHEROMONE << std::endl;
        logFile << "AVOID_WALLS: " << AVOID_WALLS << std::endl;
        logFile << "AGENT_COUNT: " << AGENT_COUNT << std::endl;
        logFile << "END_SIMULATION_AFTER_FOODP_COLLECTED: " << END_SIMULATION_AFTER_FOODP_COLLECTED << std::endl;
        logFile << "time,total_gathered_food,gathered_food_since_last_entry,number_of_ants_carrying_food" << std::endl;
        logFile.close();    
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
    window = glfwCreateWindow(1600, 900, "Ants", NULL, NULL);
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
    const char* constMapPath = MAP_PATH.c_str();
    unsigned char* mapData = stbi_load(constMapPath, &mapWidth, &mapHeight, &mapNrChannels, STBI_rgb_alpha);

    if (!mapData) {
        std::cout << "Failed to load map texture." << std::endl;
        glfwTerminate();
        return 0;
    }

    int screenWidth = mapWidth;
    int screenHeight = mapHeight;

    int foodOnMap = 0;

    // Find home pixels
    std::vector<int> homePixels = {};
    for (int i = 0; i < mapWidth * mapHeight; i++) {
        if (mapData[i * 4 + 0] == 100 &&
            mapData[i * 4 + 1] == 100 &&
            mapData[i * 4 + 2] == 100 &&
            mapData[i * 4 + 3] == 255) {
            homePixels.push_back(i);
        }
        if (mapData[i * 4 + 0] == 255 &&
            mapData[i * 4 + 1] == 0 &&
            mapData[i * 4 + 2] == 0 &&
            mapData[i * 4 + 3] == 255) {
            foodOnMap++;
        }
    }

    // Initialize agents
    for (unsigned int i = 0; i < AGENT_COUNT; i++)
    {
        int startPositionPixelIndex = std::rand() % homePixels.size();
        float x = homePixels[startPositionPixelIndex] % mapWidth;
        float y = homePixels[startPositionPixelIndex] / mapWidth;

        float distance = std::sqrt(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * static_cast<float>(std::min(mapWidth, mapHeight) / 2);
        float angle = static_cast<float>(std::rand() % static_cast<int>(2.0f * PI * 1000.0f)) / 1000.0f;
        agents.push_back({ { x, y }, angle + 1.0f * PI / 3.0f, 0, 0, 0.0f, -1000.0f, 0 });
    }

    agents[0].special = 1;

    // Texture
    unsigned int tex_TrailMap;
    GLCall(glActiveTexture(GL_TEXTURE0));
    GLCall(glGenTextures(1, &tex_TrailMap));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_TrailMap));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, mapWidth, mapHeight, 0, GL_RGBA, GL_FLOAT, nullptr));

    GLCall(glBindImageTexture(0, tex_TrailMap, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

    unsigned int tex_Agents;
    GLCall(glActiveTexture(GL_TEXTURE1));
    GLCall(glGenTextures(1, &tex_Agents));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_Agents));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, mapWidth, mapHeight, 0, GL_RGBA, GL_FLOAT, nullptr));

    GLCall(glBindImageTexture(1, tex_Agents, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

    unsigned int tex_Map;
    GLCall(glActiveTexture(GL_TEXTURE2));
    GLCall(glGenTextures(1, &tex_Map));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex_Map));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mapWidth, mapHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, mapData));

    GLCall(glBindImageTexture(2, tex_Map, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI));

    // We use this memory later
    //stbi_image_free(mapData);


    // SSBO
    unsigned int ssbo;
    GLCall(glGenBuffers(1, &ssbo));
    GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
    GLCall(glBufferData(GL_SHADER_STORAGE_BUFFER, agents.size() * sizeof(Agent), static_cast<void*>(agents.data()), GL_DYNAMIC_DRAW));
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
    findReplaceAll(computeSource, "FOLLOW_GREEN_PHEROMONE", FOLLOW_GREEN_PHEROMONE);
    findReplaceAll(computeSource, "FOLLOW_RED_PHEROMONE", FOLLOW_RED_PHEROMONE);
    findReplaceAll(computeSource, "AVOID_WALLS", AVOID_WALLS);
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
    GLCall(glUniform2f(computeTextureSizeLocation, static_cast<float>(mapWidth), static_cast<float>(mapHeight)));

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

    GLCall(int trailTextureLocation = glGetUniformLocation(quadProgram, "u_TrailTexture"));
    ASSERT(trailTextureLocation != -1);
    GLCall(glUniform1i(trailTextureLocation, 0));

    GLCall(int agentTextureLocation = glGetUniformLocation(quadProgram, "u_AgentTexture"));
    ASSERT(agentTextureLocation != -1);
    GLCall(glUniform1i(agentTextureLocation, 1));

    GLCall(int mapTextureLocation = glGetUniformLocation(quadProgram, "u_MapTexture"));
    ASSERT(mapTextureLocation != -1);
    GLCall(glUniform1i(mapTextureLocation, 2));

    GLCall(int screenSizeLocation = glGetUniformLocation(quadProgram, "u_ScreenSize"));
    ASSERT(screenSizeLocation != -1);
    GLCall(glUniform2f(screenSizeLocation, static_cast<float>(screenWidth), static_cast<float>(screenHeight)));

    GLCall(int textureSizeLocation = glGetUniformLocation(quadProgram, "u_TextureSize"));
    ASSERT(textureSizeLocation != -1);
    GLCall(glUniform2f(textureSizeLocation, static_cast<float>(mapWidth), static_cast<float>(mapHeight)));

    int roundsCounter = 0;
    int roundsPerFrame = (SAVE_DATA ? 512 : 0);
    int gatheredFood = 0;

    bool spacePressedLastFrame = false;

    float time = 0.0;

    bool runningWindow = true;

    while (runningWindow)
    {
        if (glfwWindowShouldClose(window))
        {
            runningWindow = false;
            break;
        }
        currentTime = time; // static_cast<float>(glfwGetTime());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        glfwGetWindowSize(window, &screenWidth, &screenHeight);
        GLCall(glViewport(0, 0, screenWidth, screenHeight));

        GLCall(glClear(GL_COLOR_BUFFER_BIT));

        for (int i = 0; i < roundsPerFrame; i++) {
            time += 0.01f;
            currentTime = time;

            {
                GLCall(glUseProgram(fadeProgram));
                GLCall(glDispatchCompute(mapWidth / 16, mapHeight / 16, 1));
            }

            GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

            {
                GLCall(glUseProgram(clearProgram));
                GLCall(glDispatchCompute(mapWidth / 16, mapHeight / 16, 1));
            }

            GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

            {
                for (int round = 0; round < (AGENT_COUNT / 1 / work_grp_cnt[0]) + 1; round++)
                {
                    GLCall(glUseProgram(computeProgram));
                    GLCall(glUniform1f(timeLocation, currentTime));
                    GLCall(glUniform1i(arrayOffsetLocation, round * work_grp_cnt[0]));
                    GLCall(glDispatchCompute(std::min(AGENT_COUNT / 1 - work_grp_cnt[0] * round, work_grp_cnt[0]), 1, 1));
                }
            }

            GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

            roundsCounter++;

            if (SAVE_DATA && roundsCounter % SAVE_DATA_EVERY_N_ROUNDS == 0)
            {
                int gatheredFoodTheseRounds = 0;
                int numberOfAntsCarryingFood = 0;
                GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
                GLCall(glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, agents.size() * sizeof(Agent), static_cast<void*>(agents.data())));

                for (int i = 0; i < AGENT_COUNT; i++) {
                    while (agents[i].foodLeftAtHome > 0) {
                        agents[i].foodLeftAtHome--;
                        gatheredFood++;
                        gatheredFoodTheseRounds++;
                    }

                    if (agents[i].hasFood == 1) {
                        numberOfAntsCarryingFood++;
                    }
                }

                GLCall(glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, agents.size() * sizeof(Agent), static_cast<void*>(agents.data())));

                std::ofstream logFile;

                logFile.open(EXPORTED_CSVS_FOLDER + logFileName, std::fstream::app);
                logFile << time << " " << gatheredFood << " " << gatheredFoodTheseRounds << " " << numberOfAntsCarryingFood << std::endl;
                logFile.close();
            }

            if ((roundsCounter >= END_SIMULATION_AFTER_N_ROUNDS && END_SIMULATION_AFTER_N_ROUNDS != -1 && SAVE_DATA) || (gatheredFood / foodOnMap >= END_SIMULATION_AFTER_FOODP_COLLECTED && SAVE_DATA)) 
            {
                runningWindow = false;
            }
        }

        glfwPollEvents();

        {
            /*
            GLCall(glBindTexture(GL_TEXTURE_2D, tex_Map));
            GLCall(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, mapData));

            double mouseXPos;
            double mouseYPos;

            glfwGetCursorPos(window, &mouseXPos, &mouseYPos);


            float textureAspect = (float)mapWidth / (float)mapHeight;
            float screenAspect = (float)screenWidth / (float)screenHeight;

            if (textureAspect > screenAspect)
            {
                mouseXPos = (mouseXPos / screenWidth) * mapWidth;
                mouseYPos = (1.0 - ((((2.0 * mouseYPos / screenHeight) - 1.0) * textureAspect / screenAspect) + 1.0) / 2.0) * mapHeight;
            }
            else
            {
                mouseXPos = ((((2.0 * mouseXPos / screenWidth) - 1.0) * screenAspect / textureAspect) + 1.0) / 2.0 * mapWidth;
                mouseYPos = (1.0 - (mouseYPos / screenHeight)) * mapHeight;
            }

            if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT))
            {
                if (mouseXPos >= 0 && mouseXPos < mapWidth && mouseYPos >= 0 && mouseYPos < mapHeight) {
                    for (int xOffset = -2; xOffset <= 2; xOffset++)
                    {
                        for (int yOffset = -2; yOffset <= 2; yOffset++)
                        {
                            int ind = 4 * ((int)(mouseYPos + yOffset) * mapWidth + (int)(mouseXPos + xOffset));
                            mapData[ind + 0] = 128;
                            mapData[ind + 1] = 128;
                            mapData[ind + 2] = 128;
                            mapData[ind + 3] = 255;
                        }
                    }
                }
            }

            GLCall(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mapWidth, mapHeight, GL_RGBA, GL_UNSIGNED_BYTE, mapData));
            */
        }

        if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_ESCAPE))
        {
            glfwSetWindowShouldClose(window, 1);
        }

        if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_0))
        {
            roundsPerFrame = 0;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_1))
        {
            roundsPerFrame = 1;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_2))
        {
            roundsPerFrame = 2;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_3))
        {
            roundsPerFrame = 4;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_4))
        {
            roundsPerFrame = 6;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_5))
        {
            roundsPerFrame = 8;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_6))
	    {
            roundsPerFrame = 10;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_7))
        {
            roundsPerFrame = 12;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_8))
        {
            roundsPerFrame = 16;
        }
        else if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_9))
        {
            roundsPerFrame = 512;
        }

        if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_SPACE) && !spacePressedLastFrame)
        {
            spacePressedLastFrame = true;

            GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
            GLCall(glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Agent) * agents.size(), static_cast<void*>(agents.data())));

            for (int i = 0; i < AGENT_COUNT; i++) 
            {
                agents[i].special = 0;
            }

            agents[std::rand() % AGENT_COUNT].special = 1;

            GLCall(glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Agent) * agents.size(), static_cast<void*>(agents.data())));

            //std::cout << gatheredFood << "\n
        }
        else
        {
            spacePressedLastFrame = false;
        }

        // Render to the screen
        {
            GLCall(glUseProgram(quadProgram));
            GLCall(glUniform2f(screenSizeLocation, static_cast<float>(screenWidth), static_cast<float>(screenHeight)));
            GLCall(glBindVertexArray(vao));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            GLCall(glDrawArrays(GL_TRIANGLES, 0, 3));
        }


        /* Swap front and back buffers */
        glfwSwapBuffers(window);
    }

    if (SAVE_DATA) {
        std::ofstream logFile;
        logFile.open(EXPORTED_CSVS_FOLDER + logFileName, std::fstream::app);
        logFile << std::endl << std::endl;
        logFile.close();
    }

    glfwTerminate();
    return 0;
}