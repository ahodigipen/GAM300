#include "Core.h"
#include "../includes/Audio/SoundSystem.hpp"
#include "../includes/Audio/Audio.hpp"
#include "../includes/ECS/ECS.hpp"
#include <iostream>

using namespace Boom;

void SoundSystem::Shutdown()
{
	for (auto& [eid, names] : s_activeInstances) {
		for (auto& name : names) {
			SoundEngine::Instance().StopSound(name);
			SoundEngine::Instance().UnloadSound(name);
		}
	}
	s_activeInstances.clear();
	s_lastPos.clear();
}

void SoundSystem::Update(Boom::EntityRegistry& registry, float dt)
{
	(void)dt; 
	auto view = registry.view<TransformComponent, SoundComponent>();

	for (auto entity : view)
	{
		auto& tf = view.get<TransformComponent>(entity);
		auto& sc = view.get<SoundComponent>(entity);

		uint64_t uid = static_cast<uint64_t>(static_cast<uint32_t>(entity));

		// Ensure container exists
		auto& instances = s_activeInstances[uid];

		// Iterate through entries in component and ensure each sound is active as requested
		for (size_t i =0; i < sc.entries.size(); ++i)
		{
			const auto& entry = sc.entries[i];
			// Construct unique instance name per entity + index + logical name to avoid collisions
			std::string instanceName = "ent_" + std::to_string(uid) + "_" + std::to_string(i) + "_" + entry.name;

			// If playOnStart and not yet active -> preload and play
			bool alreadyActive = false;
			for (const auto& n : instances) if (n == instanceName) { alreadyActive = true; break; }

			if (entry.playOnStart && !alreadyActive)
			{
				SoundEngine::Instance().PreloadSound(instanceName, entry.filePath, false, entry.loop);
				SoundEngine::Instance().PlaySoundAt(instanceName, entry.filePath, tf.transform.translate, entry.loop);
				instances.push_back(instanceName);
				s_lastPos[uid] = tf.transform.translate;
				continue;
			}

			// If there's an active instance for this entry, update its position
			for (const auto& n : instances)
			{
				if (n == instanceName)
				{
					glm::vec3 pos = tf.transform.translate;
					SoundEngine::Instance().SetSoundPosition(n, pos);
					s_lastPos[uid] = pos;

					// If filePath cleared, stop and unload this instance
					if (entry.filePath.empty())
					{
						SoundEngine::Instance().StopSound(n);
						SoundEngine::Instance().UnloadSound(n);
						// remove from instances vector (lazy removal below)
					}
				}
			}
		}

		// Remove instances whose corresponding entry no longer exists or whose filePath was cleared
		// Build a set of desired instance names from current component
		std::vector<std::string> desired;
		desired.reserve(sc.entries.size());
		for (size_t i =0; i < sc.entries.size(); ++i) {
			const auto& entry = sc.entries[i];
			desired.push_back("ent_" + std::to_string(uid) + "_" + std::to_string(i) + "_" + entry.name);
		}

		// Erase any instances not in desired
		auto& vec = instances;
		for (auto it = vec.begin(); it != vec.end(); )
		{
			const std::string& name = *it;
			if (std::find(desired.begin(), desired.end(), name) == desired.end())
			{
				SoundEngine::Instance().StopSound(name);
				SoundEngine::Instance().UnloadSound(name);
				it = vec.erase(it);
			}
			else ++it;
		}

		// If component has no entries left, clean up pos map
		if (sc.entries.empty()) {
			s_lastPos.erase(uid);
		}
	}

	// Remove entries for destroyed entities
	for (auto it = s_activeInstances.begin(); it != s_activeInstances.end(); )
	{
		auto eid = static_cast<Boom::EntityID>(static_cast<uint32_t>(it->first));
		if (!registry.valid(eid))
		{
			for (auto& name : it->second) {
				SoundEngine::Instance().StopSound(name);
				SoundEngine::Instance().UnloadSound(name);
			}
			s_lastPos.erase(it->first);
			it = s_activeInstances.erase(it);
		}
		else ++it;
	}
}
