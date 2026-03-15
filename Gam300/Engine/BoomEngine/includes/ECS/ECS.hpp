#pragma once
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Physics/Utilities.h" 
#include "BoomProperties.h"
#include "AI/BehaviourTree.h"


namespace Boom {
 using EntityRegistry = entt::registry;
 using EntityID = entt::entity;
 constexpr EntityID NENTT = entt::null;

 //for now include only important components instead of all
 //new includes need to add into enum and string_view and within ComponentSelector() in InspectorPanel.cpp
 enum class ComponentID : size_t {
 INFO, TRANSFORM, CAMERA, RIGIDBODY, COLLIDER,
 MODEL, ANIMATOR, DIRECT_LIGHT, POINT_LIGHT, SPOT_LIGHT,
 SOUND, SCRIPT,
 THIRD_PERSON_CAMERA,
 NAV_AGENT_COMPONENT,
 AI_COMPONENT,
 SPRITE,
 TEXT,
 MENU_COMPONENT,
 DEACTIVATED_TAG,
 VIDEO,
 CHARACTER_CONTROLLER,
 PARTICLE_EMITTER,
 COUNT
 };
 constexpr std::string_view COMPONENT_NAMES[]{
 "Info",                 //0
 "Transform",            //1
 "Camera",               //2
 "Rigidbody",            //3
 "Collider",             //4
 "Model",                //5
 "Animator",             //6
 "Direct Light",         //7
 "Point Light",          //8
 "Spot Light",           //9
 "Sound",                //10
 "Script",               //11
 "Third Person Camera" , //12
 "Nav Agent Component",  //13
 "AI Component",         //14
 "Sprite",               //15
 "Text",                 //16
 "Menu Component",       //17
 "Deactivated Tag",      //18
 "Video",                //19
 "Character Controller", //20
 "Particle Emitter",     //21
 "Count"

 };

 // transform component
 struct TransformComponent
 {
 BOOM_INLINE TransformComponent(const TransformComponent&) = default;
 BOOM_INLINE TransformComponent() = default;
 Transform3D transform;

 XPROPERTY_DEF
 ("TransformComponent", TransformComponent
 , obj_member<"Transform", &TransformComponent::transform>
 )
 };

 // camera component
 struct CameraComponent
 {
 BOOM_INLINE CameraComponent(const CameraComponent&) = default;
 BOOM_INLINE CameraComponent() = default;
 Camera3D camera;

 // CameraComponent
 XPROPERTY_DEF(
 "CameraComponent", CameraComponent,
 obj_member<"Camera", &CameraComponent::camera>
 )
 };

 struct EnttComponent {
 BOOM_INLINE EnttComponent(const EnttComponent&) = default;
 BOOM_INLINE EnttComponent() = default;
 std::string name = "Entity";

 // EnttComponent
 XPROPERTY_DEF(
 "EnttComponent", EnttComponent,
 obj_member<"name", &EnttComponent::name>
 )
 };

 struct MeshComponent
 {
 BOOM_INLINE MeshComponent(const MeshComponent&) = default;
 BOOM_INLINE MeshComponent() = default;
 Mesh3D mesh;

 // MeshComponent
 //XPROPERTY_DEF(
 //    "MeshComponent", MeshComponent,
 //    obj_member<"mesh", &MeshComponent::mesh>
 //)
 };

 struct RigidBodyComponent
 {
 BOOM_INLINE RigidBodyComponent(const RigidBodyComponent&) = default;
 BOOM_INLINE RigidBodyComponent() = default;
 RigidBody3D RigidBody;

 // RigidBodyComponent
 XPROPERTY_DEF(
 "RigidBodyComponent", RigidBodyComponent,
 obj_member<"RigidBody", &RigidBodyComponent::RigidBody>
 )
 };

 struct ColliderComponent
 {
 BOOM_INLINE ColliderComponent(const ColliderComponent&) = default;
 BOOM_INLINE ColliderComponent() = default;
 Collider3D Collider;

 // ColliderComponent
 XPROPERTY_DEF(
 "ColliderComponent", ColliderComponent,
 obj_member<"Collider", &ColliderComponent::Collider>
 )
 };

 ////Model Component
 struct ModelComponent {

 //using AssetID = uint64_t;

 AssetID modelID{ EMPTY_ASSET };
 AssetID materialID{ EMPTY_ASSET };
 std::string modelName;
 std::string materialName;
 std::string modelSource;
 std::string materialSource;
 float opacityOverride{ 1.0f }; // Per-entity opacity multiplier (1.0 = opaque, 0.0 = transparent)

 XPROPERTY_DEF(
 "ModelComponent", ModelComponent,
 obj_member<"ModelID", &ModelComponent::modelID>,
 obj_member<"MaterialID", &ModelComponent::materialID>,
 obj_member<"ModelName", &ModelComponent::modelName>,
 obj_member<"MaterialName", &ModelComponent::materialName>,
 obj_member<"ModelSource", &ModelComponent::modelSource>,
 obj_member<"MaterialSource", &ModelComponent::materialSource>
 )

 };

 //Animator Component
 struct AnimatorComponent
 {
 BOOM_INLINE AnimatorComponent(const AnimatorComponent&) = default;
 BOOM_INLINE AnimatorComponent() = default;
 Animator3D animator;
 //std::vector<std::string> additionalAnimFiles;
 };

 struct SkyboxComponent {
 AssetID skyboxID{ EMPTY_ASSET };

 XPROPERTY_DEF(
 "SkyboxComponent", SkyboxComponent,
 obj_member<"SkyboxID", &SkyboxComponent::skyboxID>
 )
 };

 //helpful for encapsulating information about an entity
 //can be used for entity hierarchies (linked-list)
 struct InfoComponent {
 AssetID parent{ EMPTY_ASSET };
 std::string name{ "Entity" };
 AssetID uid{ RandomU64() };
 int32_t sortOrder{ 0 };

