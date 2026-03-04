#pragma once
#include "Core.h"
#include <bitset>
#include <vector>
#include <cstdint>
#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

#ifdef _WIN32
#define NOMINMAX  
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

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
            m_lastMouseDelta = { 0.0f, 0.0f };
            m_firstMouseThisFrame = true;

#ifdef _WIN32
            // Use XInput for gamepad polling
            pollXInput();

            // If no XInput controller is found, fallback to GLFW (handles Bluetooth/DirectInput controllers)
            if (!m_cur.GamepadConnected) {
                pollGLFWGamepad();
            }
#else
            // Fallback to GLFW gamepad on non-Windows platforms
            pollGLFWGamepad();
#endif
        }

        // ---- State accessors ----
        const WindowInputs& current()  const { return m_cur; }
        const WindowInputs& previous() const { return m_prev; }

        // Gamepad helpers
        bool gamepadButtonDown(int button) const {
            return (button >= 0 && button <= GLFW_GAMEPAD_BUTTON_LAST) 
                ? m_cur.GamepadButtons.test(size_t(button)) : false;
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

        // Vibration support (XInput only, no-op on other platforms)
        void setVibration(float leftMotor, float rightMotor, int controllerIndex = 0) {
#ifdef _WIN32
            if (controllerIndex < 0 || controllerIndex >= 4 || !m_cur.GamepadConnected)
                return;

            XINPUT_VIBRATION vibration{};
            vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f);
            vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f);
            XInputSetState(controllerIndex, &vibration);
#else
            (void)leftMotor; (void)rightMotor; (void)controllerIndex;
