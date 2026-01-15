#pragma once
#include "common/Core.h"
#include "Callback.h"
#include "Utilities.h"
#include "Auxiliaries/Assets.h"
#include <iostream>
#include <unordered_map>
#include "PxPhysicsAPI.h"
#include <foundation/PxMath.h>
#include <characterkinematic/PxController.h>
#include <characterkinematic/PxControllerManager.h>

namespace Boom {
    struct PhysicsContext {

        // Simple line primitive for debug rendering
        struct DebugLine {
            glm::vec3 p0;
            glm::vec3 p1;
            glm::vec4 c0;
            glm::vec4 c1;
        };

        struct PhysXRayResult {
            bool hitFound = false;
            glm::vec3 position = glm::vec3(0.0f);
            glm::vec3 normal = glm::vec3(0.0f);
            float distance = 0.0f;
            entt::entity hitEntity = entt::null;
        };

        // ====================================================================================
        // REGION: LIFECYCLE & CORE
        // ====================================================================================
#pragma region Lifecycle
        BOOM_INLINE PhysicsContext()
            : m_Foundation(nullptr), m_Physics(nullptr), m_Dispatcher(nullptr), m_Scene(nullptr), m_DebugVisEnabled(false),
            m_ControllerManager(nullptr), m_ControllerMaterial(nullptr)
        {
            // Initialize PhysX SDK
            m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_AllocatorCallback, m_ErrorCallback);
            if (!m_Foundation) { BOOM_ERROR("Error initializing PhysX m_Foundation"); return; }

            // Create Physics Instance
            m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, PxTolerancesScale());
            if (!m_Physics) { BOOM_ERROR("Error initializing PhysX m_Physics"); m_Foundation->release(); return; }

            // Create Worker Threads
            m_Dispatcher = PxDefaultCpuDispatcherCreate(2);

            // Create Scene Description
            PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
            sceneDesc.simulationEventCallback = &m_EventCallback;
            sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
            sceneDesc.filterShader = CustomFilterShader;
            sceneDesc.cpuDispatcher = m_Dispatcher;

            // Create Scene
            m_Scene = m_Physics->createScene(sceneDesc);
            if (!m_Scene) { BOOM_ERROR("Error creating PhysX m_Scene"); m_Physics->release(); m_Foundation->release(); return; }

            // Create Controller Manager for character controllers
            // PxCreateControllerManager expects a reference to PxScene
            m_ControllerManager = PxCreateControllerManager(*m_Scene);
            if (!m_ControllerManager) {
                BOOM_ERROR("Failed to create PxControllerManager");
            }

            // Create a shared controller material for controllers (comfortable defaults)
            if (m_Physics) {
                m_ControllerMaterial = m_Physics->createMaterial(0.0f, 0.0f, 0.0f); // friction not used directly by controllers
            }

            m_DebugVisEnabled = false;
        }

        BOOM_INLINE ~PhysicsContext() {
            // Destroy controllers first (clean userData, release controllers)
            for (auto& kv : m_Controllers) {
                PxController* c = kv.second;
                if (c) {
                    if (c->getActor() && c->getActor()->userData) {
                        delete static_cast<EntityID*>(c->getActor()->userData);
                        c->getActor()->userData = nullptr;
                    }
                    c->release();
                }
            }
            m_Controllers.clear();

            if (m_ControllerManager) { m_ControllerManager->release(); m_ControllerManager = nullptr; }
            if (m_ControllerMaterial) { m_ControllerMaterial->release(); m_ControllerMaterial = nullptr; }

            if (m_Scene) { m_Scene->release(); }
            if (m_Physics) { m_Physics->release(); }
            if (m_Dispatcher) { m_Dispatcher->release(); }
            if (m_Foundation) { m_Foundation->release(); }
        }

        BOOM_INLINE void Simulate(uint32_t step, float dt)
        {
            for (uint32_t i = 0; i < step; ++i) {
                m_Scene->simulate(dt);
                m_Scene->fetchResults(true);
            }
        }

        BOOM_INLINE PxScene* GetPxScene() const { return m_Scene; }

        BOOM_INLINE void SetEventCallback(PxCallbackFunction&& callback) {
            m_EventCallback.m_Callback = callback;
        }
#pragma endregion

        // ====================================================================================
        // REGION: CHARACTER CONTROLLER (PxController) MANAGEMENT
        // ====================================================================================
