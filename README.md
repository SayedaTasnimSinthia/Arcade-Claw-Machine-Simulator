# Arcade Claw Machine Simulation

This project contains my custom implementation of an interactive 3D claw machine simulation developed with modern OpenGL, GLFW, and GLM. 
The code in "main.cpp" drives the core interactive gameplay, featuring manual claw movement across all axes, a realistic descending grab mechanic with height and distance validation, a live score tracker and a multi-state automated cycle triggered by the Enter key that transports collected prize spheres over a left-hinged trapdoor before safely dropping them into the collection chute. 
Visuals are powered by 'fragmentShaderForPhongShading.fs', which implements a custom Phong illumination model combining directional point sources, a moving crane spotlight and a procedural checkered floor. 

To build and run this project, you must supply the remaining base template files in your solution: the header files, the source file and the companion shaders along with linked GLFW, GLAD and GLM libraries.

## Introduction

Arcade claw machines allow players to operate a mechanical claw to collect prizes and deliver them into a collection chute. This project recreates that experience as an interactive 3D arcade environment.

The simulation features one fully playable claw machine surrounded by ten decorative machines, creating the appearance of a complete arcade hall. The environment is constructed procedurally using cube and sphere primitives, with an interactive camera, dynamic lighting, animated mechanical components and physics-based prize movement.

## Objectives

- Develop a real-time 3D claw machine simulation using OpenGL 3.3 and GLSL.
- Apply translation, rotation and scaling through transformation matrices.
- Implement perspective projection and interactive camera movement.
- Use hierarchical transformations to animate mechanical components.
- Implement multisource lighting with per-pixel Phong shading.
- Create realistic materials using different colors and shininess values.
- Develop interactive keyboard controls for gameplay and environment manipulation.
- Implement prize detection, grabbing, gravity-based movement and scoring.

## Tools & Technologies

| Category | Technology |
|---|---|
| Programming Language | C++11 |
| Graphics API | OpenGL 3.3 Core Profile |
| Shading Language | GLSL 330 Core |
| Window & Input API | GLFW 3 |
| Function Loader | GLAD |
| Mathematics Library | GLM |
| Development Environment | Visual Studio |

## System Design

### 1. Arcade Environment

- A large arcade hall containing one fully functional claw machine.
- Ten decorative claw machines arranged along two corridors, five on each side.
- Different color themes for decorative machines.
- A procedurally generated two-tone checkered floor implemented in the fragment shader.
- A movable camera for viewing the environment from different perspectives.

### 2. Main Claw Machine

The playable machine consists of:

- Blue cabinet with silver trim.
- Control console with a joystick and red button.
- Gray prize bed containing colored prize spheres.
- Four structural support posts and an overhead canopy.
- Gantry rail and sliding trolley.
- Extendable hanging cable.
- Three-finger articulated mechanical claw.
- Hinged trapdoor and prize retrieval flap.

### 3. Lighting Design

The scene uses three independent light sources:

| Light Source | Purpose |
|---|---|
| Front Showcase Light | Illuminates the front of the machine. |
| Interior Canopy Light | Illuminates the interior prize area. |
| Claw Spotlight | Follows the moving claw and provides focused illumination. |

The lighting system uses a custom Phong illumination model. Each light can be toggled independently, while the front showcase light changes color according to the arcade room's rotation.

## Gameplay Mechanics

1. The claw begins above the prize area in an open position.
2. The player moves it horizontally to position it above a prize.
3. The claw descends toward the selected prize.
4. The player closes the fingers to attempt to grab the prize.
5. A successful grab attaches the prize to the claw.
6. Pressing Enter initiates the automated delivery sequence.
7. The claw transports the prize toward the collection chute.
8. The trapdoor opens and the claw releases the prize.
9. The prize falls under gravity into the collection chute.
10. The score increases when the prize is successfully collected.

## Controls

### Camera & Environment

| Key | Function |
|---|---|
| Q / W | Move camera forward / backward |
| E / R | Move camera left / right |
| T / Y | Rotate room around Y-axis |
| U / I | Rotate room around X-axis |
| O / P | Rotate room around Z-axis |
| A | Reset camera and room rotation |

### Lighting

| Key | Function |
|---|---|
| 1 | Toggle front showcase light |
| 2 | Toggle canopy light |
| 3 | Toggle claw spotlight |

### Claw Operation

| Key | Function |
|---|---|
| Arrow Keys / S, D, F, G | Move claw |
| H / J | Lower / raise claw |
| Space | Open / close claw |
| Enter | Start automatic prize delivery |
| Esc | Exit application |

## Animation & Physics

The simulation incorporates several animated and physics-based behaviors:

- **Claw Articulation:** Three mechanical fingers smoothly rotate between open and closed positions.
- **Cable Movement:** The hanging cable extends and retracts as the claw moves vertically.
- **Prize Attachment:** Successfully grabbed spheres follow the claw during movement.
- **Gravity Simulation:** Released prizes fall naturally and settle on the prize bed or enter the collection chute.
- **Trapdoor Animation:** The hinged chute door opens during delivery and closes afterward.
- **Automated Delivery:** A finite state machine manages the sequence of claw movement, prize release and collection.
- **Environment Rotation:** The arcade hall rotates around multiple axes, accompanied by corresponding lighting changes.
- **Score Tracking:** The score updates when a prize successfully reaches the collection chute.


