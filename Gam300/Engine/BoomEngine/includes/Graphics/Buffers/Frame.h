#pragma once
#include "Core.h"
#include "GlobalConstants.h"

namespace Boom {
	class FrameBuffer {
	public:
		//formats: leave blank for default
		// or {GL_R32I, GL_RED_INTEGER} for fbo obj pick
		BOOM_INLINE FrameBuffer(int32_t w, int32_t h, bool lowRes = false, GLenum internalFormat = GL_RGBA16F, GLenum format = GL_RGBA)
			: buffId{}, depth{}, color{}
			, width{ w }, height{ h }
			, isLowPoly{lowRes}
		{
			//glSampleCoverage(1.f, GL_FALSE); //for multisampling
			glGenFramebuffers(1, &buffId);
			glBindFramebuffer(GL_FRAMEBUFFER, buffId);

			CreateColorAttachment(internalFormat, format);
			CreateBrightnessAttachment();
			CreateDepthAttachment();

			uint32_t attachments[2] {
				GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1,
			};

			glDrawBuffers(2, attachments);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				BOOM_ERROR("FrameBuffer() - frame buffer status failed.");
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		BOOM_INLINE ~FrameBuffer() {
			glDeleteTextures(1, &color);
			glDeleteTextures(1, &brightness);
			glDeleteTextures(1, &depth);
			glDeleteFramebuffers(1, &buffId);
		}

		[[nodiscard]] BOOM_INLINE float Ratio() const {
			return (float)width / (float)height;
		}
		BOOM_INLINE void Resize(int32_t w, int32_t h) {
			width = w;
			height = h;
			const int tw = targetW();
			const int th = targetH();
			//resize color attachment
			glBindTexture(GL_TEXTURE_2D, color);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			//resize depth texture
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, tw, th, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			//resize brightness attachment
			glBindTexture(GL_TEXTURE_2D, brightness);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		[[nodiscard]] BOOM_INLINE uint32_t GetTexture() const {
			return color;
		}
		
		BOOM_INLINE void SBind() {
			glBindFramebuffer(GL_FRAMEBUFFER, buffId);
			glViewport(0, 0, targetW(), targetH());
		}
		BOOM_INLINE void Begin() {
			glBindFramebuffer(GL_FRAMEBUFFER, buffId);
			
			if (isLowPoly) {
				glViewport(0, 0, 320, 240);
				glDisable(GL_MULTISAMPLE);
				glDisable(GL_POINT_SMOOTH);
				glDisable(GL_LINE_SMOOTH);
				glDisable(GL_POLYGON_SMOOTH);
				glHint(GL_POINT_SMOOTH_HINT, GL_DONT_CARE);
				glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
				glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
			}
			else {
				glViewport(0, 0, width, height);
			}
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			//glEnable(GL_SAMPLES);
		}
		BOOM_INLINE void End() {
			if (isLowPoly) {
				glEnable(GL_MULTISAMPLE);
				glEnable(GL_POINT_SMOOTH);
				glEnable(GL_LINE_SMOOTH);
				glEnable(GL_POLYGON_SMOOTH);
				glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
				glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
				glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
			}
			//glDisable(GL_SAMPLES);
			glDisable(GL_DEPTH_TEST);
		
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

		}
		BOOM_INLINE uint32_t GetBrightnessMap() {
			return brightness;
		}
		BOOM_INLINE uint32_t GetDepthTexture() const {
			return depth;
		}
		BOOM_INLINE int32_t GetWidth() const {
			return width;
		}
		BOOM_INLINE int32_t GetHeight() const {
			return height;
		}
		BOOM_INLINE uint32_t GetBufferId() const {
			return buffId;
		}

	private:
		BOOM_INLINE void CreateColorAttachment(GLenum internalFormat, GLenum format) {
			glGenTextures(1, &color);
			glBindTexture(GL_TEXTURE_2D, color);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isLowPoly ? GL_NEAREST : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isLowPoly ? GL_NEAREST : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, isLowPoly ? 320 : width, isLowPoly ? 240 : height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, isLowPoly ? 320 : width, isLowPoly ? 240 : height, 0, format, GL_UNSIGNED_BYTE, NULL);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
		}
		BOOM_INLINE void CreateDepthAttachment() {
			glGenTextures(1, &depth);
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, targetW(), targetH(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
		}
		BOOM_INLINE void CreateBrightnessAttachment() {
			glGenTextures(1, &brightness);
			glBindTexture(GL_TEXTURE_2D, brightness);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, targetW(), targetH(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glGenerateMipmap(GL_TEXTURE_2D);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, brightness, 0);
		}
		//lowpoly needs to resize using this, not width/height directly
		BOOM_INLINE int targetW() const { return isLowPoly ? 320 : width; }
		BOOM_INLINE int targetH() const { return isLowPoly ? 240 : height; }

	private:
		uint32_t brightness;
		uint32_t buffId;
		uint32_t depth;
		uint32_t color;
		int32_t width;
		int32_t height;

		bool isLowPoly;
	};
}