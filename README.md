# Arcade Claw Machine Simulation

This project contains my custom implementation of an interactive 3D claw machine simulation developed with modern OpenGL, GLFW, and GLM. 
The code in "main.cpp" drives the core interactive gameplay, featuring manual claw movement across all axes, a realistic descending grab mechanic with height and distance validation, a live score tracker and a multi-state automated cycle triggered by the Enter key that transports collected prize spheres over a left-hinged trapdoor before safely dropping them into the collection chute. 
Visuals are powered by 'fragmentShaderForPhongShading.fs', which implements a custom Phong illumination model combining directional point sources, a moving crane spotlight and a procedural checkered floor. 

To build and run this project, you must supply the remaining base template files in your solution: the header files, the source file and the companion shaders along with linked GLFW, GLAD and GLM libraries.