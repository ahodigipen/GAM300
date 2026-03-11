#pragma once
#include "Core.h"
#include "Graphics/Buffers/Frame.h"
#include "Shaders/PBR.h"
#include "Shaders/Final.h"
#include "Shaders/SkyMap.h"
#include "Shaders/Skybox.h"
#include "Shaders/Bloom.h"
#include "Shaders/Shadow.h"
#include "Shaders/Color.h"
#include "Shaders/PickingShader.h"
#include "Graphics/Instancing/InstanceManager.h"
#include "GlobalConstants.h"

#include <memory>
#include <string>
#include <utility>
#include <unordered_map>

namespace Boom {

    struct GraphicsRenderer {
    public:
        GraphicsRenderer() = delete;

        BOOM_INLINE GraphicsRenderer(int32_t w, int32_t h)
        {
            // --- GL state that should be persistent for this context ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // smooth skybox edges

            // --- GLEW init ---
            glewExperimental = GL_TRUE;
            GLenum err = glewInit();
#ifdef BOOM_ENABLE_LOG
            if (GLEW_OK != err) {
                BOOM_FATAL("Unable to initialize GLEW - error: {} abort program.\n", GetGlewString(err, true));
                std::exit(EXIT_FAILURE);
            }
            if (GLEW_VERSION_4_5) {
                BOOM_INFO("Using glew version: {}", GetGlewString(GLEW_VERSION));
            }
            else {
                BOOM_WARN("Warning: The driver may lack full compatibility with OpenGL 4.5, potentially limiting access to advanced features.");
            }
            PrintSpecs();
#else
            (void)err;
#endif

            // --- Shaders / passes ---
            skyMapShader = std::make_unique<SkyMapShader>("skymap.glsl");
            skyBoxShader = std::make_unique<SkyboxShader>("skybox.glsl");
            finalShader = std::make_unique<FinalShader>("final.glsl", w, h);
            pbrShader = std::make_unique<PBRShader>("pbr.glsl");
            bloom = std::make_unique<BloomShader>("bloom.glsl", w, h);
            shadowShader = std::make_unique<ShadowShader>("shadow.glsl");
            colorShader = std::make_unique<ColorShader>("color2D.glsl", glm::vec4(1.f));
            color3DShader = std::make_unique<Color3DShader>("color3D.glsl", glm::vec4(1.f));
            pickShader = std::make_unique<PickingShader>("picking.glsl");
            InitLightUBOs();
            // --- Framebuffers ---
            frame = std::make_unique<FrameBuffer>(w, h, /*lowPoly=*/false);
            lowPolyFrame = std::make_unique<FrameBuffer>(w, h, /*lowPoly=*/true);
            oPickFrame = std::make_unique<FrameBuffer>(w, h, false, GL_R32UI, GL_RED_INTEGER);

            // --- Meshes ---
            skyboxMesh = CreateSkyboxMesh();

            // --- Instancing ---
            m_InstanceManager = std::make_unique<InstanceManager>();

            // --- Internal bookkeeping ---
            m_Width = w;
            m_Height = h;
            m_AspectOverride = -1.0f;
            m_TouchViewport = true;     // default for non-Imgui presentation
        }

        BOOM_INLINE ~GraphicsRenderer() {}

    public: // ----------------------- Lights -----------------------
        // The PBR shader will ignore lights above MAX_LIGHTS (in-shader define)
        BOOM_INLINE void InitLightUBOs() {
            glGenBuffers(1, &m_PointLightUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_PointLightUBO);
            glBufferData(GL_UNIFORM_BUFFER,
                MAX_POINT_LIGHTS * sizeof(GPUPointLight),
                nullptr,
                GL_DYNAMIC_DRAW);

            glGenBuffers(1, &m_DirLightUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_DirLightUBO);
            glBufferData(GL_UNIFORM_BUFFER,
                MAX_DIR_LIGHTS * sizeof(GPUDirLight),
                nullptr,
                GL_DYNAMIC_DRAW);

            glGenBuffers(1, &m_SpotLightUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_SpotLightUBO);
            glBufferData(GL_UNIFORM_BUFFER,
                MAX_SPOT_LIGHTS * sizeof(GPUSpotLight),
                nullptr,
                GL_DYNAMIC_DRAW);

            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
       /* template <class TYPE>*/
    /*    BOOM_INLINE void SetLight(TYPE const& light, Transform3D const& transform, uint32_t index) {
            pbrShader->SetLight<TYPE>(light, transform, index);
        }
        BOOM_INLINE void SetSpotLightCount(int32_t count) { pbrShader->SetSpotLightCount(count); }
        BOOM_INLINE void SetPointLightCount(int32_t count) { pbrShader->SetPointLightCount(count); }
        BOOM_INLINE void SetDirectionalLightCount(int32_t count) { pbrShader->SetDirectionalLightCount(count); }*/
        BOOM_INLINE void UploadPointLights(const std::vector<GPUPointLight>& lights, int count) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_PointLightUBO);
            if (count > 0) {
                glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    count * sizeof(GPUPointLight),
                    lights.data());
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            pbrShader->SetUniform(pbrShader->GetUniformVar("noPointLight"), count);
        }

