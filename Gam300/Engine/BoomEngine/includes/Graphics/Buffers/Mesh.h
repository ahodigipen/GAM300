#pragma once
#include "Vertex.h"

namespace Boom {
	template <class Vertex>
	class Mesh {
	public:
		BOOM_INLINE Mesh() = default; //to be decided what empty mesh should be.

		//creates the mesh data in storage for the provided vertex data
		BOOM_INLINE Mesh(MeshData<Vertex>&& data)
			: numVtx{ (uint32_t)data.vtx.size() }
			, numIdx{ (uint32_t)data.idx.size() }
			, buffId{}
		{
			if (data.vtx.empty()) {
				BOOM_ERROR("Mesh() - empty construct");
				return;
			}

			//gen + bind vtx buffer array
			glGenVertexArrays(1, &buffId);
			glBindVertexArray(buffId);

			//create vtx buff
			uint32_t VBO{};
			glGenBuffers(1, &VBO);
			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(
				GL_ARRAY_BUFFER,
				numVtx * sizeof(Vertex),
				data.vtx.data(),
				GL_STATIC_DRAW
			);

			//element buffer if theres indicies
			if (numIdx) {
				uint32_t EBO{};
				glGenBuffers(1, &EBO);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
				glBufferData(
					GL_ELEMENT_ARRAY_BUFFER,
					numIdx * sizeof(uint32_t),
					data.idx.data(),
					GL_STATIC_DRAW
				);
			}

			//vert types
			//if (TypeID<Vertex>() == TypeID<ShadedVert>()) {
			if (std::is_same<Vertex, ShadedVert>::value) {
				
				SetAttribute<ShadedVert>(0, 3, (void*)offsetof(ShadedVert, pos));
				SetAttribute<ShadedVert>(1, 3, (void*)offsetof(ShadedVert, norm));
				SetAttribute<ShadedVert>(2, 2, (void*)offsetof(ShadedVert, uv));
				SetAttribute<ShadedVert>(3, 3, (void*)offsetof(ShadedVert, tangent));
				SetAttribute<ShadedVert>(4, 3, (void*)offsetof(ShadedVert, biTangent));
			}
			else if (std::is_same<Vertex, FlatVert>::value) {
				SetAttribute<FlatVert>(0, 3, (void*)offsetof(FlatVert, pos));
				SetAttribute<FlatVert>(1, 4, (void*)offsetof(FlatVert, col));
			}
			else if (std::is_same<Vertex, QuadVert>::value) {
				SetAttribute<QuadVert>(0, 2, (void*)offsetof(QuadVert, pos));
				SetAttribute<QuadVert>(1, 2, (void*)offsetof(QuadVert, uv));
			}
			else if (std::is_same<Vertex, SkyboxVert>::value) {
				SetAttribute<SkyboxVert>(0, 3, (void*)offsetof(SkyboxVert, pos));
			}
			//Skeletal - AMOS <3
			else if(std::is_same<Vertex, SkeletalVertex>::value) {
				SetAttribute<SkeletalVertex>(0, 3,	(void*)offsetof(SkeletalVertex, pos));
				SetAttribute<SkeletalVertex>(1, 3,	(void*)offsetof(SkeletalVertex, norm));
				SetAttribute<SkeletalVertex>(2, 2,	(void*)offsetof(SkeletalVertex, uv));
				SetAttribute<SkeletalVertex>(3, 3,	(void*)offsetof(SkeletalVertex, tangent));
				SetAttribute<SkeletalVertex>(4, 3,	(void*)offsetof(SkeletalVertex, biTangent));
				SetAttribute<SkeletalVertex>(5, 4,	(void*)offsetof(SkeletalVertex, joints));
				SetAttribute<SkeletalVertex>(6, 4,	(void*)offsetof(SkeletalVertex, weights));
			}
			else {
				BOOM_ERROR(false && "Mesh() - invalid vertex type.");
			}

			glBindVertexArray(0);
		}

		//simple draw mesh function
		BOOM_INLINE void Draw(uint32_t mode) {
			glBindVertexArray(buffId);
			if (numIdx) {
				glDrawElements(mode, numIdx, GL_UNSIGNED_INT, 0);
			}
			else {
				glDrawArrays(mode, 0, numVtx);
			}
			glBindVertexArray(0);
		}

		// Instanced draw for batched rendering
		// @param mode GL draw mode (e.g., GL_TRIANGLES)
		// @param instanceCount Number of instances to draw
		BOOM_INLINE void DrawInstanced(uint32_t mode, uint32_t instanceCount) {
			if (instanceCount == 0) return;
			glBindVertexArray(buffId);
			if (numIdx) {
				glDrawElementsInstanced(mode, numIdx, GL_UNSIGNED_INT, 0, instanceCount);
			}
			else {
				glDrawArraysInstanced(mode, 0, numVtx, instanceCount);
			}
			glBindVertexArray(0);
		}

		//automatically frees storage when life over
		BOOM_INLINE ~Mesh() {
			glDeleteVertexArrays(1, &buffId);
		}
	private:
		template <class TYPE>
		BOOM_INLINE void SetAttribute(uint32_t index, int32_t size, void const* val) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, sizeof(TYPE), val);
		}

	private:
		uint32_t numVtx;
		uint32_t numIdx;
		uint32_t buffId;
	};

	//3d Mesh
	using ShadedMesh = Mesh<ShadedVert>;
	using SkeletalMesh = Mesh<SkeletalVertex>;
	using Mesh3D = std::shared_ptr<ShadedMesh>;
}