#endif
        }

        // Per-frame deltas (accumulated within this frame)
        glm::vec2 mouseDelta()  const { return m_mouseDelta; }
        glm::vec2 scrollDelta() const { return m_scrollDelta; }
        glm::vec2 mouseDeltaLast() const { return m_lastMouseDelta; }

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
                ? (button < (int)m_cur.Mouse.size() && m_cur.Mouse.test(size_t(button)) && !m_prev.Mouse.test(size_t(button)))
                : false;
        }
        bool mouseReleased(int button) const {
            return (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
                ? (button < (int)m_cur.Mouse.size() && !m_cur.Mouse.test(size_t(button)) && m_prev.Mouse.test(size_t(button)))
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
            else                        m_cur.Keys.set(size_t(key), true);
        }

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

        // Deadzone configuration
        void setLeftStickDeadzone(float deadzone) { m_leftStickDeadzone = deadzone; }
        void setRightStickDeadzone(float deadzone) { m_rightStickDeadzone = deadzone; }

    private:
#ifdef _WIN32
        void pollXInput() {
            static bool s_loggedConnection = false;

            m_cur.GamepadConnected = false;

            // Check first available XInput controller
            for (DWORD i = 0; i < 4; ++i) {
                XINPUT_STATE state{};
                DWORD result = XInputGetState(i, &state);

                if (result == ERROR_SUCCESS) {
                    m_cur.GamepadConnected = true;

                    if (!s_loggedConnection) {
                        std::cout << "[Input] XInput controller connected at index " << i << std::endl;
                        s_loggedConnection = true;
                    }

                    const XINPUT_GAMEPAD& gp = state.Gamepad;

                    // Map XInput buttons to GLFW button indices
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_A, (gp.wButtons & XINPUT_GAMEPAD_A) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_B, (gp.wButtons & XINPUT_GAMEPAD_B) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_X, (gp.wButtons & XINPUT_GAMEPAD_X) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_Y, (gp.wButtons & XINPUT_GAMEPAD_Y) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, (gp.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, (gp.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_BACK, (gp.wButtons & XINPUT_GAMEPAD_BACK) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_START, (gp.wButtons & XINPUT_GAMEPAD_START) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_LEFT_THUMB, (gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_DPAD_UP, (gp.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, (gp.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_DPAD_DOWN, (gp.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
                    m_cur.GamepadButtons.set(GLFW_GAMEPAD_BUTTON_DPAD_LEFT, (gp.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);

                    // Map XInput axes to GLFW axis indices (with deadzone)
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_X] = normalizeStick(gp.sThumbLX, m_leftStickDeadzone);
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_Y] = -normalizeStick(gp.sThumbLY, m_leftStickDeadzone); // Inverted
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_X] = normalizeStick(gp.sThumbRX, m_rightStickDeadzone);
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = -normalizeStick(gp.sThumbRY, m_rightStickDeadzone); // Inverted
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = normalizeTrigger(gp.bLeftTrigger);
                    m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = normalizeTrigger(gp.bRightTrigger);

                    break; // Use first connected controller
                }
            }

            if (!m_cur.GamepadConnected && s_loggedConnection) {
                std::cout << "[Input] XInput controller disconnected" << std::endl;
                s_loggedConnection = false;
            }
        }

        float normalizeStick(SHORT value, float deadzone) const {
            float normalized = static_cast<float>(value) / 32767.0f;
            normalized = std::clamp(normalized, -1.0f, 1.0f);

            if (std::abs(normalized) < deadzone)
                return 0.0f;

            // Remap from [deadzone, 1.0] to [0.0, 1.0]
            float sign = (normalized > 0.0f) ? 1.0f : -1.0f;
            return sign * (std::abs(normalized) - deadzone) / (1.0f - deadzone);
        }

        float normalizeTrigger(BYTE value) const {
            constexpr BYTE threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            if (value < threshold)
                return 0.0f;
            return static_cast<float>(value - threshold) / static_cast<float>(255 - threshold);
        }
#endif

        float normalizeAxis(float value, float deadzone) const {
            float absVal = std::abs(value);
            if (absVal < deadzone)
                return 0.0f;

            // Remap from [deadzone, 1.0] to [0.0, 1.0]
            float sign = (value > 0.0f) ? 1.0f : -1.0f;
            return sign * (absVal - deadzone) / (1.0f - deadzone);
        }

        void pollGLFWGamepad() {
            // Original GLFW gamepad polling code (for non-Windows platforms)
            static bool s_joyLogged[GLFW_JOYSTICK_LAST + 1] = { false };
            static bool s_gamepadLogged[GLFW_JOYSTICK_LAST + 1] = { false };

            for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
                if (!glfwJoystickPresent(j)) {
                    s_joyLogged[j] = false;
                    s_gamepadLogged[j] = false;
                }
            }

            m_cur.GamepadConnected = false;
            for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
                if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
                    if (!s_gamepadLogged[jid]) {
                        std::cout << "[Input] GLFW Gamepad connected at index " << jid << std::endl;
                        s_gamepadLogged[jid] = true;
                    }

                    GLFWgamepadstate state;
                    if (glfwGetGamepadState(jid, &state)) {
                        m_cur.GamepadConnected = true;
                        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i)
                            m_cur.GamepadButtons.set(i, state.buttons[i] == GLFW_PRESS);
                        
                        // Apply deadzones to stick axes
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_X] = normalizeAxis(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], m_leftStickDeadzone);
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_Y] = normalizeAxis(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], m_leftStickDeadzone);
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_X] = normalizeAxis(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], m_rightStickDeadzone);
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = normalizeAxis(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], m_rightStickDeadzone);
                        
                        // Triggers
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
                        m_cur.GamepadAxes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
                        break;
                    }
                }
            }
        }

    private:
        WindowInputs m_cur{};
        WindowInputs m_prev{};
        glm::vec2 m_lastMouseDelta{ 0.0f, 0.0f };
        glm::vec2 m_mouseDelta{ 0.0f, 0.0f };
        glm::vec2 m_scrollDelta{ 0.0f, 0.0f };
        bool m_firstMouseThisFrame{ true };

        // XInput deadzone settings
        float m_leftStickDeadzone = 0.24f;
        float m_rightStickDeadzone = 0.24f;
    };

} // namespace Boom

