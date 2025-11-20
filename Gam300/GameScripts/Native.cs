// In: Native.cs

using Boom;
using System;
using System.Numerics;
using System.Runtime.CompilerServices;

namespace GameScripts
{
    internal static class Native
    {
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_Log(string s);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern ulong Boom_API_FindEntity(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_GetPosition(ulong handle, out Vec3 pos);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_SetPosition(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool Boom_API_IsColliding(ulong handle);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool Boom_API_IsKeyDown(int key);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool Boom_API_IsMouseDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadScene(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern string Boom_API_GetCurrentSceneName();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_QuitGame();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadSceneAdditive(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_UnloadPauseMenu();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_TogglePause();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern int Boom_API_GetApplicationState();
    }

    public static class API
    {
        public static void Log(string s) => Native.Boom_API_Log(s);
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

        public static Vec3 GetPosition(ulong h) { Native.Boom_API_GetPosition(h, out var p); return p; }
        public static void SetPosition(ulong h, Vec3 p) => Native.Boom_API_SetPosition(h, ref p);

        // --- ADDED ---
        public static Vec3 GetLinearVelocity(ulong h) { Native.Boom_API_GetLinearVelocity(h, out var v); return v; }
        public static void SetLinearVelocity(ulong h, Vec3 v) => Native.Boom_API_SetLinearVelocity(h, ref v);
        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);

        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();

        // GLFW key codes
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;

        public const int KEY_W = 87, KEY_A = 65, KEY_S = 83, KEY_D = 68, KEY_SPACE = 32;
        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int KEY_P = 80; // For Pause
        public const int KEY_R = 82; // For Resume
        public const int KEY_M = 77; // For Main Menu
        public const int KEY_Q = 81; // For Quit

        public const int APP_STATE_RUNNING = 0;
        public const int APP_STATE_PAUSED = 1;
        public const int APP_STATE_STOPPED = 2;
    }
}