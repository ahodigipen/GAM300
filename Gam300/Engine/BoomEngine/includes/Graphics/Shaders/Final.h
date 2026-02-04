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
			, bloomEnabled{ GetUniformVar("u_enableBloom") }
			, fadeAlpha{ GetUniformVar("u_fadeAlpha") } // NEW
			, bloomIntensity{ GetUniformVar("u_bloomIntensity") }
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

		//Hehe OOPS

		BOOM_INLINE ~FinalShader()
		{
			glDeleteTextures(1, &m_Final);
			glDeleteFramebuffers(1, &m_FBO);
		}

		BOOM_INLINE void Render(uint32_t vmap, uint32_t vbloom, bool useFBO, bool enableBloom = false, float intensity = 1.0f)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, useFBO ? m_FBO : 0);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			Use();

			SetUniform(bloomEnabled, enableBloom);
			SetUniform(bloomIntensity, intensity);
			SetSceneMap(vmap, vbloom);

			// NEW: apply screen fade
			SetUniform(fadeAlpha, g_ScreenFadeAlpha);

			//set color map
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
		int32_t fadeAlpha; // NEW
		int32_t bloomIntensity;

		Quad2D quad;
		int32_t bloom = 0u;
		int32_t map = 0u;
		//int32_t colLoc;
		int32_t bloomEnabled;
		glm::vec4 color;

		uint32_t m_Final = 0u;
		uint32_t m_FBO = 0u;
	};
}