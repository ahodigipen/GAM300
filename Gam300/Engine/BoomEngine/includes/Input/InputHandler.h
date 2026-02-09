#pragma once
#include "Core.h"
#include <bitset>
#include <vector>
#include <cstdint>
#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

#include <iostream>

namespace Boom {

    class InputSystem {
    public:
        // Call at the START of each frame (before glfwPollEvents()).
        // - snapshots previous state
        // - clears per-frame deltas
        void beginFrame() {
            m_prev = m_cur;
            m_mouseDelta = { 0.0f, 0.0f };
            m_scrollDelta = { 0.0f, 0.0f };
            m_lastMouseDelta = { 0.0f,0.0f };
            m_firstMouseThisFrame = true; // next cursor event seeds position

            // Track logged state to avoid spamming cout
            static bool s_joyLogged[GLFW_JOYSTICK_LAST + 1] = { false };
            static bool s_gamepadLogged[GLFW_JOYSTICK_LAST + 1] = { false };

            // Reset flags for disconnected joysticks
            for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
                if (!glfwJoystickPresent(j)) {
                    s_joyLogged[j] = false;
                    s_gamepadLogged[j] = false;
                }
            }

            // Poll Gamepad: Search for the first available gamepad
            m_cur.GamepadConnected = false;
            for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
                if (glfwJoystickPresent(jid)) {
                    if (glfwJoystickIsGamepad(jid)) {
                        // Log success once
                        if (!s_gamepadLogged[jid]) {
                            const char* name = glfwGetGamepadName(jid);
                            const char* guid = glfwGetJoystickGUID(jid);
                            std::cout << "[Input] Gamepad connected at index " << jid 
                                      << ": " << (name ? name : "Unknown") 
                                      << " (GUID: " << (guid ? guid : "N/A") << ")" << std::endl;
                            s_gamepadLogged[jid] = true;
                        }

                        GLFWgamepadstate state;
                        if (glfwGetGamepadState(jid, &state)) {
                            m_cur.GamepadConnected = true;
                            for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
                                m_cur.GamepadButtons.set(i, state.buttons[i] == GLFW_PRESS);
                            }
                            for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; ++i) {
                                m_cur.GamepadAxes[i] = state.axes[i];
                            }
                            break; // Use the first gamepad found
                        }
                    }
                    else {
                         // Log failure (missing mapping) once
                        if (!s_joyLogged[jid]) {
                            const char* name = glfwGetJoystickName(jid);
                            const char* guid = glfwGetJoystickGUID(jid);
                            std::cout << "[Input] Joystick found at index " << jid 
                                      << " but NOT recognized as Gamepad (missing mapping)." 
                                      << "\n        Name: " << (name ? name : "Unknown") 
                                      << "\n        GUID: " << (guid ? guid : "N/A") << std::endl;
                            s_joyLogged[jid] = true;
                        }
                    }
                }
            }
        }

        // ---- State accessors ----
        const WindowInputs& current()  const { return m_cur; }
        const WindowInputs& previous() const { return m_prev; }

        // Gamepad helpers
        bool gamepadButtonDown(int button) const {
            return (button >= 0 && button <= GLFW_GAMEPAD_BUTTON_LAST) ? m_cur.GamepadButtons.test(size_t(button)) : false;
        }
        bool gamepadButtonPressed(int button) const {
            return (button >= 0 && button <= GLFW_GAMEPAD_BUTTON_LAST)
                ? (m_cur.GamepadButtons.test(size_t(button)) && !m_prev.GamepadButtons.test(size_t(button)))
                : false;
        }
        bool gamepadButtonReleased(int button) const {
            return (button >= 0 && button <= GLFW_GAMEPAD_BUTTON_LAST)
                ? (!m_cur.GamepadButtons.test(size_t(button)) && m_prev.GamepadButtons.test(size_t(button)))
                : false;
        }
        float gamepadAxis(int axis) const {
            return (axis >= 0 && axis <= GLFW_GAMEPAD_AXIS_LAST) ? m_cur.GamepadAxes[axis] : 0.0f;
        }
        bool isGamepadConnected() const { return m_cur.GamepadConnected; }

        // Per-frame deltas (accumulated within this frame)
        glm::vec2 mouseDelta()  const { return m_mouseDelta; }
        glm::vec2 scrollDelta() const { return m_scrollDelta; }

        // Convenience helpers
        bool keyDown(int key) const {
            return (key >= 0 && key <= GLFW_KEY_LAST) ? m_cur.Keys.test(size_t(key)) : false;
        }
        bool keyPressed(int key) const {
            return (key >= 0 && key <= GLFW_KEY_LAST)
                ? (m_cur.Keys.test(size_t(key)) && !m_prev.Keys.test(size_t(key)))
                : false;
        }
        bool keyReleased(int key) const {
            return (key >= 0 && key <= GLFW_KEY_LAST)
                ? (!m_cur.Keys.test(size_t(key)) && m_prev.Keys.test(size_t(key)))
                : false;
        }

        bool mouseDown(int button) const {
            return (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST) ? m_cur.Mouse.test(size_t(button)) : false;
        }
        bool mousePressed(int button) const {
            return (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
                ? (m_cur.Mouse.test(size_t(button)) && !m_prev.Mouse.test(size_t(button)))
                : false;
        }
        bool mouseReleased(int button) const {
            return (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
                ? (!m_cur.Mouse.test(size_t(button)) && m_prev.Mouse.test(size_t(button)))
                : false;
        }

        // Digital axis helper: +posKey / -negKey in {-1,0,1}
        float axis(int posKey, int negKey) const {
            float p = keyDown(posKey) ? 1.f : 0.f;
            float n = keyDown(negKey) ? 1.f : 0.f;
            return p - n;
        }

        // ---- Forward-only entry points for GLFW callbacks (STATE ONLY) ----
        void onKey(int key, int /*scancode*/, int action, int /*mods*/) {
            if (key < 0 || key > GLFW_KEY_LAST) return;
            if (action == GLFW_RELEASE) m_cur.Keys.set(size_t(key), false);
            else                        m_cur.Keys.set(size_t(key), true); // PRESS or REPEAT treated as down
        }
        glm::vec2 mouseDeltaLast() const { return m_lastMouseDelta; }
        void onMouseButton(int button, int action, int /*mods*/) {
            if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return;
            if (action == GLFW_RELEASE) m_cur.Mouse.set(size_t(button), false);
            else                        m_cur.Mouse.set(size_t(button), true);
        }

        void onCursorPos(double x, double y) {
            glm::vec2 d{ float(x - m_cur.MouseX), float(y - m_cur.MouseY) };
            if (m_firstMouseThisFrame) {
                m_firstMouseThisFrame = false;
                m_mouseDelta += glm::vec2{ float(x - m_prev.MouseX), float(y - m_prev.MouseY) };
                m_lastMouseDelta = glm::vec2{ float(x - m_prev.MouseX), float(y - m_prev.MouseY) };
            }
            else {
                m_mouseDelta += d;
                m_lastMouseDelta = d;           
            }
            m_cur.MouseX = x;
            m_cur.MouseY = y;
        }
        glm::dvec2 cursorPos() const { return { m_cur.MouseX, m_cur.MouseY }; }
        void onScroll(double sx, double sy) {
            m_scrollDelta += glm::vec2{ float(sx), float(sy) };
        }

    private:
        WindowInputs m_cur{};
        WindowInputs m_prev{};
        glm::vec2 m_lastMouseDelta{ 0.0f, 0.0f };
        glm::vec2 m_mouseDelta{ 0.0f, 0.0f };
        glm::vec2 m_scrollDelta{ 0.0f, 0.0f };
        bool      m_firstMouseThisFrame{ true };
    };

} // namespace Boom