        XPROPERTY_DEF(
            "InfoComponent", InfoComponent,
            obj_member<"Parent", &InfoComponent::parent>,
            obj_member<"Name", &InfoComponent::name>,
            obj_member<"UID", &InfoComponent::uid>,
            obj_member<"SortOrder", &InfoComponent::sortOrder>
        )
    };
    BOOM_INLINE entt::entity FindEntityByName(entt::registry& reg, std::string_view name) {
        auto view = reg.view<const InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.name == name) return e;
        }
        return entt::null;
    }

    // ========== HIERARCHY HELPER FUNCTIONS ==========
    // NOTE: Parent relationships use UID (not entity ID) for serialization stability

    // Get parent entity by UID lookup (returns entt::null if no parent)
    BOOM_INLINE entt::entity GetParentEntity(entt::registry& reg, entt::entity entity) {
        if (!reg.valid(entity) || !reg.all_of<InfoComponent>(entity)) {
            return entt::null;
        }
        const auto& info = reg.get<InfoComponent>(entity);
        AssetID parentUID = info.parent;

        if (parentUID == EMPTY_ASSET || parentUID == 0) {
            return entt::null;
        }

        // Find entity with matching UID
        auto view = reg.view<InfoComponent>();
        for (auto e : view) {
            if (view.get<InfoComponent>(e).uid == parentUID) {
                // Found parent
                return e;
            }
        }

        // Parent UID is set but entity not found - this is a warning condition
        BOOM_WARN("[GetParentEntity] Entity '{}' has parent UID {} but no matching entity found!",
                 info.name, parentUID);
        return entt::null;
    }

    // Get all children of an entity (by UID)
    BOOM_INLINE std::vector<entt::entity> GetChildren(entt::registry& reg, entt::entity parent) {
        std::vector<entt::entity> children;
        if (!reg.valid(parent) || !reg.all_of<InfoComponent>(parent)) return children;

        AssetID parentUID = reg.get<InfoComponent>(parent).uid;

        struct ChildEntity {
            entt::entity entity;
            int32_t sortOrder;
            AssetID uid;
        };
        std::vector<ChildEntity> childrenSorted;

        auto view = reg.view<InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.parent == parentUID) {
                childrenSorted.push_back({e, info.sortOrder, info.uid});
            }
        }

        // Sort by sortOrder first, UID as tiebreaker for stable ordering
        std::sort(childrenSorted.begin(), childrenSorted.end(),
                 [](const ChildEntity& a, const ChildEntity& b) {
                     if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
                     return a.uid < b.uid;
                 });

        children.reserve(childrenSorted.size());
        for (const auto& child : childrenSorted) {
            children.push_back(child.entity);
        }

        return children;
    }

    // Get siblings of an entity (all entities sharing the same parent, including the entity itself)
    BOOM_INLINE std::vector<entt::entity> GetSiblings(entt::registry& reg, entt::entity entity) {
        if (!reg.valid(entity) || !reg.all_of<InfoComponent>(entity)) return {};

        AssetID parentUID = reg.get<InfoComponent>(entity).parent;

        struct SiblingEntry {
            entt::entity entity;
            int32_t sortOrder;
            AssetID uid;
        };
        std::vector<SiblingEntry> siblings;

        auto view = reg.view<InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.parent == parentUID) {
                siblings.push_back({e, info.sortOrder, info.uid});
            }
        }

        std::sort(siblings.begin(), siblings.end(),
                 [](const SiblingEntry& a, const SiblingEntry& b) {
                     if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
                     return a.uid < b.uid;
                 });

        std::vector<entt::entity> result;
        result.reserve(siblings.size());
        for (const auto& s : siblings) {
            result.push_back(s.entity);
        }
        return result;
    }

    // Reassign sortOrder values 0, 1, 2, ... to a list of entities
    BOOM_INLINE void ReorderSiblings(entt::registry& reg, const std::vector<entt::entity>& orderedEntities) {
        for (int32_t i = 0; i < static_cast<int32_t>(orderedEntities.size()); ++i) {
            if (reg.valid(orderedEntities[i]) && reg.all_of<InfoComponent>(orderedEntities[i])) {
                reg.get<InfoComponent>(orderedEntities[i]).sortOrder = i;
            }
        }
    }

    // Check if 'ancestor' is in the parent chain of 'entity' (prevents circular parenting)
    BOOM_INLINE bool HasAncestor(entt::registry& reg, entt::entity entity, entt::entity ancestor) {
        if (!reg.all_of<InfoComponent>(ancestor)) return false;

        AssetID ancestorUID = reg.get<InfoComponent>(ancestor).uid;
        entt::entity current = entity;
        int depth = 0;
        const int maxDepth = 1000; // Prevent infinite loops

        while (current != entt::null && depth < maxDepth) {
            if (reg.all_of<InfoComponent>(current)) {
                if (reg.get<InfoComponent>(current).uid == ancestorUID) {
                    return true;
                }
            }
            current = GetParentEntity(reg, current);
            depth++;
        }
        return false;
    }

    // Get world transform matrix (combines local transform with parent chain)
    BOOM_INLINE glm::mat4 GetWorldMatrix(entt::registry& reg, entt::entity entity) {
        if (!reg.valid(entity) || !reg.all_of<TransformComponent>(entity)) {
            return glm::mat4(1.0f);
        }

        const auto& transform = reg.get<TransformComponent>(entity);
        glm::mat4 localMatrix = transform.transform.Matrix();

        entt::entity parent = GetParentEntity(reg, entity);
        if (parent == entt::null) {
            return localMatrix;
        }

        // Recursively get parent's world matrix and combine
        glm::mat4 parentMatrix = GetWorldMatrix(reg, parent);
        return parentMatrix * localMatrix;
    }

    // Get world position from transform hierarchy
    BOOM_INLINE glm::vec3 GetWorldPosition(entt::registry& reg, entt::entity entity) {
        glm::mat4 worldMatrix = GetWorldMatrix(reg, entity);
        return glm::vec3(worldMatrix[3]);
    }

    // --- Utility: Decompose TRS matrix into translate / euler-deg / scale ---
    //     without normalizing away scale.
    BOOM_INLINE void DecomposeTRS(const glm::mat4& m,
        glm::vec3& outT,
        glm::vec3& outRDeg,
        glm::vec3& outS)
    {
        // Translation = 4th column
        outT = glm::vec3(m[3]);

        // Basis vectors (3x3 block columns)
        glm::vec3 col0 = glm::vec3(m[0]);
        glm::vec3 col1 = glm::vec3(m[1]);
        glm::vec3 col2 = glm::vec3(m[2]);

        // Scale = lengths of basis vectors
        outS.x = glm::length(col0);
        outS.y = glm::length(col1);
        outS.z = glm::length(col2);

        // Guard against zero scale
        glm::vec3 safeS = outS;
        if (safeS.x == 0.f) safeS.x = 1.f;
        if (safeS.y == 0.f) safeS.y = 1.f;
        if (safeS.z == 0.f) safeS.z = 1.f;

        // Remove scale from basis vectors to get rotation-only matrix
        glm::mat3 rotMat;
        rotMat[0] = col0 / safeS.x;
        rotMat[1] = col1 / safeS.y;
        rotMat[2] = col2 / safeS.z;

        glm::quat q = glm::quat_cast(rotMat);
        glm::vec3 eulerRad = glm::eulerAngles(q);
        outRDeg = glm::degrees(eulerRad);
    }



    // Helper to decompose world matrix back to transform (for gizmos)
    BOOM_INLINE void SetWorldMatrix(entt::registry& reg, entt::entity entity, const glm::mat4& worldMatrix) {
        if (!reg.valid(entity) || !reg.all_of<TransformComponent>(entity)) {
            return;
        }

        auto& transform = reg.get<TransformComponent>(entity);
        entt::entity parent = GetParentEntity(reg, entity);

        // Compute local = inverse(parentWorld) * world
        glm::mat4 localMatrix;
        if (parent == entt::null) {
            localMatrix = worldMatrix;
        }
        else {
            glm::mat4 parentMatrix = GetWorldMatrix(reg, parent);
            localMatrix = glm::inverse(parentMatrix) * worldMatrix;
        }

        glm::vec3 T, RDeg, S;
        DecomposeTRS(localMatrix, T, RDeg, S);

        transform.transform.translate = T;
        transform.transform.rotate = RDeg;
        transform.transform.scale = S;
    }




    // Set parent of an entity using UID (validates to prevent circular references)