#pragma region Controllers

        // Expose manager (if needed elsewhere)
        BOOM_INLINE PxControllerManager* GetControllerManager() const { return m_ControllerManager; }

        // Create a capsule controller for an entity.
        // radius: capsule radius
        // height: total capsule height (including hemispheres). Some APIs expect 'height' to be cylinder height; adjust as needed.
        // stepOffset: maximum step height agent can climb
        BOOM_INLINE bool CreateCapsuleController(Entity& entity, float radius, float height, float stepOffset = 0.25f, float contactOffset = 0.1f)
        {
            if (!m_ControllerManager || !m_Physics || !m_Scene) return false;
            if (!entity.Has<TransformComponent>()) return false;

            // Avoid double-creation
            uint32_t id = static_cast<uint32_t>(entity.ID());
            if (m_Controllers.find(id) != m_Controllers.end()) return true;

            // Get transform and character controller component for offset
            const auto& t = entity.Get<TransformComponent>().transform;

            // Apply local offset if CharacterControllerComponent exists
            glm::vec3 initialPos = t.translate;
            if (entity.Has<CharacterControllerComponent>()) {
                const auto& cc = entity.Get<CharacterControllerComponent>();
                initialPos += cc.localOffset;
            }

            // Prepare descriptor
            PxCapsuleControllerDesc desc;
            desc.radius = radius;
            // PhysX controller 'height' typically represents distance between capsule hemispheres (cylinder height).
            // To be safe, clamp to small positive value.
            desc.height = std::max(0.01f, height - 2.0f * radius);
            desc.stepOffset = stepOffset;
            desc.contactOffset = contactOffset;
            // Keep Y-up
            desc.upDirection = physx::PxVec3(0, 1, 0);
            desc.slopeLimit = cosf(glm::radians(50.0f)); // default slope limit (cos of angle)
            desc.nonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING;
            desc.material = m_ControllerMaterial;

            // Initial position from entity transform + local offset
            desc.position = PxExtendedVec3(initialPos.x, initialPos.y, initialPos.z);

            PxController* controller = m_ControllerManager->createController(desc);
            if (!controller) {
                BOOM_ERROR("[Physics] Failed to create capsule controller for entity {}", id);
                return false;
            }

            // Attach userData so callbacks and raycasts can map back to entity
            if (controller->getActor()) {
                controller->getActor()->userData = new EntityID(entity.ID());

                // Enable the controller's shapes for scene queries (trigger detection)
                PxRigidActor* actor = controller->getActor();
                PxU32 numShapes = actor->getNbShapes();
                std::vector<PxShape*> shapes(numShapes);
                actor->getShapes(shapes.data(), numShapes);

                for (PxShape* shape : shapes) {
                    // Enable scene query so raycasts and overlap tests can detect the controller
                    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                }
            }

            m_Controllers[id] = controller;
            return true;
        }

        BOOM_INLINE bool ResizeCapsuleController(uint32_t entityID, float newRadius, float newHeight) {
            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end() || !it->second) {
                BOOM_WARN("[Physics] ResizeCapsuleController: No controller for entity {}", entityID);
                return false;
            }

            PxController* ctrl = it->second;
            if (ctrl->getType() != PxControllerShapeType::eCAPSULE) {
                BOOM_WARN("[Physics] ResizeCapsuleController: Controller is not a capsule");
                return false;
            }

            PxCapsuleController* capsule = static_cast<PxCapsuleController*>(ctrl);

            // PhysX capsule height is the cylinder portion (excluding hemispheres)
            float cylinderHeight = std::max(0.01f, newHeight - 2.0f * newRadius);

            capsule->setRadius(newRadius);
            capsule->setHeight(cylinderHeight);

            BOOM_INFO("[Physics] Resized controller {} to radius={}, height={}", entityID, newRadius, newHeight);
            return true;
        }

        // Get current controller dimensions
        BOOM_INLINE bool GetControllerDimensions(uint32_t entityID, float& outRadius, float& outHeight) const {
            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end() || !it->second) {
                return false;
            }

            PxController* ctrl = it->second;
            if (ctrl->getType() == PxControllerShapeType::eCAPSULE) {
                PxCapsuleController* capsule = static_cast<PxCapsuleController*>(ctrl);
                outRadius = capsule->getRadius();
                // Convert back to total height (cylinder + 2 hemispheres)
                outHeight = capsule->getHeight() + 2.0f * capsule->getRadius();
                return true;
            }
            return false;
        }

        BOOM_INLINE void SetControllerPosition(uint32_t entityID, const glm::vec3& position) {
            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end() || !it->second) {
                BOOM_WARN("[Physics] SetControllerPosition: No controller for entity {}", entityID);
                return;
            }

            PxController* ctrl = it->second;
            ctrl->setPosition(PxExtendedVec3(position.x, position.y, position.z));
            BOOM_INFO("[Physics] Teleported controller {} to ({}, {}, {})", entityID, position.x, position.y, position.z);
        }

        // Overload that automatically applies the local offset from CharacterControllerComponent
        BOOM_INLINE void SetControllerPositionWithOffset(Entity& entity, const glm::vec3& entityPosition) {
            uint32_t id = static_cast<uint32_t>(entity.ID());

            glm::vec3 controllerPos = entityPosition;
            if (entity.Has<CharacterControllerComponent>()) {
                const auto& cc = entity.Get<CharacterControllerComponent>();
                controllerPos += cc.localOffset;
            }

            SetControllerPosition(id, controllerPos);
        }

        // Destroy all controllers (used when stopping play mode or loading a new scene)
        BOOM_INLINE void DestroyAllControllers() {
            for (auto& [entityID, ctrl] : m_Controllers) {
                if (ctrl) {
                    if (ctrl->getActor() && ctrl->getActor()->userData) {
                        delete static_cast<EntityID*>(ctrl->getActor()->userData);
                        ctrl->getActor()->userData = nullptr;
                    }
                    ctrl->release();
                }
            }
            m_Controllers.clear();
            m_ControllerCollisionFlags.clear();
            BOOM_INFO("[Physics] Destroyed all character controllers");
        }

        BOOM_INLINE const std::unordered_map<uint32_t, PxController*>& GetControllers() const {
            return m_Controllers;
        }

        //BOOM_INLINE PxControllerCollisionFlags MoveController(Entity& entity, glm::vec3 const& displacement, float minDist, float elapsedTime) {
        //    uint32_t id = static_cast<uint32_t>(entity.ID());
        //    auto it = m_Controllers.find(id);
        //    if (it == m_Controllers.end()) return PxControllerCollisionFlags();
        //    PxController* ctrl = it->second;
        //    if (!ctrl) return PxControllerCollisionFlags();

        //    // PhysX controller move expects a PxVec3 displacement and elapsed time.
        //    PxVec3 disp = ToPxVec3(displacement);
        //    PxControllerCollisionFlags flags = ctrl->move(disp, minDist, elapsedTime, nullptr);

        //    // After moving the controller, sync the ECS Transform (if present) with the controller actor pose.
        //    // This keeps the entity TransformComponent in sync for rendering / scripting.
        //    if (ctrl->getActor()) {
        //        physx::PxTransform pose = ctrl->getActor()->getGlobalPose();

        //        // Convert PhysX pose to glm types
        //        glm::vec3 worldPos = ToGLMVec3(pose.p);
        //        glm::quat rotQuat(pose.q.w, pose.q.x, pose.q.y, pose.q.z);
        //        glm::vec3 worldRotDeg = glm::degrees(glm::eulerAngles(rotQuat));

        //        if (entity.Has<TransformComponent>()) {
        //            auto& tc = entity.Get<TransformComponent>().transform;
        //            tc.translate = worldPos;
        //            tc.rotate = worldRotDeg;
        //        }
        //    }

        //    return flags;
        //}

        BOOM_INLINE std::vector<EntityID> GetControllerTriggerOverlaps(uint32_t entityID) const {
            std::vector<EntityID> overlappingTriggers;

            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end() || !it->second) return overlappingTriggers;

            PxController* ctrl = it->second;
            PxRigidActor* actor = ctrl->getActor();
            if (!actor) return overlappingTriggers;

            // Get controller shape for overlap query
            PxU32 numShapes = actor->getNbShapes();
            if (numShapes == 0) return overlappingTriggers;

            std::vector<PxShape*> shapes(numShapes);
            actor->getShapes(shapes.data(), numShapes);

            PxShape* controllerShape = shapes[0];
            PxGeometryHolder geom = controllerShape->getGeometry();
            PxTransform pose = actor->getGlobalPose() * controllerShape->getLocalPose();

            // Perform overlap query
            const PxU32 bufferSize = 64;
            PxOverlapHit hitBuffer[bufferSize];
            PxOverlapBuffer overlapBuffer(hitBuffer, bufferSize);

            PxQueryFilterData filterData;
            filterData.flags = PxQueryFlag::eANY_HIT | PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;

            bool hasOverlaps = false;
            switch (geom.getType()) {
            case PxGeometryType::eCAPSULE:
                hasOverlaps = m_Scene->overlap(geom.capsule(), pose, overlapBuffer, filterData);
                break;
            case PxGeometryType::eBOX:
                hasOverlaps = m_Scene->overlap(geom.box(), pose, overlapBuffer, filterData);
                break;
            case PxGeometryType::eSPHERE:
                hasOverlaps = m_Scene->overlap(geom.sphere(), pose, overlapBuffer, filterData);
                break;
            default:
                break;
            }

            if (hasOverlaps) {
                for (PxU32 i = 0; i < overlapBuffer.getNbTouches(); ++i) {
                    const PxOverlapHit& hit = overlapBuffer.getTouch(i);

                    // Skip self
                    if (hit.actor == actor) continue;

                    // Check if the hit shape is a trigger
                    if (hit.shape && (hit.shape->getFlags() & PxShapeFlag::eTRIGGER_SHAPE)) {
                        if (hit.actor && hit.actor->userData) {
                            EntityID* triggerEntityID = static_cast<EntityID*>(hit.actor->userData);
                            if (triggerEntityID) {
                                overlappingTriggers.push_back(*triggerEntityID);
                            }
                        }
                    }
                }
            }

            return overlappingTriggers;
        }

        BOOM_INLINE void DestroyController(Entity& entity) {
            DestroyController(static_cast<uint32_t>(entity.ID()));
        }

        BOOM_INLINE void DestroyController(uint32_t entityID) {
            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end()) return;
            PxController* ctrl = it->second;
            if (ctrl) {
                if (ctrl->getActor() && ctrl->getActor()->userData) {
                    delete static_cast<EntityID*>(ctrl->getActor()->userData);
                    ctrl->getActor()->userData = nullptr;
                }
                ctrl->release();
            }
            m_Controllers.erase(it);
        }

        BOOM_INLINE PxControllerCollisionFlags MoveController(Entity& entity, glm::vec3 const& displacement, float minDist, float elapsedTime) {
            uint32_t id = static_cast<uint32_t>(entity.ID());
            auto it = m_Controllers.find(id);
            if (it == m_Controllers.end()) return PxControllerCollisionFlags();
            PxController* ctrl = it->second;
            if (!ctrl) return PxControllerCollisionFlags();

            PxVec3 disp = ToPxVec3(displacement);
            PxControllerCollisionFlags flags = ctrl->move(disp, minDist, elapsedTime, nullptr);

            // Store the collision flags for later query
            m_ControllerCollisionFlags[id] = flags;

            // Sync transform - subtract offset to get entity's actual position
            if (ctrl->getActor()) {
                physx::PxTransform pose = ctrl->getActor()->getGlobalPose();
                glm::vec3 controllerPos = ToGLMVec3(pose.p);

                if (entity.Has<TransformComponent>()) {
                    auto& tc = entity.Get<TransformComponent>().transform;

                    // Subtract local offset to get the entity's feet/pivot position
                    if (entity.Has<CharacterControllerComponent>()) {
                        const auto& cc = entity.Get<CharacterControllerComponent>();
                        tc.translate = controllerPos - cc.localOffset;
                    }
                    else {
                        tc.translate = controllerPos;
                    }
                }
            }

            return flags;
        }

        // Add a new method to query grounded state:
        // Replace or add this more robust ground check:
        BOOM_INLINE bool IsControllerGrounded(uint32_t entityID) const {
            auto it = m_Controllers.find(entityID);
            if (it == m_Controllers.end()) return false;

            PxController* ctrl = it->second;
            if (!ctrl) return false;

            // Get controller position and dimensions
            PxExtendedVec3 pos = ctrl->getPosition();
            float radius = 0.0f;
            float halfHeight = 0.0f;

            if (ctrl->getType() == PxControllerShapeType::eCAPSULE) {
                PxCapsuleController* capsule = static_cast<PxCapsuleController*>(ctrl);
                radius = capsule->getRadius();
                halfHeight = capsule->getHeight() / 2.0f;
            }

            // Raycast from bottom of controller downward
            float skinWidth = ctrl->getContactOffset();
            PxVec3 origin((float)pos.x, (float)pos.y - halfHeight - radius + skinWidth, (float)pos.z);
            PxVec3 dir(0, -1, 0);
            float maxDist = skinWidth + 0.1f;  // Small distance below feet

            PxRaycastBuffer hit;
            if (m_Scene->raycast(origin, dir, maxDist, hit)) {
                return true;
            }

            // Fallback to collision flags if raycast misses
            auto flagIt = m_ControllerCollisionFlags.find(entityID);
            if (flagIt != m_ControllerCollisionFlags.end()) {
                return flagIt->second.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
            }

            return false;
        }
        // Query whether an entity has a controller
        BOOM_INLINE bool HasController(Entity& entity) const {
            return m_Controllers.find(static_cast<uint32_t>(entity.ID())) != m_Controllers.end();
        }

