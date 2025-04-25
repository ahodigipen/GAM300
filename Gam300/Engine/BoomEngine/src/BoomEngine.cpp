// BoomEngine.cpp : Defines the functions for the static library.
//
//Build BoomEngine before running debuger or building Gam300
#include "BoomEngine.h"
#include "Core.h"
#include "framework.h"
#include <iostream>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
using namespace std;
// TODO: This is an example of a library function
void MyEngineClass::whatup() {
    // Make sure this is called AFTER you've created a valid OpenGL context with GLFW

    // Initialize GLEW
  

    // Use glm to show it's available
    glm::vec3 testVec(1.0f, 2.0f, 3.0f);
    std::cout << "GLM vec3: (" << testVec.x << ", " << testVec.y << ", " << testVec.z << ")\n";

    // Use ImGui version to confirm ImGui is linked
    std::cout << "ImGui version: " << ImGui::GetVersion() << std::endl;

   

    // Use GLFW version string
    std::cout << "GLFW version: " << glfwGetVersionString() << std::endl;
}
