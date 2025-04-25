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
    cout << "nig\n";
    #ifdef BOOM_ENABLE_LOG
        BOOM_INFO("Logger is active!");
        BOOM_DEBUG("DEBUG TEST");
        BOOM_WARN("WARNING TEST");
    #else
        std::cout << "BOOM_ENABLE_LOG not defined." << std::endl;
    #endif
}