#pragma endregion

        // ====================================================================================
        // REGION: ACTOR MANAGEMENT (ADD / REMOVE)
        // ====================================================================================
#pragma region ActorManagement

// Adds a RigidBody (Dynamic/Static/Kinematic) to the entity
        BOOM_INLINE void AddRigidBody(Entity& entity, AssetRegistry& assetRegistry) {
            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;

            // 1. Cleanup existing actors to prevent leaks/duplicates
            RemoveRigidBody(entity);

            // 2. Prepare Transform
            PxTransform pose = ToPxTransform(transform);

            // 3. Create Collider Shape (if available)
            PxShape* shape = nullptr;
            if (entity.Has<ColliderComponent>()) {
                shape = CreatePxShape(entity.Get<ColliderComponent>().Collider, transform, assetRegistry);
            }

            // 4. Create Actor
            if (body.type == RigidBody3D::DYNAMIC || body.type == RigidBody3D::KINEMATIC) {
                // Must have a shape for dynamic simulation usually, but we handle nulls safely
                if (shape) {
                    body.actor = PxCreateDynamic(*m_Physics, pose, *shape, body.density);
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                }
                else {
                    body.actor = m_Physics->createRigidDynamic(pose);
                }

                PxRigidDynamic* dyn = static_cast<PxRigidDynamic*>(body.actor);
                if (dyn) {
                    dyn->setLinearVelocity(ToPxVec3(body.initialVelocity));
                    dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, body.type == RigidBody3D::KINEMATIC);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                    dyn->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
                }
            }
            else { // STATIC
                if (shape) body.actor = PxCreateStatic(*m_Physics, pose, *shape);
                else       body.actor = m_Physics->createRigidStatic(pose);
            }

            if (!body.actor) { BOOM_ERROR("Error creating actor"); if (shape) shape->release(); return; }

            // 5. Final Setup
            body.actor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
            body.actor->userData = new EntityID(entity.ID());
            m_Scene->addActor(*body.actor);

            // Shape ref count is incremented by attach, we can release our local pointer if we don't need it, 
            // but PhysX createShape returns a ref count of 1. PxCreateDynamic attaches it (inc ref). 
            // The helper 'CreatePxShape' returns an unattached shape.
            if (shape) shape->release();
        }

        // Adds a Static Actor for an entity that ONLY has a collider (Triggers, Environment)
        BOOM_INLINE void AddColliderOnly(Entity& entity, AssetRegistry& assetRegistry) {
            if (entity.Has<RigidBodyComponent>()) return; // RB handles it
            if (!entity.Has<ColliderComponent>()) { BOOM_WARN("AddColliderOnly called without Collider"); return; }

            auto& transform = entity.Get<TransformComponent>().transform;
            auto& collider = entity.Get<ColliderComponent>().Collider;

            // 1. Cleanup
            RemoveColliderActor(entity);

            // 2. Create Shape
            PxShape* shape = CreatePxShape(collider, transform, assetRegistry);
            if (!shape) { BOOM_ERROR("Failed to create shape for ColliderOnly"); return; }

            // 3. Create Static Actor
            PxTransform pose = ToPxTransform(transform);
            PxRigidStatic* staticActor = m_Physics->createRigidStatic(pose);

            staticActor->attachShape(*shape);
            staticActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
            staticActor->userData = new EntityID(entity.ID());

            m_Scene->addActor(*staticActor);

            // Store reference
            collider.actor = staticActor;

            // Release local shape ref (actor holds one now)
            shape->release();
            collider.Shape = shape; // Keep a raw pointer if needed for updates, but rely on actor

            BOOM_INFO("Created collider-only entity");
        }

        BOOM_INLINE void RemoveRigidBody(Entity& entity) {
            PxRigidActor* actor = nullptr;

            if (entity.Has<RigidBodyComponent>()) {
                auto& rb = entity.Get<RigidBodyComponent>();
                actor = rb.RigidBody.actor;
                rb.RigidBody.actor = nullptr;
            }
            else if (entity.Has<ColliderComponent>()) {
                // Check if we stored it in the collider component
                auto& col = entity.Get<ColliderComponent>().Collider;
                actor = col.actor;
                col.actor = nullptr;
                // Fallback: If not stored, check shape
                if (!actor && col.Shape) actor = col.Shape->getActor();
            }

            if (!actor) return;

            // Clean User Data
            if (actor->userData) {
                delete static_cast<EntityID*>(actor->userData);
                actor->userData = nullptr;
            }

            m_Scene->removeActor(*actor);
            actor->release();
        }

        // Helper for entities that strictly only have ColliderComponent
        BOOM_INLINE void RemoveColliderActor(Entity& entity) {
            if (!entity.Has<ColliderComponent>()) return;
            // Re-use generic logic
            RemoveRigidBody(entity);
        }

        BOOM_INLINE void ForceRemoveActor(uint32_t entityID) {
            if (!m_Scene) return;
            PxU32 nbActors = m_Scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
            std::vector<PxActor*> actors(nbActors);
            m_Scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, actors.data(), nbActors);

            for (PxActor* actor : actors) {
                if (actor->userData && *static_cast<EntityID*>(actor->userData) == static_cast<EntityID>(entityID)) {
                    m_Scene->removeActor(*actor);
                    delete static_cast<EntityID*>(actor->userData);
                    actor->release();
                    BOOM_INFO("[Physics] Force removed actor for Entity {}", entityID);
                }
            }
        }
