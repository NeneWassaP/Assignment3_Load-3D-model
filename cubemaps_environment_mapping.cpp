// Full file — textured walls (copy & paste)
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

using namespace std;

// ---------- small helper to compile an OpenGL shader program from strings ----------
static GLuint compileShaderProgram(const char* vsSource, const char* fsSource) {
    auto compile = [&](GLenum type, const char* src)->GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char buf[1024]; glGetShaderInfoLog(s, 1024, NULL, buf);
            std::cerr << "Shader compile error: " << buf << std::endl;
        }
        return s;
        };
    GLuint vs = compile(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, 1024, NULL, buf);
        std::cerr << "Program link error: " << buf << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------- basic texture loader ----------
unsigned int loadTexture(const std::string& path)
{
    int width, height, nrComponents;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (nrComponents == 1) format = GL_RED;
    else if (nrComponents == 3) format = GL_RGB;
    else if (nrComponents == 4) format = GL_RGBA;

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // reasonable parameters for repeating tiled walls
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}

// ---------- cubemap loader (unchanged) ----------
unsigned int loadCubemap(vector<string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            // assume RGB images for skybox
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

// callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera (Camera class only used for projection values; we compute position/front ourselves)
Camera camera(glm::vec3(0.0f, 2.0f, 5.0f)); // initial
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// custom 3rd-person orbit controls
float camYaw = -90.0f;   // degrees, yaw around Y
float camPitch = 12.0f;  // degrees, slightly down
float camDistance = 3.0f; // closer camera
float mouseSensitivity = 0.12f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// object
glm::vec3 objectPos(-17.0f, 0.0f, -17.0f); // starting position (open spot)
float objectSpeed = 4.0f;
float objectRadius = 0.5f; // used for collision (sphere radius)

// simple cube for platform/obstacle (positions only)
float cubeVertices[] = {
    // positions (36 vertices)
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f
};

float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// Simple Box struct (AABB)
struct Box {
    glm::vec3 min;
    glm::vec3 max;
};

// ---------- multiple platforms & obstacles ----------
vector<Box> platforms;   // ground + elevated
vector<Box> obstacles;   // maze walls and blockers

// sphere vs AABB collision test
bool sphereIntersectsAABB(const glm::vec3& center, float radius, const Box& b) {
    float x = std::max(b.min.x, std::min(center.x, b.max.x));
    float y = std::max(b.min.y, std::min(center.y, b.max.y));
    float z = std::max(b.min.z, std::min(center.z, b.max.z));
    float distSq = (x - center.x) * (x - center.x) + (y - center.y) * (y - center.y) + (z - center.z) * (z - center.z);
    return distSq < (radius * radius);
}

bool collidesWithAnyObstacle(const glm::vec3& center, float radius) {
    for (auto& b : obstacles) {
        if (sphereIntersectsAABB(center, radius, b)) return true;
    }
    return false;
}

// find highest platform top under XZ
bool highestPlatformTopAtXZ(float x, float z, float& outTopY) {
    bool found = false;
    float best = -1e9f;
    for (auto& p : platforms) {
        if (x >= p.min.x && x <= p.max.x && z >= p.min.z && z <= p.max.z) {
            if (p.max.y > best) {
                best = p.max.y;
                found = true;
            }
        }
    }
    if (found) outTopY = best;
    return found;
}

// ------------------------- MAIN -------------------------
int main()
{
    // glfw init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3rd-Person Movement & Maze (textured walls)", NULL, NULL);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to init GLAD\n"; return -1; }
    glEnable(GL_DEPTH_TEST);

    // shaders
    Shader modelShader("6.2.cubemaps.vs", "6.2.cubemaps.fs"); // used for model & textured things
    Shader skyboxShader("6.2.skybox.vs", "6.2.skybox.fs");   // skybox

    // compile small wall shader (uses tiled texture via world XZ coords)
    const char* wallVs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform float uvScale;
        out vec2 TexCoord;
        void main() {
            vec4 world = model * vec4(aPos, 1.0);
            // tile using world XZ, uvScale controls tiling density
            TexCoord = fract(world.xz * uvScale);
            gl_Position = projection * view * world;
        }
    )";
    const char* wallFs = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D wallTex;
        uniform vec3 tint;
        void main() {
            vec3 tex = texture(wallTex, TexCoord).rgb;
            FragColor = vec4(tex * tint, 1.0);
        }
    )";
    GLuint wallProg = compileShaderProgram(wallVs, wallFs);
    GLint wall_uModel = glGetUniformLocation(wallProg, "model");
    GLint wall_uView = glGetUniformLocation(wallProg, "view");
    GLint wall_uProj = glGetUniformLocation(wallProg, "projection");
    GLint wall_uUVScale = glGetUniformLocation(wallProg, "uvScale");
    GLint wall_uTint = glGetUniformLocation(wallProg, "tint");
    GLint wall_uTex = glGetUniformLocation(wallProg, "wallTex");

    // model
    Model ourModel(FileSystem::getPath("resources/objects/winter-girl/Winter_Girl.obj"));

    // cube VAO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // load skybox textures
    vector<string> faces = {
        FileSystem::getPath("resources/textures/skybox/right.jpg"),
        FileSystem::getPath("resources/textures/skybox/left.jpg"),
        FileSystem::getPath("resources/textures/skybox/top.jpg"),
        FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox/front.jpg"),
        FileSystem::getPath("resources/textures/skybox/back.jpg")
    };
    unsigned int cubemapTexture = loadCubemap(faces);
    skyboxShader.use(); skyboxShader.setInt("skybox", 0);

    // load wall texture (place your wall.jpg at resources/textures/wall.jpg)
    unsigned int wallTexture = loadTexture(FileSystem::getPath("resources/textures/brickwall.jpg"));
    if (!wallTexture) {
        std::cerr << "Warning: wall texture failed to load. Walls will appear tinted.\n";
    }

    // ----------------- BUILD MAZE -----------------
    platforms.clear();
    platforms.push_back({ glm::vec3(-20.0f, -0.1f, -20.0f), glm::vec3(20.0f, 0.0f, 20.0f) });
    platforms.push_back({ glm::vec3(-12.0f, 0.6f, 6.0f), glm::vec3(-4.0f, 1.6f, 10.0f) });
    platforms.push_back({ glm::vec3(6.0f, 1.1f, -8.0f), glm::vec3(12.0f, 2.1f, -2.0f) });

    obstacles.clear();
    // boundary walls (leave small gap at entry -19.5..-18.0 to allow start area)
    obstacles.push_back({ glm::vec3(-19.5f, 0.0f, -19.5f), glm::vec3(-18.5f, 2.5f, 19.5f) });
    obstacles.push_back({ glm::vec3(18.5f, 0.0f, -19.5f), glm::vec3(19.5f, 2.5f, 19.5f) });
    obstacles.push_back({ glm::vec3(-19.5f, 0.0f, 18.5f), glm::vec3(19.5f, 2.5f, 19.5f) });
    // bottom wall shortened so starting area is open
    obstacles.push_back({ glm::vec3(-19.5f, 0.0f, -19.5f), glm::vec3(19.5f, 2.5f, -18.5f) });

    // internal walls — adjusted to avoid overlapping edges and to form corridors
    obstacles.push_back({ glm::vec3(-12.0f, 0.0f, -12.0f), glm::vec3(-11.0f, 2.2f, 6.0f) });
    obstacles.push_back({ glm::vec3(-6.0f, 0.0f, -6.0f), glm::vec3(6.0f, 2.0f, -5.0f) });
    obstacles.push_back({ glm::vec3(5.0f, 0.0f, -3.0f), glm::vec3(6.0f, 2.0f, 13.0f) });
    obstacles.push_back({ glm::vec3(-2.0f, 0.0f, 2.0f), glm::vec3(10.0f, 2.0f, 3.0f) });
    obstacles.push_back({ glm::vec3(-10.0f, 0.0f, 7.5f), glm::vec3(-0.5f, 2.2f, 8.5f) });
    obstacles.push_back({ glm::vec3(-4.0f, 0.0f, 4.0f), glm::vec3(-3.0f, 2.0f, 14.0f) });
    obstacles.push_back({ glm::vec3(2.0f, 0.0f, 10.0f), glm::vec3(4.0f, 1.6f, 12.0f) });
    obstacles.push_back({ glm::vec3(-8.0f, 0.0f, -3.0f), glm::vec3(-6.5f, 1.6f, -1.0f) });

    // ensure object starts in an open area
    objectPos = glm::vec3(-17.0f, 0.0f, -17.0f);

    // initial camera computed from camYaw/camPitch
    {
        float yawRad = glm::radians(camYaw);
        float pitchRad = glm::radians(camPitch);
        glm::vec3 forward = glm::normalize(glm::vec3(cos(yawRad), 0.0f, sin(yawRad)));
        float heightOffset = camDistance * sin(pitchRad);
        glm::vec3 camPos = objectPos - forward * camDistance + glm::vec3(0.0f, heightOffset, 0.0f);
        camera.Position = camPos;
        camera.Front = forward;
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // camera: compute behind-the-object position using yaw/pitch/distance
        // camera: compute behind-the-object position using yaw/pitch/distance
        float yawRad = glm::radians(camYaw);
        float pitchRad = glm::radians(camPitch);

        // compute a spherical offset (nice orbit) or behind-forward offset — either is fine.
        // Here we'll compute a behind offset but keep vertical from pitch:
        glm::vec3 forward = glm::normalize(glm::vec3(cos(yawRad), 0.0f, sin(yawRad)));
        float heightOffset = camDistance * sin(pitchRad);
        glm::vec3 camPos = objectPos - forward * camDistance + glm::vec3(0.0f, heightOffset, 0.0f);

        // IMPORTANT: always look at the model's center/eye (not camPos + forward)
        glm::vec3 targetOffset = glm::vec3(0.0f, 0.8f, 0.0f); // tweak 0.8f to match model eye-height
        glm::vec3 camTarget = objectPos + targetOffset;

        // update camera struct and compute view from lookAt so it stays focused on model
        camera.Position = camPos;
        camera.Front = glm::normalize(camTarget - camera.Position); // optional, but keep Camera consistent


        glClearColor(0.18f, 0.18f, 0.22f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Model shader (used for the model)
        modelShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camera.Position, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);

        // draw model at objectPos
        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::translate(modelMat, objectPos);
        modelMat = glm::rotate(modelMat, glm::radians(-camYaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMat = glm::scale(modelMat, glm::vec3(1.0f));
        modelShader.setMat4("model", modelMat);
        ourModel.Draw(modelShader);

        // draw platforms using a slightly tinted texture (reuse wall shader but with a different tint)
        glUseProgram(wallProg);
        glUniformMatrix4fv(wall_uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(wall_uProj, 1, GL_FALSE, glm::value_ptr(projection));
        // bind wall texture to unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, wallTexture);
        glUniform1i(wall_uTex, 0);

        // tile scale (how many texture repeats per world unit) - tweak to taste
        float uvScale = 0.25f; // lower = larger tiles, higher = more repeats
        glUniform1f(wall_uUVScale, uvScale);

        // draw platforms (tinted slightly darker)
        for (auto& p : platforms) {
            glm::mat4 pm = glm::mat4(1.0f);
            glm::vec3 size = p.max - p.min;
            glm::vec3 center = (p.min + p.max) * 0.5f;
            pm = glm::translate(pm, center);
            pm = glm::scale(pm, size);
            glUniformMatrix4fv(wall_uModel, 1, GL_FALSE, glm::value_ptr(pm));
            glUniform3f(wall_uTint, 0.9f, 0.9f, 0.9f); // near-white tint for floor
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // draw obstacles (walls) with stronger tint
        for (auto& o : obstacles) {
            glm::mat4 om = glm::mat4(1.0f);
            glm::vec3 size = o.max - o.min;
            glm::vec3 center = (o.min + o.max) * 0.5f;
            om = glm::translate(om, center);
            om = glm::scale(om, size);
            glUniformMatrix4fv(wall_uModel, 1, GL_FALSE, glm::value_ptr(om));
            glUniform3f(wall_uTint, 1.0f, 1.0f, 1.0f); // neutral tint (texture shows)
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyView = glm::mat4(glm::mat3(glm::lookAt(camera.Position, camera.Position + camera.Front, glm::vec3(0.0f, 1.0f, 0.0f))));
        skyboxShader.setMat4("view", skyView);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteProgram(wallProg);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);

    glfwTerminate();
    return 0;
}

// ---------------- Input & collision logic ----------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // horizontal forward/right from camYaw (movement follows camera heading)
    float yawRad = glm::radians(camYaw);
    glm::vec3 forward = glm::normalize(glm::vec3(cos(yawRad), 0.0f, sin(yawRad)));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    float velocity = objectSpeed * deltaTime;
    glm::vec3 desired = objectPos;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) desired += forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) desired -= forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) desired -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) desired += right * velocity;

    desired.y = objectPos.y;

    // collision handling with obstacles (slide)
    bool collide = collidesWithAnyObstacle(desired, objectRadius);
    if (!collide) {
        objectPos = desired;
    }
    else {
        glm::vec3 tryX = objectPos; tryX.x = desired.x;
        if (!collidesWithAnyObstacle(tryX, objectRadius)) objectPos.x = tryX.x;
        glm::vec3 tryZ = objectPos; tryZ.z = desired.z;
        if (!collidesWithAnyObstacle(tryZ, objectRadius)) objectPos.z = tryZ.z;
    }

    // snap Y to highest platform under player's X/Z
    float topY;
    if (highestPlatformTopAtXZ(objectPos.x, objectPos.z, topY)) {
        objectPos.y = topY;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// mouse updates the camYaw/camPitch (orbit around object)
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed to match typical FPS mouse

    lastX = xpos; lastY = ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    camYaw += xoffset;
    camPitch += yoffset;

    // clamp pitch to avoid flipping
    if (camPitch > 89.0f) camPitch = 89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camDistance -= static_cast<float>(yoffset) * 0.4f;
    camDistance = std::max(1.2f, std::min(10.0f, camDistance));
}
