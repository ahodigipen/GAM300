#include "Core.h"
#include "Auxiliaries/SerializationRegistry.h"
#include "Auxiliaries/Assets.h"

namespace Boom
{

    /**
    * @brief Hybrid asset serializer: manual creation, property-based serialization
    */
    template<typename T>
    void RegisterPropertyAsset(
        AssetType type,
        std::function<std::shared_ptr<T>(AssetRegistry&, AssetID, const std::string&, const YAML::Node&)> createFunc
    )
    {
        auto& registry = SerializationRegistry::Instance();

        registry.RegisterAssetSerializer(
            type,
            // ===== SERIALIZE (Property-based) =====
            [](YAML::Emitter& e, Asset* asset) {
                auto typedAsset = static_cast<T*>(asset);

                e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;

                xproperty::settings::context ctx;
                if (auto* pObj = xproperty::getObject(*typedAsset)) {
                    SerializeObjectToYAML(e, pObj, (void*)typedAsset, ctx);
                }

                e << YAML::EndMap;
            },
            // ===== DESERIALIZE (Manual creation + property loading) =====
            [createFunc](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
                // Manual asset creation (handles file loading)
                auto asset = createFunc(reg, uid, src, props);

                // Property-based deserialization (fills in data)
                if (props.IsMap()) {
                    xproperty::settings::context ctx;
                    if (auto* pObj = xproperty::getObject(*asset)) {
                        DeserializeObjectFromYAML(props, pObj, (void*)asset.get(), ctx);
                    }
                }

                return static_cast<Asset*>(asset.get());
            }
        );
    }


    void RegisterAllAssetSerializers()
    {
        auto& registry = SerializationRegistry::Instance();

        // ===== PROPERTY-BASED ASSETS =====
        RegisterPropertyAsset<MaterialAsset>(
            AssetType::MATERIAL,
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) {
                std::array<AssetID, 6> textureIDs = {};

                // Try NEW format first (AlbedoMapID)
                if (props["AlbedoMapID"]) {
                    textureIDs[0] = props["AlbedoMapID"].as<AssetID>();
                    textureIDs[1] = props["NormalMapID"].as<AssetID>();
                    textureIDs[2] = props["RoughnessMapID"].as<AssetID>();
                    textureIDs[3] = props["MetallicMapID"].as<AssetID>();
                    textureIDs[4] = props["OcclusionMapID"].as<AssetID>();
                    textureIDs[5] = props["EmissiveMapID"].as<AssetID>();
                }
                // Fallback to OLD format (AlbedoMap)
                else if (props["AlbedoMap"]) {
                    textureIDs[0] = props["AlbedoMap"].as<AssetID>();
                    textureIDs[1] = props["NormalMap"].as<AssetID>();
                    textureIDs[2] = props["RoughnessMap"].as<AssetID>();
                    textureIDs[3] = props["MetallicMap"].as<AssetID>();
                    textureIDs[4] = props["OcclusionMap"].as<AssetID>();
                    textureIDs[5] = props["EmissiveMap"].as<AssetID>();
                }

                auto asset = reg.AddMaterial(uid, src, textureIDs);

                // Read old flat PbrMaterial data if present
                if (props["Albedo"]) {
                    asset->data.albedo = props["Albedo"].as<glm::vec3>();
                    asset->data.metallic = props["Metallic"].as<float>();
                    asset->data.roughness = props["Roughness"].as<float>();
                    asset->data.occlusion = props["Occlusion"].as<float>();
                    asset->data.emissive = props["Emissive"].as<glm::vec3>();
                }

                return asset;
            }
        );

        // === MATERIAL ===
        //registry.RegisterAssetSerializer(
        //    AssetType::MATERIAL,
        //    // Serialize
        //    [](YAML::Emitter& e, Asset* asset) {
        //        auto mtl = static_cast<MaterialAsset*>(asset);
        //        e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
        //        e << YAML::Key << "AlbedoMap" << YAML::Value << mtl->albedoMapID;
        //        e << YAML::Key << "NormalMap" << YAML::Value << mtl->normalMapID;
        //        e << YAML::Key << "RoughnessMap" << YAML::Value << mtl->roughnessMapID;
        //        e << YAML::Key << "MetallicMap" << YAML::Value << mtl->metallicMapID;
        //        e << YAML::Key << "OcclusionMap" << YAML::Value << mtl->occlusionMapID;
        //        e << YAML::Key << "EmissiveMap" << YAML::Value << mtl->emissiveMapID;
        //        e << YAML::Key << "Albedo" << YAML::Value << mtl->data.albedo;
        //        e << YAML::Key << "Metallic" << YAML::Value << mtl->data.metallic;
        //        e << YAML::Key << "Roughness" << YAML::Value << mtl->data.roughness;
        //        e << YAML::Key << "Occlusion" << YAML::Value << mtl->data.occlusion;
        //        e << YAML::Key << "Emissive" << YAML::Value << mtl->data.emissive;
        //        e << YAML::EndMap;
        //    },
        //    // Deserialize
        //    [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
        //        auto mtl = reg.AddMaterial(uid, src);
        //        mtl->albedoMapID = props["AlbedoMap"].as<AssetID>();
        //        mtl->normalMapID = props["NormalMap"].as<AssetID>();
        //        mtl->roughnessMapID = props["RoughnessMap"].as<AssetID>();
        //        mtl->metallicMapID = props["MetallicMap"].as<AssetID>();
        //        mtl->occlusionMapID = props["OcclusionMap"].as<AssetID>();
        //        mtl->emissiveMapID = props["EmissiveMap"].as<AssetID>();
        //        mtl->data.albedo = props["Albedo"].as<glm::vec3>();
        //        mtl->data.roughness = props["Roughness"].as<float>();
        //        mtl->data.metallic = props["Metallic"].as<float>();
        //        mtl->data.occlusion = props["Occlusion"].as<float>();
        //        mtl->data.emissive = props["Emissive"].as<glm::vec3>();
        //        return static_cast<Asset*>(mtl.get());
        //    }
        //);