#pragma endregion

        // ====================================================================================
        // REGION: RUNTIME UPDATES
        // ====================================================================================
#pragma region RuntimeUpdates

        BOOM_INLINE void UpdateColliderShape(Entity& entity, AssetRegistry& assetRegistry) {
            // Handle both RigidBody and Collider-only entities
            PxRigidActor* actor = nullptr;

            if (entity.Has<RigidBodyComponent>()) {
                actor = entity.Get<RigidBodyComponent>().RigidBody.actor;
            }
            else if (entity.Has<ColliderComponent>()) {
                actor = entity.Get<ColliderComponent>().Collider.actor;
            }

            if (!actor) return;
            if (!entity.Has<ColliderComponent>()) return;

            auto& collider = entity.Get<ColliderComponent>().Collider;
            auto& transform = entity.Get<TransformComponent>().transform;

            // 1. Detach and Release Old Shape
            if (collider.Shape) {
                actor->detachShape(*collider.Shape);
                // Don't release here - the shape might be shared or already released
                collider.Shape = nullptr;
            }

            // 2. Create New Shape (this also sets collider.Shape)
            PxShape* newShape = CreatePxShape(collider, transform, assetRegistry);

            // 3. Attach New Shape
            if (newShape) {
                actor->attachShape(*newShape);
                newShape->release(); // Actor now owns it, release our reference

                // Recalculate mass for dynamic objects
                if (entity.Has<RigidBodyComponent>()) {
                    auto& body = entity.Get<RigidBodyComponent>().RigidBody;
                    if (body.type == RigidBody3D::DYNAMIC) {
                        PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(actor), body.density);
                    }
                }
            }
        }

        BOOM_INLINE void UpdateRigidBodyTransform(Entity entity, const Transform3D& transform) {
            if (!entity.Has<Boom::RigidBodyComponent>()) return;
            auto& rc = entity.Get<Boom::RigidBodyComponent>();
            if (!rc.RigidBody.actor) return;

            PxTransform pose = ToPxTransform(transform);
            rc.RigidBody.actor->setGlobalPose(pose);

            if (rc.RigidBody.type == RigidBody3D::Type::DYNAMIC) {
                static_cast<PxRigidDynamic*>(rc.RigidBody.actor)->wakeUp();
            }
        }

        BOOM_INLINE void SetRigidBodyType(Entity& entity, RigidBody3D::Type newType) {
            if (!entity.Has<RigidBodyComponent>()) return;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            if (!body.actor || body.type == newType) return;

            // Cache properties
            PxTransform pose = body.actor->getGlobalPose();
            EntityID* userData = static_cast<EntityID*>(body.actor->userData);

            // Get Shapes
            const PxU32 numShapes = body.actor->getNbShapes();
            std::vector<PxShape*> shapes(numShapes);
            body.actor->getShapes(shapes.data(), numShapes);

            // Remove old actor
            m_Scene->removeActor(*body.actor);
            body.actor->release();

            // Create new actor
            PxRigidActor* newActor = nullptr;
            if (newType == RigidBody3D::STATIC) {
                newActor = m_Physics->createRigidStatic(pose);
            }
            else {
                PxRigidDynamic* dyn = m_Physics->createRigidDynamic(pose);
                PxRigidBodyExt::updateMassAndInertia(*dyn, body.density);
                dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, newType == RigidBody3D::KINEMATIC);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                newActor = dyn;
            }

            // Re-attach shapes & Add to scene
            if (newActor) {
                for (PxShape* shape : shapes) newActor->attachShape(*shape);
                newActor->userData = userData;
                m_Scene->addActor(*newActor);
            }

            body.actor = newActor;
            body.type = newType;
        }

        BOOM_INLINE void SetColliderType(Entity& entity, Collider3D::Type newType, AssetRegistry& assetRegistry) {
            if (!entity.Has<ColliderComponent>()) return;
            auto& collider = entity.Get<ColliderComponent>().Collider;
            if (collider.type == newType) return;

            collider.type = newType;
            UpdateColliderShape(entity, assetRegistry);
        }

        BOOM_INLINE void UpdatePhysicsMaterial(Entity& ent) {
            if (!ent.Has<ColliderComponent>()) return;
            auto& collider = ent.Get<ColliderComponent>().Collider;
            if (!collider.Shape) return;

            PxMaterial* material;
            if (collider.Shape->getMaterials(&material, 1) > 0) {
                material->setDynamicFriction(collider.dynamicFriction);
                material->setStaticFriction(collider.staticFriction);
                material->setRestitution(collider.restitution);
            }
        }

        BOOM_INLINE void SetRotationLock(Boom::Entity entity, bool lockX, bool lockY, bool lockZ) {
            if (!entity.Has<Boom::RigidBodyComponent>()) return;
            auto& rc = entity.Get<Boom::RigidBodyComponent>();
            if (auto* dyn = rc.RigidBody.actor->is<physx::PxRigidDynamic>()) {
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, lockX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, lockY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, lockZ);
            }
        }
