/**
 * @file    Interface.h
 * @brief   Core interface for engine layers and layer-management utilities.
 */

#pragma once
#ifndef INTERFACE_H
#define INTERFACE_H

#include "Context.h"

namespace Boom
{

    /**
     * @struct   AppInterface
     * @brief    Base interface every engine layer derives from.
     *
     * Provides lifecycle hooks (OnStart, OnUpdate) and
     * layer-management utilities via templates.
     */
    struct AppInterface
    {
        /**
         * @brief  Virtual destructor for safe polymorphic cleanup.
         *
         * BOOM_INLINE hints the compiler to inline this call,
         * minimizing overhead in the engine’s hot loops.
         */
        BOOM_INLINE virtual ~AppInterface() = default;

        /**
         * @brief   Retrieve the first attached layer of type Layer.
         * @tparam  Layer  Must derive from AppInterface.
         * @return  Pointer to the layer instance, or nullptr if not found.
         */
        template<typename Layer>
        BOOM_INLINE Layer* GetLayer()
        {
            // Compile-time check: Layer inherits from AppInterface
            BOOM_STATIC_ASSERT(std::is_base_of<AppInterface, Layer>::value);

            // Search the context’s layer list by matching the LayerID
            auto it = std::find_if(
                m_Context->Layers.begin(),
                m_Context->Layers.end(),
                [](AppInterface* layer)
                {
                    return layer->m_LayerID == TypeID<Layer>();
                });

            return (it != m_Context->Layers.end())
                ? static_cast<Layer*>(*it)
                : nullptr;
        }

        /**
         * @brief   Create and attach a new layer of type Layer.
         * @tparam  Layer      Must derive from AppInterface.
         * @param   args       Arguments forwarded to Layer’s constructor.
         * @return  Pointer to the newly created layer, or nullptr on error.
         */
        template<typename Layer, typename... Args>
        BOOM_INLINE Layer* AttachLayer(Args&&... args)
        {
            BOOM_STATIC_ASSERT(std::is_base_of<AppInterface, Layer>::value);

            if (GetLayer<Layer>() != nullptr)
            {
                BOOM_ERROR("Layer already attached!");
                return nullptr;
            }

            auto layer = new Layer(std::forward<Args>(args)...);
            m_Context->Layers.push_back(layer);

            layer->m_LayerID = TypeID<Layer>();
            layer->m_Context = m_Context;
            layer->OnStart();

            return layer;
        }

    protected:
        /** @brief  Called once when the layer is attached. Override to initialize. */
        BOOM_INLINE virtual void OnStart() {}

        /** @brief  Called each frame. Override for per-frame logic. */
        BOOM_INLINE virtual void OnUpdate() {}

    private:
        // Allow Application to set up layers
        friend struct Application;

        AppContext* m_Context;   ///< Pointer to shared application context
        uint32_t    m_LayerID;   ///< Unique identifier for this layer
    };

} // namespace Boom

#endif // INTERFACE_H