        // === TEXTURE ===
        //registry.RegisterAssetSerializer(
        //    AssetType::TEXTURE,
        //    // Serialize
        //    [](YAML::Emitter& e, Asset* asset) {
        //        auto tex = static_cast<TextureAsset*>(asset);
        //        e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
        //        e << YAML::Key << "IsHDR" << YAML::Value << tex->isHDR;
        //        e << YAML::Key << "IsFlipY" << YAML::Value << tex->isFlipY;
        //        e << YAML::EndMap;
        //    },
        //    // Deserialize
        //    [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
        //        bool isHDR = props["IsHDR"].as<bool>();
        //        bool isFlipY = props["IsFlipY"].as<bool>();
        //        return static_cast<Asset*>(reg.AddTexture(uid, src, isHDR, isFlipY).get());
        //    }
        //);
        RegisterPropertyAsset<TextureAsset>(
            AssetType::TEXTURE,
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) {
				(void)props; // Unused

                return reg.AddTexture(uid, src);
            }
        );

        // === SKYBOX ===
        registry.RegisterAssetSerializer(
            AssetType::SKYBOX,
            // Serialize
            [](YAML::Emitter& e, Asset* asset) {
                auto skybox = static_cast<SkyboxAsset*>(asset);
                e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
                e << YAML::Key << "Size" << YAML::Value << skybox->size;
                e << YAML::EndMap;
            },
            // Deserialize
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
                int32_t size = props["Size"].as<int32_t>();
                return static_cast<Asset*>(reg.AddSkybox(uid, src, size).get());
            }
        );

        // === MODEL ===
        //registry.RegisterAssetSerializer(
        //    AssetType::MODEL,
        //    // Serialize
        //    [](YAML::Emitter& e, Asset* asset) {
        //        auto model = static_cast<ModelAsset*>(asset);
        //        e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
        //        e << YAML::Key << "HasJoints" << YAML::Value << model->hasJoints;
        //        e << YAML::EndMap;
        //    },
        //    // Deserialize
        //    [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
        //        bool hasJoints = props["HasJoints"].as<bool>();
        //        return static_cast<Asset*>(reg.AddModel(uid, src, hasJoints).get());
        //    }
        //);
        RegisterPropertyAsset<ModelAsset>(
            AssetType::MODEL,
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) {
                bool hasJoints = false;

                // Try NEW format (lowercase)
                if (props["hasJoints"]) {
                    hasJoints = props["hasJoints"].as<bool>();
                }
                // Fallback to OLD format (capitalized)
                else if (props["HasJoints"]) {
                    hasJoints = props["HasJoints"].as<bool>();
                }

                return reg.AddModel(uid, src, hasJoints);
            }
        );

        // === PREFAB ===
        registry.RegisterAssetSerializer(
            AssetType::PREFAB,
            // Serialize
            [](YAML::Emitter& e, Asset* asset) {
                auto prefab = static_cast<PrefabAsset*>(asset);
                e << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;
                e << YAML::Key << "SerializedData" << YAML::Value << YAML::Literal << prefab->serializedData;
                e << YAML::EndMap;
            },
            // Deserialize
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node& props) -> Asset* {
                auto prefab = reg.AddPrefab(uid, src);
                prefab->serializedData = props["SerializedData"].as<std::string>();
                return static_cast<Asset*>(prefab.get());
            }
        );

        // === PHYSICS MESH ===
        registry.RegisterAssetSerializer(
            AssetType::PHYSICS_MESH, // Or whatever your enum is called
            // Serialize (no properties)
            [](YAML::Emitter&, Asset*) {
                // No properties to serialize, the path is enough
            },
            // Deserialize
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node&) -> Asset* {
                // This just adds the asset to the registry. 
                // The 'src' is the path to the .pxm file.
                return static_cast<Asset*>(reg.AddPhysicsMesh(uid, src).get());
            }
        );

        // === Audio ===
        registry.RegisterAssetSerializer(
            AssetType::AUDIO,
            [](YAML::Emitter&, Asset*) {

            },
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node&)->Asset* {
                return static_cast<Asset*>(reg.AddAudio(uid, src).get());
            }
        );

        // === SCENE ===
        registry.RegisterAssetSerializer(
            AssetType::SCENE,
            // Serialize (no properties)
            [](YAML::Emitter&, Asset*) {
                // No properties to serialize
            },
            // Deserialize
            [](AssetRegistry& reg, AssetID uid, const std::string& src, const YAML::Node&) -> Asset* {
                return static_cast<Asset*>(reg.AddScene(uid, src).get());
            }
        );

        BOOM_INFO("[AssetSerializers] All asset serializers registered");
    }
}