#pragma endregion

        // ====================================================================================
        // REGION: QUERIES (RAYCAST)
        // ====================================================================================
#pragma region Queries

        BOOM_INLINE PhysXRayResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDist) {
            PhysXRayResult result;
            if (!m_Scene) return result;

            PxRaycastBuffer hit;
            if (m_Scene->raycast(ToPxVec3(origin), ToPxVec3(glm::normalize(direction)), maxDist, hit)) {
                result.hitFound = true;
                result.position = ToGLMVec3(hit.block.position);
                result.normal = ToGLMVec3(hit.block.normal);
                result.distance = hit.block.distance;
                if (hit.block.actor && hit.block.actor->userData) {
                    result.hitEntity = (entt::entity)(*static_cast<EntityID*>(hit.block.actor->userData));
                }
            }
            return result;
        }

        BOOM_INLINE glm::vec3 ResolveThirdPersonCameraPosition(glm::vec3 const& playerEye, glm::vec3 const& idealCamPosition, float minDist = 0.5f) {
            PxVec3 targetPos = ToPxVec3(playerEye);
            PxVec3 idealCamPos = ToPxVec3(idealCamPosition);
            PxVec3 dir = (idealCamPos - targetPos).getNormalized();
            PxReal maxDist = (idealCamPos - targetPos).magnitude();

            PxRaycastBuffer hit;
            if (m_Scene->raycast(targetPos, dir, maxDist, hit))
                return ToGLMVec3(targetPos + dir * PxMax(hit.block.distance - 0.05f, minDist));
            return ToGLMVec3(idealCamPos);
        }

