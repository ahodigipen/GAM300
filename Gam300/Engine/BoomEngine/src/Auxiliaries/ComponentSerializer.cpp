#include "Core.h"
#include "Auxiliaries\SerializationRegistry.h"
#include "ECS/ECS.hpp"

namespace Boom
{
    /**
     * @brief Generic component serializer using xproperty
     * Works for ANY component that has XPROPERTY_DEF defined
     */
    template<typename T>
    void RegisterPropertyComponent(const char* componentName)
    {
        auto& registry = SerializationRegistry::Instance();
        std::string name(componentName);

        registry.RegisterComponentSerializer(
            name,
            // ===== SERIALIZE =====
            [name](YAML::Emitter& e, EntityRegistry& scene, EntityID entity) {
                if (scene.all_of<T>(entity)) {
                    auto& comp = scene.get<T>(entity);

                    BOOM_INFO("[PropertySerializer] Serializing {} for entity {}", name, (uint32_t)entity);  // ADD THIS

                    e << YAML::Key << name << YAML::Value << YAML::BeginMap;

                    xproperty::settings::context ctx;
                    if (auto* pObj = xproperty::getObject(comp)) {
                        SerializeObjectToYAML(e, pObj, (void*)&comp, ctx);
                    }
                    else {
                        BOOM_ERROR("[PropertySerializer] Failed to get object info for {}", name);
                    }

                    e << YAML::EndMap;
                }
            },
            // ===== DESERIALIZE =====
            [name](const YAML::Node& node, EntityRegistry& scene, EntityID entity, AssetRegistry& /*assets*/) {
                BOOM_INFO("[PropertySerializer] Deserializing {} for entity {}", name, (uint32_t)entity);  // ADD THIS

                auto& comp = scene.get_or_emplace<T>(entity);

                if (node.IsMap()) {
                    xproperty::settings::context ctx;
                    if (auto* pObj = xproperty::getObject(comp)) {
                        DeserializeObjectFromYAML(node, pObj, (void*)&comp, ctx);
                        BOOM_INFO("[PropertySerializer] Successfully deserialized {}", name);  // ADD THIS
                    }
                    else {
                        BOOM_ERROR("[PropertySerializer] Failed to get object info for {} during deserialize", name);
                    }
                }
                else {
                    BOOM_WARN("[PropertySerializer] Node is not a map for {}", name);  // ADD THIS
                }
            }
        );
    }


