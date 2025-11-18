#include "Panels/AudioPanel.h"
#include "Audio/Audio.hpp"         // SoundEngine
#include "Context/DebugHelpers.h"          // optional logging

namespace EditorUI
{
    AudioPanel::AudioPanel(AppInterface* ctx)
        : IWidget(ctx)
        , m_Tracks{
            { "Menu",   "Resources/Audio/Fetty Wap.wav" },
            { "BOOM",   "Resources/Audio/vboom.wav"     },
            { "BGM",   "Resources/Audio/Deathless BGM 1 v2.mp3"      },
            { "Ambient",   "Resources/Audio/outdoorAmbience.wav" },
            { "Schizo", "Resources/Audio/the voices.wav" },
            { "Enemy Atk 1",   "Resources/Audio/enemyAtk_1.wav"     },
            { "Enemy Atk 2",   "Resources/Audio/enemyAtk_2.wav"     },
            { "Enemy Atk 3",   "Resources/Audio/enemyAtk_3.wav"     },
            { "Enemy Hurt 1",   "Resources/Audio/enemyHurt_1.wav"     },
            { "Enemy Hurt 1",   "Resources/Audio/enemyHurt_2.wav"     },
            { "Enemy Atk",   "Resources/Audio/enemtAtk_1.wav"     },
            { "Player Atk 1",   "Resources/Audio/playerAttack_1.wav"     },
            { "Player Atk 2",   "Resources/Audio/playerAttack_2.wav"     },
            { "Player Hurt 1",   "Resources/Audio/playerHurt_1.wav"     },
            { "Player Hurt 2",   "Resources/Audio/playerHurt_2.wav"     },
            { "Player Punch 1",   "Resources/Audio/playerPunch_1.wav"     },
            { "Player Punch 2",   "Resources/Audio/playerPunch_2.wav"     },
            { "Player Punch 3",   "Resources/Audio/playerPunch_3.wav"     },
            { "Player Run 1",   "Resources/Audio/playerRun_01.wav"     },
            { "Player Run 2",   "Resources/Audio/playerRun_02.wav"     },
            { "Player Run 3",   "Resources/Audio/playerRun_03.wav"     },
            { "Player Run 4",   "Resources/Audio/playerRun_04.wav"     },
            { "Player Run 5",   "Resources/Audio/playerRun_05.wav"     },
            { "Player Run 6",   "Resources/Audio/playerRun_06.wav"     },
            { "Player Walk 1",   "Resources/Audio/playerWalk_01.wav"     },
            { "Player Walk 2",   "Resources/Audio/playerWalk_02.wav"     },
            { "Player Walk 3",   "Resources/Audio/playerWalk_03.wav"     },
            { "Player Walk 4",   "Resources/Audio/playerWalk_04.wav"     },
            { "Player Walk 5",   "Resources/Audio/playerWalk_05.wav"     },
            { "Poison",   "Resources/Audio/poison.wav"     },
            { "Landing",   "Resources/Audio/landing.wav"     },
            { "Freeze",   "Resources/Audio/freeze.wav"     },
            { "Unfreeze",   "Resources/Audio/unfreeze.wav"     },
            { "Sheathe",   "Resources/Audio/Sheathe.wav"     },
            { "Unsheathe",   "Resources/Audio/Unsheathe.wav"     },
            { "Absorb",   "Resources/Audio/power absorb.wav"     }  

        }
    {
        EnsureVolumeKeys();
        if (m_Tracks.empty()) m_Selected = -1;
    }

    void AudioPanel::Render()
    {
        OnShow();
    }

    void AudioPanel::OnShow()
    {
        if (!m_Show) return;

        auto& audio = SoundEngine::Instance();
        EnsureVolumeKeys();

        if (ImGui::Begin("Music", &m_Show))
        {
            // Early out if you have no tracks
            if (m_Tracks.empty())
            {
                ImGui::TextUnformatted("No tracks configured.");
                ImGui::End();
                return;
            }

            // Clamp selection so static analyzers don’t warn
            if (m_Selected < 0 || m_Selected >= static_cast<int>(m_Tracks.size()))
                m_Selected = 0;

            // ----- Track picker -----
            {
                const char* current = m_Tracks[static_cast<size_t>(m_Selected)].name.c_str();
                if (ImGui::BeginCombo("Track", current))
                {
                    for (int i = 0; i < static_cast<int>(m_Tracks.size()); ++i)
                    {
                        const bool isSel = (i == m_Selected);
                        if (ImGui::Selectable(m_Tracks[static_cast<size_t>(i)].name.c_str(), isSel))
                            m_Selected = i;
                        if (isSel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            const std::string& name = m_Tracks[static_cast<size_t>(m_Selected)].name;
            const std::string& path = m_Tracks[static_cast<size_t>(m_Selected)].path;

            // ----- Loop + Restart -----
            if (ImGui::Checkbox("Loop", &m_Loop)) {
                audio.SetLooping(name, m_Loop);
            }
            ImGui::SameLine();
            if (ImGui::Button("Restart")) {
                audio.StopAllExcept(std::string{}); // avoid implicit conv warnings
                audio.PlaySound(name, path, m_Loop);
                audio.SetVolume(name, m_Volume[name]);
                m_Paused = false;
            }

            // ----- Volume -----
            {
                float vol = m_Volume[name];
                if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f, "%.2f")) {
                    m_Volume[name] = vol;
                    audio.SetVolume(name, vol);
                }
            }

            // ----- Play / Stop / Pause -----
            {
                const bool playing = audio.IsPlaying(name);
                if (!playing) {
                    if (ImGui::Button("Play")) {
                        audio.StopAllExcept(std::string{});
                        audio.PlaySound(name, path, m_Loop);
                        audio.SetVolume(name, m_Volume[name]);
                        m_Paused = false;
                    }
                }
                else {
                    if (ImGui::Button("Stop")) {
                        audio.StopSound(name);
                        m_Paused = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Paused", &m_Paused)) {
                        audio.Pause(name, m_Paused);
                    }
                }
            }

            // ----- Quick Switch -----
            ImGui::SeparatorText("Quick Switch");
            for (int i = 0; i < static_cast<int>(m_Tracks.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::Button(m_Tracks[static_cast<size_t>(i)].name.c_str())) {
                    m_Selected = i;
                    const auto& n = m_Tracks[static_cast<size_t>(i)].name;
                    const auto& p = m_Tracks[static_cast<size_t>(i)].path;

                    audio.StopAllExcept(std::string{});
                    audio.PlaySound(n, p, m_Loop);
                    audio.SetVolume(n, m_Volume[n]);
                    m_Paused = false;
                }
                ImGui::PopID();
                if ((i % 3) != 2) ImGui::SameLine();
            }
        }
        ImGui::End();
    }

    void AudioPanel::EnsureVolumeKeys()
    {
        for (const auto& t : m_Tracks)
        {
            if (!m_Volume.count(t.name))
                m_Volume[t.name] = 1.0f;
        }
    }
} // namespace EditorUI
