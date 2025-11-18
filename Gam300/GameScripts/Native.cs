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

        // GLFW key codes
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;
    }
}