#pragma endregion

        // ====================================================================================
        // REGION: MESH COOKING & IO
        // ====================================================================================
#pragma region Cooking

        BOOM_INLINE bool CompileAndSavePhysicsMesh(ModelAsset& modelAsset, const std::string& savePath) {
            return GenericCookMesh(modelAsset, savePath, false);
        }

        BOOM_INLINE bool CompileAndSaveTriangleMesh(ModelAsset& modelAsset, const std::string& savePath) {
            return GenericCookMesh(modelAsset, savePath, true);
        }

        // Generic Mesh Loader (Convex)
        BOOM_INLINE physx::PxConvexMesh* LoadCookedMesh(const std::string& path) {
            auto buffer = ReadFileToBuffer(path);
            if (!buffer.data) return nullptr;
            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer.data), static_cast<PxU32>(buffer.size));
            PxConvexMesh* mesh = m_Physics->createConvexMesh(input);
            delete[] buffer.data;
            return mesh;
        }

        // Generic Mesh Loader (Triangle)
        BOOM_INLINE physx::PxTriangleMesh* LoadCookedTriangleMesh(const std::string& path) {
            auto buffer = ReadFileToBuffer(path);
            if (!buffer.data) return nullptr;
            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer.data), static_cast<PxU32>(buffer.size));
            PxTriangleMesh* mesh = m_Physics->createTriangleMesh(input);
            delete[] buffer.data;
            return mesh;
        }

        BOOM_INLINE PxConvexMeshGeometry CookMesh(const MeshData<ShadedVert>& data) {
            std::vector<PxVec3> vertices;
            for (auto& vertex : data.vtx) vertices.push_back(ToPxVec3(vertex.pos));

            PxConvexMeshDesc meshDesc;
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());
            meshDesc.indices.data = data.idx.data();
            meshDesc.indices.count = static_cast<PxU32>(data.idx.size());
            meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

            return CookConvexGeometryFromDesc(meshDesc);
        }
#pragma endregion

        // ====================================================================================
        // REGION: DEBUG VISUALIZATION
        // ====================================================================================
#pragma region DebugViz

        BOOM_INLINE void EnableDebugVisualization(bool enable, float scale = 1.0f) {
            m_DebugVisEnabled = enable;
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, enable ? scale : 0.0f);
            if (!enable) return;
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
        }

        BOOM_INLINE void CollectDebugLines(std::vector<DebugLine>& outLines) const {
            outLines.clear();
            if (!m_DebugVisEnabled || !m_Scene) return;
            const PxRenderBuffer& rb = m_Scene->getRenderBuffer();

            // Lines
            for (PxU32 i = 0; i < rb.getNbLines(); ++i) {
                const auto& l = rb.getLines()[i];
                outLines.push_back({ {l.pos0.x, l.pos0.y, l.pos0.z}, {l.pos1.x, l.pos1.y, l.pos1.z}, UnpackPxColor(l.color0), UnpackPxColor(l.color1) });
            }
            // Triangles
            for (PxU32 i = 0; i < rb.getNbTriangles(); ++i) {
                const auto& t = rb.getTriangles()[i];
                glm::vec3 a{ t.pos0.x, t.pos0.y, t.pos0.z }, b{ t.pos1.x, t.pos1.y, t.pos1.z }, c{ t.pos2.x, t.pos2.y, t.pos2.z };
                glm::vec4 ca = UnpackPxColor(t.color0), cb = UnpackPxColor(t.color1), cc = UnpackPxColor(t.color2);
                outLines.push_back({ a, b, ca, cb }); outLines.push_back({ b, c, cb, cc }); outLines.push_back({ c, a, cc, ca });
            }
        }
