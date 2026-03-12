#pragma once
#include "../Utilities/Data.h"

//frustum culling helper functions
namespace Boom {
	struct FrustumPlanes { glm::vec4 planes[6]; };
	BOOM_INLINE glm::vec4 normalizePlane(const glm::vec4& plane) {
		float length = glm::length(glm::vec3(plane));
		return plane / length;
	}
	BOOM_INLINE FrustumPlanes extractFrustum(const glm::mat4& viewProjection) {
		glm::mat4 t = glm::transpose(viewProjection);
		FrustumPlanes frustumPlanes;
		frustumPlanes.planes[0] = normalizePlane(t[3] + t[0]); // left
		frustumPlanes.planes[1] = normalizePlane(t[3] - t[0]); // right
		frustumPlanes.planes[2] = normalizePlane(t[3] + t[1]); // bottom
		frustumPlanes.planes[3] = normalizePlane(t[3] - t[1]); // top
		frustumPlanes.planes[4] = normalizePlane(t[3] + t[2]); // near
		frustumPlanes.planes[5] = normalizePlane(t[3] - t[2]); // far
		return frustumPlanes;
	}
	BOOM_INLINE bool sphereInside(const FrustumPlanes& frustum, const glm::vec3& center, float radius) {
		for (int i = 0; i < 6; ++i) {
			const glm::vec4& plane = frustum.planes[i];
			if (glm::dot(glm::vec3(plane), center) + plane.w + radius < 0) {
				return false; // Sphere is outside this plane
			}
		}
		return true; // Sphere is inside all planes
	}
	BOOM_INLINE void ToWorldSphere(const Transform3D& t,
		const glm::vec3& localC, float localR,
		glm::vec3& worldC, float& worldR)
	{
		glm::quat q = glm::quat(glm::radians(t.rotate));
		worldC = t.translate + (q * (localC * t.scale));
		float s = std::max({ std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z) });
		worldR = localR * s;
	}

	/*
	 * Clip-space AABB frustum test. Transforms all 8 local AABB corners by the
	 * combined model-view-projection matrix and tests them against the 6 clip planes
	 * without perspective division. Returns true only when the entire box lies
	 * outside one of the planes — conservative (never false-culls visible objects).
	 */
	BOOM_INLINE bool IsAabbOutsideFrustum(const glm::mat4& mvp,
		const glm::vec3& localMin,
		const glm::vec3& localMax,
		float scale = 1.0f)
	{
		glm::vec4 corners[8];
		for (int i = 0; i < 8; ++i)
			corners[i] = mvp * glm::vec4(
				i & 1 ? localMax.x : localMin.x,
				i & 2 ? localMax.y : localMin.y,
				i & 4 ? localMax.z : localMin.z,
				1.f);

		int out;
		out = 0; for (auto& c : corners) out += (c.x < -c.w * scale); if (out == 8) return true;
		out = 0; for (auto& c : corners) out += (c.x >  c.w * scale); if (out == 8) return true;
		out = 0; for (auto& c : corners) out += (c.y < -c.w * scale); if (out == 8) return true;
		out = 0; for (auto& c : corners) out += (c.y >  c.w * scale); if (out == 8) return true;
		out = 0; for (auto& c : corners) out += (c.z < -c.w); if (out == 8) return true;
		out = 0; for (auto& c : corners) out += (c.z >  c.w); if (out == 8) return true;
		return false;
	}

}