    void RegisterAllComponentSerializers()
    {
        auto& registry = SerializationRegistry::Instance();

        // === INFO COMPONENT ===
        RegisterPropertyComponent<InfoComponent>("InfoComponent");

        // === TRANSFORM COMPONENT ===
        RegisterPropertyComponent<TransformComponent>("TransformComponent");

        // === CAMERA COMPONENT ===
        RegisterPropertyComponent<CameraComponent>("CameraComponent");

        // === RIGID BODY COMPONENT ===
		RegisterPropertyComponent<RigidBodyComponent>("RigidBodyComponent");

        // === COLLIDER COMPONENT ===
		RegisterPropertyComponent<ColliderComponent>("ColliderComponent");

        // === MODEL COMPONENT ===
		RegisterPropertyComponent<ModelComponent>("ModelComponent");

        // === ANIMATOR COMPONENT ===
        registry.RegisterComponentSerializer(
            "AnimatorComponent",
            // Serialize
            [](YAML::Emitter& e, EntityRegistry& reg, EntityID ent) {
                if (reg.all_of<AnimatorComponent>(ent)) {
                    auto& animatorComp = reg.get<AnimatorComponent>(ent);
                    auto& animator = animatorComp.animator;
                    if (!animator) return;

                    e << YAML::Key << "AnimatorComponent" << YAML::Value << YAML::BeginMap;

                    // Serialize clip file paths
                    e << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
                    for (size_t i = 0; i < animator->GetClipCount(); ++i) {
                        const auto* clip = animator->GetClip(i);
                        if (clip) {
                            e << YAML::BeginMap;
                            e << YAML::Key << "name" << YAML::Value << clip->name;
                            e << YAML::Key << "filePath" << YAML::Value << clip->filePath;
                            e << YAML::EndMap;
                        }
                    }
                    e << YAML::EndSeq;

                    // Serialize states
                    e << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
                    for (const auto& state : animator->GetStates()) {
                        e << YAML::BeginMap;
                        e << YAML::Key << "name" << YAML::Value << state.name;
                        e << YAML::Key << "clipIndex" << YAML::Value << state.clipIndex;
                        e << YAML::Key << "speed" << YAML::Value << state.speed;
                        e << YAML::Key << "loop" << YAML::Value << state.loop;

                        // Serialize transitions
                        e << YAML::Key << "transitions" << YAML::Value << YAML::BeginSeq;
                        for (const auto& trans : state.transitions) {
                            e << YAML::BeginMap;
                            e << YAML::Key << "targetStateIndex" << YAML::Value << trans.targetStateIndex;
                            e << YAML::Key << "conditionType" << YAML::Value << static_cast<int>(trans.conditionType);
                            e << YAML::Key << "parameterName" << YAML::Value << trans.parameterName;
                            e << YAML::Key << "floatValue" << YAML::Value << trans.floatValue;
                            e << YAML::Key << "boolValue" << YAML::Value << trans.boolValue;
                            e << YAML::Key << "transitionDuration" << YAML::Value << trans.transitionDuration;
                            e << YAML::Key << "hasExitTime" << YAML::Value << trans.hasExitTime;
                            e << YAML::Key << "exitTime" << YAML::Value << trans.exitTime;
                            e << YAML::EndMap;
                        }
                        e << YAML::EndSeq;
                        e << YAML::EndMap;
                    }
                    e << YAML::EndSeq;

                    // Serialize parameters
                    e << YAML::Key << "FloatParams" << YAML::Value << YAML::BeginMap;
                    for (const auto& [name, value] : animator->GetFloatParams()) {
                        e << YAML::Key << name << YAML::Value << value;
                    }
                    e << YAML::EndMap;

                    e << YAML::Key << "BoolParams" << YAML::Value << YAML::BeginMap;
                    for (const auto& [name, value] : animator->GetBoolParams()) {
                        e << YAML::Key << name << YAML::Value << value;
                    }
                    e << YAML::EndMap;

                    // Save current state
                    e << YAML::Key << "CurrentStateIndex" << YAML::Value << animator->GetCurrentStateIndex();

                    e << YAML::EndMap;
                }
            },
            // Deserialize
            [](const YAML::Node& data, EntityRegistry& reg, EntityID ent, AssetRegistry& assets) {
                // Create animator component
                auto& animatorComp = reg.get_or_emplace<AnimatorComponent>(ent);

                // Check if we should initialize from model or standalone
                if (reg.all_of<ModelComponent>(ent)) {
                    auto& modelComp = reg.get<ModelComponent>(ent);
                    ModelAsset& modelAsset = assets.Get<ModelAsset>(modelComp.modelID);
                    if (modelAsset.hasJoints) {
                        auto skeletalModel = std::dynamic_pointer_cast<SkeletalModel>(modelAsset.data);
                        if (skeletalModel && skeletalModel->GetAnimator()) {
                            animatorComp.animator = skeletalModel->GetAnimator()->Clone();
                        }
                    }
                }

                // Create animator if not from model
                if (!animatorComp.animator) {
                    animatorComp.animator = std::make_shared<Animator>();
                }

                auto& animator = animatorComp.animator;

                // Deserialize clips
                if (data["Clips"]) {
                    for (const auto& clipNode : data["Clips"]) {
                        std::string filePath = clipNode["filePath"].as<std::string>("");
                        std::string name = clipNode["name"].as<std::string>("");
                        if (!filePath.empty()) {
                            animator->LoadAnimationFromFile(filePath, name);
                        }
                    }
                }

                // Deserialize states
                if (data["States"]) {
                    for (const auto& stateNode : data["States"]) {
                        size_t stateIdx = animator->AddState(
                            stateNode["name"].as<std::string>("State"),
                            stateNode["clipIndex"].as<size_t>(0)
                        );
                        auto* state = animator->GetState(stateIdx);
                        if (state) {
                            state->speed = stateNode["speed"].as<float>(1.0f);
                            state->loop = stateNode["loop"].as<bool>(true);

                            // Deserialize transitions
                            if (stateNode["transitions"]) {
                                for (const auto& transNode : stateNode["transitions"]) {
                                    Animator::Transition trans;
                                    trans.targetStateIndex = transNode["targetStateIndex"].as<size_t>(0);
                                    trans.conditionType = static_cast<Animator::Transition::ConditionType>(
                                        transNode["conditionType"].as<int>(0));
                                    trans.parameterName = transNode["parameterName"].as<std::string>("");
                                    trans.floatValue = transNode["floatValue"].as<float>(0.0f);
                                    trans.boolValue = transNode["boolValue"].as<bool>(false);
                                    trans.transitionDuration = transNode["transitionDuration"].as<float>(0.25f);
                                    trans.hasExitTime = transNode["hasExitTime"].as<bool>(false);
                                    trans.exitTime = transNode["exitTime"].as<float>(0.9f);
                                    state->transitions.push_back(trans);
                                }
                            }
                        }
                    }
                }

                // Deserialize parameters
                if (data["FloatParams"]) {
                    for (const auto& param : data["FloatParams"]) {
                        animator->SetFloat(param.first.as<std::string>(), param.second.as<float>());
                    }
                }
                if (data["BoolParams"]) {
                    for (const auto& param : data["BoolParams"]) {
                        animator->SetBool(param.first.as<std::string>(), param.second.as<bool>());
                    }
                }

                // Restore current state
                if (data["CurrentStateIndex"]) {
                    animator->SetDefaultState(data["CurrentStateIndex"].as<size_t>(0));
                }

                BOOM_INFO("[AnimatorComponent] Deserialized with {} clips, {} states",
                    animator->GetClipCount(), animator->GetStateCount());
            }
        );

        // === DIRECT LIGHT COMPONENT ===
		RegisterPropertyComponent<DirectLightComponent>("DirectLightComponent");

        // === POINT LIGHT COMPONENT ===
		RegisterPropertyComponent<PointLightComponent>("PointLightComponent");


        // === SPOT LIGHT COMPONENT ===
		RegisterPropertyComponent<SpotLightComponent>("SpotLightComponent");

        // === SKYBOX COMPONENT ===
		RegisterPropertyComponent<SkyboxComponent>("SkyboxComponent");

        // === THIRD PERSON CAMERA COMPONENT ===
        RegisterPropertyComponent<ThirdPersonCameraComponent>("ThirdPersonCameraComponent");


       

        BOOM_INFO("[ComponentSerializers] All component serializers registered");
    }
}