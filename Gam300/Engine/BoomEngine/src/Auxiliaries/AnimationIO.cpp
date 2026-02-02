#include "Core.h"
#include "Graphics/Models/AnimationIO.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Boom {

// ========== SAVE ANIMATION TO .anim FILE ==========
bool SaveAnimationClip(const AnimationClip& clip, const std::string& filepath) {
    try {
        YAML::Emitter out;

        out << YAML::BeginMap;
        out << YAML::Key << "AnimationClip" << YAML::Value << YAML::BeginMap;

        // Metadata
        out << YAML::Key << "name" << YAML::Value << clip.name;
        out << YAML::Key << "duration" << YAML::Value << clip.duration;
        out << YAML::Key << "ticksPerSecond" << YAML::Value << clip.ticksPerSecond;
        out << YAML::Key << "filePath" << YAML::Value << filepath;

        // Bone tracks
        out << YAML::Key << "tracks" << YAML::Value << YAML::BeginSeq;
        for (const auto& [boneName, keyframes] : clip.tracks) {
            if (keyframes.empty()) continue;  // Skip empty tracks

            out << YAML::BeginMap;
            out << YAML::Key << "boneName" << YAML::Value << boneName;

            out << YAML::Key << "keyframes" << YAML::Value << YAML::BeginSeq;
            for (const auto& kf : keyframes) {
                out << YAML::BeginMap;
                out << YAML::Key << "time" << YAML::Value << kf.timeStamp;

                // Position (flow style for compactness)
                out << YAML::Key << "position" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.position.x << kf.position.y << kf.position.z << YAML::EndSeq;

                // Rotation (xyzw quaternion)
                out << YAML::Key << "rotation" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.rotation.x << kf.rotation.y << kf.rotation.z << kf.rotation.w << YAML::EndSeq;

                // Scale
                out << YAML::Key << "scale" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.scale.x << kf.scale.y << kf.scale.z << YAML::EndSeq;

                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // end keyframes

            out << YAML::EndMap;  // end track
        }
        out << YAML::EndSeq;  // end tracks

        // Audio events
        out << YAML::Key << "audioEvents" << YAML::Value << YAML::BeginSeq;
        for (const auto& audioEvent : clip.audioEvents) {
            out << YAML::BeginMap;
            out << YAML::Key << "timeStamp" << YAML::Value << audioEvent.timeStamp;
            out << YAML::Key << "soundFile" << YAML::Value << audioEvent.soundFile;
            out << YAML::Key << "volume" << YAML::Value << audioEvent.volume;
            out << YAML::Key << "pitch" << YAML::Value << audioEvent.pitch;
            out << YAML::Key << "is3D" << YAML::Value << audioEvent.is3D;
            out << YAML::Key << "loop" << YAML::Value << audioEvent.loop;
            out << YAML::Key << "groupName" << YAML::Value << audioEvent.groupName;
            out << YAML::Key << "eventName" << YAML::Value << audioEvent.eventName;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;  // end audioEvents

        out << YAML::EndMap;  // end AnimationClip
        out << YAML::EndMap;  // end root

        // Write to file
        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            BOOM_ERROR("[AnimationIO] Failed to open file for writing: {}", filepath);
            return false;
        }

        fout << out.c_str();
        fout.close();

        BOOM_INFO("[AnimationIO] Saved animation clip '{}' to {}", clip.name, filepath);
        return true;
    }
    catch (const std::exception& e) {
        BOOM_ERROR("[AnimationIO] Exception while saving: {}", e.what());
        return false;
    }
}

// ========== LOAD ANIMATION FROM .anim FILE ==========
std::shared_ptr<AnimationClip> LoadAnimationClip(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);

        if (!root["AnimationClip"]) {
            BOOM_ERROR("[AnimationIO] Invalid .anim file (missing AnimationClip): {}", filepath);
            return nullptr;
        }

        YAML::Node data = root["AnimationClip"];

        auto clip = std::make_shared<AnimationClip>();
        clip->name = data["name"].as<std::string>("Unnamed");
        clip->duration = data["duration"].as<float>(0.0f);
        clip->ticksPerSecond = data["ticksPerSecond"].as<float>(24.0f);
        clip->filePath = filepath;

        // Load tracks
        if (data["tracks"]) {
            for (const auto& trackNode : data["tracks"]) {
                std::string boneName = trackNode["boneName"].as<std::string>("");
                if (boneName.empty()) continue;

                std::vector<KeyFrame> keyframes;

                if (trackNode["keyframes"]) {
                    for (const auto& kfNode : trackNode["keyframes"]) {
                        KeyFrame kf;
                        kf.timeStamp = kfNode["time"].as<float>(0.0f);

                        // Position
                        auto pos = kfNode["position"];
                        kf.position = glm::vec3(
                            pos[0].as<float>(0.0f),
                            pos[1].as<float>(0.0f),
                            pos[2].as<float>(0.0f)
                        );

                        // Rotation (xyzw)
                        auto rot = kfNode["rotation"];
                        kf.rotation = glm::quat(
                            rot[3].as<float>(1.0f),  // w
                            rot[0].as<float>(0.0f),  // x
                            rot[1].as<float>(0.0f),  // y
                            rot[2].as<float>(0.0f)   // z
                        );

                        // Scale
                        auto scl = kfNode["scale"];
                        kf.scale = glm::vec3(
                            scl[0].as<float>(1.0f),
                            scl[1].as<float>(1.0f),
                            scl[2].as<float>(1.0f)
                        );

                        keyframes.push_back(kf);
                    }
                }

                clip->tracks[boneName] = keyframes;
            }
        }

        // Load audio events
        if (data["audioEvents"]) {
            for (const auto& eventNode : data["audioEvents"]) {
                AudioEventMarker event;
                event.timeStamp = eventNode["timeStamp"].as<float>(0.0f);
                event.soundFile = eventNode["soundFile"].as<std::string>("");
                event.volume = eventNode["volume"].as<float>(1.0f);
                event.pitch = eventNode["pitch"].as<float>(1.0f);
                event.is3D = eventNode["is3D"].as<bool>(false);
                event.loop = eventNode["loop"].as<bool>(false);
                event.groupName = eventNode["groupName"].as<std::string>("SFX");
                event.eventName = eventNode["eventName"].as<std::string>("");

                clip->audioEvents.push_back(event);
            }
        }

        BOOM_INFO("[AnimationIO] Loaded animation clip '{}' from {} ({} tracks, {} audio events)",
                  clip->name, filepath, clip->tracks.size(), clip->audioEvents.size());
        return clip;
    }
    catch (const std::exception& e) {
        BOOM_ERROR("[AnimationIO] Exception while loading: {}", e.what());
        return nullptr;
    }
}

}  // namespace Boom
