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
#include "GlobalConstants.h"

#include <memory>
#include <string>
#include <utility>

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

            // Position the light camera at distance along the light direction
            // For directional lights, we look "backwards" from the direction
            float lightDistance = 10.0f; // Distance from scene center
            glm::vec3 lightPos = -lightDir * lightDistance; // Negative because we want to look towards the scene
            glm::vec3 sceneCenter = glm::vec3(0.0f, 0.0f, 0.0f); // Look at scene origin

            // Calculate view direction (from camera to scene)
            glm::vec3 viewDir = glm::normalize(sceneCenter - lightPos);

            // Calculate up vector (must not be parallel to view direction)
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (glm::abs(glm::dot(viewDir, up)) > 0.99f) {
                // If viewing straight up/down, use right vector as up
                up = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            // prepare projection and view mtx
            float orthoSize = 10.0f; // Shadow coverage area
            auto proj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.f, 25.0f);
            auto view = glm::lookAt(lightPos, sceneCenter, up);

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

            float nearPlane = 0.1f;
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
        }

        BOOM_INLINE glm::vec3 GetCameraPosition() const { return m_CameraPosition; }

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

    public: // ---------------------- Frame lifecycle ----------------------
        BOOM_INLINE void NewFrame() {
            pbrShader->showDither = showLowPoly;
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
                bloom->Compute(lowPolyFrame->GetBrightnessMap(), 10);
            }
            else {
                frame->End();
                bloom->Compute(frame->GetBrightnessMap(), 10);
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

            if (showLowPoly) {
                if (m_TouchViewport) glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
                finalShader->Render(lowPolyFrame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom);
            }
            else {
                if (m_TouchViewport) glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
                //shadowShader->GetDepthMap() //frame->GetTexture()
                finalShader->Render(isDepthBufferView ? shadowShader->GetDepthMap() : frame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom); // toggle bloom inside final if needed
            }
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
    public:  // ---------------------- ImGui-exposed toggles ----------------
        bool isDrawDebugMode{};
        bool showLowPoly{};
        bool showNormalTexture{};
        bool enabledBloom{};
        bool isPickIgnoreGUI{};
        bool isDepthBufferView{};
        bool enableTransparentBackfaceCulling{ true };
    };

} // namespace Boom
