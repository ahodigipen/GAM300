#include "Core.h"
#include "Audio.hpp"
#include <iostream>
#include <filesystem>
#include <mutex>

static FMOD::ChannelGroup* sMasterGroup = nullptr;
static FMOD::ChannelGroup* sMusicGroup  = nullptr;
static FMOD::ChannelGroup* sSFXGroup    = nullptr;

static inline bool FMODCheck(FMOD_RESULT result, const char* context) {
    if (result != FMOD_OK) {
        std::cerr << "[FMOD] " << context << " failed: " << result << std::endl;
        return false;
    }
    return true;
}

BOOM_API SoundEngine& SoundEngine::Instance() {
    static SoundEngine instance;
    return instance;
}

BOOM_API bool SoundEngine::Init() {
    FMOD_RESULT result = FMOD::System_Create(&mSystem);
    if (result != FMOD_OK) {
        std::cerr << "FMOD error: " << result << std::endl;
        return false;
    }

    result = mSystem->init(512, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        std::cerr << "FMOD init failed: " << result << std::endl;
        return false;
    }

    result = mSystem->getMasterChannelGroup(&sMasterGroup);
    if (!FMODCheck(result, "getMasterChannelGroup")) return false;

    result = mSystem->createChannelGroup("Music", &sMusicGroup);
    if (!FMODCheck(result, "createChannelGroup Music")) return false;

    result = mSystem->createChannelGroup("SFX", &sSFXGroup);
    if (!FMODCheck(result, "createChannelGroup SFX")) return false;

    if (sMasterGroup && sMusicGroup) sMasterGroup->addGroup(sMusicGroup);
    if (sMasterGroup && sSFXGroup)   sMasterGroup->addGroup(sSFXGroup);

    {
        std::scoped_lock lock(mMutex);
        mChannelGroups["Master"] = sMasterGroup;
        mChannelGroups["Music"] = sMusicGroup;
        mChannelGroups["SFX"] = sSFXGroup;
    }

    return true;
}

BOOM_API bool SoundEngine::CreateChannelGroup(const std::string& groupName, const std::string& parentGroup)
{
    if (!mSystem) return false;
    if (groupName.empty()) return false;

    std::scoped_lock lock(mMutex);

    if (mChannelGroups.find(groupName) != mChannelGroups.end()) { return false; }

    FMOD::ChannelGroup* newGroup = nullptr;
    FMOD_RESULT result = mSystem->createChannelGroup(groupName.c_str(), &newGroup);
    if (result != FMOD_OK || !newGroup) 
    {
        std::cerr << "[SoundEngine] createChannelGroup failed for " << groupName << " error: " << result << std::endl;
        return false;
    }

    FMOD::ChannelGroup* parent = nullptr;
    auto it = mChannelGroups.find(parentGroup);
    if (it != mChannelGroups.end()) parent = it->second;
    if (!parent) parent = sMasterGroup;

    if (parent && newGroup) parent->addGroup(newGroup);

    mChannelGroups[groupName] = newGroup;
    return true;
}

BOOM_API bool SoundEngine::RemoveChannelGroup(const std::string& groupName)
{
    if (groupName.empty() || groupName == "Master") return false;

    std::scoped_lock lock(mMutex);

    auto it = mChannelGroups.find(groupName);
    if (it == mChannelGroups.end()) return false;

    FMOD::ChannelGroup* group = it->second;

    for (auto& [name, ch] : mChannels) 
    {
        if (ch) 
        {
            FMOD::ChannelGroup* current = nullptr;
            ch->getChannelGroup(&current);
            if (current == group) { ch->stop(); }
        }
    }

    if (group) { group->release(); }

    mChannelGroups.erase(it);
    return true;
}

