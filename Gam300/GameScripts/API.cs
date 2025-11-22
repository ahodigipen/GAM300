// Boom/API.cs - MONO COMPATIBLE VERSION
using System;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Boom
{



    // Internal calls implemented in C++ and registered with Mono
    internal static class Native
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_Log(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Boom_API_FindEntity(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetPosition(ulong handle, out Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetPosition(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsKeyDown(int key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsMouseDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasTransform(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasScript(ulong handle);

        // ========= PHYSICS / RIGIDBODY INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetRotation(ulong handle, out Vec3 outRot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetRotation(ulong handle, ref Vec3 rot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_DrawDebugVisionCone(ulong entityHandle,
            float range, float angle, float r, float g, float b, float a);


        // ========= NEW PHYSICS / RIGIDBODY INTERNAL CALLS =========

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsColliding(ulong handle);

        // ========= ANIMATOR INTERNAL   CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetFloat(ulong h, string name, float v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetBool(ulong h, string name, bool v);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetTrigger(ulong h, string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorPlay(ulong h, string state);
        
        

        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadScene(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern string Boom_API_GetCurrentSceneName();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_QuitGame();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadSceneAdditive(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_UnloadPauseMenu();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_TogglePause();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern int Boom_API_GetApplicationState();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool Boom_API_IsPauseMenuLoaded();

    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec3
    {
        public float X, Y, Z;

        public Vec3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }

    public static class API
    {
        // ===== Logging =====
        public static void Log(string s) => Native.Boom_API_Log(s);

        // ===== Entity queries =====
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

        // ===== Transform with validation =====
        public static Vec3 GetPosition(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetPosition(h, out var p);
            return p;
        }

        public static void SetPosition(ulong h, Vec3 p)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set position.");
                return;
            }
            Native.Boom_API_SetPosition(h, ref p);
        }

        // ===== Component checking =====
        public static bool HasTransform(ulong h) => Native.Boom_API_HasTransform(h);
        public static bool HasScript(ulong h) => Native.Boom_API_HasScript(h);

        // ===== Input =====
        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);

        // ===== PHYSICS / RIGIDBODY HELPERS =====

        public static Vec3 GetLinearVelocity(ulong h)
        {
            Native.Boom_API_GetLinearVelocity(h, out var v);
            return v;
        }

        public static void SetLinearVelocity(ulong h, Vec3 v)
        {
            Native.Boom_API_SetLinearVelocity(h, ref v);
        }

        // ===== Rotation methods =====
        public static Vec3 GetRotation(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetRotation(h, out var r);
            return r;
        }

        public static void SetRotation(ulong h, Vec3 r)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set rotation.");
                return;
            }
            Native.Boom_API_SetRotation(h, ref r);
        }

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle, Vec4 color)
        {
            Native.Boom_API_DrawDebugVisionCone(entityHandle, range, halfAngle,
                color.X, color.Y, color.Z, color.W);
        }

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle)
        {
            DrawDebugVisionCone(entityHandle, range, halfAngle, new Vec4(0f, 1f, 0f, 0.3f));
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Vec4
        {
            public float X, Y, Z, W;

            public Vec4(float x, float y, float z, float w)
            {
                X = x; Y = y; Z = z; W = w;
            }
        }
        /// <summary>
        /// Returns true if the rigidbody is colliding / grounded according to the engine.
        /// </summary>
        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();
        public static bool IsPauseMenuLoaded() => Native.Boom_API_IsPauseMenuLoaded();

        // ===== GLFW key codes =====
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;
        public const int KEY_W = 87;
        public const int KEY_A = 65;
        public const int KEY_S = 83;
        public const int KEY_D = 68;
        public const int KEY_SPACE = 32;

        public const int KEY_H = 72; // For How to Play

        public const int KEY_P = 80; // For Pause
        public const int KEY_R = 82; // For Resume
        public const int KEY_Y = 89; // For Restart
        public const int KEY_M = 77; // For Main Menu

        public const int KEY_Q = 81; // For Quit
        public const int KEY_LEFT_CONTROL = 341;

        public const int APP_STATE_RUNNING = 0;
        public const int APP_STATE_PAUSED = 1;
        public const int APP_STATE_STOPPED = 2;

        // ===== Mouse buttons =====
        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int MOUSE_MIDDLE = 2;


        public static void AnimatorSetFloat(ulong h, string n, float v) => Native.Boom_API_AnimatorSetFloat(h, n, v);
        public static void AnimatorSetBool(ulong h, string n, bool v) => Native.Boom_API_AnimatorSetBool(h, n, v);
        public static void AnimatorSetTrigger(ulong h, string n) => Native.Boom_API_AnimatorSetTrigger(h, n);
        public static void AnimatorPlay(ulong h, string state) => Native.Boom_API_AnimatorPlay(h, state);
    }

    public static class Transform
    {
        public static Vec3 GetPosition(ulong handle) => API.GetPosition(handle);
        public static void SetPosition(ulong handle, Vec3 p) => API.SetPosition(handle, p);
    }


}

namespace GameScripts
{
    /// <summary>
    /// Script registry that exposes available script types to the C++ engine.
    /// Returns a managed string[], C++ will use mono_runtime_invoke to read it.
    /// </summary>
    public static class ScriptRegistry
    {
        public static string[] GetAvailableScriptTypes()
        {
            try
            {
                // Get all non-abstract classes that look like scripts
                var scriptTypes = System.Reflection.Assembly.GetExecutingAssembly()
                    .GetTypes()
                    .Where(t => t.IsClass && !t.IsAbstract && IsScriptType(t))
                    .Select(t => t.FullName ?? t.Name)
                    .OrderBy(name => name)
                    .ToArray();

                //Boom.API.Log($"[C# ScriptRegistry] Found {scriptTypes.Length} script types");
                return scriptTypes;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[C# ScriptRegistry] Error getting script types: {ex.Message}");
                return Array.Empty<string>();
            }
        }

        /// <summary>
        /// Determines if a type is a valid script that can be instantiated.
        /// A valid script must have at least one of: OnStart, OnUpdate, or OnDestroy methods.
        /// </summary>
        private static bool IsScriptType(Type type)
        {
            bool hasOnStart = type.GetMethod("OnStart",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(string) }, null) != null;

            bool hasOnUpdate = type.GetMethod("OnUpdate",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(float) }, null) != null;

            bool hasOnDestroy = type.GetMethod("OnDestroy",
                BindingFlags.Public | BindingFlags.Instance,
                null, Type.EmptyTypes, null) != null;

            return hasOnStart || hasOnUpdate || hasOnDestroy;
        }
    }
}