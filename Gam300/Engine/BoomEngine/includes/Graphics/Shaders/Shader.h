#pragma once
#include "Core.h"
#include "GlobalConstants.h"
#include <iostream>

namespace Boom {
	int32_t const BITS = 512;

	class Shader {
	public:
		BOOM_INLINE Shader(std::string const& filename) 
			: shaderId{ Load(CONSTANTS::SHADERS_LOCATION.data() + filename)}
		{ }
		BOOM_INLINE virtual ~Shader() {
			glDeleteProgram(shaderId);
		}
		BOOM_INLINE void Use() {
			glUseProgram(shaderId);
		}
		BOOM_INLINE void UnUse() {
			glUseProgram(0);
		}

	private: //main logic for initializing shader
		BOOM_INLINE uint32_t Build(char const* src, uint32_t type) {
			uint32_t id{ glCreateShader(type) };
			glShaderSource(id, 1, &src, NULL);
			glCompileShader(id);

			int32_t status{};
			glGetShaderiv(id, GL_COMPILE_STATUS, &status);
			if (!status) {
				char err[512];
				glGetShaderInfoLog(id, 512, NULL, err);
				glDeleteShader(id);
				id = 0u;
				throw std::runtime_error(std::string("Compile") + err);
			}

			return id;
		}
		BOOM_INLINE uint32_t Link(uint32_t vert, uint32_t frag) {
			uint32_t pgmId{ glCreateProgram() };

			glAttachShader(pgmId, vert);
			glAttachShader(pgmId, frag);
			glLinkProgram(pgmId);

			int32_t status{};
			glGetProgramiv(pgmId, GL_LINK_STATUS, &status);
			if (!status) {
				char err[BITS];
				glGetProgramInfoLog(pgmId, BITS, NULL, err);
				glDeleteProgram(pgmId);
				throw std::runtime_error(std::string("Link") + err);
			}

			glDeleteShader(vert);
			glDeleteShader(frag);

			return pgmId;
		}
		BOOM_INLINE void Validate(uint32_t pgmId) {
			glValidateProgram(pgmId);

			int32_t status{};
			glGetProgramiv(pgmId, GL_VALIDATE_STATUS, &status);
			if (!status) {
				char err[BITS];
				glGetProgramInfoLog(pgmId, BITS, NULL, err);
				glDeleteProgram(pgmId);
				throw std::runtime_error(std::string("Validate") + err);
			}
		}

		uint32_t Load(std::string const& filename) {
			std::ifstream fs;
			fs.exceptions(std::ifstream::failbit | std::fstream::badbit);
			try {
				bool isValidVtx{ true };
				fs.open(filename);

				std::string line;
				std::string vtxStr;
				std::string fragStr;

				//load vtx & frag 
				while (std::getline(fs, line)) {
					if (isValidVtx) {
						if (line.compare("==VERTEX==")) {
							vtxStr.append(line + '\n');
							continue;
						}
						isValidVtx = false;
						continue;
					}
					else {
						if (!line.compare("==FRAGMENT==")) break;
						fragStr.append(line + '\n');
					}
				}
				fs.close();

				uint32_t vtx{ Build(vtxStr.c_str(), GL_VERTEX_SHADER) };
				uint32_t frag{ Build(fragStr.c_str(), GL_FRAGMENT_SHADER) };
				uint32_t pgmId{ Link(vtx, frag) };
				Validate(pgmId);
				return pgmId;
			}
			catch (std::exception const& e) {
				char const* errStr{ e.what() };
				BOOM_ERROR("Load('{}') Failed: {}", filename, errStr);
			}
			return 0u;
		}

	public: //helper functions for accessing uniform variables in vert & frag
		BOOM_INLINE int32_t GetUniformVar(std::string const& str) {
			int32_t res{ glGetUniformLocation(shaderId, str.c_str()) };
			if (res < 0) {
				BOOM_ERROR("Shader_{} - invalid uniform var:{}", shaderId, str);
			}
			return res;
		}
		//uint
		void SetUniform(int32_t loc, uint32_t val) const {
			glUniform1ui(loc, val);
		}
		//int
		void SetUniform(int32_t loc, int32_t val) const {
			glUniform1i(loc, val);
		}
		//float
		void SetUniform(int32_t loc, float val) const {
			glUniform1f(loc, val);
		}
		//vec3
		void SetUniform(int32_t loc, glm::vec3 const& val) const {
			glUniform3fv(loc, 1, glm::value_ptr(val));
		}
		//vec4
		void SetUniform(int32_t loc, glm::vec4 const& val) const {
			glUniform4fv(loc, 1, glm::value_ptr(val));
		}
		//mat3
		void SetUniform(int32_t loc, glm::mat3 const& val) const {
			glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(val));
		}
		//mat4
		void SetUniform(int32_t loc, glm::mat4 const& val) const {
			glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(val));
		}

	protected:
		uint32_t shaderId;
	};
}