#include "NavmeshPanel.h"

#include <string>             // std::strncpy
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ECS/ECS.hpp"           // ModelComponent, Transform3D (your ECS aggregator)
#include "Editor.h"              // Editor to get registry/app/context
#include "Application/Interface.h" // Boom::AppInterface
#include "../src/Recast/RecastBaker.h"       // RecastBakeToFile()
#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/ImGuizmo.h"
#include "AI/DetourNavSystem.h"
namespace EditorUI {

    NavmeshPanel::NavmeshPanel(Editor* owner)
        : m_Owner(owner)
    {
        if (m_Owner) {
            m_App = m_Owner->GetAppInterface();
            m_Ctx = m_App ? m_App->GetContext() : nullptr;
            m_Reg = m_Owner->GetRegistry();
        }
        // default output path already set in ctor member init
    }

    void NavmeshPanel::SetOutputPath(const std::string& path) {
        if (!path.empty()) m_OutPath = path;
    }

    // Collect static triangles from entities that have ModelComponent + Transform3D
    RecastBakeInput NavmeshPanel::GatherTriangleSoupFromScene(entt::registry& /*reg_ignored*/)
    {
        RecastBakeInput out;

        if (!m_Ctx || !m_Ctx->assets) {
            BOOM_ERROR("[NavBake] No AppContext/assets available.");
            return out;
        }

        // Always use the live scene registry from AppContext (avoids pointing at the wrong registry).
        entt::registry& reg = m_Ctx->scene;

        size_t entCount = 0, usedCount = 0, triCount = 0;

        // Do NOT require TransformComponent in the view. We’ll read it if present.
        auto view = reg.view<Boom::ModelComponent>();

        for (auto e : view) {
            ++entCount;
            const auto& mc = view.get<Boom::ModelComponent>(e);
            if (mc.modelID == Boom::EMPTY_ASSET) continue;

            // Build transform matrix (identity if no TransformComponent)
            glm::mat4 M(1.f);
            if (reg.any_of<Boom::TransformComponent>(e)) {
                const auto& tr = reg.get<Boom::TransformComponent>(e);
                // If your TransformComponent exposes transform.Matrix(), use it:
                M = tr.transform.Matrix();
                // If not, fallback (Z-up/2D example):
                // M = glm::translate(glm::mat4(1.f), glm::vec3(tr.transform.translate, 0.f))
                //   * glm::toMat4(glm::quat(glm::radians(tr.transform.rotate)))  // rotate is degrees
                //   * glm::scale(glm::mat4(1.f), glm::vec3(tr.transform.scale, 1.f));
            }

            // Pull model from AssetRegistry
            auto* modelAsset = m_Ctx->assets->TryGet<Boom::ModelAsset>(mc.modelID);
            if (!modelAsset || !modelAsset->data) continue;

            // Expect StaticModel; skip otherwise
            auto staticModel = std::dynamic_pointer_cast<Boom::StaticModel>(modelAsset->data);
            if (!staticModel) continue;

            // Your submesh API (from your screenshot: .vtx (ShadedVert) / .idx)
            const auto& submeshes = staticModel->GetMeshData();

            for (const auto& sub : submeshes) {
                const auto& positions = sub.vtx; // std::vector<Boom::ShadedVert> (has glm::vec3 pos)
                const auto& indices = sub.idx; // u16/u32

                if (positions.empty() || indices.size() < 3) continue;

                const int base = static_cast<int>(out.verts.size() / 3);

                out.verts.reserve(out.verts.size() + positions.size() * 3);
                for (const auto& v : positions) {
                    const glm::vec4 wp = M * glm::vec4(v.pos, 1.f);
                    out.verts.push_back(wp.x);
                    out.verts.push_back(wp.y);
                    out.verts.push_back(wp.z);
                }

                out.tris.reserve(out.tris.size() + indices.size());
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    out.tris.push_back(base + static_cast<int>(indices[i + 0]));
                    out.tris.push_back(base + static_cast<int>(indices[i + 1]));
                    out.tris.push_back(base + static_cast<int>(indices[i + 2]));
                    ++triCount;
                }

                ++usedCount;
            }
        }

