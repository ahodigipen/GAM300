#pragma once
#include "Vendors/imGuizmo/ImSequencer.h"
#include <vector>
#include <string>
#include <map>
#include "Vendors/imgui/imgui.h"

namespace EditorUI
{
    class Editor;

    struct SerializedKeyframe
    {
        int frame;
        float valueX, valueY, valueZ, valueW; // Support up to Vec4
    };

    struct SequenceTrack
    {
        std::string label; // Display Name (e.g. "Player : Position")
        std::string entityName; // Actual Entity Name
        int type; // 0 = Position, 1 = Rotation, etc.
        std::vector<int> keyFrameTimes;
        std::vector<SerializedKeyframe> keyFrames;
        bool expanded = true;
    };

    struct DeferredTrack
    {
        std::string entityName;
        int type;
    };

    class CutsceneSequencerPanel : public ImSequencer::SequenceInterface
    {
    public:
        CutsceneSequencerPanel(Editor* owner);
        virtual ~CutsceneSequencerPanel() = default;

        void Render();

        // ImSequencer Interface
        virtual int GetFrameMin() const override { return m_FrameMin; }
        virtual int GetFrameMax() const override { return m_FrameMax; }
        virtual int GetItemCount() const override { return (int)m_Tracks.size(); }

        virtual int GetItemTypeCount() const override { return 5; } // FIX: Return actual count (Position, Rotation, Scale, Color, Anim)
        virtual const char* GetItemTypeName(int typeIndex) const override; // FIX: Return actual names
        virtual const char* GetItemLabel(int index) const override;
        virtual const char* GetCollapseFmt() const override { return "%d Frames / %d tracks"; }

        virtual void Get(int index, int** start, int** end, int* type, unsigned int* color) override;
        virtual void Add(int type) override;
        virtual void Del(int index) override;
        virtual void Duplicate(int index) override;

        virtual void Copy() override {}
        virtual void Paste() override {}

        virtual size_t GetCustomHeight(int index) override { return 0; }
        virtual void DoubleClick(int index) override;
        virtual void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect) override;
        virtual void CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect) override {}

        // Custom Methods
        void SaveSequence(const std::string& path);
        void LoadSequence(const std::string& path);
        
        // Helpers
        void AddTrack(const std::string& entityName, int propertyType);
        void ApplyFrame(int frame);

    private:
        Editor* m_Owner;
        int m_FrameMin = 0;
        int m_FrameMax = 600;
        int m_CurrentFrame = 0;
        int m_SelectedTrack = -1;
        bool m_Expanded = true;
        
        // Playback & Preview
        bool m_IsPlaying = false;
        bool m_PreviewIndex = true; // "Preview" toggle
        float m_TimeAccumulator = 0.0f;

        std::vector<SequenceTrack> m_Tracks;
        std::vector<DeferredTrack> m_DeferredTracks; // FIX: Queue additions to avoid crash during render
        
        // Editor State
        void RenderMenuBar();
        void RenderTransportControls();
        void RenderBottomBar();
        
        // Popup States
        bool m_ShowAddTrackPopup = false;
        std::string m_PendingEntityName;
    };
}
