#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"
#include "../Utilities/Data.h"
#include "GlobalConstants.h"

namespace Boom {
	struct LoadingShader : Shader {
		BOOM_INLINE LoadingShader(std::string const& filename)
			: Shader{ filename }
			, color{ 1.f }
			, colLoc{ GetUniformVar("color") }
			, projLoc{ GetUniformVar("uProj") }
			, quad{ CreateQuad2D() }
		{
		}
		BOOM_INLINE void Show(glm::mat4 const& proj) {
			Use();
			SetUniform(projLoc, proj * quadTransform.Matrix());
			SetUniform(colLoc, color);
			quad->Draw(GL_TRIANGLE_STRIP);
			UnUse();
		}
		//center of quad is pivot
		BOOM_INLINE void SetTransform(glm::vec2 const& pos, glm::vec2 const& scale, float rot) {
			quadTransform.translate = { pos.x, pos.y, 0.f };
			quadTransform.scale = { scale.x, scale.y, 1.f };
			quadTransform.rotate = { 0.f, 0.f, rot };
		}
		BOOM_INLINE void SetColor(glm::vec4 const& col) { color = col; }

	private:
		glm::vec4 color;
		Transform3D quadTransform;
		int32_t projLoc;
		int32_t colLoc;
		Quad2D quad;
	};
}