#pragma endregion


    private:
        // ====================================================================================
        // REGION: PRIVATE HELPERS
        // ====================================================================================

        // Centralized Shape Creation to avoid code duplication in AddRigidBody/AddColliderOnly
                // Centralized Shape Creation to avoid code duplication in AddRigidBody/AddColliderOnly
        BOOM_INLINE PxShape* CreatePxShape(Collider3D& collider, Transform3D& transform, AssetRegistry& assetRegistry)
        {
            if (!m_Physics) return nullptr;

            // 1. Create Material if needed
            if (!collider.material) {
                collider.material = m_Physics->createMaterial(collider.staticFriction, collider.dynamicFriction, collider.restitution);
            }

            // Convert user's local rotation from Euler degrees to quaternion
            PxQuat userRotation = ToPxQuat(collider.localRotation);
            PxTransform userLocalPose(ToPxVec3(collider.localPosition), userRotation);

            PxShape* shape = nullptr;
            glm::vec3 s = glm::abs(transform.scale * collider.localScale);

            // 2. Geometry Switch
            if (collider.type == Collider3D::BOX) {
                glm::vec3 halfExtents = s / 2.0f;
                halfExtents = glm::max(halfExtents, glm::vec3(0.01f)); // Safe min size
                shape = m_Physics->createShape(PxBoxGeometry(ToPxVec3(halfExtents)), *collider.material);
                shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::SPHERE) {
                float radius = std::max(s.x * 0.5f, 0.01f);
                shape = m_Physics->createShape(PxSphereGeometry(radius), *collider.material);
                shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::CAPSULE) {
                // Determine Axis based on scale
                int axis = 0; // 0=X, 1=Y, 2=Z
                if (s.y > s.x && s.y > s.z) axis = 1;
                else if (s.z > s.x && s.z > s.y) axis = 2;

                float radius, halfHeight;
                if (axis == 1) { radius = 0.5f * std::max(s.x, s.z); halfHeight = 0.5f * s.y; }
                else if (axis == 2) { radius = 0.5f * std::max(s.x, s.y); halfHeight = 0.5f * s.z; }
                else { radius = 0.5f * std::max(s.y, s.z); halfHeight = 0.5f * s.x; }

                halfHeight = std::max(halfHeight - radius, 0.01f);
                radius = std::max(radius, 0.01f);

                shape = m_Physics->createShape(PxCapsuleGeometry(radius, halfHeight), *collider.material);

                // PhysX capsules are X-axis aligned by default, rotate to desired axis
                PxQuat axisAlignmentRot = PxQuat(PxIdentity);
                if (axis == 1) axisAlignmentRot = PxQuat(PxHalfPi, PxVec3(0, 0, 1));
                else if (axis == 2) axisAlignmentRot = PxQuat(-PxHalfPi, PxVec3(0, 1, 0));

                // Combine: user rotation * axis alignment (apply axis alignment in user's rotated space)
                PxTransform finalPose(userLocalPose.p, userRotation * axisAlignmentRot);
                shape->setLocalPose(finalPose);
            }
            else if (collider.type == Collider3D::PLANE) {
                shape = m_Physics->createShape(PxPlaneGeometry(), *collider.material);
                // Planes need special handling - they face +X by default in PhysX
                PxQuat planeRot = PxQuat(PxIdentity);
                if (s.y < s.x && s.y < s.z) planeRot = PxQuat(PxHalfPi, PxVec3(0, 0, 1)); // Floor (face +Y)
                else if (s.z < s.x && s.z < s.y) planeRot = PxQuat(-PxHalfPi, PxVec3(0, 1, 0)); // Wall

                PxTransform finalPose(userLocalPose.p, userRotation * planeRot);
                shape->setLocalPose(finalPose);
            }
            else if (collider.type == Collider3D::CONVEX_MESH) {
                if (collider.physicsMeshID != EMPTY_ASSET) {
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);
                    if (!asset.mesh) asset.mesh = LoadCookedMesh(asset.cookedMeshPath);
                    if (asset.mesh) {
                        shape = m_Physics->createShape(PxConvexMeshGeometry(asset.mesh, PxMeshScale(ToPxVec3(s))), *collider.material);
                        shape->setLocalPose(userLocalPose);
                    }
                }
            }
            else if (collider.type == Collider3D::TRIANGLE_MESH) {
                if (collider.physicsMeshID != EMPTY_ASSET) {
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);
                    if (!asset.triangleMesh) asset.triangleMesh = LoadCookedTriangleMesh(asset.cookedMeshPath);
                    if (asset.triangleMesh) {
                        shape = m_Physics->createShape(PxTriangleMeshGeometry(asset.triangleMesh, PxMeshScale(ToPxVec3(s))), *collider.material);
                        shape->setLocalPose(userLocalPose);
                    }
                }
            }
            else if (collider.type == Collider3D::CYLINDER) {
                PxConvexMesh* mesh = CreateCylinderMesh(1.0f, 1.0f);
                if (mesh) {
                    float r = 0.5f * std::max(s.x, s.z);
                    shape = m_Physics->createShape(PxConvexMeshGeometry(mesh, PxMeshScale(PxVec3(r, 0.5f * s.y, r))), *collider.material);
                    shape->setLocalPose(userLocalPose);
                }
            }

            // 3. Flags
            if (shape) {
                shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
                if (collider.isTrigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                else {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                }

                // Store shape reference in collider
                collider.Shape = shape;
            }

            return shape;
        }

        // Helpers
        BOOM_INLINE static glm::vec4 UnpackPxColor(PxU32 c) {
            return { ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, ((c >> 0) & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f };
        }

        BOOM_INLINE PxTransform ToPxTransform(const Transform3D& t) {
            PxTransform p;
            p.p = ToPxVec3(t.translate);
            glm::quat q = glm::normalize(glm::quat(glm::radians(t.rotate)));
            p.q = PxQuat(q.x, q.y, q.z, q.w);
            return p;
        }

        struct FileBuffer { char* data; std::streamsize size; };
        BOOM_INLINE FileBuffer ReadFileToBuffer(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return { nullptr, 0 };
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            char* buf = new char[size];
            file.read(buf, size);
            return { buf, size };
        }

        BOOM_INLINE PxConvexMeshGeometry CookConvexGeometryFromDesc(PxConvexMeshDesc& desc) {
            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
            PxDefaultMemoryOutputStream buf;
            PxConvexMeshCookingResult::Enum res;
            PxConvexMesh* mesh = cooking->createConvexMesh(desc, m_Physics->getPhysicsInsertionCallback(), &res);
            cooking->release();
            return PxConvexMeshGeometry(mesh);
        }

        BOOM_INLINE PxConvexMesh* CreateCylinderMesh(float radius, float halfHeight, int segments = 16) {
            // (Previous implementation of cylinder generation logic)
            std::vector<PxVec3> vertices;
            for (int i = 0; i < segments; ++i) {
                float a = (float)i / segments * PxTwoPi;
                vertices.push_back(PxVec3(radius * cosf(a), halfHeight, radius * sinf(a)));
                vertices.push_back(PxVec3(radius * cosf(a), -halfHeight, radius * sinf(a)));
            }
            vertices.push_back(PxVec3(0, halfHeight, 0)); vertices.push_back(PxVec3(0, -halfHeight, 0));
            PxConvexMeshDesc md; md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size(); md.flags = PxConvexFlag::eCOMPUTE_CONVEX;
            return CookConvexGeometryFromDesc(md).convexMesh;
        }

        BOOM_INLINE bool GenericCookMesh(ModelAsset& modelAsset, const std::string& savePath, bool isTriangle) {
            auto staticModel = std::dynamic_pointer_cast<StaticModel>(modelAsset.data);
            if (!staticModel || staticModel->GetMeshData().empty()) return false;
            auto& meshData = staticModel->GetMeshData()[0];

            std::vector<PxVec3> vertices;
            for (const auto& v : meshData.vtx) vertices.push_back(ToPxVec3(v.pos));

            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
            PxDefaultMemoryOutputStream buf;
            bool status = false;

            if (isTriangle) {
                PxTriangleMeshDesc md;
                md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size();
                md.triangles.data = meshData.idx.data(); md.triangles.stride = 3 * sizeof(uint32_t); md.triangles.count = (PxU32)meshData.idx.size() / 3;
                status = cooking->cookTriangleMesh(md, buf);
            }
            else {
                PxConvexMeshDesc md;
                md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size();
                md.flags = PxConvexFlag::eCOMPUTE_CONVEX;
                status = cooking->cookConvexMesh(md, buf);
            }
            cooking->release();

            if (status) {
                std::ofstream outFile(savePath, std::ios::binary);
                outFile.write(reinterpret_cast<const char*>(buf.getData()), buf.getSize());
                return true;
            }
            return false;
        }

        static PxFilterFlags CustomFilterShader(
            PxFilterObjectAttributes attr0, [[maybe_unused]] PxFilterData fd0,
            PxFilterObjectAttributes attr1, [[maybe_unused]] PxFilterData fd1,
            PxPairFlags& pairFlags, [[maybe_unused]] const void* constantBlock,
            [[maybe_unused]] PxU32 constantBlockSize)
        {
            (void)fd0;
            (void)fd1;
            (void)constantBlock;
            (void)constantBlockSize;

            if (PxFilterObjectIsTrigger(attr0) || PxFilterObjectIsTrigger(attr1)) {
                pairFlags = PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_LOST | PxPairFlag::eDETECT_DISCRETE_CONTACT;
            }
            else {
                pairFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_TOUCH_LOST;
            }
            return PxFilterFlag::eDEFAULT;
        }

        PxDefaultErrorCallback m_ErrorCallback;
        PxDefaultAllocator m_AllocatorCallback;
        PxDefaultCpuDispatcher* m_Dispatcher;
        PxEventCallback m_EventCallback;
        PxFoundation* m_Foundation;
        PxPhysics* m_Physics;
        PxScene* m_Scene;
        bool m_DebugVisEnabled;

        // --- Character Controller support ---
        PxControllerManager* m_ControllerManager;
        PxMaterial* m_ControllerMaterial;
        std::unordered_map<uint32_t, PxController*> m_Controllers;
        std::unordered_map<uint32_t, PxControllerCollisionFlags> m_ControllerCollisionFlags;


    };
}