BOOM_API void SoundEngine::SetGroupVolume(const std::string& groupName, float volume)
{
    std::scoped_lock lock(mMutex);
    auto it = mChannelGroups.find(groupName);
    if (it != mChannelGroups.end() && it->second) { it->second->setVolume(volume); }

    else { std::cerr << "[SoundEngine] Unknown group '" << groupName << "'\n"; }
}

BOOM_API float SoundEngine::GetGroupVolume(const std::string& groupName) const
{
    std::scoped_lock lock(mMutex);
    float vol = 0.0f;
    auto it = mChannelGroups.find(groupName);

    if (it != mChannelGroups.end() && it->second) { it->second->getVolume(&vol); }
    return vol;
}

BOOM_API void SoundEngine::Update() {
    if (mSystem) mSystem->update();

    for (auto it = mChannels.begin(); it != mChannels.end(); ) 
    {
        bool isPlaying = false;
        if (it->second) { it->second->isPlaying(&isPlaying); }

        if (!isPlaying) { it = mChannels.erase(it); } 

        else { ++it; }
	}
}

BOOM_API void SoundEngine::PauseAll(bool pause)
{
	std::scoped_lock lock(mMutex);

	// Pause/unpause all channels we track
	for (auto& [name, ch] : mChannels) {
		if (ch) ch->setPaused(pause);
	}

	// Also set pause on master group to be safe
	if (sMasterGroup) sMasterGroup->setPaused(pause);
}

BOOM_API void SoundEngine::StopAll()
{
	std::scoped_lock lock(mMutex);

	// Stop individual channels
	for (auto& [name, ch] : mChannels) {
		if (ch) ch->stop();
	}
	mChannels.clear();

	// Stop master group
	if (sMasterGroup) sMasterGroup->stop();

	// Unload all sounds
	for (auto& [name, sound] : mSounds) { if (sound) sound->release(); }
	mSounds.clear();
}

BOOM_API void SoundEngine::Shutdown() {
    for (auto& [name, ch] : mChannels) { if (ch) ch->stop(); }
    mChannels.clear();

    // Release sounds
    for (auto& [name, sound] : mSounds) { if (sound) sound->release(); }
    mSounds.clear();

    // Release custom channel groups (skip Master - owned by system)
    {
        std::scoped_lock lock(mMutex);
        for (auto it = mChannelGroups.begin(); it != mChannelGroups.end(); ) 
        {
            const std::string& n = it->first;
            FMOD::ChannelGroup* g = it->second;
            if (n != "Master" && g) 
            {
                g->release();
                it = mChannelGroups.erase(it);
            }

            else { ++it; }
        }
        mChannelGroups.clear();
    }

    sMusicGroup = nullptr;
    sSFXGroup = nullptr;
    sMasterGroup = nullptr; 


    if (mSystem) 
    {
        mSystem->close();
        mSystem->release();
        mSystem = nullptr;
    }
}

