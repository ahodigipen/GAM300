#pragma once
#include "Animation.h"
#include "common/Core.h"
#include <memory>
#include <string>

namespace Boom {
    /**
     * @brief Save an AnimationClip to a .anim file (YAML format)
     * @param clip The clip to save
     * @param filepath Path to .anim file (e.g., "Resources/Animations/walk.anim")
     * @return true if successful
     */
    BOOM_API bool SaveAnimationClip(const AnimationClip& clip, const std::string& filepath);

    /**
     * @brief Load an AnimationClip from a .anim file
     * @param filepath Path to .anim file
     * @return Loaded clip or nullptr on failure
     */
    BOOM_API std::shared_ptr<AnimationClip> LoadAnimationClip(const std::string& filepath);
}
