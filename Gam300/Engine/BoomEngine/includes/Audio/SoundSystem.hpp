#pragma once

#include "Core.h"
#include "Audio/Audio.hpp"
#include "ECS/ECS.hpp"
#include <unordered_map>
#include <glm/vec3.hpp>
#include <vector>

class SoundSystem {
public:
 static void Update(Boom::EntityRegistry& registry, float dt);

 static void Shutdown();

private:
 inline static std::unordered_map<uint64_t, std::vector<std::string>> s_activeInstances;
 inline static std::unordered_map<uint64_t, glm::vec3> s_lastPos;
 // per-entity -> per-instance last play timestamp (seconds)
 inline static std::unordered_map<uint64_t, std::unordered_map<std::string, double>> s_lastTriggerTime;
};