        BOOM_INLINE void UploadDirLights(const std::vector<GPUDirLight>& lights, int count) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_DirLightUBO);
            if (count > 0) {
                glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    count * sizeof(GPUDirLight),
                    lights.data());
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            pbrShader->SetUniform(pbrShader->GetUniformVar("noDirLight"), count);
        }

        BOOM_INLINE void UploadSpotLights(const std::vector<GPUSpotLight>& lights, int count) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_SpotLightUBO);
            if (count > 0) {
                glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    count * sizeof(GPUSpotLight),
                    lights.data());
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            pbrShader->SetUniform(pbrShader->GetUniformVar("noSpotLight"), count);
        }

        BOOM_INLINE void DrawShadow(Model3D& model, Transform3D& transform, std::vector<glm::mat4>& joints) {
            if (!joints.empty()) shadowShader->SetJoints(joints);
            shadowShader->Draw(model, transform);
        }

        BOOM_INLINE void DrawShadow(Model3D& model, Transform3D& transform, std::vector<glm::mat4>& joints, const PbrMaterial& material) {
            if (!joints.empty()) shadowShader->SetJoints(joints);
            shadowShader->Draw(model, transform, material);
        }
        BOOM_INLINE void BeginShadowPass(const glm::vec3& LightRotation, bool enableShadows = true)
        {
            // Convert Euler angles (degrees) to direction vector
            glm::vec3 eulerRadians = glm::radians(LightRotation);
            glm::quat rotation = glm::quat(eulerRadians);
            glm::vec3 lightDir = rotation * glm::vec3(0.0f, 0.0f, -1.0f); // Forward direction

            // Use camera position as shadow focus point (camera-centric shadows)
            // This ensures shadows are always rendered around where the player is looking
            float lightDistance = 50.0f;
            glm::vec3 shadowFocus = m_CameraPosition; // Follow the camera
            glm::vec3 lightPos = shadowFocus - lightDir * lightDistance;

            // Calculate up vector (must not be parallel to light direction)
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
                // If light is pointing straight up/down, use forward as up
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // prepare projection and view mtx
            // Smaller values for better shadow quality (less pixelation)
            float orthoSize = 50.0f; // Shadow coverage area (100x100 units total)
            float nearPlane = 1.0f;
            float farPlane = lightDistance * 2.5f; // 125.0f - tighter range for better depth precision
            auto proj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
            auto view = glm::lookAt(lightPos, shadowFocus, up);

            // compute light space
            auto lightSpaceMtx = proj * view;

            // set pbr shader light space mtx and depth map
            pbrShader->Use();
            pbrShader->SetLightSpaceMatrix(lightSpaceMtx);
            pbrShader->SetEnvMaps(0, 0, 0, shadowShader->GetDepthMap());
            pbrShader->SetShadowsEnabled(enableShadows);

            // begin depth rendering
            shadowShader->BeginFrame(lightSpaceMtx);
        }
        BOOM_INLINE void EndShadowPass()
        {
            // End shadow rendering and restore to main framebuffer
            shadowShader->EndFrame();

            // Rebind the main framebuffer that was active from NewFrame()
            if (showLowPoly) {
                lowPolyFrame->SBind();
            } else {
                frame->SBind();
            }

            // Re-enable depth test (SBind doesn't do this)
            glEnable(GL_DEPTH_TEST);

            // Re-bind the PBR shader for subsequent rendering
            pbrShader->Use();
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_PointLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_DirLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_SpotLightUBO);
        }

        // === Spot Light Shadow Functions ===
        BOOM_INLINE void BeginSpotShadowPass(int index, const glm::vec3& position, const glm::vec3& rotation, float cutOffAngle, float range)
        {
            if (index < 0 || index >= MAX_SPOT_SHADOW_LIGHTS) return;

            // Convert Euler angles (degrees) to direction vector
            glm::vec3 eulerRadians = glm::radians(rotation);
            glm::quat rot = glm::quat(eulerRadians);
            glm::vec3 lightDir = rot * glm::vec3(0.0f, 0.0f, -1.0f);

            // Calculate up vector (must not be parallel to light direction)
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
                up = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            // Perspective projection based on spot light cone angle
            // Use outer cutoff angle for FOV, doubled because cutoff is half-angle
            float fov = glm::radians(cutOffAngle * 2.0f);
            fov = glm::clamp(fov, glm::radians(10.0f), glm::radians(170.0f)); // Clamp to reasonable range

            float nearPlane = 0.5f;
            float farPlane = range > 0.0f ? range : 50.0f;

            glm::mat4 proj = glm::perspective(fov, 1.0f, nearPlane, farPlane);
            glm::mat4 view = glm::lookAt(position, position + lightDir, up);
            glm::mat4 lightSpaceMtx = proj * view;

            // Begin shadow rendering for this spot light
            shadowShader->BeginSpotLightFrame(index, lightSpaceMtx);
        }

        BOOM_INLINE void EndSpotShadowPass()
        {
            shadowShader->EndSpotLightFrame();

            // Rebind the main framebuffer
            if (showLowPoly) {
                lowPolyFrame->SBind();
            } else {
                frame->SBind();
            }

            glEnable(GL_DEPTH_TEST);
        }

        BOOM_INLINE void UploadSpotShadowData(int count)
        {
            pbrShader->Use();
            pbrShader->SetSpotShadowCount(count);

            // Upload all spot light shadow maps and matrices
            for (int i = 0; i < count && i < MAX_SPOT_SHADOW_LIGHTS; ++i)
            {
                pbrShader->SetSpotShadowMap(i, shadowShader->GetSpotDepthMap(i));
                pbrShader->SetSpotLightSpaceMatrix(i, shadowShader->GetSpotLightSpaceMatrix(i));
            }
        }

        BOOM_INLINE ShadowShader* GetShadowShader() { return shadowShader.get(); }

    public: // ----------------------- Skybox -----------------------
        BOOM_INLINE void InitSkybox(Skybox& sky, Texture const& tex, int32_t size) {
            sky.cubeMap = skyMapShader->Generate(tex, skyboxMesh, size);
        }
        BOOM_INLINE void DrawSkybox(Skybox const& sky, Transform3D const& transform) {
            skyBoxShader->Draw(skyboxMesh, sky.cubeMap, transform);
            pbrShader->SetEnvMaps(0, 0, 0, shadowShader->GetDepthMap());
        }

    public: // -------------------- Animator (skinning) -------------
        BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms, bool isPick = false) {
            if (!isPick) pbrShader->SetJoints(transforms);
            else pickShader->SetJoints(transforms);
        }

    public: // -------- Camera / draw (uses aspect override if set) --------
        BOOM_INLINE void SetPickCamera(Camera3D& cam, Transform3D const& transform) {
            const float aspect =
                (m_AspectOverride > 0.0f) ? m_AspectOverride
                : frame->Ratio();

            pickShader->SetCamera(cam, transform, aspect);
        }
        BOOM_INLINE void SetCamera(Camera3D& cam, Transform3D const& transform) {
            const float aspect =
                (m_AspectOverride > 0.0f) ? m_AspectOverride
                : frame->Ratio();
            pbrShader->SetCamera(cam, transform, aspect);
            skyBoxShader->SetCamera(cam, transform, aspect);
            color3DShader->SetCamera(cam, transform, aspect);
            pbrShader->Use();
            m_CameraPosition = transform.translate;
            // Cache for volumetric fog depth reconstruction
            m_NearPlane = cam.nearPlane;
            m_FarPlane = cam.farPlane;
            m_InvViewProj = glm::inverse(cam.Frustum(transform, aspect));
        }

        BOOM_INLINE glm::vec3 GetCameraPosition() const { return m_CameraPosition; }

        // Updates just the camera position without setting up shaders
        // Call this BEFORE shadow pass to ensure shadow frustum is centered correctly
        BOOM_INLINE void SetCameraPositionOnly(const glm::vec3& position) {
            m_CameraPosition = position;
        }

        BOOM_INLINE void Draw(Mesh3D const& mesh, Transform3D const& transform) {
            pbrShader->Draw(mesh, transform);
        }

        BOOM_INLINE void Draw(Model3D const& model, Transform3D const& transform, PbrMaterial const& material = {}) {
            if (isDrawDebugMode) {
                pbrShader->DrawDebug(model, transform, material.albedo, showNormalTexture);
            }
            else {
                pbrShader->Draw(model, transform, material, showNormalTexture);
            }
        }

        BOOM_INLINE void DrawPick(Model3D const& model, Transform3D const& transform) {
            pickShader->Draw(model, transform);
        }
        BOOM_INLINE void DrawPick(Transform3D const& transform) {
            pickShader->Draw(transform);
        }
        BOOM_INLINE void DrawPick(Transform2D const& transform) {
            if (isPickIgnoreGUI) return;
            pickShader->Draw(transform);
        }

        BOOM_INLINE void DrawQuad(Texture const& tex, Transform3D const& transform, glm::vec4 col = glm::vec4{ 1.f }) {
            color3DShader->ChangeColor(col);
            color3DShader->Show(*tex.get(), transform);
        }
        BOOM_INLINE void DrawQuad(Texture const& tex, Transform2D const& transform, glm::vec4 col = glm::vec4{ 1.f }) {
            colorShader->ChangeColor(col);
            colorShader->Show(*tex.get(), transform);
        }

        // Raw texture ID overloads (for video playback, dynamic textures, etc.)
        BOOM_INLINE void DrawQuadRaw(uint32_t textureId, Transform3D const& transform, glm::vec4 col = glm::vec4{ 1.f }) {
            color3DShader->ChangeColor(col);
            color3DShader->Show(textureId, transform);
        }
        BOOM_INLINE void DrawQuadRaw(uint32_t textureId, Transform2D const& transform, glm::vec4 col = glm::vec4{ 1.f }) {
            colorShader->ChangeColor(col);
            colorShader->Show(textureId, transform);
        }

        BOOM_INLINE float Aspect() const { return frame->Ratio(); } // kept for backward compatibility

    public: // ---------------------- Instanced Rendering ----------------------
        /**
         * @brief Begin collecting instances for this frame.
         * Call at start of render phase, before adding any instances.
         */
        BOOM_INLINE void BeginInstanceCollection() {
            m_InstanceManager->BeginFrame();
        }

        /**
         * @brief Add a static instance to be rendered this frame.
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param isAnimated If true, use AddAnimatedInstance instead (returns false)
         * @return true if batched, false if should use immediate draw
         */
        BOOM_INLINE bool AddInstance(AssetID modelID, AssetID materialID,
                                    const glm::mat4& worldMatrix, bool isAnimated) {
            return m_InstanceManager->AddInstance(modelID, materialID, worldMatrix, isAnimated);
        }

        /**
         * @brief Add an animated instance to be rendered this frame.
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param jointMatrices Current frame's joint matrices
         * @return true if batched
         */
        BOOM_INLINE bool AddAnimatedInstance(AssetID modelID, AssetID materialID,
                                            const glm::mat4& worldMatrix,
                                            const std::vector<glm::mat4>& jointMatrices) {
            return m_InstanceManager->AddAnimatedInstance(modelID, materialID, worldMatrix, jointMatrices);
        }

        /**
         * @brief Add a transparent instance to be rendered this frame.
         *
         * Transparent instances are batched by model+material and sorted by distance.
         * Within a batch, instances are rendered together (not individually sorted).
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param distanceToCamera Distance from camera for batch sorting
         * @return true if batched
         */
        BOOM_INLINE bool AddTransparentInstance(AssetID modelID, AssetID materialID,
                                               const glm::mat4& worldMatrix, float distanceToCamera) {
            return m_InstanceManager->AddTransparentInstance(modelID, materialID, worldMatrix, distanceToCamera);
        }

        /**
         * @brief Render all collected instance batches (static and animated).
         *
         * @param assets Asset registry to look up models and materials
         */
        template<typename AssetRegistryT>
        BOOM_INLINE void RenderInstancedBatches(AssetRegistryT& assets) {
            // Upload all instance transforms and joint matrices to GPU
            m_InstanceManager->UploadBatches();

            size_t totalStatic = m_InstanceManager->GetTotalInstances();
            size_t totalAnimated = m_InstanceManager->GetTotalAnimatedInstances();

            if (totalStatic == 0 && totalAnimated == 0) return;

            // Bind both SSBOs (transform and joint)
            m_InstanceManager->BindAllSSBOs();

            // === Render Single-Sided Static Batches (GL_CULL_FACE already ON from caller) ===
            if (totalStatic > 0) {
                for (const auto& [key, batch] : m_InstanceManager->GetBatches()) {
                    if (batch.IsEmpty()) continue;

                    auto* matAsset = batch.materialID != EMPTY_ASSET
                        ? assets.template TryGet<MaterialAsset>(batch.materialID) : nullptr;
                    if (matAsset && matAsset->doubleSided) continue; // handled in second pass

                    auto* modelAsset = assets.template TryGet<ModelAsset>(batch.modelID);
                    if (!modelAsset || !modelAsset->data) continue;

                    PbrMaterial material{};
                    if (matAsset) {
                        assets.ResolveMaterialTextures(matAsset);
                        material = matAsset->data;
                    }

                    pbrShader->DrawInstanced(modelAsset->data, material,
                                            static_cast<uint32_t>(batch.Count()),
                                            static_cast<uint32_t>(batch.ssboOffset),
                                            showNormalTexture);
                }

                // === Render Double-Sided Static Batches (disable culling for this group) ===
                bool disabledCulling = false;
                for (const auto& [key, batch] : m_InstanceManager->GetBatches()) {
                    if (batch.IsEmpty()) continue;

                    auto* matAsset = batch.materialID != EMPTY_ASSET
                        ? assets.template TryGet<MaterialAsset>(batch.materialID) : nullptr;
                    if (!matAsset || !matAsset->doubleSided) continue;

                    if (!disabledCulling) {
                        glDisable(GL_CULL_FACE);
                        disabledCulling = true;
                    }

                    auto* modelAsset = assets.template TryGet<ModelAsset>(batch.modelID);
                    if (!modelAsset || !modelAsset->data) continue;

                    assets.ResolveMaterialTextures(matAsset);
                    pbrShader->DrawInstanced(modelAsset->data, matAsset->data,
                                            static_cast<uint32_t>(batch.Count()),
                                            static_cast<uint32_t>(batch.ssboOffset),
                                            showNormalTexture);
                }
                if (disabledCulling) {
                    glEnable(GL_CULL_FACE);
                }
            }

            // === Render Animated Batches ===
            if (totalAnimated > 0) {
                for (const auto& [key, batch] : m_InstanceManager->GetAnimatedBatches()) {
                    if (batch.IsEmpty()) continue;

                    // Get model
                    auto* modelAsset = assets.template TryGet<ModelAsset>(batch.modelID);
                    if (!modelAsset || !modelAsset->data) continue;

                    // Get material
                    PbrMaterial material{};
                    if (batch.materialID != EMPTY_ASSET) {
                        auto* matAsset = assets.template TryGet<MaterialAsset>(batch.materialID);
                        if (matAsset) {
                            assets.ResolveMaterialTextures(matAsset);
                            material = matAsset->data;
                        }
                    }

                    // Draw all animated instances in this batch (mode 2)
                    // baseInstance = transform SSBO offset, jointBaseInstance = joint SSBO offset / joints per instance
                    pbrShader->DrawAnimatedInstanced(modelAsset->data, material,
                                                     static_cast<uint32_t>(batch.Count()),
                                                     static_cast<uint32_t>(batch.transformSsboOffset),
                                                     static_cast<uint32_t>(batch.jointSsboOffset / MAX_ANIMATED_JOINTS),
                                                     showNormalTexture);
                }
            }
        }

        /**
         * @brief Render all collected transparent instance batches.
         *
         * Call this AFTER RenderInstancedBatches() and with blending enabled.
         * Batches are rendered in back-to-front order (sorted during UploadBatches).
         *
         * @param assets Asset registry to look up models and materials
         */
        template<typename AssetRegistryT>
        BOOM_INLINE void RenderTransparentBatches(AssetRegistryT& assets) {
            size_t totalTransparent = m_InstanceManager->GetTotalTransparentInstances();
            if (totalTransparent == 0) return;

            // SSBO should already be bound from RenderInstancedBatches
            // Just ensure it's bound in case this is called standalone
            m_InstanceManager->BindSSBO(3);

            // Get sorted batches (back-to-front order)
            const auto& sortedBatches = m_InstanceManager->GetSortedTransparentBatches();

            for (const auto* batch : sortedBatches) {
                if (batch->IsEmpty()) continue;

                // Get model
                auto* modelAsset = assets.template TryGet<ModelAsset>(batch->modelID);
                if (!modelAsset || !modelAsset->data) continue;

                // Get material
                PbrMaterial material{};
                if (batch->materialID != EMPTY_ASSET) {
                    auto* matAsset = assets.template TryGet<MaterialAsset>(batch->materialID);
                    if (matAsset) {
                        assets.ResolveMaterialTextures(matAsset);
                        material = matAsset->data;
                    }
                }

                // Draw all transparent instances in this batch
                pbrShader->DrawTransparentInstanced(modelAsset->data, material,
                                                    static_cast<uint32_t>(batch->Count()),
                                                    static_cast<uint32_t>(batch->ssboOffset),
                                                    showNormalTexture);
            }
        }

        /**
         * @brief Render all collected instance batches as shadow casters.
         *
         * Uses the shadow shader with instanced draw calls instead of per-entity draws.
         * Call after BeginShadowPass() and after collecting instances via AddInstance/AddAnimatedInstance.
         *
         * @param assets Asset registry to look up models and materials
         */
        template<typename AssetRegistryT>
        BOOM_INLINE void RenderShadowInstancedBatches(AssetRegistryT& assets) {
            m_InstanceManager->UploadBatches();

            size_t totalStatic = m_InstanceManager->GetTotalInstances();
            size_t totalAnimated = m_InstanceManager->GetTotalAnimatedInstances();

            if (totalStatic == 0 && totalAnimated == 0) return;

            // Bind SSBOs for the shadow shader to read
            m_InstanceManager->BindAllSSBOs();

            // Ensure shadow shader is active
            shadowShader->Use();

            // === Render Static Shadow Batches ===
            if (totalStatic > 0) {
                for (const auto& [key, batch] : m_InstanceManager->GetBatches()) {
                    if (batch.IsEmpty()) continue;

                    auto* modelAsset = assets.template TryGet<ModelAsset>(batch.modelID);
                    if (!modelAsset || !modelAsset->data) continue;

                    // Check if material has opacity for shadow dithering
                    if (batch.materialID != EMPTY_ASSET) {
                        auto* matAsset = assets.template TryGet<MaterialAsset>(batch.materialID);
                        if (matAsset) {
                            assets.ResolveMaterialTextures(matAsset);
                            shadowShader->DrawInstanced(modelAsset->data,
                                                        static_cast<uint32_t>(batch.Count()),
                                                        static_cast<uint32_t>(batch.ssboOffset),
                                                        matAsset->data);
                            continue;
                        }
                    }

                    shadowShader->DrawInstanced(modelAsset->data,
                                                static_cast<uint32_t>(batch.Count()),
                                                static_cast<uint32_t>(batch.ssboOffset));
                }
            }

            // === Render Animated Shadow Batches ===
            if (totalAnimated > 0) {
                for (const auto& [key, batch] : m_InstanceManager->GetAnimatedBatches()) {
                    if (batch.IsEmpty()) continue;

                    auto* modelAsset = assets.template TryGet<ModelAsset>(batch.modelID);
                    if (!modelAsset || !modelAsset->data) continue;

                    uint32_t jointBase = static_cast<uint32_t>(batch.jointSsboOffset / MAX_ANIMATED_JOINTS);

                    if (batch.materialID != EMPTY_ASSET) {
                        auto* matAsset = assets.template TryGet<MaterialAsset>(batch.materialID);
                        if (matAsset) {
                            assets.ResolveMaterialTextures(matAsset);
                            shadowShader->DrawAnimatedInstanced(modelAsset->data,
                                                                static_cast<uint32_t>(batch.Count()),
                                                                static_cast<uint32_t>(batch.transformSsboOffset),
                                                                jointBase,
                                                                matAsset->data);
                            continue;
                        }
                    }

                    shadowShader->DrawAnimatedInstanced(modelAsset->data,
                                                        static_cast<uint32_t>(batch.Count()),
                                                        static_cast<uint32_t>(batch.transformSsboOffset),
                                                        jointBase);
                }
            }
        }

        /**
         * @brief Get instancing statistics for debugging/profiling.
         */
        BOOM_INLINE size_t GetInstanceBatchCount() const {
            return m_InstanceManager->GetActiveBatchCount();
        }

        BOOM_INLINE size_t GetAnimatedInstanceBatchCount() const {
            return m_InstanceManager->GetActiveAnimatedBatchCount();
        }

        BOOM_INLINE size_t GetTransparentInstanceBatchCount() const {
            return m_InstanceManager->GetActiveTransparentBatchCount();
        }

        BOOM_INLINE size_t GetTotalInstanceCount() const {
            return m_InstanceManager->GetTotalInstances();
        }

        BOOM_INLINE size_t GetTotalAnimatedInstanceCount() const {
            return m_InstanceManager->GetTotalAnimatedInstances();
        }

        BOOM_INLINE size_t GetTotalTransparentInstanceCount() const {
            return m_InstanceManager->GetTotalTransparentInstances();
        }

    public: // ---------------------- Frame lifecycle ----------------------
        BOOM_INLINE void NewFrame() {
            pbrShader->showDither = showLowPoly;
            pbrShader->bloomThreshold = bloomThreshold;
            if (showLowPoly) {
                lowPolyFrame->Begin();
            }
            else {
                frame->Begin();
            }
            pbrShader->Use();
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_PointLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_DirLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_SpotLightUBO);
        }

        BOOM_INLINE void EndFrame() {
            pbrShader->UnUse();
            if (showLowPoly) {
                lowPolyFrame->End();
                bloom->Compute(lowPolyFrame->GetBrightnessMap(), bloomIterations);
            }
            else {
                frame->End();
                bloom->Compute(frame->GetBrightnessMap(), bloomIterations);
            }
        }

        BOOM_INLINE void StartPickFrame() {
            oPickFrame->Begin();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            GLuint zero = 0u;
            glClearBufferuiv(GL_COLOR, 0, &zero);

            pickShader->Use();
        }

        BOOM_INLINE void EndPickFrame() {
            pickShader->UnUse();
            oPickFrame->End();
        }

        BOOM_INLINE void SetPickUniform(uint32_t id) {
            pickShader->SetIDUniform(id);
        }

        // NOTE: When embedding in ImGui, prefer calling Final pass yourself via ImGui::Image with the texture returned by GetFrame().
        // If you still call ShowFrame() while embedded, set SetTouchViewport(false) so we don't stomp ImGui's viewport.
        BOOM_INLINE void ShowFrame() {
            if (showLowPoly) {
                if (m_TouchViewport) glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
                finalShader->Show(lowPolyFrame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
            }
            else {
                if (m_TouchViewport) glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
                finalShader->Show(frame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
            }
        }

        BOOM_INLINE void ShowFrame(bool useFBO) {
            // Blend proximity red tint on top of the warm tint before sending to shader
            glm::vec3 activeTint = tonemapWarmTint;
            if (proximityRedAmount > 0.0f) {
                float a = glm::clamp(proximityRedAmount, 0.0f, 1.0f);
                glm::vec3 redTint{ 1.5f, 0.55f, 0.55f };
                activeTint = glm::mix(tonemapWarmTint, redTint, a);
            }
            finalShader->SetToneMapping(tonemapExposure, tonemapGamma, activeTint);
            uint32_t depthTex = showLowPoly ? lowPolyFrame->GetDepthTexture() : frame->GetDepthTexture();
            finalShader->SetFog(enabledFog, fogColor, fogDensity, fogHeightFalloff, fogHeight,
                                m_InvViewProj, m_CameraPosition, depthTex);

            if (showLowPoly) {
                if (m_TouchViewport) glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
                finalShader->Render(lowPolyFrame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom, bloomIntensity);
            }
            else {
                if (m_TouchViewport) glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
                finalShader->Render(isDepthBufferView ? shadowShader->GetDepthMap() : frame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom, bloomIntensity);
            }
        }

    public: // ---- Full-res overlay (for text on top of composited frame) ----
        BOOM_INLINE void BeginFullResOverlay(bool useFBO) {
            glBindFramebuffer(GL_FRAMEBUFFER, useFBO ? finalShader->GetFBOId() : 0);
            glViewport(0, 0, m_Width, m_Height);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        BOOM_INLINE void EndFullResOverlay() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        BOOM_INLINE void SetSpriteToneMap(bool enable) {
            colorShader->SetToneMap(enable);
        }

    public: // ---------------------- Utilities / helpers -------------------
        BOOM_INLINE void Resize(int32_t w, int32_t h) {
            if (w <= 0 || h <= 0) return;

            m_Width = w;
            m_Height = h;

            frame->Resize(w, h);
            lowPolyFrame->Resize(w, h);

            // If FinalShader / BloomShader expose resizing, call them.
            if (finalShader) finalShader->Resize(w, h);
            if (bloom)       bloom->Resize(w, h);
        }

        BOOM_INLINE uint32_t GetFrame() const {
            return finalShader->GetMap();
        }

        BOOM_INLINE uint32_t GetFrameEnttID(int x, int y) const {
            uint32_t enttID{};
            oPickFrame->SBind();

            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &enttID);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            //BOOM_DEBUG("Mouse[{},{}] id[{}]", x, y, enttID);
            return enttID;
        }

        BOOM_INLINE float& DitherThreshold() { return pbrShader->ditherThreshold; }
        BOOM_INLINE float& AmbientStrength()
        {
            return pbrShader->ambientStrength;
        }
        BOOM_INLINE void SetShadowsEnabled(bool enabled)
        {
            pbrShader->Use();
            pbrShader->SetShadowsEnabled(enabled);
        }
        BOOM_INLINE float AspectRatio() const { return frame->Ratio(); }

        // --- Editor-facing toggles for embedded rendering ---
        BOOM_INLINE void SetAspectOverride(float aspect) { m_AspectOverride = aspect; } // <= set panel aspect here
        BOOM_INLINE void ClearAspectOverride() { m_AspectOverride = -1.0f; }
        BOOM_INLINE void SetTouchViewport(bool on) { m_TouchViewport = on; }
        BOOM_INLINE std::pair<int32_t, int32_t> BackbufferSize() const { return { m_Width, m_Height }; }
        BOOM_INLINE std::pair<int32_t, int32_t> GetPickSize() const { return { oPickFrame->GetWidth(), oPickFrame->GetHeight() }; }

    private:
        BOOM_INLINE void PrintSpecs() {
            GLint ver[2];
            glGetIntegerv(GL_MAJOR_VERSION, &ver[0]);
            glGetIntegerv(GL_MINOR_VERSION, &ver[1]);
            BOOM_INFO("GL Version: {}.{}", ver[0], ver[1]);

            GLboolean isDB;
            glGetBooleanv(GL_DOUBLEBUFFER, &isDB);
            if (isDB) BOOM_INFO("Current OpenGL Context is double-buffered");
            else      BOOM_INFO("Current OpenGL Context is not double-buffered");

            GLint output;
            glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &output);
            BOOM_INFO("Maximum Vertex Count: {}", output);
            glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &output);
            BOOM_INFO("Maximum Indices Count: {}", output);
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &output);
            BOOM_INFO("Maximum texture size: {}", output);

            GLint viewport[2];
            glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
            BOOM_INFO("Maximum Viewport Dimensions: {} x {}", viewport[0], viewport[1]);

            glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &output);
            BOOM_INFO("Maximum generic vertex attributes: {}", output);
            glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &output);
            BOOM_INFO("Maximum vertex buffer bindings: {}\n", output);
        }

        BOOM_INLINE std::string GetGlewString(GLenum name, bool isError = false) {
            if (isError) {
                return reinterpret_cast<char const*>(glewGetErrorString(name));
            }
            else {
                char const* ret{ reinterpret_cast<char const*>(glewGetString(name)) };
                return ret ? ret : "Unknown glewGetString(" + std::to_string(name) + ')';
            }
        }

    private: // ---------------------- Owned resources ----------------------
        std::unique_ptr<SkyMapShader>  skyMapShader;
        std::unique_ptr<SkyboxShader>  skyBoxShader;
        std::unique_ptr<FinalShader>   finalShader;
        std::unique_ptr<PBRShader>     pbrShader;
        std::unique_ptr<ShadowShader>  shadowShader;
        std::unique_ptr<PickingShader> pickShader;
        std::unique_ptr<FrameBuffer>   frame;
        std::unique_ptr<FrameBuffer>   lowPolyFrame;
        std::unique_ptr<FrameBuffer>   oPickFrame;
        std::unique_ptr<BloomShader>   bloom;
        std::unique_ptr<ColorShader>   colorShader;
        std::unique_ptr<Color3DShader> color3DShader;
        std::unique_ptr<InstanceManager> m_InstanceManager;
        SkyboxMesh                     skyboxMesh;

    private: // ---------------------- Internal state -----------------------
        int32_t m_Width{};
        int32_t m_Height{};
        float   m_AspectOverride{ -1.0f }; // < 0.0f => use FBO ratio
        bool    m_TouchViewport{ true };    // false when embedded in ImGui
        GLuint m_PointLightUBO = 0;
        GLuint m_DirLightUBO = 0;
        GLuint m_SpotLightUBO = 0;
        glm::vec3 m_CameraPosition{};
        // Fog depth reconstruction cache (updated each frame in SetCamera)
        float m_NearPlane{ 0.3f };
        float m_FarPlane{ 1000.f };
        glm::mat4 m_InvViewProj{ 1.0f };
    public:  // ---------------------- ImGui-exposed toggles ----------------
        bool isDrawDebugMode{};
        bool showLowPoly{};
        bool showNormalTexture{};
        bool enabledBloom{};
        bool isPickIgnoreGUI{};
        bool isDepthBufferView{};
        bool enableTransparentBackfaceCulling{ true };
        float bloomIntensity{ 1.0f };
        float bloomThreshold{ 1.0f };
        int bloomIterations{ 10 };
        float pointLightBloomMultiplier{ 1.0f };  // Global multiplier for point light bloom contribution

        // Tone mapping
        float     tonemapExposure{ 1.0f };
        float     tonemapGamma{ 2.2f };
        glm::vec3 tonemapWarmTint{ 1.08f, 0.98f, 0.82f };

        // Proximity danger red tint (set each frame from scripts; 0 = none, 1 = full red)
        float proximityRedAmount{ 0.0f };

        // Volumetric fog toggles
        bool enabledFog{};
        glm::vec3 fogColor{ 0.5f, 0.6f, 0.7f };
        float fogDensity{ 0.01f };
        float fogHeightFalloff{ 0.5f };
        float fogHeight{ 0.0f };

    public: // ---------------------- Material Preview ----------------------
        // Call this to reset the material preview (e.g., after scene change)
        BOOM_INLINE void ResetMaterialPreview() {
            m_MatPreviewFrame.reset();
            m_PreviewSphere = nullptr;
            // Clean up cached preview textures
            for (auto& [id, texId] : m_MaterialPreviewCache) {
                if (texId != 0) {
                    glDeleteTextures(1, &texId);
                }
            }
            m_MaterialPreviewCache.clear();
        }

        // Invalidate a specific material's cached preview (call when material changes)
        BOOM_INLINE void InvalidateMaterialPreview(uint64_t assetId) {
            auto it = m_MaterialPreviewCache.find(assetId);
            if (it != m_MaterialPreviewCache.end()) {
                if (it->second != 0) {
                    glDeleteTextures(1, &it->second);
                }
                m_MaterialPreviewCache.erase(it);
            }
        }

        BOOM_INLINE void InitMaterialPreview(Model3D sphereModel) {
            m_PreviewSphere = sphereModel;

            // Debug: check if model has meshes
            if (m_PreviewSphere) {
                BOOM_INFO("[Renderer] Sphere modelTransform: translate({},{},{}), scale({},{},{})",
                    m_PreviewSphere->modelTransform.translate.x,
                    m_PreviewSphere->modelTransform.translate.y,
                    m_PreviewSphere->modelTransform.translate.z,
                    m_PreviewSphere->modelTransform.scale.x,
                    m_PreviewSphere->modelTransform.scale.y,
                    m_PreviewSphere->modelTransform.scale.z);
            }

            // Save current FBO to restore later (important for ImGui compatibility)
            GLint prevFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

            // Create framebuffer using the existing FrameBuffer class
            m_MatPreviewFrame = std::make_unique<FrameBuffer>(m_MatPreviewSize, m_MatPreviewSize, false, GL_RGB, GL_RGB);

            // Restore previous FBO (FrameBuffer constructor binds to 0)
            glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
            BOOM_INFO("[MaterialPreview] Initialized successfully");
        }

        // Renders a sphere with the given material and returns the texture ID for ImGui
        // cameraYaw, cameraPitch: orbit camera angles (radians)
        // cameraDistance: distance from center
        BOOM_INLINE uint32_t RenderMaterialPreview(PbrMaterial const& material,
                                                    float cameraYaw = 0.0f,
                                                    float cameraPitch = 0.3f,
                                                    float cameraDistance = 2.5f) {
            if (!m_MatPreviewFrame || !m_PreviewSphere) {
                return 0;
            }

            // Clear any stale GL errors before we start
            while (glGetError() != GL_NO_ERROR) {}

            // Save current state FIRST before any GL calls
            GLint prevFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
            GLint prevViewport[4];
            glGetIntegerv(GL_VIEWPORT, prevViewport);

            // Bind preview framebuffer using SBind (doesn't clear automatically)
            m_MatPreviewFrame->SBind();

            // Clear with background color
            glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            // Activate shader before setting uniforms
            pbrShader->Use();

            // Bind light UBOs
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_PointLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_DirLightUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_SpotLightUBO);

            // Disable shadows for preview
            pbrShader->SetShadowsEnabled(false);

            // Set up lighting for preview - use local UBO updates to avoid affecting main scene
            std::vector<GPUDirLight> dirLights(1);
            dirLights[0].dir_intensity = glm::vec4(glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f)), 1.0f);
            dirLights[0].radiance = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            glBindBuffer(GL_UNIFORM_BUFFER, m_DirLightUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPUDirLight), dirLights.data());
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            pbrShader->SetUniform(pbrShader->GetUniformVar("noDirLight"), 1);

            // Clear point and spot lights for preview
            pbrShader->SetUniform(pbrShader->GetUniformVar("noPointLight"), 0);
            pbrShader->SetUniform(pbrShader->GetUniformVar("noSpotLight"), 0);

            float savedAmbient = pbrShader->ambientStrength;
            pbrShader->ambientStrength = 0.4f;

            // Calculate camera position (spherical coordinates)
            float camX = cameraDistance * cos(cameraPitch) * sin(cameraYaw);
            float camY = cameraDistance * sin(cameraPitch);
            float camZ = cameraDistance * cos(cameraPitch) * cos(cameraYaw);
            glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);
            glm::vec3 cameraTarget = glm::vec3(0.0f);

            // Set up camera matrices directly on PBR shader only (avoid touching other shaders)
            Camera3D camera{};
            camera.FOV = 45.0f;
            camera.nearPlane = 0.01f;
            camera.farPlane = 100.0f;

            glm::vec3 direction = glm::normalize(cameraTarget - cameraPos);
            float yaw = atan2(-direction.x, -direction.z);
            float pitch = asin(direction.y);

            Transform3D cameraTransform{};
            cameraTransform.translate = cameraPos;
            cameraTransform.rotate = glm::vec3(glm::degrees(pitch), glm::degrees(yaw), 0.0f);
            cameraTransform.scale = glm::vec3(1.0f);

            // Set camera directly on pbr shader with 1:1 aspect ratio
            pbrShader->SetCamera(camera, cameraTransform, 1.0f);
            pbrShader->Use(); // Ensure shader is active after SetCamera

            // Simple approach: just use identity transform and let the model's built-in transform work
            Transform3D modelTransform{};
            modelTransform.translate = glm::vec3(0.0f);
            modelTransform.rotate = glm::vec3(0.0f);
            modelTransform.scale = glm::vec3(1.0f);

            // Clear any existing joint transforms (for static models)
            std::vector<glm::mat4> emptyJoints;
            pbrShader->SetJoints(emptyJoints);

            // Draw sphere with material
            pbrShader->Draw(m_PreviewSphere, modelTransform, material, false);

            // Restore state (don't use End() as it binds to FBO 0)
            pbrShader->ambientStrength = savedAmbient;
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

            return m_MatPreviewFrame->GetTexture();
        }

        // Cached version - renders once per material and returns cached texture
        // Use this for ResourcePanel thumbnails to avoid re-rendering every frame
        BOOM_INLINE uint32_t RenderMaterialPreviewCached(uint64_t assetId,
                                                          PbrMaterial const& material,
                                                          float cameraYaw = 0.0f,
                                                          float cameraPitch = 0.3f,
                                                          float cameraDistance = 2.5f) {
            if (!m_MatPreviewFrame || !m_PreviewSphere) {
                return 0;
            }

            // Check if already cached
            auto it = m_MaterialPreviewCache.find(assetId);
            if (it != m_MaterialPreviewCache.end() && it->second != 0) {
                return it->second;
            }

            // Render to the shared FBO first
            uint32_t tempTex = RenderMaterialPreview(material, cameraYaw, cameraPitch, cameraDistance);
            if (tempTex == 0) return 0;

            // Create a new texture to store this material's preview
            uint32_t cachedTex = 0;
            glGenTextures(1, &cachedTex);
            glBindTexture(GL_TEXTURE_2D, cachedTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_MatPreviewSize, m_MatPreviewSize, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Copy from the shared FBO texture to the cached texture
            GLint prevFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

            // Create temporary FBOs for the copy operation
            GLuint readFBO, drawFBO;
            glGenFramebuffers(1, &readFBO);
            glGenFramebuffers(1, &drawFBO);

            // Bind source texture to read FBO
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempTex, 0);

            // Bind destination texture to draw FBO
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cachedTex, 0);

            // Blit (copy) the texture
            glBlitFramebuffer(0, 0, m_MatPreviewSize, m_MatPreviewSize,
                              0, 0, m_MatPreviewSize, m_MatPreviewSize,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);

            // Cleanup temporary FBOs
            glDeleteFramebuffers(1, &readFBO);
            glDeleteFramebuffers(1, &drawFBO);

            // Restore previous FBO
            glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

            // Cache and return
            m_MaterialPreviewCache[assetId] = cachedTex;
            return cachedTex;
        }

        BOOM_INLINE bool IsMaterialPreviewInitialized() const { return m_MatPreviewFrame != nullptr && m_PreviewSphere != nullptr; }
        BOOM_INLINE int32_t GetMaterialPreviewSize() const { return m_MatPreviewSize; }

    private: // ---------------------- Material Preview State ----------------
        std::unique_ptr<FrameBuffer> m_MatPreviewFrame;
        Model3D m_PreviewSphere;
        int32_t m_MatPreviewSize = 200;
        std::unordered_map<uint64_t, uint32_t> m_MaterialPreviewCache; // AssetID -> Cached texture ID
    };

} // namespace Boom