        BOOM_INFO("[NavBake] Gathered: {} entities scanned, {} submeshes used, {} triangles.",
            entCount, usedCount, triCount);

        if (entCount == 0) {
            BOOM_WARN("[NavBake] View<ModelComponent>() is empty. Are you iterating the correct registry?");
            BOOM_INFO("[NavBake] regs: &m_Ctx->scene={}, m_Reg={}, GetRegistry()={}",
                (void*)&m_Ctx->scene, (void*)m_Reg, (void*)m_Owner->GetRegistry());
        }

        return out;
    }





    void NavmeshPanel::Render()
    {
        // Respect external show flag if provided
        bool open = (m_ShowNavmesh ? *m_ShowNavmesh : true);
        if (!open) return;

        if (!ImGui::Begin("Navmesh Baker", m_ShowNavmesh)) {
            ImGui::End();
            return;
        }

        // --- Settings UI ---
        ImGui::TextUnformatted("Recast Settings");
        ImGui::Separator();
        ImGui::DragFloat("Cell Size", &m_Cfg.cellSize, 0.01f, 0.05f, 1.0f);
        ImGui::DragFloat("Cell Height", &m_Cfg.cellHeight, 0.01f, 0.05f, 1.0f);
        ImGui::DragFloat("Agent Height", &m_Cfg.agentHeight, 0.01f, 0.5f, 4.0f);
        ImGui::DragFloat("Agent Radius", &m_Cfg.agentRadius, 0.01f, 0.1f, 2.0f);
        ImGui::DragFloat("Max Climb", &m_Cfg.agentMaxClimb, 0.01f, 0.1f, 2.0f);
        ImGui::DragFloat("Max Slope", &m_Cfg.agentMaxSlope, 0.1f, 0.0f, 80.0f);

        ImGui::Separator();
        ImGui::DragInt("Region Min Area", &m_Cfg.regionMinArea, 1, 1, 150);
        ImGui::DragInt("Region Merge Area", &m_Cfg.regionMergeArea, 1, 1, 400);
        ImGui::DragFloat("Edge Max Len (m)", &m_Cfg.edgeMaxLen, 0.1f, 1.0f, 50.0f);
        ImGui::DragFloat("Edge Max Error", &m_Cfg.edgeMaxError, 0.01f, 0.1f, 3.0f);
        ImGui::DragInt("Verts/Poly", &m_Cfg.vertsPerPoly, 1, 3, 6);
        ImGui::DragFloat("Detail Sample Dist", &m_Cfg.detailSampleDist, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Detail Max Error", &m_Cfg.detailSampleMaxError, 0.01f, 0.1f, 3.0f);

        ImGui::Separator();
        char buf[260];
        std::snprintf(buf, sizeof(buf), "%s", m_OutPath.c_str());
        if (ImGui::InputText("Bake Output (.bin)", buf, IM_ARRAYSIZE(buf))) {
            m_OutPath = buf; // push changes back into std::string
        }

        // --- Bake button ---
        if (ImGui::Button("Bake Navmesh", ImVec2(-1, 32))) {
            if (!m_Reg) {
                BOOM_ERROR("[NavBake] No registry available.");
            }
            else {
                std::string err;
                RecastBakeInput in = GatherTriangleSoupFromScene(m_Ctx ? m_Ctx->scene : *m_Reg);
                if (in.verts.empty() || in.tris.empty()) {
                    BOOM_WARN("[NavBake] No geometry gathered. Check your Model asset CPU arrays.");
                }
                else if (!RecastBakeToFile(in, m_Cfg, m_OutPath, &err)) {
                    BOOM_ERROR("[NavBake] Bake failed: {}", err);
                }
                else {
                    BOOM_INFO("[NavBake] Success: {}", m_OutPath);

                    // Hot-reload into live runtime via AppInterface
                    if (m_App) {
                        if (auto* nav = m_App->GetNavSystem()) {
                            if (nav->reloadFromFile(m_OutPath))
                                BOOM_INFO("[Nav] Reloaded runtime navmesh: {}", m_OutPath);
                            else
                                BOOM_ERROR("[Nav] Reload failed for: {}", m_OutPath);
                        }
                    }
                }
            }
        }

        // ---------------------------
        // Load Section (folder + list)
        // ---------------------------
        ImGui::Separator();
        ImGui::TextUnformatted("Load Navmesh (.bin)");

        // Static UI state for the load panel
        static std::string              s_BinDir = "Resources/NavData/";
        static std::vector<std::string> s_BinFiles;
        static int                      s_Selected = -1;
        static bool                     s_FirstScan = true;

        auto RefreshBinList = [&]() {
            s_BinFiles.clear();
            std::error_code ec;
            if (!std::filesystem::exists(s_BinDir, ec) || !std::filesystem::is_directory(s_BinDir, ec)) {
                s_Selected = -1;
                return;
            }
            for (auto& e : std::filesystem::directory_iterator(s_BinDir, ec)) {
                if (e.is_regular_file(ec) && e.path().extension() == ".bin")
                    s_BinFiles.push_back(e.path().filename().string());
            }
            if (s_BinFiles.empty()) s_Selected = -1;
            else if (s_Selected < 0 || s_Selected >= (int)s_BinFiles.size()) s_Selected = 0;
            };

        // Folder field + Refresh button
        static char dirBuf[260];
        std::snprintf(dirBuf, sizeof(dirBuf), "%s", s_BinDir.c_str());
        if (ImGui::InputText("Folder", dirBuf, IM_ARRAYSIZE(dirBuf))) {
            s_BinDir = dirBuf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            RefreshBinList();
        }
        if (s_FirstScan) { RefreshBinList(); s_FirstScan = false; }

        // Combo + Load
        if (s_BinFiles.empty()) {
            ImGui::TextDisabled("No .bin files found in folder.");
        }
        else {
            const char* preview = (s_Selected >= 0 && s_Selected < (int)s_BinFiles.size())
                ? s_BinFiles[s_Selected].c_str() : "(none)";

            if (ImGui::BeginCombo("File", preview)) {
                for (int i = 0; i < (int)s_BinFiles.size(); ++i) {
                    const bool sel = (i == s_Selected);
                    if (ImGui::Selectable(s_BinFiles[i].c_str(), sel)) s_Selected = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const bool canLoad = (s_Selected >= 0 && s_Selected < (int)s_BinFiles.size());
            if (!canLoad) ImGui::BeginDisabled();

            if (ImGui::Button("Load Navmesh", ImVec2(-1, 28))) {
                const std::string full = (std::filesystem::path(s_BinDir) / s_BinFiles[s_Selected]).string();
                bool ok = false;

                if (m_App) {
                    if (auto* nav = m_App->GetNavSystem())
                        ok = nav->reloadFromFile(full);
                }

                if (ok) BOOM_INFO("[Nav] Loaded: {}", full);
                else    BOOM_ERROR("[Nav] Failed to load: {}", full);
            }

            if (!canLoad) ImGui::EndDisabled();
        }

        // --- Debug toggles ---
        ImGui::Separator();
        ImGui::TextUnformatted("Debug Visualization");
        if (!m_Ctx) {
            ImGui::TextDisabled("No context");
        }
        else {
            ImGui::Checkbox("Draw Navmesh (edges + centroids)", &m_Ctx->ShowNavDebug);
            ImGui::SameLine();
            // add more toggles if needed…
        }

        ImGui::End();
    }



} // namespace EditorUI