// preserveWorldTransform: if true, adjusts local transform to keep same world position (Unity/Unreal behavior)
    BOOM_INLINE bool SetParent(entt::registry& reg,
        entt::entity entity,
        entt::entity newParent,
        bool preserveWorldTransform = true)
    {
        if (!reg.valid(entity) || !reg.all_of<InfoComponent>(entity)) {
            BOOM_WARN("[SetParent] Entity invalid or missing InfoComponent");
            return false;
        }

        // Prevent self-parenting
        if (entity == newParent) {
            BOOM_WARN("[SetParent] Cannot parent entity to itself");
            return false;
        }

        // Prevent circular parenting (entity can't be parent of its ancestor)
        if (newParent != entt::null && HasAncestor(reg, newParent, entity)) {
            BOOM_WARN("[SetParent] Circular reference prevented");
            return false;
        }

        auto& info = reg.get<InfoComponent>(entity);
        AssetID oldParent = info.parent;

        const bool hasTransform = reg.all_of<TransformComponent>(entity);

        // 1) Capture current WORLD matrix and original scale BEFORE changing parent
        glm::mat4 worldMatrix(1.0f);
        glm::vec3 originalLocalScale(1.0f);

        if (preserveWorldTransform && hasTransform) {
            auto& t = reg.get<TransformComponent>(entity);
            originalLocalScale = t.transform.scale;  // SAVE THIS FIRST!
            worldMatrix = GetWorldMatrix(reg, entity);

            BOOM_INFO("[SetParent] BEFORE - Entity '{}' local: pos({}, {}, {}), scale({}, {}, {})",
                info.name,
                t.transform.translate.x, t.transform.translate.y, t.transform.translate.z,
                t.transform.scale.x, t.transform.scale.y, t.transform.scale.z);

            glm::vec3 worldPos = GetWorldPosition(reg, entity);
            BOOM_INFO("[SetParent] World position: ({}, {}, {})", worldPos.x, worldPos.y, worldPos.z);
        }

        // 2) Update parent relationship (InfoComponent.parent stores UID)
        if (newParent == entt::null) {
            info.parent = EMPTY_ASSET;
            BOOM_INFO("[SetParent] Cleared parent: '{}' (UID:{}) is now root (was parent UID:{})",
                info.name, info.uid, oldParent);
        }
        else {
            if (!reg.all_of<InfoComponent>(newParent)) {
                BOOM_WARN("[SetParent] New parent missing InfoComponent");
                return false;
            }

            AssetID newParentUID = reg.get<InfoComponent>(newParent).uid;
            info.parent = newParentUID;

            BOOM_INFO("[SetParent] Set parent: '{}' (UID:{}) -> parent '{}' (UID:{})",
                info.name, info.uid,
                reg.get<InfoComponent>(newParent).name, newParentUID);
        }

        // 3) Recompute LOCAL transform so that WORLD stays the same
        if (preserveWorldTransform && hasTransform) {
            auto& t = reg.get<TransformComponent>(entity);

            // Extract the ACTUAL current world scale from the world matrix
            glm::vec3 worldT, worldRDeg, worldScale;
            DecomposeTRS(worldMatrix, worldT, worldRDeg, worldScale);

            glm::mat4 parentWorld(1.0f);
            glm::vec3 parentScale(1.0f);

            if (newParent != entt::null && reg.all_of<TransformComponent>(newParent)) {
                parentWorld = GetWorldMatrix(reg, newParent);

                // Extract parent's world scale
                glm::vec3 parentT, parentRDeg;
                DecomposeTRS(parentWorld, parentT, parentRDeg, parentScale);
            }

            glm::mat4 localMatrix = glm::inverse(parentWorld) * worldMatrix;

            glm::vec3 T, RDeg, S;
            DecomposeTRS(localMatrix, T, RDeg, S);

            t.transform.translate = T;
            t.transform.rotate = RDeg;

            // Adjust local scale to compensate for parent's scale
            // localScale = worldScale / parentScale (component-wise division)
            t.transform.scale.x = (parentScale.x != 0.0f) ? (worldScale.x / parentScale.x) : worldScale.x;
            t.transform.scale.y = (parentScale.y != 0.0f) ? (worldScale.y / parentScale.y) : worldScale.y;
            t.transform.scale.z = (parentScale.z != 0.0f) ? (worldScale.z / parentScale.z) : worldScale.z;

            BOOM_INFO("[SetParent] AFTER - local: pos({}, {}, {}), scale({}, {}, {})",
                T.x, T.y, T.z,
                t.transform.scale.x, t.transform.scale.y, t.transform.scale.z);
            BOOM_INFO("[SetParent] Parent scale: ({}, {}, {}), World scale preserved: ({}, {}, {})",
                parentScale.x, parentScale.y, parentScale.z,
                worldScale.x, worldScale.y, worldScale.z);
        }

        return true;
    }


    struct DirectLightComponent
    {
        BOOM_INLINE DirectLightComponent(const DirectLightComponent&) = default;
        BOOM_INLINE DirectLightComponent() = default;
        DirectionalLight light;

 XPROPERTY_DEF(
 "DirectLightComponent", DirectLightComponent,
 obj_member<"Light", &DirectLightComponent::light>
 )
 };

 struct PointLightComponent
 {
 BOOM_INLINE PointLightComponent(const PointLightComponent&) = default;
 BOOM_INLINE PointLightComponent() = default;
 PointLight light;

 XPROPERTY_DEF(
 "PointLightComponent", PointLightComponent,
 obj_member<"Light", &PointLightComponent::light>
 )
 };

 struct SpotLightComponent
 {
 BOOM_INLINE SpotLightComponent(const SpotLightComponent&) = default;
 BOOM_INLINE SpotLightComponent() = default;
 SpotLight light;

 XPROPERTY_DEF(
 "SpotLightComponent", SpotLightComponent,
 obj_member<"Light", &SpotLightComponent::light>
 )
 };

 // Chris I have no idea how your sound component works
 struct SoundComponent
 {
     struct Entry {
         std::string name;      // logical name ("bgm", "jump", etc.)
         // Backwards-compatible single file path
         std::string filePath;  // actual sound file path (legacy)
         // New: allow multiple alternate files for randomization
         std::vector<std::string> filePaths;

         // Core audio properties (Unity-style)
         bool loop = false;
         float volume = 1.0f;
         int priority = 128;           // 0-256, lower = higher priority (128 = default)
         float pitch = 1.0f;           // Playback speed/pitch (0.5 = half, 2.0 = double)
         float stereoPan = 0.0f;       // Stereo panning (-1.0 = left, 0 = center, 1.0 = right)
         float spatialBlend = 1.0f;    // 2D/3D blend (0.0 = fully 2D, 1.0 = fully 3D)
         bool mute = false;            // Whether this sound is muted

         bool playOnStart = false;

         // New trigger options
         int triggerKey = -1; // GLFW key code to trigger this sound (-1 = none)
         bool playOnMove = false; // play while moving (uses rigidbody velocity)
         float moveThreshold = 0.1f; // minimum speed to consider "moving"
         float repeatInterval = 0.5f; // minimum seconds between automatic retriggers

         // Animation trigger name (e.g. "Footstep")
         std::string animTrigger;

         // 3D Audio settings
         float minDistance = 1.0f;   // Distance at which sound is at full volume
         float maxDistance = 15.0f;  // Distance at which sound is silent

         void serialize(nlohmann::json& j) const {
             j["name"] = name;
             // If filePaths present, write as array; otherwise write legacy filePath
             if (!filePaths.empty()) {
                 j["filePaths"] = filePaths;
             }
             else {
                 j["filePath"] = filePath;
             }
             j["loop"] = loop;
             j["volume"] = volume;
             j["priority"] = priority;
             j["pitch"] = pitch;
             j["stereoPan"] = stereoPan;
             j["spatialBlend"] = spatialBlend;
             j["mute"] = mute;
             j["playOnStart"] = playOnStart;
             j["triggerKey"] = triggerKey;
             j["playOnMove"] = playOnMove;
             j["moveThreshold"] = moveThreshold;
             j["repeatInterval"] = repeatInterval;
             if (!animTrigger.empty()) j["animTrigger"] = animTrigger;
             j["minDistance"] = minDistance;
             j["maxDistance"] = maxDistance;
         }
         void deserialize(const nlohmann::json& j) {
             if (j.contains("name")) j.at("name").get_to(name);
             // Prefer filePaths if present
             if (j.contains("filePaths") && j.at("filePaths").is_array()) {
                 filePaths.clear();
                 for (const auto& fp : j.at("filePaths")) {
                     filePaths.push_back(fp.get<std::string>());
                 }
                 // keep filePath for legacy access (first element)
                 filePath = filePaths.empty() ? std::string() : filePaths.front();
             }
             else if (j.contains("filePath")) {
                 j.at("filePath").get_to(filePath);
                 filePaths.clear();
                 if (!filePath.empty()) filePaths.push_back(filePath);
             }
             if (j.contains("loop")) j.at("loop").get_to(loop);
             if (j.contains("volume")) j.at("volume").get_to(volume);
             if (j.contains("priority")) j.at("priority").get_to(priority);
             if (j.contains("pitch")) j.at("pitch").get_to(pitch);
             if (j.contains("stereoPan")) j.at("stereoPan").get_to(stereoPan);
             if (j.contains("spatialBlend")) j.at("spatialBlend").get_to(spatialBlend);
             if (j.contains("mute")) j.at("mute").get_to(mute);
             if (j.contains("playOnStart")) j.at("playOnStart").get_to(playOnStart);
             if (j.contains("triggerKey")) j.at("triggerKey").get_to(triggerKey);
             if (j.contains("playOnMove")) j.at("playOnMove").get_to(playOnMove);
             if (j.contains("moveThreshold")) j.at("moveThreshold").get_to(moveThreshold);
             if (j.contains("repeatInterval")) j.at("repeatInterval").get_to(repeatInterval);
             if (j.contains("animTrigger")) j.at("animTrigger").get_to(animTrigger);
             if (j.contains("minDistance")) j.at("minDistance").get_to(minDistance);
             if (j.contains("maxDistance")) j.at("maxDistance").get_to(maxDistance);
         }
     };

     std::vector<Entry> entries; // timo was here

     void serialize(nlohmann::json& j) const {
         j = nlohmann::json::array();
         for (const auto& e : entries) {
             nlohmann::json ej;
             e.serialize(ej);
             j.push_back(ej);
         }
     }
     void deserialize(const nlohmann::json& j) {
         entries.clear();
         if (j.is_array()) {
             for (const auto& ej : j) {
                 Entry e;
                 e.deserialize(ej);
                 entries.push_back(std::move(e));
             }
         }
     }
 };

 struct ScriptComponent
 {
 // Managed type name in C# (e.g., "PhysicsDropDemo" or "MyGame.PlayerController")
 std::string TypeName;

 // Runtime handle returned by script_create_instance(...).
 // Do NOT serialize this; it’s valid only while the game is running.
 uint64_t InstanceId = 0;

 // Allow toggling without removing the component
 bool Enabled = true;

 nlohmann::json Params = nlohmann::json::object();

 // ---- Serialization (save only authoring data) ----
 void serialize(nlohmann::json& j) const {
 j["TypeName"] = TypeName;
 j["Enabled"] = Enabled;
 if (!Params.is_null() && !Params.empty())
 j["Params"] = Params;

 // NOTE: InstanceId is intentionally NOT serialized (runtime only)
 }

 void deserialize(const nlohmann::json& j) {
 if (j.contains("TypeName")) j.at("TypeName").get_to(TypeName);
 if (j.contains("Enabled"))  j.at("Enabled").get_to(Enabled);
 if (j.contains("Params"))   j.at("Params").get_to(Params);

 // Ensure runtime handle starts cleared when loading a scene
 InstanceId = 0;
 }
 };

 struct ThirdPersonCameraComponent {
AssetID   targetUID = 0;                     // The UID of the target entity
glm::vec3 offset = glm::vec3(0.0f, 2.0f, 0.0f);
float     currentDistance = 2.0f;
float     minDistance = 1.0f;
float     maxDistance = 5.0f;
float     currentYaw = 0.0f;
float     currentPitch = 20.0f;
float     mouseSensitivity = 1.0f;
float     scrollSensitivity = 1.0f;

// Runtime-only camera effects (not serialized)
float     shakeIntensity = 0.0f;   // Max positional shake offset in world units
float     shakeDuration  = 0.0f;   // Total duration of current shake
float     shakeTimer     = 0.0f;   // Remaining time
float     shakePhase     = 0.0f;   // Continuously accumulated phase (never reset by scripts)

// Add this back in
XPROPERTY_DEF(
"ThirdPersonCameraComponent", ThirdPersonCameraComponent,

// NEW: persist/browse the target entity UID
obj_member<"Target UID", &ThirdPersonCameraComponent::targetUID>,

obj_member<"Offset", &ThirdPersonCameraComponent::offset>,
obj_member<"Current Distance", &ThirdPersonCameraComponent::currentDistance>,
obj_member<"Min Distance", &ThirdPersonCameraComponent::minDistance>,
obj_member<"Max Distance", &ThirdPersonCameraComponent::maxDistance>,
obj_member<"Current Yaw", &ThirdPersonCameraComponent::currentYaw>,
obj_member<"Current Pitch", &ThirdPersonCameraComponent::currentPitch>,
obj_member<"Mouse Sensitivity", &ThirdPersonCameraComponent::mouseSensitivity>,
obj_member<"Scroll Sensitivity", &ThirdPersonCameraComponent::scrollSensitivity>
)
 };
 struct NavAgentComponent {
 glm::vec3 target{ 0.f };
 std::vector<glm::vec3> path; // straight path
 int   waypoint = 0;
 float speed = 2.5f;  // m/s
 float arrive = 0.15f; // meters
 glm::vec3 velocity = glm::vec3(0.f);
 bool  active = true;
 bool  dirty = false; // set true when target changes
 std::string followName;
 entt::entity follow = entt::null; //this is player entity to follow
 float repathCooldown = 0.25f;     // seconds between path rebuilds
 float retargetDist = 0.5f;      // re-path if player moved this far
 float repathTimer = 0.f;       // internal timer
 XPROPERTY_DEF
 ("NavAgentComponent", NavAgentComponent
 , obj_member<"Target", &NavAgentComponent::target>
 , obj_member<"Speed", &NavAgentComponent::speed>
 , obj_member<"Velocity", &NavAgentComponent::velocity>
 , obj_member<"ArriveRadius", &NavAgentComponent::arrive>
 , obj_member<"Active", &NavAgentComponent::active>
 , obj_member<"RepathCooldown", &NavAgentComponent::repathCooldown>
 , obj_member<"RetargetDistance", &NavAgentComponent::retargetDist>
 )
 };
 struct AIComponent {
 enum class AIMode : int { Auto = 0, Idle = 1, Patrol = 2, Seek = 3 };

 bool active = true;
 AIMode mode = AIMode::Auto;   // exposed in Inspector
 AIMode lastMode = AIMode::Auto;
 float detectRadius = 8.0f;    // start seeking when within this distance
 float loseRadius = 12.0f;   // stop seeking when beyond this distance
 float idleWait = 1.0f;    // wait at patrol points
 float idleTimer = 0.0f;
 std::string playerName = "Samurai";   // find by name instead of PlayerTag
 entt::entity player = entt::null;     // cached after first successful lookup
 // Patrol
 std::vector<glm::vec3> patrolPoints;
 std::int32_t patrolIndex = 0;


 // BT root
 BTNodePtr root;

 XPROPERTY_DEF
 ("AIComponent", AIComponent
 , obj_member<"Active", &AIComponent::active>
 , obj_member<"DetectRadius", &AIComponent::detectRadius>
 , obj_member<"LoseRadius", &AIComponent::loseRadius>
 , obj_member<"IdleWait", &AIComponent::idleWait>
 , obj_member<"IdleTimer", &AIComponent::idleTimer>    // include if you want to see the live timer
 , obj_member<"PlayerName", &AIComponent::playerName>

 , obj_member<"PatrolIndex", &AIComponent::patrolIndex>
 )
 };

 struct SpriteComponent {
 AssetID textureID{ EMPTY_ASSET };
 glm::vec4 color{ 1.0f };
 bool renderAs3D{ false };  // false = 2D UI overlay, true = 3D world space (attachable to objects)

 XPROPERTY_DEF(
 "SpriteComponent", SpriteComponent,
 obj_member<"textureID", &SpriteComponent::textureID>,
 obj_member<"color", &SpriteComponent::color>,
 obj_member<"renderAs3D", &SpriteComponent::renderAs3D>
 )
 };

 // Text Component - Unity-like text rendering using FontManager
 struct TextComponent {
     BOOM_INLINE TextComponent(const TextComponent&) = default;
     BOOM_INLINE TextComponent() = default;

     std::string text = "New Text";               // The actual text to display
     std::string fontName = "Roboto-Regular";     // Font to use (must be loaded in FontManager)
     glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };  // RGBA color
     float scale = 1.0f;                          // Size multiplier
     glm::vec2 screenPosition{ 100.0f, 100.0f };  // Screen space position (pixels from bottom-left)
     bool renderAs3D = false;                     // false = 2D overlay, true = 3D world space
     bool billboardMode = true;                   // true = always face camera (billboard), false = fixed world rotation

     // Text alignment (for future implementation)
     enum class Alignment : int32_t {
         Left = 0,
         Center = 1,
         Right = 2
     };
     Alignment alignment = Alignment::Left;

     XPROPERTY_DEF(
         "TextComponent", TextComponent,
         obj_member<"text", &TextComponent::text>,
         obj_member<"fontName", &TextComponent::fontName>,
         obj_member<"color", &TextComponent::color>,
         obj_member<"scale", &TextComponent::scale>,
         obj_member<"screenPosition", &TextComponent::screenPosition>,
         obj_member<"renderAs3D", &TextComponent::renderAs3D>,
         obj_member<"billboardMode", &TextComponent::billboardMode>,
         obj_member<"alignment", &TextComponent::alignment,
             member_enum_value<"Left", TextComponent::Alignment::Left>,
             member_enum_value<"Center", TextComponent::Alignment::Center>,
             member_enum_value<"Right", TextComponent::Alignment::Right>
         >
         ) };