BOOM_API void SoundEngine::PlaySound(const std::string& name, const std::string& filePath, bool loop)
{
    if (!mSystem) 
    {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    std::cout << "[SoundEngine] Current working directory: "
        << std::filesystem::current_path().string() << std::endl;

    if (!std::filesystem::exists(filePath)) {
        std::cerr << "[SoundEngine] File does NOT exist: " << filePath << std::endl;
    }
    else {
        std::cout << "[SoundEngine] File found: " << filePath << std::endl;
    }

    if (mSounds.find(name) == mSounds.end()) {
        FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_DEFAULT | FMOD_2D;
        FMOD::Sound* sound = nullptr;

        FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
        if (result != FMOD_OK) {
            std::cerr << "FMOD failed to load " << filePath << " error: " << result << std::endl;
            return;
        }

        mSounds[name] = sound;
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, false, &channel);
    if (result != FMOD_OK) {
        std::cerr << "FMOD failed to play " << name << " error: " << result << std::endl;
        return;
    }

    // Route looped audio to Music group, otherwise to SFX. (Heuristic)
    FMOD::ChannelGroup* targetGroup = loop ? sMusicGroup : sSFXGroup;

    if (channel && targetGroup) { channel->setChannelGroup(targetGroup); }

    // Set defaults and unpause
    if (channel) 
    {
        channel->setVolume(1.0f);
        channel->setPaused(false);
        mChannels[name] = channel;
    }
}


BOOM_API void SoundEngine::PlaySound(const std::string& name, const std::string& filePath, bool loop, const std::string& groupName)
{
    if (!mSystem) 
    {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    if (mSounds.find(name) == mSounds.end()) {
        FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_DEFAULT | FMOD_2D;
        FMOD::Sound* sound = nullptr;

        FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
        if (result != FMOD_OK) {
            std::cerr << "FMOD failed to load " << filePath << " error: " << result << std::endl;
            return;
        }

        mSounds[name] = sound;
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, false, &channel);
    if (result != FMOD_OK) {
        std::cerr << "FMOD failed to play " << name << " error: " << result << std::endl;
        return;
    }

    FMOD::ChannelGroup* targetGroup = nullptr;
    {
        std::scoped_lock lock(mMutex);
        auto it = mChannelGroups.find(groupName);
        if (it != mChannelGroups.end()) targetGroup = it->second;
    }

    if (!targetGroup) targetGroup = loop ? sMusicGroup : sSFXGroup;

    if (channel && targetGroup) { channel->setChannelGroup(targetGroup); }

    if (channel) 
    {
        channel->setVolume(1.0f);
        channel->setPaused(false);
        std::scoped_lock lock(mMutex);
        mChannels[name] = channel;
    }
}

BOOM_API void SoundEngine::StopSound(const std::string& name) {
	auto it = mChannels.find(name);

    if (it != mChannels.end())
    {
        if (it->second) it->second->stop();
        mChannels.erase(it);
    }
}

BOOM_API void SoundEngine::SetVolume(const std::string& name, float volume) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setVolume(volume); }
}

BOOM_API bool SoundEngine::IsPlaying(const std::string& name) const {
    auto it = mChannels.find(name);
    if (it == mChannels.end() || !it->second) return false;
    bool playing = false;
    it->second->isPlaying(&playing);
    return playing;
}

BOOM_API void SoundEngine::Pause(const std::string& name, bool pause) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setPaused(pause); }
}

BOOM_API void SoundEngine::SetLooping(const std::string& name, bool loop) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF); }
}

BOOM_API void SoundEngine::StopAllExcept(const std::string& keepName) {
    for (auto it = mChannels.begin(); it != mChannels.end(); ) 
    {
        const std::string& n = it->first;
        FMOD::Channel* ch = it->second;
        if (n != keepName && ch) ch->stop();

        if (n != keepName)
            it = mChannels.erase(it);
        else
            ++it;
    }
}

BOOM_API bool SoundEngine::PreloadSound(const std::string& name, const std::string& filePath, bool stream, bool loop)
{
    if (!mSystem) return false;

    std::scoped_lock lock(mMutex);

    auto it = mSounds.find(name);
    if (it != mSounds.end()) 
    {
        mSoundRefCount[name] += 1;
        return true;
    }

    FMOD_MODE mode = FMOD_DEFAULT | FMOD_2D;
    if (stream) mode = static_cast<FMOD_MODE>(mode | FMOD_CREATESTREAM);
    mode = static_cast<FMOD_MODE>(mode | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));

    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);

    if (result != FMOD_OK) 
    {
        std::cerr << "FMOD failed to preload " << filePath << " error: " << result << std::endl;
        return false;
    }

    mSounds[name] = sound;
    mSoundRefCount[name] = 1;
    return true;
}

