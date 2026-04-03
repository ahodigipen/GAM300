#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"
#include "GlobalConstants.h"

namespace Boom {
	struct FinalShader : Shader {
		BOOM_INLINE FinalShader(std::string const& filename, int32_t width, int32_t height, glm::vec4 col = glm::vec4{ 1.f })
			: Shader{ filename }
			, map{ GetUniformVar("map") }
			, bloom{ GetUniformVar("u_bloom") }
			, depthMapLoc{ GetUniformVar("u_depth") }
			, bloomEnabled{ GetUniformVar("u_enableBloom") }
			, fadeAlpha{ GetUniformVar("u_fadeAlpha") }
			, bloomIntensity{ GetUniformVar("u_bloomIntensity") }
			, fogEnabledLoc{ GetUniformVar("u_enableFog") }
			, fogColorLoc{ GetUniformVar("u_fogColor") }
			, fogDensityLoc{ GetUniformVar("u_fogDensity") }
			, fogHeightFalloffLoc{ GetUniformVar("u_fogHeightFalloff") }
			, fogHeightLoc{ GetUniformVar("u_fogHeight") }
			, fogInvViewProjLoc{ GetUniformVar("u_invViewProj") }
			, fogCameraPosLoc{ GetUniformVar("u_cameraPos") }
			, gammaLoc{ GetUniformVar("u_gamma") }
			, exposureLoc{ GetUniformVar("u_exposure") }
			, warmTintLoc{ GetUniformVar("u_warmTint") }
			, vignetteEnabledLoc{ GetUniformVar("u_enableVignette") }
			, vignetteIntensityLoc{ GetUniformVar("u_vignetteIntensity") }
			, vignetteRadiusLoc{ GetUniformVar("u_vignetteRadius") }
			, quad{ CreateQuad2D() }
			, color{ col }
		{
			CreateBuffer(width, height);
		}
		BOOM_INLINE void SetSceneMap(uint32_t m,uint32_t blm) {
			//set color map
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m);
			SetUniform(map, 0);
			//set bloom map
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, blm);
			SetUniform(bloom, 1);
		}
		BOOM_INLINE void Show(uint32_t m,uint32_t blm,bool enabled) {
			Use();
			SetSceneMap(m,blm);
			glUniform1i(bloomEnabled, enabled ? 1 : 0);
			//SetUniform(colLoc, color);
			quad->Draw(GL_TRIANGLE_STRIP);
			UnUse();
		}

		BOOM_INLINE ~FinalShader()
		{
			glDeleteTextures(1, &m_Final);
			glDeleteFramebuffers(1, &m_FBO);
		}

		BOOM_INLINE void SetToneMapping(float exposure, float gamma, const glm::vec3& warmTint)
	{
		m_Exposure = exposure;
		m_Gamma    = gamma;
		m_WarmTint = warmTint;
	}

		BOOM_INLINE void SetVignette(bool enabled, float intensity, float radius)
	{
		m_VignetteEnabled = enabled;
		m_VignetteIntensity = intensity;
		m_VignetteRadius = radius;
	}

	// Call before Render() to configure volumetric fog for the next draw.
		// depthTex: the scene depth texture (slot 2)
		BOOM_INLINE void SetFog(bool enabled, const glm::vec3& fogCol, float density,
			float heightFalloff, float height,
			const glm::mat4& invViewProj, const glm::vec3& camPos,
			uint32_t depthTex)
		{
			m_FogEnabled = enabled;
			m_FogColor = fogCol;
			m_FogDensity = density;
			m_FogHeightFalloff = heightFalloff;
			m_FogHeight = height;
			m_InvViewProj = invViewProj;
			m_CameraPos = camPos;
			m_DepthTex = depthTex;
		}

		BOOM_INLINE void Render(uint32_t vmap, uint32_t vbloom, bool useFBO, bool enableBloom = false, float intensity = 1.0f)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, useFBO ? m_FBO : 0);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			Use();

			SetUniform(bloomEnabled, enableBloom);
			SetUniform(bloomIntensity, intensity);
			SetUniform(exposureLoc, m_Exposure);
			SetUniform(gammaLoc,    m_Gamma);
			SetUniform(warmTintLoc, m_WarmTint);
			SetSceneMap(vmap, vbloom);

			// apply screen fade
			SetUniform(fadeAlpha, g_ScreenFadeAlpha);

			// volumetric fog
			SetUniform(fogEnabledLoc, m_FogEnabled);
			if (m_FogEnabled) {
				SetUniform(fogColorLoc, m_FogColor);
				SetUniform(fogDensityLoc, m_FogDensity);
				SetUniform(fogHeightFalloffLoc, m_FogHeightFalloff);
				SetUniform(fogHeightLoc, m_FogHeight);
				SetUniform(fogInvViewProjLoc, m_InvViewProj);
				SetUniform(fogCameraPosLoc, m_CameraPos);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, m_DepthTex);
				SetUniform(depthMapLoc, 2);
			}

			SetUniform(vignetteEnabledLoc, m_VignetteEnabled);
			if (m_VignetteEnabled) {
				SetUniform(vignetteIntensityLoc, m_VignetteIntensity);
				SetUniform(vignetteRadiusLoc, m_VignetteRadius);
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, vmap);

			//render quad
			quad->Draw(GL_TRIANGLE_STRIP);
			UnUse();

			//bind default fbo
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		BOOM_INLINE void Resize(int32_t width, int32_t height)
		{
			glBindTexture(GL_TEXTURE_2D, m_Final);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		BOOM_INLINE uint32_t GetMap()
		{
			return m_Final;
		}

		BOOM_INLINE uint32_t GetFBOId() const
		{
			return m_FBO;
		}

		BOOM_INLINE void CreateBuffer(int32_t width, int32_t height)
		{
			// create frame buffer
			glGenFramebuffers(1, &m_FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

			// create attachment
			glGenTextures(1, &m_Final);
			glBindTexture(GL_TEXTURE_2D, m_Final);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Final, 0);

			// check frame buffer
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				BOOM_ERROR("glCheckFramebufferStatus() Failed!");
			}

			// unbind frame buffer
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

	private:
		int32_t fadeAlpha;
		int32_t bloomIntensity;
		int32_t gammaLoc;
		int32_t exposureLoc;
		int32_t warmTintLoc;
		int32_t depthMapLoc;
		int32_t fogEnabledLoc;
		int32_t fogColorLoc;
		int32_t fogDensityLoc;
		int32_t fogHeightFalloffLoc;
		int32_t fogHeightLoc;
		int32_t fogInvViewProjLoc;
		int32_t fogCameraPosLoc;
		int32_t vignetteEnabledLoc;
		int32_t vignetteIntensityLoc;
		int32_t vignetteRadiusLoc;

		Quad2D quad;
		int32_t bloom = 0u;
		int32_t map = 0u;
		int32_t bloomEnabled;
		glm::vec4 color;

		// Fog state (written by SetFog, consumed by Render)
		bool m_FogEnabled = false;
		glm::vec3 m_FogColor = glm::vec3(0.5f, 0.6f, 0.7f);
		float m_FogDensity = 0.01f;
		float m_FogHeightFalloff = 0.5f;
		float m_FogHeight = 0.0f;
		glm::mat4 m_InvViewProj = glm::mat4(1.0f);
		glm::vec3 m_CameraPos = {};
		uint32_t m_DepthTex = 0u;

		// Tone mapping state (written by SetToneMapping, consumed by Render)
		float     m_Exposure{ 1.0f };
		float     m_Gamma{ 2.2f };
		glm::vec3 m_WarmTint{ 1.08f, 0.98f, 0.82f };

		// Vignette state (written by SetVignette, consumed by Render)
		bool  m_VignetteEnabled{ false };
		float m_VignetteIntensity{ 0.5f };
		float m_VignetteRadius{ 0.75f };

		uint32_t m_Final = 0u;
		uint32_t m_FBO = 0u;
	};
}