enum class MenuType { Pause = 0, Death = 1, Settings = 2, Main = 3, End = 4, PopUp = 5, Inventory = 6 };
struct MenuComponent {
     BOOM_INLINE MenuComponent(const MenuComponent&) = default;
     BOOM_INLINE MenuComponent() = default;

     MenuType menuType = MenuType::Pause;

     XPROPERTY_DEF("MenuComponent", MenuComponent)
 };

    struct SceneNavmeshComponent {
        std::string navmeshFile;   // e.g. "Resources/NavData/level1.bin"
        float ambientStrength = 0.5f;  // Default ambient light strength for the scene

        // Bloom settings
        bool bloomEnabled = false;
        float bloomIntensity = 1.0f;
        float bloomThreshold = 1.0f;  // Brightness threshold for bloom extraction
        int bloomIterations = 10;     // Number of blur passes (default 10)
        float pointLightBloomMultiplier = 1.0f;  // Global multiplier for point light bloom contribution

        // Volumetric fog settings
        bool fogEnabled = false;
        glm::vec3 fogColor = glm::vec3(0.5f, 0.6f, 0.7f);
        float fogDensity = 0.01f;
        float fogHeightFalloff = 0.5f;  // How fast fog thins with height (larger = thinner at height)
        float fogHeight = 0.0f;         // World-space Y below which fog is thickest

        // Tone mapping settings
        float tonemapExposure = 1.0f;
        float tonemapGamma = 2.2f;
        glm::vec3 tonemapWarmTint = glm::vec3(1.08f, 0.98f, 0.82f);

        XPROPERTY_DEF("SceneNavmeshComponent", SceneNavmeshComponent,
            obj_member<"NavmeshFile", &SceneNavmeshComponent::navmeshFile>,
            obj_member<"AmbientStrength", &SceneNavmeshComponent::ambientStrength>,
            obj_member<"BloomEnabled", &SceneNavmeshComponent::bloomEnabled>,
            obj_member<"BloomIntensity", &SceneNavmeshComponent::bloomIntensity>,
            obj_member<"BloomThreshold", &SceneNavmeshComponent::bloomThreshold>,
            obj_member<"BloomIterations", &SceneNavmeshComponent::bloomIterations>,
            obj_member<"PointLightBloomMultiplier", &SceneNavmeshComponent::pointLightBloomMultiplier>,
            obj_member<"FogEnabled", &SceneNavmeshComponent::fogEnabled>,
            obj_member<"FogColor", &SceneNavmeshComponent::fogColor>,
            obj_member<"FogDensity", &SceneNavmeshComponent::fogDensity>,
            obj_member<"FogHeightFalloff", &SceneNavmeshComponent::fogHeightFalloff>,
            obj_member<"FogHeight", &SceneNavmeshComponent::fogHeight>,
            obj_member<"TonemapExposure", &SceneNavmeshComponent::tonemapExposure>,
            obj_member<"TonemapGamma", &SceneNavmeshComponent::tonemapGamma>,
            obj_member<"TonemapWarmTint", &SceneNavmeshComponent::tonemapWarmTint>)
    };

    struct DeactivatedComponent {
        BOOM_INLINE DeactivatedComponent(const DeactivatedComponent&) = default;
        BOOM_INLINE DeactivatedComponent() = default;

        bool isTag = true;

        XPROPERTY_DEF(
            "DeactivatedComponent", DeactivatedComponent
        )
    };

    // Video Component - for playing MPEG1 videos on entities
    struct VideoComponent {
        BOOM_INLINE VideoComponent(const VideoComponent&) = default;
        BOOM_INLINE VideoComponent() = default;

        // Video file path (relative to Resources/Videos/)
        std::string videoPath;

        // Playback settings
        bool playOnStart = false;
        bool loop = false;
        float volume = 1.0f;
        float playbackSpeed = 1.0f;

        // Display settings
        glm::vec4 tintColor = glm::vec4(1.0f);  // Tint/multiply color
        float brightness = 1.0f;                 // Brightness multiplier (0.0 = black, 1.0 = normal, >1.0 = overbright)
        bool renderAs3D = false;                 // true = 3D quad in world, false = 2D UI overlay

        bool removeBlackBackground = false;

        // Runtime state (not serialized)
        bool isPlaying = false;
        double currentTime = 0.0;

        // Serialization
        void serialize(nlohmann::json& j) const {
            j["videoPath"] = videoPath;
            j["playOnStart"] = playOnStart;
            j["loop"] = loop;
            j["volume"] = volume;
            j["playbackSpeed"] = playbackSpeed;
            j["tintColor"] = { tintColor.r, tintColor.g, tintColor.b, tintColor.a };
            j["brightness"] = brightness;
            j["renderAs3D"] = renderAs3D;
            j["removeBlackBackground"] = removeBlackBackground;
        }

        void deserialize(const nlohmann::json& j) {
            if (j.contains("videoPath")) j.at("videoPath").get_to(videoPath);
            if (j.contains("playOnStart")) j.at("playOnStart").get_to(playOnStart);
            if (j.contains("loop")) j.at("loop").get_to(loop);
            if (j.contains("volume")) j.at("volume").get_to(volume);
            if (j.contains("playbackSpeed")) j.at("playbackSpeed").get_to(playbackSpeed);
            if (j.contains("tintColor") && j.at("tintColor").is_array() && j.at("tintColor").size() == 4) {
                tintColor.r = j.at("tintColor")[0];
                tintColor.g = j.at("tintColor")[1];
                tintColor.b = j.at("tintColor")[2];
                tintColor.a = j.at("tintColor")[3];
            }
            if (j.contains("brightness")) j.at("brightness").get_to(brightness);
            if (j.contains("renderAs3D")) j.at("renderAs3D").get_to(renderAs3D);
            if (j.contains("removeBlackBackground")) j.at("removeBlackBackground").get_to(removeBlackBackground);

            // Reset runtime state
            isPlaying = false;
            currentTime = 0.0;
        }

        XPROPERTY_DEF(
            "VideoComponent", VideoComponent,
            obj_member<"VideoPath", &VideoComponent::videoPath>,
            obj_member<"PlayOnStart", &VideoComponent::playOnStart>,
            obj_member<"Loop", &VideoComponent::loop>,
            obj_member<"Volume", &VideoComponent::volume>,
            obj_member<"PlaybackSpeed", &VideoComponent::playbackSpeed>,
            obj_member<"TintColor", &VideoComponent::tintColor>,
            obj_member<"Brightness", &VideoComponent::brightness>,
            obj_member<"RenderAs3D", &VideoComponent::renderAs3D>,
            obj_member<"RemoveBlackBackground", &VideoComponent::removeBlackBackground>
            )
    };


    // Character Controller Component (PxController wrapper)
    struct CharacterControllerComponent {
        BOOM_INLINE CharacterControllerComponent(const CharacterControllerComponent&) = default;
        BOOM_INLINE CharacterControllerComponent() = default;

        // Configuration (serialized to YAML)
        float radius = 0.5f;
        float height = 2.0f;
        float stepOffset = 0.3f;
        float contactOffset = 0.1f;
        float slopeLimit = 45.0f;
        glm::vec3 localOffset = glm::vec3(0.0f); // NEW: Local offset from entity transform
        bool isCreated = false;

        XPROPERTY_DEF(
            "CharacterControllerComponent", CharacterControllerComponent,
            obj_member<"Radius", &CharacterControllerComponent::radius>,
            obj_member<"Height", &CharacterControllerComponent::height>,
            obj_member<"StepOffset", &CharacterControllerComponent::stepOffset>,
            obj_member<"ContactOffset", &CharacterControllerComponent::contactOffset>,
            obj_member<"LocalOffset", &CharacterControllerComponent::localOffset>,
            obj_member<"SlopeLimit", &CharacterControllerComponent::slopeLimit>
        )
    };

    // ─── Particle Emitter Component ─────────────────────────────────────
    struct ParticleEmitterComponent {
        BOOM_INLINE ParticleEmitterComponent(const ParticleEmitterComponent&) = default;
        BOOM_INLINE ParticleEmitterComponent() = default;

        // Emission
        float emissionRate    = 20.0f;   // particles per second
        int   maxParticles    = 500;     // pool size
        bool  looping         = true;
        float duration        = 5.0f;    // emitter lifetime if not looping (seconds)
        bool  playOnStart     = true;

        // Particle lifetime
        float lifetimeMin     = 1.0f;    // seconds
        float lifetimeMax     = 3.0f;

        // Initial speed
        float speedMin        = 1.0f;
        float speedMax        = 3.0f;

        // Spawn shape: 0 = point, 1 = sphere, 2 = cone, 3 = box
        int   shapeType       = 0;
        float shapeRadius     = 1.0f;    // sphere/cone radius
        float shapeAngle      = 25.0f;   // cone half-angle (degrees)
        glm::vec3 shapeSize   = glm::vec3(1.0f); // box half-extents

        // Direction (for cone/directional emission; local space)
        glm::vec3 direction   = glm::vec3(0.0f, 1.0f, 0.0f);

        // Gravity multiplier (applied as world-Y acceleration)
        float gravity         = -9.81f;

        // Size over lifetime
        float startSizeMin    = 0.1f;
        float startSizeMax    = 0.3f;
        float endSize         = 0.0f;    // size at death (lerped)

        // Color over lifetime
        glm::vec4 startColor  = glm::vec4(1.0f, 0.9f, 0.3f, 1.0f);  // warm yellow
        glm::vec4 endColor    = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);  // fade-out red

        // Texture (AssetID for sprite sheet; 0 = default white)
        AssetID textureID     = EMPTY_ASSET;

        // Billboard mode: true = always face camera
        bool billboard        = true;

        // Additive blending (fire/sparks) vs alpha blending (smoke/dust)
        bool additiveBlend    = false;

        // Runtime state (not serialized)
        bool  isPlaying       = false;
        float emitterTimer    = 0.0f;   // time since emitter started
        float spawnAccum      = 0.0f;   // fractional particle accumulator

        XPROPERTY_DEF(
            "ParticleEmitterComponent", ParticleEmitterComponent,
            obj_member<"EmissionRate",  &ParticleEmitterComponent::emissionRate>,
            obj_member<"MaxParticles",  &ParticleEmitterComponent::maxParticles>,
            obj_member<"Looping",       &ParticleEmitterComponent::looping>,
            obj_member<"Duration",      &ParticleEmitterComponent::duration>,
            obj_member<"PlayOnStart",   &ParticleEmitterComponent::playOnStart>,
            obj_member<"LifetimeMin",   &ParticleEmitterComponent::lifetimeMin>,
            obj_member<"LifetimeMax",   &ParticleEmitterComponent::lifetimeMax>,
            obj_member<"SpeedMin",      &ParticleEmitterComponent::speedMin>,
            obj_member<"SpeedMax",      &ParticleEmitterComponent::speedMax>,
            obj_member<"ShapeType",     &ParticleEmitterComponent::shapeType>,
            obj_member<"ShapeRadius",   &ParticleEmitterComponent::shapeRadius>,
            obj_member<"ShapeAngle",    &ParticleEmitterComponent::shapeAngle>,
            obj_member<"ShapeSize",     &ParticleEmitterComponent::shapeSize>,
            obj_member<"Direction",     &ParticleEmitterComponent::direction>,
            obj_member<"Gravity",       &ParticleEmitterComponent::gravity>,
            obj_member<"StartSizeMin",  &ParticleEmitterComponent::startSizeMin>,
            obj_member<"StartSizeMax",  &ParticleEmitterComponent::startSizeMax>,
            obj_member<"EndSize",       &ParticleEmitterComponent::endSize>,
            obj_member<"StartColor",    &ParticleEmitterComponent::startColor>,
            obj_member<"EndColor",      &ParticleEmitterComponent::endColor>,
            obj_member<"TextureID",     &ParticleEmitterComponent::textureID>,
            obj_member<"Billboard",     &ParticleEmitterComponent::billboard>,
            obj_member<"AdditiveBlend", &ParticleEmitterComponent::additiveBlend>
        )
    };

    struct Entity
    {
        BOOM_INLINE Entity(EntityRegistry* registry, EntityID entity) :
            m_Registry(registry), m_EnttID(entity)
        {
        }
       
        BOOM_INLINE Entity(EntityRegistry* registry) :
            m_Registry(registry)
        {
            m_EnttID = m_Registry->create();
        }

 BOOM_INLINE virtual ~Entity() = default;
 BOOM_INLINE Entity() = default;

 BOOM_INLINE operator EntityID ()
 {
 return m_EnttID;
 }

 BOOM_INLINE operator bool()
 {
 return m_Registry != nullptr &&
 m_Registry->valid(m_EnttID);
 }

 BOOM_INLINE EntityID ID()
 {
 return m_EnttID;
 }

 template<typename T, typename... Args>
 BOOM_INLINE T& Attach(Args&&... args)
 {
 return m_Registry->get_or_emplace<T>(m_EnttID, std::forward<Args>(args)...);
 }

 template<typename T>
 BOOM_INLINE void Detach()
 {
 m_Registry->remove<T>(m_EnttID);
 }

 BOOM_INLINE void Destroy()
 {
 if (m_Registry)
 {
 m_Registry->destroy(m_EnttID);
 }
 }

 template<typename T>
 BOOM_INLINE bool Has() const
 {
 return m_Registry != nullptr &&
 m_Registry->all_of<T>(m_EnttID);
 }

 template<typename T>
 BOOM_INLINE T& Get()
 {
 return m_Registry->get<T>(m_EnttID);
 }

 protected:
 EntityRegistry* m_Registry = nullptr;
 EntityID m_EnttID = NENTT;
 };

    // ========== ENTITY MANIPULATION FUNCTIONS ==========
    // NOTE: These are placed at the end of the file after all component declarations

    // Delete entity and all its children recursively (with physics cleanup)
    BOOM_INLINE void DeleteEntityRecursive(entt::registry& reg, entt::entity entity, void* physicsCtx = nullptr) {
        if (!reg.valid(entity)) return;

        // Get entity name and UID for logging and cleanup
        std::string entityName = "Unknown";
        AssetID deletedUID = 0;
        if (reg.all_of<InfoComponent>(entity)) {
            const auto& info = reg.get<InfoComponent>(entity);
            entityName = info.name;
            deletedUID = info.uid;
        }

        // First, delete all children recursively
        auto children = GetChildren(reg, entity);
        for (entt::entity child : children) {
            DeleteEntityRecursive(reg, child, physicsCtx);
        }

        // Clean up physics if context provided
        if (physicsCtx != nullptr) {
            // Cast to PhysicsContext and remove rigid body
            // Note: PhysicsContext is forward-declared, so we need to include it where this is called
            // For now, just log
            BOOM_INFO("[DeleteEntity] Physics cleanup for '{}'", entityName);
        }

        // Clean up orphaned entities: find any entities that reference this entity as parent
        // and clear their parent reference (make them root entities)
        if (deletedUID != 0 && deletedUID != EMPTY_ASSET) {
            auto view = reg.view<InfoComponent>();
            for (auto e : view) {
                if (e == entity) continue; // Skip the entity being deleted
                auto& childInfo = view.get<InfoComponent>(e);
                if (childInfo.parent == deletedUID) {
                    childInfo.parent = EMPTY_ASSET; // Orphan becomes root
                    BOOM_INFO("[DeleteEntity] Orphaned '{}' (parent '{}' was deleted, now root)",
                             childInfo.name, entityName);
                }
            }
        }

        // Destroy the entity
        BOOM_INFO("[DeleteEntity] Destroying entity '{}'", entityName);
        reg.destroy(entity);
    }

    // Duplicate entity with all components (optionally with children)
    BOOM_INLINE entt::entity DuplicateEntity(entt::registry& reg, entt::entity source, bool duplicateChildren = true) {
        if (!reg.valid(source)) {
            BOOM_WARN("[DuplicateEntity] Source entity is invalid");
            return entt::null;
        }

        // Create new entity
        entt::entity duplicate = reg.create();

        // Copy InfoComponent with new UID and modified name
        if (reg.all_of<InfoComponent>(source)) {
            auto& srcInfo = reg.get<InfoComponent>(source);
            auto& dstInfo = reg.emplace<InfoComponent>(duplicate);
            dstInfo.name = srcInfo.name + " (Copy)";
            dstInfo.uid = RandomU64(); // New unique ID
            dstInfo.parent = srcInfo.parent; // Keep same parent
        }

        // Copy TransformComponent
        if (reg.all_of<TransformComponent>(source)) {
            auto& srcTrans = reg.get<TransformComponent>(source);
            auto& dstTrans = reg.emplace<TransformComponent>(duplicate);
            dstTrans = srcTrans; // Copy all transform data
        }

        // Copy CameraComponent
        if (reg.all_of<CameraComponent>(source)) {
            auto& srcCam = reg.get<CameraComponent>(source);
            reg.emplace<CameraComponent>(duplicate, srcCam);
        }

        // Copy ModelComponent
        if (reg.all_of<ModelComponent>(source)) {
            auto& srcModel = reg.get<ModelComponent>(source);
            auto& dstModel = reg.emplace<ModelComponent>(duplicate);
            dstModel.modelID = srcModel.modelID;
            dstModel.materialID = srcModel.materialID;
        }

        // Copy AnimatorComponent
        if (reg.all_of<AnimatorComponent>(source)) {
            auto& srcAnim = reg.get<AnimatorComponent>(source);
            auto& dstAnim = reg.emplace<AnimatorComponent>(duplicate);
            dstAnim.animator = srcAnim.animator; // Copy animator data
        }

        // Copy RigidBodyComponent (but NOT the physics actor - that needs special handling)
        if (reg.all_of<RigidBodyComponent>(source)) {
            auto& srcRB = reg.get<RigidBodyComponent>(source);
            auto& dstRB = reg.emplace<RigidBodyComponent>(duplicate);
            dstRB = srcRB;
            dstRB.RigidBody.actor = nullptr;
            // Note: Physics actor will be created by physics system on next update
        }

        // Copy ColliderComponent
        if (reg.all_of<ColliderComponent>(source)) {
            auto& srcCol = reg.get<ColliderComponent>(source);
            auto& dstCol = reg.emplace<ColliderComponent>(duplicate, srcCol);
            dstCol.Collider.Shape = nullptr;
            dstCol.Collider.material = nullptr;
            dstCol.Collider.actor = nullptr;
        }

        // Copy Light Components
        if (reg.all_of<DirectLightComponent>(source)) {
            auto& srcLight = reg.get<DirectLightComponent>(source);
            reg.emplace<DirectLightComponent>(duplicate, srcLight);
        }
        if (reg.all_of<PointLightComponent>(source)) {
            auto& srcLight = reg.get<PointLightComponent>(source);
            reg.emplace<PointLightComponent>(duplicate, srcLight);
        }
        if (reg.all_of<SpotLightComponent>(source)) {
            auto& srcLight = reg.get<SpotLightComponent>(source);
            reg.emplace<SpotLightComponent>(duplicate, srcLight);
        }

        // Copy SoundComponent
        if (reg.all_of<SoundComponent>(source)) {
            auto& srcSound = reg.get<SoundComponent>(source);
            auto& dstSound = reg.emplace<SoundComponent>(duplicate);
            dstSound.entries = srcSound.entries;
        }

        // Copy ScriptComponent
        if (reg.all_of<ScriptComponent>(source)) {
            auto& srcScript = reg.get<ScriptComponent>(source);
            auto& dstScript = reg.emplace<ScriptComponent>(duplicate);
            dstScript.TypeName = srcScript.TypeName;
            dstScript.Enabled = srcScript.Enabled;
            // Note: InstanceId will be set by scripting system
        }

        // Copy NavAgentComponent
        if (reg.all_of<NavAgentComponent>(source)) {
            auto& srcNav = reg.get<NavAgentComponent>(source);
            reg.emplace<NavAgentComponent>(duplicate, srcNav);
        }

        // Copy AIComponent
        if (reg.all_of<AIComponent>(source)) {
            auto& srcAI = reg.get<AIComponent>(source);
            auto& dstAI = reg.emplace<AIComponent>(duplicate);
            dstAI.detectRadius = srcAI.detectRadius;
            dstAI.loseRadius = srcAI.loseRadius;
            dstAI.idleWait = srcAI.idleWait;
            dstAI.playerName = srcAI.playerName;
            dstAI.patrolPoints = srcAI.patrolPoints;
            // Note: Don't copy runtime state (idleTimer, patrolIndex, etc.)
        }

        // Copy SpriteComponent
        if (reg.all_of<SpriteComponent>(source)) {
            auto& srcSprite = reg.get<SpriteComponent>(source);
            reg.emplace<SpriteComponent>(duplicate, srcSprite);
        }

        // Copy ThirdPersonCameraComponent
        if (reg.all_of<ThirdPersonCameraComponent>(source)) {
            auto& srcCam = reg.get<ThirdPersonCameraComponent>(source);
            reg.emplace<ThirdPersonCameraComponent>(duplicate, srcCam);
        }

        // Copy SkyboxComponent
        if (reg.all_of<SkyboxComponent>(source)) {
            auto& srcSky = reg.get<SkyboxComponent>(source);
            reg.emplace<SkyboxComponent>(duplicate, srcSky);
        }

        // Copy Menu Component
        if (reg.all_of<MenuComponent>(source)) {
            reg.emplace<MenuComponent>(duplicate);
        }

        // Copy DeactivatedComponent
        if (reg.all_of<DeactivatedComponent>(source)) {
            reg.emplace<DeactivatedComponent>(duplicate);
        }

        // Copy VideoComponent
        if (reg.all_of<VideoComponent>(source)) {
            auto& srcVideo = reg.get<VideoComponent>(source);
            auto& dstVideo = reg.emplace<VideoComponent>(duplicate);
            dstVideo.videoPath = srcVideo.videoPath;
            dstVideo.playOnStart = srcVideo.playOnStart;
            dstVideo.loop = srcVideo.loop;
            dstVideo.volume = srcVideo.volume;
            dstVideo.playbackSpeed = srcVideo.playbackSpeed;
            dstVideo.tintColor = srcVideo.tintColor;
            dstVideo.renderAs3D = srcVideo.renderAs3D;
            // Runtime state is not copied (starts fresh)
        }

        BOOM_INFO("[DuplicateEntity] Duplicated '{}' -> '{}'",
                 reg.get<InfoComponent>(source).name,
                 reg.get<InfoComponent>(duplicate).name);

        // Recursively duplicate children if requested
        if (duplicateChildren) {
            auto children = GetChildren(reg, source);
            AssetID duplicateUID = reg.get<InfoComponent>(duplicate).uid;

            for (entt::entity child : children) {
                entt::entity childDuplicate = DuplicateEntity(reg, child, true);
                if (reg.valid(childDuplicate) && reg.all_of<InfoComponent>(childDuplicate)) {
                    // Set duplicated child's parent to the duplicated parent
                    reg.get<InfoComponent>(childDuplicate).parent = duplicateUID;
                }
            }
        }

        return duplicate;
    }
    

    BOOM_INLINE entt::entity GetOrCreateSceneSettings(entt::registry& reg)
    {
        auto view = reg.view<SceneNavmeshComponent>();
        if (!view.empty())
        {
            return *view.begin();
        }

        // Otherwise create + attach component
        auto e = reg.create();
        reg.emplace<SceneNavmeshComponent>(e);
        return e;
    }
    BOOM_INLINE entt::entity TryGetSceneSettings(entt::registry& reg)
    {
        auto view = reg.view<SceneNavmeshComponent>();
        if (!view.empty())
            return *view.begin();
        return entt::null;
    }
}
