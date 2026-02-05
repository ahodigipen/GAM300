#pragma once
#include "Core.h"

#include <cstdint>
#include <utility>      // std::move
namespace Boom
{    



    template <typename Event> 
    struct EventListener
    {
        using CallbackFn = std::function<void(const Event&)>;

        BOOM_INLINE EventListener(CallbackFn&& callback, uint32_t listnrid): 
            Callback(std::move(callback)), ID(listnrid) 
        {}
        
        CallbackFn Callback;
        uint32_t ID;
    };

    // Event Registry

    template <typename Event> 
    struct EventRegistry
    {
        using Listener = std::unique_ptr<EventListener<Event>>;
        std::queue<std::unique_ptr<Event>> Queue;
        std::vector<Listener> Listeners;
    };

    // Event Dispatcher

    struct EventDispatcher
    { 
        BOOM_INLINE ~EventDispatcher() 
        {
            for(auto& [_, ptr] : m_Registry) 
            {
                auto registry = CastRegistry<char>(ptr);
                BOOM_DELETE(registry);
            }
        }     

        template <typename Event, typename Callback>
        BOOM_INLINE void AttachCallback(Callback&& callback, uint32_t listnrid) 
        { 
            auto listener = std::make_unique<EventListener<Event>>(std::move(callback), listnrid);
            GetRegistry<Event>()->Listeners.push_back(std::move(listener));
        }
       
        template <typename Event>
        BOOM_INLINE void DetachCallback(uint32_t listnrid) 
        { 
            auto& listeners = GetRegistry<Event>()->Listeners;
            listeners.erase(std::remove_if(listeners.begin(), 
            listeners.end(), [&] (auto& listener) 
            { 
                return (listener->ID == listnrid);
            }), 
            listeners.end());
        } 

        BOOM_INLINE void EraseListener(uint32_t listnrid) 
        { 
            for(auto& [_, registry] : m_Registry) 
            {
                auto& listeners = CastRegistry<int8_t>(registry)->Listeners;
                listeners.erase(std::remove_if(listeners.begin(), 
                listeners.end(), [&] (auto& listener) 
                { 
                    return (listener->ID == listnrid); 
                }), 
                listeners.end());
            }
        } 
        
        template <typename Event, typename... Args>
        BOOM_INLINE void PostEvent(Args&&...args) 
        {
            auto registry = GetRegistry<Event>();
            if(registry->Listeners.empty()) { return; }
            registry->Queue.push(std::make_unique<Event>(std::forward<Args>(args)...));
        }

        template <typename Task>
        BOOM_INLINE void PostTask(Task&& task) 
        {
            m_Tasks.push(std::move(task));
        }

        BOOM_INLINE void PollEvents() 
        {
            // persistent callbacks
            for(auto& [_, pointer] : m_Registry) 
            {   
                auto registry = CastRegistry<char>(pointer);

                while(!registry->Queue.empty())
                {
                    for(auto& listener : registry->Listeners)
                    {
                        listener->Callback(*registry->Queue.front());
                    }
                    registry->Queue.pop();
                }
            }
            
            // frame callbacks
            while(!m_Tasks.empty()) 
            { 
                m_Tasks.front()();
                m_Tasks.pop();
            }
        } 
                
    private:
        template <typename Event>
        BOOM_INLINE EventRegistry<Event>* CastRegistry(void* p)
        {
            return static_cast<EventRegistry<Event>*>(p); 
        }

        template <typename Event>
        BOOM_INLINE EventRegistry<Event>* GetRegistry() 
        {
            auto it = m_Registry.find(TypeID<Event>());
            if(it != m_Registry.end()) 
            {
                return CastRegistry<Event>(it->second); 
            }
            auto registry = new EventRegistry<Event>();
            m_Registry[TypeID<Event>()] = registry;
            return registry;
        }
      
    private:    
        std::unordered_map<uint32_t, void*> m_Registry;
        std::queue<std::function<void()>> m_Tasks;
    };

    struct WindowResizeEvent {
        WindowResizeEvent(int32_t w, int32_t h)
            : width{ w }, height{h}
        {}
        int32_t width{};
        int32_t height{};
    };

    struct WindowTitleRenameEvent {
        WindowTitleRenameEvent(std::string const& str) : title{ str } {}
        std::string title{}; 
    };
    struct WindowInputs
    {
        std::bitset<GLFW_MOUSE_BUTTON_LAST+1> Mouse;
        std::bitset<GLFW_KEY_LAST+1> Keys;
        double MouseX = 0.0;
        double MouseY = 0.0;
    };
    struct KeyReleaseEvent
    {
        KeyReleaseEvent(int32_t key) : Key(key) {}
        int32_t Key = -1;
    };
    struct KeyPressEvent
    {
        KeyPressEvent(int32_t key) : Key(key) {}
        int32_t Key = -1;
    };

    struct KeyRepeatEvent
    {
        KeyRepeatEvent(int32_t key) : Key(key) {}
        int32_t Key = -1;
    };

    // Mouse Events

    struct MouseReleaseEvent
    {
        MouseReleaseEvent(int32_t b) : Button(b) {}
        int32_t Button = -1;
    };

    struct MouseDownEvent
    {
        MouseDownEvent(int32_t b) : Button(b) {}
        int32_t Button = -1;
    };

    struct MouseDragEvent
    {
        MouseDragEvent(double dx, double dy) :
            DeltaX(dx), DeltaY(dy) {
        }
        double DeltaX = 0, DeltaY = 0;
    };

    struct MouseMotionEvent
    {
        MouseMotionEvent(double x, double y) :
            TaregtX(x), TargetY(y) {
        }
        double TaregtX = 0, TargetY = 0;
    };

    struct MouseWheelEvent
    {
        MouseWheelEvent(double sx, double sy) :
            ScrollX(sx), ScrollY(sy) {
        }
        double ScrollX = 0, ScrollY = 0;
    };
}