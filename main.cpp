#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "pointLight.h"
#include "sphere.h"

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// set window size
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// set starting camera position and frame timers
Camera camera(glm::vec3(0.0f, 1.45f, 5.2f), glm::vec3(0.0f, 1.0f, 0.0f), 90.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// angles for spinning the whole arcade room
float sceneRotationX = 0.0f;
float sceneRotationY = 0.0f;
float sceneRotationZ = 0.0f;

// switches to turn lights on and off
bool light1_on = true; // Front Showcase Light
bool light2_on = true; // Interior Canopy Light
bool light3_on = true; // Moving Claw Spotlight

// claw starting spot and grab angle
glm::vec3 clawPos(0.0f, 2.3f, 0.0f);
float clawOpenAngle = 26.0f;
bool isClawClosed = false;

// steps for moving and dropping a ball automatically
enum AutoState { IDLE, LIFTING, MOVING_TO_CHUTE, DROPPING, RETURNING };
AutoState autoState = IDLE;
const glm::vec3 chuteTargetPos(-0.50f, 2.3f, 0.50f);

// opening angle for the white drop door
float chuteDoorAngle = 0.0f;

// gravity pull speed for falling balls
const float GRAVITY = 2.6f;

// player score tracking numbers
int score = 0;
int lastDisplayedScore = -1;

// boundary box limits for the prize drop hole
const float CHUTE_MIN_X = -0.73f;
const float CHUTE_MAX_X = -0.27f;
const float CHUTE_MIN_Z = 0.27f;
const float CHUTE_MAX_Z = 0.72f;

// checks if any object is above the drop hole
bool isOverChute(const glm::vec3& pos)
{
    return pos.x > CHUTE_MIN_X && pos.x < CHUTE_MAX_X &&
        pos.z > CHUTE_MIN_Z && pos.z < CHUTE_MAX_Z;
}

// all information stored for one prize ball
struct Prize {
    glm::vec3 pos;
    glm::vec3 originalPos;
    glm::vec3 color;
    float radius;
    bool isGrabbed;
    bool inChute;
    bool falling;   // true while under gravity after release
    float vy;       // fall velocity
};

// list of all prize balls and the chosen ball index
std::vector<Prize> prizes;
int grabbedPrizeIndex = -1;

// storage IDs for box shapes on the graphics card
unsigned int cubeVAO = 0, cubeVBO = 0;

// list of helper functions used in this file
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

void updateGameplayAutomation(float dt);
void initCube();
void drawCube(Shader& shader, glm::mat4 model, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec, float shiny = 32.0f, bool isChecker = false, glm::vec3 checkerA = glm::vec3(0.0f), glm::vec3 checkerB = glm::vec3(0.0f), float tileSize = 0.42f);
void drawShadowBlob(Shader& shader, glm::mat4 sceneRoot, glm::vec3 worldPos, float scale);
void drawStaticBackgroundClawMachine(Shader& shader, Sphere& sphereMesh, glm::mat4 parentMatrix, glm::vec3 bodyColor, glm::vec3 marqueeColor, int prizeTheme);

int main()
{
    // start window tools and pick modern drawing features
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // open the main game window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Arcade Claw Machine Hall", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // keep the mouse pointer normal and do not let it spin the view
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // load drawing commands for the computer graphics card
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // set the drawing area size to match the screen window
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // aim the camera directly at the center machine
    camera.Yaw = 90.0f;
    camera.Pitch = 0.0f;
    camera.ProcessMouseMovement(0.0f, 0.0f);

    // make sure near objects hide far objects correctly
    glEnable(GL_DEPTH_TEST);

    // load light shaders and build the box shape data
    Shader phongShader("vertexShaderForPhongShading.vs", "fragmentShaderForPhongShading.fs");
    initCube();

    // create one master ball model for drawing
    Sphere sphereMesh(0.11f, 24, 16);

    // list of bright colors for the prize balls
    std::vector<glm::vec3> palette = {
        glm::vec3(0.95f, 0.15f, 0.20f),
        glm::vec3(0.10f, 0.65f, 0.95f),
        glm::vec3(0.98f, 0.75f, 0.10f),
        glm::vec3(0.70f, 0.20f, 0.90f),
        glm::vec3(0.15f, 0.85f, 0.35f),
        glm::vec3(0.98f, 0.40f, 0.10f),
        glm::vec3(0.95f, 0.50f, 0.70f)
    };

    // fill the prize floor with balls and leave the drop hole empty
    int pIdx = 0;
    for (float x = -0.62f; x <= 0.62f; x += 0.21f)
    {
        for (float z = -0.62f; z <= 0.62f; z += 0.21f)
        {
            // Skip the chute opening using the SAME bounds check used everywhere else
            if (isOverChute(glm::vec3(x, 0.90f, z))) continue;

            Prize p;
            p.pos = glm::vec3(x, 0.90f, z);
            p.originalPos = p.pos;
            p.color = palette[(pIdx++) % palette.size()];
            p.radius = 0.11f;
            p.isGrabbed = false;
            p.inChute = false;
            p.falling = false;
            p.vy = 0.0f;
            prizes.push_back(p);
        }
    }

    // set up the big front light bulb
    PointLight frontShowcaseLight(0.0f, 2.1f, 2.2f,
        0.35f, 0.35f, 0.40f,
        1.25f, 1.25f, 1.30f,
        1.0f, 1.0f, 1.0f,
        1.0f, 0.045f, 0.0075f, 1);

    // set up the inside roof light bulb
    PointLight interiorCanopyLight(0.0f, 2.65f, 0.30f,
        0.25f, 0.25f, 0.30f,
        0.90f, 0.90f, 0.95f,
        0.8f, 0.8f, 0.8f,
        1.0f, 0.07f, 0.017f, 2);

    // main game loop running every frame
    while (!glfwWindowShouldClose(window))
    {
        // count elapsed time between frames
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // read keys and run automated movement
        processInput(window);
        updateGameplayAutomation(deltaTime);

        // find out if any dropped ball is falling inside the hole
        bool prizeFallingIntoChute = false;
        for (const auto& p : prizes)
        {
            if (p.falling && isOverChute(p.pos))
            {
                prizeFallingIntoChute = true;
                break;
            }
        }

        // open the trapdoor when dropping balls or running the drop routine
        bool shouldChuteBeOpen = (autoState == LIFTING || autoState == MOVING_TO_CHUTE || autoState == DROPPING || prizeFallingIntoChute);

        // swing door open or closed smoothly
        if (shouldChuteBeOpen)
        {
            if (chuteDoorAngle < 85.0f)
            {
                chuteDoorAngle += 200.0f * deltaTime;
                if (chuteDoorAngle > 85.0f) chuteDoorAngle = 85.0f; // clamp on the way up too
            }
        }
        else
        {
            if (chuteDoorAngle > 0.0f)
                chuteDoorAngle -= 200.0f * deltaTime;
            if (chuteDoorAngle < 0.0f)
                chuteDoorAngle = 0.0f;
        }

        // wipe the screen clean before drawing new frames
        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // send camera position into the light shader
        phongShader.use();
        phongShader.setVec3("viewPos", camera.Position);

        // combine room rotations into one transform
        glm::mat4 sceneRoot = glm::mat4(1.0f);
        sceneRoot = glm::rotate(sceneRoot, glm::radians(sceneRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        sceneRoot = glm::rotate(sceneRoot, glm::radians(sceneRotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        sceneRoot = glm::rotate(sceneRoot, glm::radians(sceneRotationZ), glm::vec3(0.0f, 0.0f, 1.0f));

        // shift light colors as the whole room rotates
        float angleSum = glm::radians(sceneRotationY * 0.8f + sceneRotationX * 1.2f + sceneRotationZ * 1.5f);
        float rTint = 0.5f + 0.5f * sin(angleSum);
        float gTint = 0.5f + 0.5f * sin(angleSum + 2.094f);
        float bTint = 0.5f + 0.5f * sin(angleSum + 4.188f);
        glm::vec3 dynamicLightColor = glm::normalize(glm::vec3(rTint + 0.3f, gTint + 0.3f, bTint + 0.3f));

        // rotate the front light position with the whole room
        glm::vec3 worldFrontLightPos = glm::vec3(sceneRoot * glm::vec4(frontShowcaseLight.position, 1.0f));
        PointLight rotFront = frontShowcaseLight;
        rotFront.position = worldFrontLightPos;
        rotFront.diffuse = light1_on ? (dynamicLightColor * 1.35f) : glm::vec3(0.0f);
        rotFront.specular = light1_on ? dynamicLightColor : glm::vec3(0.0f);
        rotFront.setUpPointLight(phongShader);

        // rotate the inside roof light position with the room
        glm::vec3 worldInteriorLightPos = glm::vec3(sceneRoot * glm::vec4(interiorCanopyLight.position, 1.0f));
        PointLight rotInterior = interiorCanopyLight;
        rotInterior.position = worldInteriorLightPos;
        rotInterior.diffuse = light2_on ? (glm::vec3(0.95f, 0.95f, 1.0f) * (0.85f + 0.15f * sin(angleSum))) : glm::vec3(0.0f);
        rotInterior.setUpPointLight(phongShader);

        // attach the flashlight beam directly onto the moving claw
        glm::vec3 worldClawPos = glm::vec3(sceneRoot * glm::vec4(clawPos.x, clawPos.y + 0.1f, clawPos.z, 1.0f));
        glm::vec3 worldClawDir = glm::normalize(glm::vec3(sceneRoot * glm::vec4(0.0f, -1.0f, 0.10f, 0.0f)));

        phongShader.setVec3("spotLight.position", worldClawPos);
        phongShader.setVec3("spotLight.direction", worldClawDir);
        phongShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(24.0f)));
        phongShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(32.0f)));
        phongShader.setVec3("spotLight.ambient", light3_on ? glm::vec3(0.06f) : glm::vec3(0.0f));
        phongShader.setVec3("spotLight.diffuse", light3_on ? glm::vec3(1.2f, 1.15f, 0.95f) : glm::vec3(0.0f));
        phongShader.setVec3("spotLight.specular", light3_on ? glm::vec3(1.0f) : glm::vec3(0.0f));
        phongShader.setFloat("spotLight.k_c", 1.0f);
        phongShader.setFloat("spotLight.k_l", 0.09f);
        phongShader.setFloat("spotLight.k_q", 0.032f);

        // send view lens and camera zoom matrices to the graphics card
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        phongShader.setMat4("projection", projection);
        phongShader.setMat4("view", view);

        // draw the giant checkered tile floor underneath all machines
        glm::mat4 floorModel = sceneRoot;
        floorModel = glm::translate(floorModel, glm::vec3(0.0f, -0.075f, -5.5f));
        floorModel = glm::scale(floorModel, glm::vec3(14.5f, 0.05f, 21.0f));
        drawCube(phongShader, floorModel,
            glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.6f, 0.3f, 0.7f), 64.0f,
            true, glm::vec3(0.18f, 0.05f, 0.32f), glm::vec3(0.85f, 0.08f, 0.58f), 0.42f);

        // draw side arcade claw machines on both sides of the hall
        float zPositions[5] = { 1.6f, -1.6f, -4.8f, -8.0f, -11.2f };

        glm::vec3 leftThemes[5] = {
            glm::vec3(0.85f, 0.10f, 0.45f),
            glm::vec3(0.08f, 0.70f, 0.55f),
            glm::vec3(0.65f, 0.15f, 0.85f),
            glm::vec3(0.85f, 0.40f, 0.05f),
            glm::vec3(0.10f, 0.55f, 0.90f)
        };
        glm::vec3 leftMarquees[5] = {
            glm::vec3(1.0f, 0.2f, 0.7f),
            glm::vec3(0.2f, 1.0f, 0.8f),
            glm::vec3(0.9f, 0.3f, 1.0f),
            glm::vec3(1.0f, 0.6f, 0.1f),
            glm::vec3(0.3f, 0.8f, 1.0f)
        };

        glm::vec3 rightThemes[5] = {
            glm::vec3(0.12f, 0.65f, 0.25f),
            glm::vec3(0.85f, 0.20f, 0.15f),
            glm::vec3(0.15f, 0.50f, 0.85f),
            glm::vec3(0.75f, 0.10f, 0.60f),
            glm::vec3(0.80f, 0.60f, 0.10f)
        };
        glm::vec3 rightMarquees[5] = {
            glm::vec3(0.3f, 1.0f, 0.4f),
            glm::vec3(1.0f, 0.3f, 0.2f),
            glm::vec3(0.4f, 0.7f, 1.0f),
            glm::vec3(1.0f, 0.3f, 0.9f),
            glm::vec3(1.0f, 0.8f, 0.2f)
        };

        for (int i = 0; i < 5; ++i)
        {
            glm::mat4 leftM = glm::translate(sceneRoot, glm::vec3(-3.6f, 0.0f, zPositions[i]));
            drawStaticBackgroundClawMachine(phongShader, sphereMesh, leftM, leftThemes[i], leftMarquees[i], i);

            glm::mat4 rightM = glm::translate(sceneRoot, glm::vec3(3.6f, 0.0f, zPositions[i]));
            drawStaticBackgroundClawMachine(phongShader, sphereMesh, rightM, rightThemes[i], rightMarquees[i], (i + 2) % 5);
        }

        // pick material colors for the main player claw machine
        glm::vec3 primaryBlue(0.04f, 0.38f, 0.88f);
        glm::vec3 midIceBlue(0.20f, 0.50f, 0.82f);
        glm::vec3 darkCanopyBlue(0.03f, 0.28f, 0.68f);
        glm::vec3 metalSilver(0.85f, 0.86f, 0.90f);
        glm::vec3 darkChassis(0.12f, 0.14f, 0.18f);

        // draw the bottom cabinet body
        glm::mat4 baseM = sceneRoot;
        baseM = glm::translate(baseM, glm::vec3(0.0f, 0.38f, 0.0f));
        baseM = glm::scale(baseM, glm::vec3(1.9f, 0.75f, 1.9f));
        drawCube(phongShader, baseM, primaryBlue * 0.35f, primaryBlue, metalSilver, 64.0f);

        // draw the thin silver trim strip
        glm::mat4 trimM = sceneRoot;
        trimM = glm::translate(trimM, glm::vec3(0.0f, 0.76f, 0.0f));
        trimM = glm::scale(trimM, glm::vec3(1.94f, 0.05f, 1.94f));
        drawCube(phongShader, trimM, metalSilver * 0.4f, metalSilver, glm::vec3(0.9f), 128.0f);

        // set grey colors for the interior floor segments
        glm::vec3 bedAmb(0.15f);
        glm::vec3 bedDiff(0.25f);
        glm::vec3 bedSpec(0.4f);

        // draw the large right floor panel
        glm::mat4 floorRightM = sceneRoot;
        floorRightM = glm::translate(floorRightM, glm::vec3(0.345f, 0.78f, 0.0f));
        floorRightM = glm::scale(floorRightM, glm::vec3(1.21f, 0.02f, 1.90f));
        drawCube(phongShader, floorRightM, bedAmb, bedDiff, bedSpec);

        // draw the floor section behind the drop hole
        glm::mat4 floorBackLeftM = sceneRoot;
        floorBackLeftM = glm::translate(floorBackLeftM, glm::vec3(-0.605f, 0.78f, -0.34f));
        floorBackLeftM = glm::scale(floorBackLeftM, glm::vec3(0.69f, 0.02f, 1.22f));
        drawCube(phongShader, floorBackLeftM, bedAmb, bedDiff, bedSpec);

        // draw the floor border on the left wall side
        glm::mat4 floorLeftBorderM = sceneRoot;
        floorLeftBorderM = glm::translate(floorLeftBorderM, glm::vec3(-0.84f, 0.78f, 0.495f));
        floorLeftBorderM = glm::scale(floorLeftBorderM, glm::vec3(0.22f, 0.02f, 0.45f));
        drawCube(phongShader, floorLeftBorderM, bedAmb, bedDiff, bedSpec);

        // draw the floor border near the front glass
        glm::mat4 floorFrontBorderM = sceneRoot;
        floorFrontBorderM = glm::translate(floorFrontBorderM, glm::vec3(-0.605f, 0.78f, 0.835f));
        floorFrontBorderM = glm::scale(floorFrontBorderM, glm::vec3(0.69f, 0.02f, 0.23f));
        drawCube(phongShader, floorFrontBorderM, bedAmb, bedDiff, bedSpec);

        // draw the black pit box inside the hole
        glm::mat4 chutePitM = sceneRoot;
        chutePitM = glm::translate(chutePitM, glm::vec3(-0.50f, 0.786f, 0.495f));
        chutePitM = glm::scale(chutePitM, glm::vec3(0.46f, 0.01f, 0.45f));
        drawCube(phongShader, chutePitM, glm::vec3(0.0f), glm::vec3(0.005f), glm::vec3(0.0f), 1.0f);

        // draw the swinging white trapdoor hinged on the left
        glm::mat4 dropDoorM = sceneRoot;
        dropDoorM = glm::translate(dropDoorM, glm::vec3(-0.73f, 0.798f, 0.495f)); // Left hinge line
        dropDoorM = glm::rotate(dropDoorM, glm::radians(-chuteDoorAngle), glm::vec3(0.0f, 0.0f, 1.0f)); // Swings downwards to left
        dropDoorM = glm::translate(dropDoorM, glm::vec3(0.23f, 0.0f, 0.0f)); // Centers the flap relative to the hinge
        dropDoorM = glm::scale(dropDoorM, glm::vec3(0.46f, 0.015f, 0.45f));
        drawCube(phongShader, dropDoorM, glm::vec3(0.35f), glm::vec3(0.95f, 0.95f, 0.98f), glm::vec3(0.9f), 64.0f);

        // draw the front control table
        glm::mat4 consoleM = sceneRoot;
        consoleM = glm::translate(consoleM, glm::vec3(0.0f, 0.58f, 1.12f));
        consoleM = glm::scale(consoleM, glm::vec3(1.35f, 0.22f, 0.36f));
        drawCube(phongShader, consoleM, midIceBlue * 0.35f, midIceBlue, metalSilver, 80.0f);

        // draw the thin joystick handle stick
        glm::mat4 stickM = sceneRoot;
        stickM = glm::translate(stickM, glm::vec3(-0.35f, 0.76f, 1.12f));
        stickM = glm::scale(stickM, glm::vec3(0.025f, 0.18f, 0.025f));
        drawCube(phongShader, stickM, glm::vec3(0.2f), glm::vec3(0.3f), glm::vec3(0.9f), 128.0f);

        // draw the red ball knob on top of the joystick
        {
            glm::vec3 savedAmb = sphereMesh.ambient, savedDiff = sphereMesh.diffuse, savedSpec = sphereMesh.specular;
            float savedShiny = sphereMesh.shininess;

            glm::mat4 stickBallM = sceneRoot;
            stickBallM = glm::translate(stickBallM, glm::vec3(-0.35f, 0.86f, 1.12f));
            sphereMesh.ambient = glm::vec3(0.4f, 0.05f, 0.05f);
            sphereMesh.diffuse = glm::vec3(0.95f, 0.15f, 0.15f);
            sphereMesh.drawSphere(phongShader, stickBallM);

            sphereMesh.ambient = savedAmb;
            sphereMesh.diffuse = savedDiff;
            sphereMesh.specular = savedSpec;
            sphereMesh.shininess = savedShiny;
        }

        // draw the red round push button
        glm::mat4 btnM = sceneRoot;
        btnM = glm::translate(btnM, glm::vec3(0.25f, 0.71f, 1.12f));
        btnM = glm::scale(btnM, glm::vec3(0.14f, 0.04f, 0.14f));
        drawCube(phongShader, btnM, glm::vec3(0.4f, 0.0f, 0.0f), glm::vec3(1.0f, 0.15f, 0.2f), glm::vec3(0.8f), 64.0f);

        // draw the lower prize pick up door
        glm::mat4 doorM = sceneRoot;
        doorM = glm::translate(doorM, glm::vec3(-0.45f, 0.32f, 0.96f));
        doorM = glm::scale(doorM, glm::vec3(0.45f, 0.38f, 0.02f));
        drawCube(phongShader, doorM, glm::vec3(0.05f), glm::vec3(0.08f), glm::vec3(0.2f), 8.0f);

        // draw the four tall corner support poles
        float pillars[4][2] = { {-0.90f, -0.90f}, {0.90f, -0.90f}, {-0.90f, 0.90f}, {0.90f, 0.90f} };
        for (int i = 0; i < 4; ++i)
        {
            glm::mat4 pil = sceneRoot;
            pil = glm::translate(pil, glm::vec3(pillars[i][0], 1.70f, pillars[i][1]));
            pil = glm::scale(pil, glm::vec3(0.08f, 1.85f, 0.08f));
            drawCube(phongShader, pil, darkChassis * 0.4f, darkChassis, metalSilver, 96.0f);
        }

        // draw the top roof canopy block
        glm::mat4 topM = sceneRoot;
        topM = glm::translate(topM, glm::vec3(0.0f, 2.75f, 0.0f));
        topM = glm::scale(topM, glm::vec3(1.92f, 0.30f, 1.92f));
        drawCube(phongShader, topM, darkCanopyBlue * 0.4f, darkCanopyBlue, metalSilver, 64.0f);

        // draw the glowing roof edge border
        glm::mat4 trimTop = sceneRoot;
        trimTop = glm::translate(trimTop, glm::vec3(0.0f, 2.91f, 0.0f));
        trimTop = glm::scale(trimTop, glm::vec3(1.95f, 0.04f, 1.95f));
        drawCube(phongShader, trimTop, dynamicLightColor * 0.35f, dynamicLightColor, glm::vec3(1.0f), 128.0f);

        // draw overhead track bars for the moving claw
        glm::mat4 railX = sceneRoot;
        railX = glm::translate(railX, glm::vec3(0.0f, 2.55f, clawPos.z));
        railX = glm::scale(railX, glm::vec3(1.72f, 0.035f, 0.06f));
        drawCube(phongShader, railX, metalSilver * 0.4f, metalSilver, glm::vec3(0.8f));

        // draw moving trolley box on the tracks
        glm::mat4 trolleyM = sceneRoot;
        trolleyM = glm::translate(trolleyM, glm::vec3(clawPos.x, 2.55f, clawPos.z));
        trolleyM = glm::scale(trolleyM, glm::vec3(0.20f, 0.08f, 0.20f));
        drawCube(phongShader, trolleyM, darkChassis, glm::vec3(0.25f), glm::vec3(0.8f));

        // draw rope wire line holding the claw
        float cableLength = (2.55f - (clawPos.y + 0.12f));
        glm::mat4 cableM = sceneRoot;
        cableM = glm::translate(cableM, glm::vec3(clawPos.x, clawPos.y + 0.12f + cableLength / 2.0f, clawPos.z));
        cableM = glm::scale(cableM, glm::vec3(0.015f, cableLength, 0.015f));
        drawCube(phongShader, cableM, glm::vec3(0.3f), metalSilver, glm::vec3(0.9f));

        // draw center metal hub above the fingers
        glm::mat4 hubM = sceneRoot;
        hubM = glm::translate(hubM, glm::vec3(clawPos.x, clawPos.y + 0.08f, clawPos.z));
        hubM = glm::scale(hubM, glm::vec3(0.12f, 0.08f, 0.12f));
        drawCube(phongShader, hubM, metalSilver * 0.4f, metalSilver, glm::vec3(1.0f), 128.0f);

        // draw three claw fingers with two moving parts each
        float prongAngles[3] = { 0.0f, 120.0f, 240.0f };
        for (int i = 0; i < 3; ++i)
        {
            glm::mat4 armM = sceneRoot;
            armM = glm::translate(armM, glm::vec3(clawPos.x, clawPos.y + 0.06f, clawPos.z));
            armM = glm::rotate(armM, glm::radians(prongAngles[i]), glm::vec3(0.0f, 1.0f, 0.0f));
            armM = glm::rotate(armM, glm::radians(clawOpenAngle), glm::vec3(1.0f, 0.0f, 0.0f));

            glm::mat4 upperM = glm::translate(armM, glm::vec3(0.0f, -0.10f, 0.05f));
            upperM = glm::scale(upperM, glm::vec3(0.025f, 0.16f, 0.025f));
            drawCube(phongShader, upperM, metalSilver * 0.4f, metalSilver, glm::vec3(0.9f), 64.0f);

            glm::mat4 lowerM = glm::translate(armM, glm::vec3(0.0f, -0.20f, 0.08f));
            lowerM = glm::rotate(lowerM, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            lowerM = glm::scale(lowerM, glm::vec3(0.022f, 0.11f, 0.022f));
            drawCube(phongShader, lowerM, metalSilver * 0.4f, glm::vec3(0.95f), glm::vec3(1.0f), 128.0f);
        }

        // draw claw floor shadow only when not above the empty hole
        bool clawOverChute = isOverChute(clawPos);
        if (!clawOverChute)
            drawShadowBlob(phongShader, sceneRoot, glm::vec3(clawPos.x, 0.0f, clawPos.z), 0.14f);

        // pick up a ball only if the claw is lowered down to the floor
        glm::vec3 clawCradle = clawPos - glm::vec3(0.0f, 0.12f, 0.0f);

        // Only allow picking up a prize when the claw has actually been lowered near the prize bed!
        if (isClawClosed && grabbedPrizeIndex == -1 && clawPos.y < 1.35f)
        {
            float bestDist = 0.16f; // Strict pickup radius
            int bestIdx = -1;
            for (size_t i = 0; i < prizes.size(); ++i)
            {
                // Ignore balls that are falling, already in the chute, or already grabbed
                if (prizes[i].inChute || prizes[i].falling || prizes[i].isGrabbed) continue;

                // Prize must actually be resting on the prize bed floor
                if (std::abs(prizes[i].pos.y - prizes[i].originalPos.y) > 0.08f) continue;

                float d = glm::distance(clawCradle, prizes[i].pos);
                if (d < bestDist)
                {
                    bestDist = d;
                    bestIdx = (int)i;
                }
            }
            if (bestIdx != -1)
            {
                prizes[bestIdx].isGrabbed = true;
                grabbedPrizeIndex = bestIdx;
            }
        }

        // move falling balls and draw all prize spheres
        for (size_t i = 0; i < prizes.size(); ++i)
        {
            if (prizes[i].isGrabbed)
            {
                prizes[i].pos = clawCradle;

                if (!isClawClosed)
                {
                    prizes[i].isGrabbed = false;
                    grabbedPrizeIndex = -1;
                    prizes[i].falling = true;
                    prizes[i].vy = 0.0f;
                }
            }
            else if (prizes[i].falling)
            {
                prizes[i].vy -= GRAVITY * deltaTime;
                prizes[i].pos.y += prizes[i].vy * deltaTime;

                bool overChute = isOverChute(prizes[i].pos);
                float landingY = overChute ? 0.35f : prizes[i].originalPos.y;

                if (prizes[i].pos.y <= landingY)
                {
                    prizes[i].pos.y = landingY;
                    prizes[i].vy = 0.0f;
                    prizes[i].falling = false;
                    if (overChute) prizes[i].inChute = true;
                }
            }

            if (!prizes[i].inChute)
            {
                bool prizeOverChute = isOverChute(prizes[i].pos);
                if (!prizeOverChute)
                    drawShadowBlob(phongShader, sceneRoot, glm::vec3(prizes[i].pos.x, 0.0f, prizes[i].pos.z), prizes[i].radius * 1.1f);
            }

            glm::mat4 prizeModel = sceneRoot;
            prizeModel = glm::translate(prizeModel, prizes[i].pos);
            sphereMesh.ambient = prizes[i].color * 0.35f;
            sphereMesh.diffuse = prizes[i].color;
            sphereMesh.specular = glm::vec3(0.8f);
            sphereMesh.shininess = 64.0f;
            sphereMesh.drawSphere(phongShader, prizeModel);
        }

        // count collected balls and update window score title
        score = 0;
        for (auto& p : prizes) if (p.inChute) score++;
        if (score != lastDisplayedScore)
        {
            lastDisplayedScore = score;
            std::string title = "Arcade Claw Machine Hall - Score: " + std::to_string(score);
            glfwSetWindowTitle(window, title.c_str());
        }

        // swap screen buffers and read new input events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // clean up mesh memory and close program
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glfwTerminate();
    return 0;
}

// draws one decorative side machine using scaled sizes
void drawStaticBackgroundClawMachine(Shader& shader, Sphere& sphereMesh, glm::mat4 parentMatrix, glm::vec3 bodyColor, glm::vec3 marqueeColor, int prizeTheme)
{
    glm::vec3 darkMetal(0.12f, 0.14f, 0.18f);
    glm::vec3 silver(0.75f, 0.78f, 0.82f);
    glm::vec3 metalSilver(0.85f, 0.86f, 0.90f);
    glm::vec3 darkChassis(0.12f, 0.14f, 0.18f);

    // Save caller's sphere material so we can restore it before returning
    glm::vec3 savedAmb = sphereMesh.ambient;
    glm::vec3 savedDiff = sphereMesh.diffuse;
    glm::vec3 savedSpec = sphereMesh.specular;
    float savedShiny = sphereMesh.shininess;

    glm::mat4 localRoot = glm::scale(parentMatrix, glm::vec3(0.75f, 0.75f, 0.75f));

    // lower body box
    glm::mat4 baseM = glm::translate(localRoot, glm::vec3(0.0f, 0.38f, 0.0f));
    baseM = glm::scale(baseM, glm::vec3(1.75f, 0.75f, 1.75f));
    drawCube(shader, baseM, bodyColor * 0.3f, bodyColor, silver, 48.0f);

    // prize door flap
    glm::mat4 doorM = glm::translate(localRoot, glm::vec3(-0.42f, 0.32f, 0.88f));
    doorM = glm::scale(doorM, glm::vec3(0.42f, 0.36f, 0.02f));
    drawCube(shader, doorM, glm::vec3(0.05f), glm::vec3(0.08f), glm::vec3(0.2f), 8.0f);

    // fake black drop hole
    glm::mat4 bgChuteHole = glm::translate(localRoot, glm::vec3(-0.45f, 0.77f, 0.45f));
    bgChuteHole = glm::scale(bgChuteHole, glm::vec3(0.44f, 0.015f, 0.44f));
    drawCube(shader, bgChuteHole, glm::vec3(0.01f), glm::vec3(0.02f), glm::vec3(0.1f), 8.0f);

    // fake white trapdoor flap
    glm::mat4 bgDropDoor = glm::translate(localRoot, glm::vec3(-0.45f, 0.805f, 0.45f));
    bgDropDoor = glm::scale(bgDropDoor, glm::vec3(0.44f, 0.015f, 0.44f));
    drawCube(shader, bgDropDoor, glm::vec3(0.35f), glm::vec3(0.92f, 0.92f, 0.95f), glm::vec3(0.9f), 64.0f);

    // front control shelf
    glm::mat4 shelfM = glm::translate(localRoot, glm::vec3(0.0f, 0.56f, 1.0f));
    shelfM = glm::scale(shelfM, glm::vec3(1.2f, 0.18f, 0.25f));
    drawCube(shader, shelfM, bodyColor * 0.4f, bodyColor, silver, 64.0f);

    // joystick stick
    glm::mat4 bgStickM = glm::translate(localRoot, glm::vec3(-0.30f, 0.72f, 1.0f));
    bgStickM = glm::scale(bgStickM, glm::vec3(0.022f, 0.16f, 0.022f));
    drawCube(shader, bgStickM, glm::vec3(0.2f), glm::vec3(0.3f), silver, 128.0f);

    // joystick top ball
    glm::mat4 bgStickBallM = glm::translate(localRoot, glm::vec3(-0.30f, 0.81f, 1.0f));
    bgStickBallM = glm::scale(bgStickBallM, glm::vec3(0.60f));
    sphereMesh.ambient = marqueeColor * 0.4f;
    sphereMesh.diffuse = marqueeColor;
    sphereMesh.specular = glm::vec3(0.9f);
    sphereMesh.shininess = 64.0f;
    sphereMesh.drawSphere(shader, bgStickBallM);

    // round red button
    glm::mat4 bgBtnM = glm::translate(localRoot, glm::vec3(0.22f, 0.67f, 1.0f));
    bgBtnM = glm::scale(bgBtnM, glm::vec3(0.12f, 0.035f, 0.12f));
    drawCube(shader, bgBtnM, glm::vec3(0.4f, 0.0f, 0.0f), glm::vec3(0.95f, 0.1f, 0.15f), glm::vec3(0.8f), 64.0f);

    // corner posts
    float pil[4][2] = { {-0.82f, -0.82f}, {0.82f, -0.82f}, {-0.82f, 0.82f}, {0.82f, 0.82f} };
    for (int i = 0; i < 4; ++i) {
        glm::mat4 pM = glm::translate(localRoot, glm::vec3(pil[i][0], 1.68f, pil[i][1]));
        pM = glm::scale(pM, glm::vec3(0.07f, 1.85f, 0.07f));
        drawCube(shader, pM, darkMetal * 0.5f, darkMetal, silver, 64.0f);
    }

    // upper roof box
    glm::mat4 topM = glm::translate(localRoot, glm::vec3(0.0f, 2.70f, 0.0f));
    topM = glm::scale(topM, glm::vec3(1.78f, 0.28f, 1.78f));
    drawCube(shader, topM, bodyColor * 0.35f, bodyColor, silver, 64.0f);

    // glowing roof marquee line
    glm::mat4 signM = glm::translate(localRoot, glm::vec3(0.0f, 2.86f, 0.0f));
    signM = glm::scale(signM, glm::vec3(1.82f, 0.05f, 1.82f));
    drawCube(shader, signM, marqueeColor * 0.4f, marqueeColor, glm::vec3(1.0f), 128.0f);

    // top track rails
    glm::mat4 bgRail = glm::translate(localRoot, glm::vec3(0.0f, 2.55f, 0.0f));
    bgRail = glm::scale(bgRail, glm::vec3(1.72f, 0.035f, 0.06f));
    drawCube(shader, bgRail, metalSilver * 0.4f, metalSilver, glm::vec3(0.8f));

    // top trolley block
    glm::mat4 bgTrolley = glm::translate(localRoot, glm::vec3(0.0f, 2.55f, 0.0f));
    bgTrolley = glm::scale(bgTrolley, glm::vec3(0.18f, 0.06f, 0.18f));
    drawCube(shader, bgTrolley, darkChassis, glm::vec3(0.25f), glm::vec3(0.8f));

    // hanging metal wire
    glm::mat4 bgCable = glm::translate(localRoot, glm::vec3(0.0f, 2.10f, 0.0f));
    bgCable = glm::scale(bgCable, glm::vec3(0.015f, 0.75f, 0.015f));
    drawCube(shader, bgCable, metalSilver * 0.3f, metalSilver, glm::vec3(0.9f));

    // claw connector hub
    glm::mat4 bgHub = glm::translate(localRoot, glm::vec3(0.0f, 1.70f, 0.0f));
    bgHub = glm::scale(bgHub, glm::vec3(0.12f, 0.08f, 0.12f));
    drawCube(shader, bgHub, metalSilver * 0.4f, metalSilver, glm::vec3(1.0f), 128.0f);

    // static articulated claw fingers
    float prongAngles[3] = { 0.0f, 120.0f, 240.0f };
    for (int a = 0; a < 3; ++a) {
        glm::mat4 bgArmM = glm::translate(localRoot, glm::vec3(0.0f, 1.68f, 0.0f));
        bgArmM = glm::rotate(bgArmM, glm::radians(prongAngles[a]), glm::vec3(0.0f, 1.0f, 0.0f));
        bgArmM = glm::rotate(bgArmM, glm::radians(26.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 bgUpper = glm::translate(bgArmM, glm::vec3(0.0f, -0.10f, 0.05f));
        bgUpper = glm::scale(bgUpper, glm::vec3(0.025f, 0.16f, 0.025f));
        drawCube(shader, bgUpper, metalSilver * 0.4f, metalSilver, glm::vec3(0.9f), 64.0f);

        glm::mat4 bgLower = glm::translate(bgArmM, glm::vec3(0.0f, -0.20f, 0.08f));
        bgLower = glm::rotate(bgLower, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        bgLower = glm::scale(bgLower, glm::vec3(0.022f, 0.11f, 0.022f));
        drawCube(shader, bgLower, metalSilver * 0.4f, glm::vec3(0.95f), glm::vec3(1.0f), 128.0f);
    }

    // colored balls resting on the floor
    std::vector<glm::vec3> themeColors[5] = {
        { glm::vec3(0.95f, 0.2f, 0.4f), glm::vec3(0.98f, 0.7f, 0.8f), glm::vec3(0.6f, 0.1f, 0.7f) },
        { glm::vec3(0.1f, 0.8f, 0.9f), glm::vec3(0.2f, 0.4f, 0.9f), glm::vec3(0.8f, 0.9f, 1.0f) },
        { glm::vec3(0.98f, 0.8f, 0.1f), glm::vec3(0.95f, 0.4f, 0.1f), glm::vec3(0.95f, 0.2f, 0.2f) },
        { glm::vec3(0.2f, 0.9f, 0.4f), glm::vec3(0.1f, 0.6f, 0.3f), glm::vec3(0.7f, 0.95f, 0.2f) },
        { glm::vec3(0.8f, 0.2f, 0.9f), glm::vec3(0.5f, 0.1f, 0.8f), glm::vec3(0.9f, 0.5f, 0.8f) }
    };

    auto& activeSet = themeColors[prizeTheme % 5];
    int c = 0;
    for (float x = -0.45f; x <= 0.45f; x += 0.30f) {
        for (float z = -0.45f; z <= 0.45f; z += 0.30f) {
            glm::mat4 bM = glm::translate(localRoot, glm::vec3(x, 0.88f, z));
            glm::mat4 scaledM = glm::scale(bM, glm::vec3(1.09f));
            sphereMesh.ambient = activeSet[c % 3] * 0.35f;
            sphereMesh.diffuse = activeSet[c % 3];
            sphereMesh.specular = glm::vec3(0.6f);
            sphereMesh.shininess = 32.0f;
            sphereMesh.drawSphere(shader, scaledM);
            c++;
        }
    }

    // put original ball color values back
    sphereMesh.ambient = savedAmb;
    sphereMesh.diffuse = savedDiff;
    sphereMesh.specular = savedSpec;
    sphereMesh.shininess = savedShiny;
}

// moves the claw automatically to lift drop and return
void updateGameplayAutomation(float dt)
{
    float speed = 1.0f * dt;

    switch (autoState)
    {
    case LIFTING:
        // raise the claw straight up to the top
        if (clawPos.y < 2.30f)
        {
            clawPos.y += speed;
        }
        else
        {
            clawPos.y = 2.30f;
            autoState = MOVING_TO_CHUTE;
        }
        break;

    case MOVING_TO_CHUTE:
    {
        // slide the claw across the ceiling toward the drop hole
        bool reachedX = false;
        bool reachedZ = false;

        if (std::abs(clawPos.x - chuteTargetPos.x) > 0.02f)
            clawPos.x += (clawPos.x > chuteTargetPos.x ? -speed : speed);
        else
            reachedX = true;

        if (std::abs(clawPos.z - chuteTargetPos.z) > 0.02f)
            clawPos.z += (clawPos.z > chuteTargetPos.z ? -speed : speed);
        else
            reachedZ = true;

        if (reachedX && reachedZ)
        {
            autoState = DROPPING;
        }
        break;
    }

    case DROPPING:
        // open the claw fingers to release the ball
        isClawClosed = false;
        autoState = RETURNING;
        break;

    case RETURNING:
        // slide the claw back to the center of the machine
        if (std::abs(clawPos.x - 0.0f) > 0.02f)
            clawPos.x += (clawPos.x > 0.0f ? -speed : speed);
        if (std::abs(clawPos.z - 0.0f) > 0.02f)
            clawPos.z += (clawPos.z > 0.0f ? -speed : speed);

        if (std::abs(clawPos.x) <= 0.02f && std::abs(clawPos.z) <= 0.02f)
        {
            clawPos.x = 0.0f;
            clawPos.z = 0.0f;
            autoState = IDLE;
        }
        break;

    case IDLE:
    default:
        break;
    }
}

// defines 3d corner points and normal directions for making boxes
void initCube()
{
    float vertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        // Left face
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

        // Right face
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

         // Bottom face
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

         // Top face
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    // copy the points into graphics memory
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // tell the graphics card how to read positions and light normals
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// draws any box using chosen size position and color
void drawCube(Shader& shader, glm::mat4 model, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec, float shiny,
    bool isChecker, glm::vec3 checkerA, glm::vec3 checkerB, float tileSize)
{
    shader.use();
    shader.setMat4("model", model);
    shader.setVec3("material.ambient", amb);
    shader.setVec3("material.diffuse", diff);
    shader.setVec3("material.specular", spec);
    shader.setFloat("material.shininess", shiny);
    shader.setBool("material.isChecker", isChecker);
    if (isChecker)
    {
        shader.setVec3("material.checkerColorA", checkerA);
        shader.setVec3("material.checkerColorB", checkerB);
        shader.setFloat("material.tileSize", tileSize);
    }

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// draws a flat dark square on the floor to look like a shadow
void drawShadowBlob(Shader& shader, glm::mat4 sceneRoot, glm::vec3 worldPos, float scale)
{
    glm::mat4 m = sceneRoot;
    m = glm::translate(m, glm::vec3(worldPos.x, 0.001f, worldPos.z));
    m = glm::scale(m, glm::vec3(scale, 0.01f, scale));
    drawCube(shader, m, glm::vec3(0.0f), glm::vec3(0.015f, 0.015f, 0.02f), glm::vec3(0.0f), 1.0f);
}

// checks all keyboard buttons and updates the game
void processInput(GLFWwindow* window)
{
    // close the window when pressing escape
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // move camera in 3d space
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // rotate the entire room around all three directions
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
        sceneRotationY += 50.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
        sceneRotationY -= 50.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
        sceneRotationX += 45.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        sceneRotationX -= 45.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        sceneRotationZ += 45.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        sceneRotationZ -= 45.0f * deltaTime;

    // reset camera and rotations back to original starting spots
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        sceneRotationX = 0.0f;
        sceneRotationY = 0.0f;
        sceneRotationZ = 0.0f;
        camera.Position = glm::vec3(0.0f, 1.45f, 5.2f);
        camera.Yaw = 90.0f;
        camera.Pitch = 0.0f;
        camera.Zoom = 45.0f;
        camera.ProcessMouseMovement(0.0f, 0.0f);
    }

    // toggle the three light sources on and off
    static bool k1 = false, k2 = false, k3 = false;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) { if (!k1) { light1_on = !light1_on; k1 = true; } }
    else k1 = false;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { if (!k2) { light2_on = !light2_on; k2 = true; } }
    else k2 = false;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) { if (!k3) { light3_on = !light3_on; k3 = true; } }
    else k3 = false;

    // press enter to lift and drop the prize automatically
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS)
    {
        if (autoState == IDLE && grabbedPrizeIndex != -1)
            autoState = LIFTING;
    }

    // manual claw control keys
    if (autoState == IDLE)
    {
        float clawSpeed = 0.85f * deltaTime;
        // move claw left
        if ((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) && clawPos.x > -0.65f)
            clawPos.x -= clawSpeed;

        // move claw right
        if ((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) && clawPos.x < 0.65f)
            clawPos.x += clawSpeed;

        // move claw forward
        if ((glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) && clawPos.z > -0.65f)
            clawPos.z -= clawSpeed;

        // move claw backward
        if ((glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) && clawPos.z < 0.65f)
            clawPos.z += clawSpeed;

        // lower claw down
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && clawPos.y > 1.02f)
            clawPos.y -= clawSpeed;

        // raise claw up
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS && clawPos.y < 2.35f)
            clawPos.y += clawSpeed;

        // press space to open and close the claw
        static bool spacePressedLast = false;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            if (!spacePressedLast)
            {
                isClawClosed = !isClawClosed;
                spacePressedLast = true;
            }
        }
        else
        {
            spacePressedLast = false;
        }
    }

    // animate the claw fingers opening and closing
    if (isClawClosed)
    {
        if (clawOpenAngle > -4.0f)
            clawOpenAngle -= 140.0f * deltaTime;
    }
    else
    {
        if (clawOpenAngle < 26.0f)
            clawOpenAngle += 140.0f * deltaTime;
    }
}

// resize the view when window size changes
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
}


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}