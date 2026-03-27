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

// Status string for UI display
static std::string g_NavBakeStatus;

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
            g_NavBakeStatus = "ERROR: No AppContext/assets available";
            return out;
        }

        // Always use the live scene registry from AppContext (avoids pointing at the wrong registry).
        entt::registry& reg = m_Ctx->scene;

        size_t entCount = 0, usedCount = 0, triCount = 0;

        // Do NOT require TransformComponent in the view. We'll read it if present.
        auto view = reg.view<Boom::ModelComponent>();

        for (auto e : view) {
            ++entCount;
            const auto& mc = view.get<Boom::ModelComponent>(e);

            if (mc.modelID == Boom::EMPTY_ASSET) {
                continue;
            }

            // Build transform matrix (identity if no TransformComponent)
            glm::mat4 M(1.f);
            if (reg.all_of<Boom::TransformComponent>(e)) {
                const auto& tr = reg.get<Boom::TransformComponent>(e);
                M = tr.transform.Matrix();
            }

            // Pull model from AssetRegistry
            auto* modelAsset = m_Ctx->assets->TryGet<Boom::ModelAsset>(mc.modelID);
            if (!modelAsset || !modelAsset->data) {
                continue;
            }

            // Check if it's a StaticModel
            auto staticModel = std::dynamic_pointer_cast<Boom::StaticModel>(modelAsset->data);
            if (!staticModel) {
                continue;
            }

            // Your submesh API (from your screenshot: .vtx (ShadedVert) / .idx)
            const auto& submeshes = staticModel->GetMeshData();

            if (submeshes.empty()) {
                continue;
            }

            for (size_t subIdx = 0; subIdx < submeshes.size(); ++subIdx) {
                const auto& sub = submeshes[subIdx];
                const auto& positions = sub.vtx; // std::vector<Boom::ShadedVert> (has glm::vec3 pos)
                const auto& indices = sub.idx; // u16/u32

                if (positions.empty() || indices.size() < 3) {
                    continue;
                }

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

        // Build status string for UI
        char summary[256];
        snprintf(summary, sizeof(summary), "Collected: %zu triangles from %zu submeshes",
            triCount, usedCount);
        g_NavBakeStatus = summary;

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
                g_NavBakeStatus = "ERROR: No registry available";
            }
            else {
                std::string err;
                RecastBakeInput in = GatherTriangleSoupFromScene(m_Ctx ? m_Ctx->scene : *m_Reg);
                if (in.verts.empty() || in.tris.empty()) {
                    g_NavBakeStatus = "FAILED: No geometry gathered";
                }
                else if (!RecastBakeToFile(in, m_Cfg, m_OutPath, &err)) {
                    g_NavBakeStatus = "FAILED: " + err;
                }
                else {
                    g_NavBakeStatus = "SUCCESS: Saved to " + m_OutPath;

                    // Hot-reload into live runtime via AppInterface
                    if (m_App) {
                        if (auto* nav = m_App->GetNavSystem()) {
                            nav->reloadFromFile(m_OutPath);
                        }
                    }
                }
            }
        }

        // Show status
        if (!g_NavBakeStatus.empty()) {
            ImGui::Separator();
            if (g_NavBakeStatus.find("SUCCESS") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", g_NavBakeStatus.c_str());
            }
            else if (g_NavBakeStatus.find("FAILED") != std::string::npos || g_NavBakeStatus.find("ERROR") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", g_NavBakeStatus.c_str());
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", g_NavBakeStatus.c_str());
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
                const std::string full =
                    (std::filesystem::path(s_BinDir) / s_BinFiles[s_Selected]).string();
                bool ok = false;

                if (m_App) {
                    if (auto* nav = m_App->GetNavSystem())
                        ok = nav->reloadFromFile(full);
                }

                if (ok) {
                    g_NavBakeStatus = "Loaded: " + s_BinFiles[s_Selected];

                    if (m_Ctx) {
                        auto& reg = m_Ctx->scene;
                        entt::entity settings = Boom::GetOrCreateSceneSettings(reg);
                        auto& sn = reg.get<Boom::SceneNavmeshComponent>(settings);
                        sn.navmeshFile = full;
                    }
                }
                else {
                    g_NavBakeStatus = "FAILED: Could not load " + s_BinFiles[s_Selected];
                }
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
            ImGui::Checkbox("Show AI Vision Cones", &m_Ctx->ShowVisionCones);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Show enemy vision cones (yellow = patrolling, red = chasing).\nPer-enemy cone can be toggled via the AI Component inspector.");
        }

        ImGui::End();
    }



} // namespace EditorUI