BOOM_API void SoundEngine::UnloadSound(const std::string& name)
{
    std::scoped_lock lock(mMutex);

    auto rcIt = mSoundRefCount.find(name);
    if (rcIt == mSoundRefCount.end()) { return; }

    rcIt->second -= 1;
    if (rcIt->second > 0) { return; }

    mSoundRefCount.erase(rcIt);

    auto sIt = mSounds.find(name);
    if (sIt != mSounds.end()) 
    {
        if (sIt->second) { sIt->second->release(); }

        mSounds.erase(sIt);
    }
}

BOOM_API void SoundEngine::PlaySoundAt(const std::string& name, const std::string& filePath, const glm::vec3& position, bool loop)
{
    if (!mSystem) {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    std::cout << "[SoundEngine] PlaySoundAt: " << name << " -> " << filePath << " pos(" << position.x << "," << position.y << "," << position.z << ")\n";

    // Ensure sound loaded (3D mode)
    {
        std::scoped_lock lock(mMutex);
        if (mSounds.find(name) == mSounds.end()) {
            FMOD_MODE mode = static_cast<FMOD_MODE>((loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_3D | FMOD_DEFAULT);
            FMOD::Sound* sound = nullptr;
            FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
            if (result != FMOD_OK) {
                std::cerr << "FMOD failed to load (3D) " << filePath << " error: " << result << std::endl;
                return;
            }
            mSounds[name] = sound;
        }
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, true, &channel); // start paused to set 3D attrs
    if (result != FMOD_OK) {
        std::cerr << "FMOD failed to play (3D) " << name << " error: " << result << std::endl;
        return;
    }

    // route to group: fallback heuristic (loop->Music, else SFX) but prefer named groups if present
    FMOD::ChannelGroup* targetGroup = nullptr;
    {
        std::scoped_lock lock(mMutex);
        auto it = mChannelGroups.find(loop ? "Music" : "SFX");
        if (it != mChannelGroups.end()) targetGroup = it->second;
    }
    if (channel && targetGroup) channel->setChannelGroup(targetGroup);

    // Convert position to FMOD_VECTOR and set 3D attributes (velocity zero)
    FMOD_VECTOR fpos = { position.x, position.y, position.z };
    FMOD_VECTOR fvel = { 0.0f, 0.0f, 0.0f };
    if (channel) {
        channel->set3DAttributes(&fpos, &fvel);
        channel->setVolume(1.0f);
        channel->setPaused(false);
        std::scoped_lock lock(mMutex);
        mChannels[name] = channel;
    }
}

BOOM_API void SoundEngine::SetSoundPosition(const std::string& name, const glm::vec3& position)
{
    std::scoped_lock lock(mMutex);
    auto it = mChannels.find(name);
    if (it == mChannels.end() || !it->second) return;

    FMOD::Channel* channel = it->second;
    FMOD_VECTOR fpos = { position.x, position.y, position.z };
    FMOD_VECTOR fvel = {0.0f,0.0f,0.0f };
    channel->set3DAttributes(&fpos, &fvel);
}

BOOM_API void SoundEngine::SetListenerAttributes(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& forward, const glm::vec3& up)
{
	if (!mSystem) return;
	FMOD_VECTOR p = { pos.x, pos.y, pos.z };
	FMOD_VECTOR v = { vel.x, vel.y, vel.z };
	FMOD_VECTOR f = { forward.x, forward.y, forward.z };
	FMOD_VECTOR u = { up.x, up.y, up.z };
	mSystem->set3DListenerAttributes(0, &p, &v, &f, &u);
}

BOOM_API bool SoundEngine::HasChannelGroup(const std::string& groupName) const
{
	std::scoped_lock lock(mMutex);
	return mChannelGroups.find(groupName) != mChannelGroups.end();
}

BOOM_API std::vector<std::string> SoundEngine::GetChannelGroupNames() const
{
    std::vector<std::string> names;
    std::scoped_lock lock(mMutex);
    names.reserve(mChannelGroups.size());
    for (auto& [k, v] : mChannelGroups) {
        names.push_back(k);
    }
    return names;
}
