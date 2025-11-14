#include "Core.h"
#include "Scripting/MonoRuntime.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>

namespace Boom {

    bool MonoRuntime::Init(const char* domainName, const char* assembliesPath)
    {
        if (assembliesPath && *assembliesPath)
            mono_set_assemblies_path(assembliesPath);

#ifdef _WIN32
        // Point to actual Mono installation
        mono_set_dirs("C:/Program Files/Mono/lib", "C:/Program Files/Mono/etc");
#else
        mono_set_dirs("/usr/lib", "/etc/mono");
#endif

        m_RootDomain = mono_jit_init_version(domainName ? domainName : "BoomDomain", "v4.0.30319");
        if (!m_RootDomain) {
#ifdef DEBUG
            BOOM_ERROR("Mono: mono_jit_init_version failed");
#endif
            return false;
        }

        // Fresh app domain for your scripts (this is what we later unload)
        m_AppDomain = mono_domain_create_appdomain(const_cast<char*>("BoomApp"), nullptr);
        if (!m_AppDomain) {
#ifdef DEBUG
            BOOM_ERROR("Mono: failed to create app domain");
#endif
            return false;
        }
        mono_domain_set(m_AppDomain, /*force*/false);

#ifdef DEBUG
        BOOM_INFO("[Mono] Initialized: {}", RuntimeInfo());
#endif
        m_LoadedAssemblies.clear();
        return true;
    }

    void MonoRuntime::UnloadDomain()
    {
        // Leave app domain, switch back to root, then unload
        if (m_AppDomain) {
            mono_domain_set(m_RootDomain, /*force*/false);
            mono_domain_unload(m_AppDomain);
            m_AppDomain = nullptr;
        }
        // The handles to images/assemblies become invalid; clear our cache.
        m_LoadedAssemblies.clear();
    }

    void MonoRuntime::Shutdown()
    {
        UnloadDomain();
        if (m_RootDomain) {
            mono_jit_cleanup(m_RootDomain);
            m_RootDomain = nullptr;
        }
#ifdef DEBUG
        BOOM_INFO("[Mono] Shutdown complete");
#endif
    }

    MonoAssembly* MonoRuntime::LoadAssembly(const char* path)
    {
        if (!path) return nullptr;

        // Ensure we are in app domain
        if (m_AppDomain) mono_domain_set(m_AppDomain, /*force*/false);

        MonoAssembly* asmHandle = mono_domain_assembly_open(mono_domain_get(), path);
        if (!asmHandle) {
#ifdef DEBUG
            BOOM_ERROR("[Mono] Failed to load assembly: {}", path);
#endif
            return nullptr;
        }
#ifdef DEBUG
        MonoImage* img = mono_assembly_get_image(asmHandle);
        BOOM_INFO("[Mono] Loaded assembly: {} (image ok={})", path, img ? "true" : "false");
#endif
        m_LoadedAssemblies.push_back(asmHandle);
        return asmHandle;
    }

    bool MonoRuntime::InvokeStatic(const char* fullMethodDesc, void** args, int argCount)
    {
        (void)argCount;
        if (!fullMethodDesc) return false;

        if (m_AppDomain) mono_domain_set(m_AppDomain, /*force*/false);

        MonoMethodDesc* desc = mono_method_desc_new(fullMethodDesc, /*include_namespace*/ true);
        if (!desc) {
#ifdef _DEBUG
            BOOM_ERROR("[Mono] Bad method desc: {}", fullMethodDesc);
#endif
            return false;
        }

        MonoMethod* method = nullptr;

        if (MonoImage* imgCorlib = mono_get_corlib())
            method = mono_method_desc_search_in_image(desc, imgCorlib);

        if (!method) {
            for (MonoAssembly* a : m_LoadedAssemblies) {
                if (!a) continue;
                if (MonoImage* img = mono_assembly_get_image(a)) {
                    method = mono_method_desc_search_in_image(desc, img);
                    if (method) break;
                }
            }
        }

        mono_method_desc_free(desc);
        if (!method) {
#ifdef _DEBUG
            BOOM_ERROR("[Mono] Method not found: {}", fullMethodDesc);
#endif
            return false;
        }

        MonoObject* exc = nullptr;
        mono_runtime_invoke(method, nullptr, args, &exc);
        if (exc) {
            LogException(exc, "[Mono] Exception");
            return false;
        }
#ifdef _DEBUG
        BOOM_INFO("[Mono] Invoked: {}", fullMethodDesc);
#endif
        return true;
    }

    // ===== Helpers for instance creation / reflection =====

    // Accepts "Namespace.Type" (no assembly name).
    MonoClass* MonoRuntime::FindClassByName(const char* fullName) const
    {
        if (!fullName) return nullptr;

        // Split into namespace + name
        const char* dot = strrchr(fullName, '.');
        std::string ns, name;
        if (dot) { ns.assign(fullName, dot - fullName); name.assign(dot + 1); }
        else { ns = ""; name = fullName; }

        // Search each loaded assembly image
        for (MonoAssembly* a : m_LoadedAssemblies) {
            if (!a) continue;
            if (MonoImage* img = mono_assembly_get_image(a)) {
                if (MonoClass* k = mono_class_from_name(img, ns.c_str(), name.c_str()))
                    return k;
            }
        }
        return nullptr;
    }

    MonoMethod* MonoRuntime::FindMethod(MonoClass* klass, const char* name, int argc) const
    {
        if (!klass || !name) return nullptr;
        // mono_class_get_method_from_name is straightforward if you know argc
        return mono_class_get_method_from_name(klass, name, argc);
    }

    MonoClassField* MonoRuntime::FindField(MonoClass* klass, const char* name) const
    {
        if (!klass || !name) return nullptr;
        return mono_class_get_field_from_name(klass, name);
    }

    void MonoRuntime::LogException(MonoObject* exc, const char* prefix) const
    {
        if (!exc) return;
        MonoString* s = mono_object_to_string(exc, nullptr);
        char* c = mono_string_to_utf8(s);
#ifdef _DEBUG
        BOOM_ERROR("{}: {}", prefix ? prefix : "[Mono] Exception", c ? c : "(null)");
#endif
        if (c) mono_free(c);
    }

    const char* MonoRuntime::RuntimeInfo() const
    {
        return mono_get_runtime_build_info();
    }

} // namespace Boom
