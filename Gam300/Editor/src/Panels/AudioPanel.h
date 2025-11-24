#pragma once

#include "Context/Context.h"
#include <unordered_map>
#include <string>
#include <vector>

#pragma warning(push)
#pragma warning(disable: 26495 26812 26451 4244 4267 4100 4996)
#include "Vendors/imgui/imgui.h"
#pragma warning(pop)

namespace EditorUI
{
    class AudioPanel : public IWidget
    {
    public:
        explicit AudioPanel(AppInterface* ctx);
        void Render();              // wrapper -> calls OnShow()
        void OnShow() override;

        void Show(bool v) { m_Show = v; }
        bool IsVisible() const { return m_Show; }

    private:
        struct Track { std::string name; std::string path; };
        
        void EnsureVolumeKeys();
        void RefreshAudioAssets();  // New method to refresh from asset registry
        void PlaySelectedTrack();

        bool m_Show = true;

        // UI state
        int  m_Selected = 0;
        bool m_Loop = false;
        bool m_Paused = false;

        // Catalog (adjust paths as needed)
        std::vector<Track> m_Tracks;

        // Per-track volume
        std::unordered_map<std::string, float> m_Volume;

		Boom::AppContext* m_Ctx = nullptr;   // cached from m_App->GetContext()
        Boom::AppInterface* m_App = nullptr;   // preferred access (Interface.h)
    
        bool m_NeedsRefresh = true; // Flag to trigger refresh
        double m_LastRefreshTime = 0.0;
        const double AUTO_REFRESH_INTERVAL = 2.0; // Refresh every 2 seconds
    };
} // namespace EditorUI
