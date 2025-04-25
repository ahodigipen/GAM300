// Core.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef CORE_H
#define CORE_H

// add headers that you want to pre-compile here
#include <queue>
#include <vector>
#include <string>
#include <bitset>
#include <memory>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <unordered_map>


// include spdlog
//#define FMT_HEADER_ONLY
//#define SPDLOG_FMT_EXTERNAL
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "framework.h"

#ifdef BOOM_EXPORT
	#define BOOM_API __declspec(dllexport)
#else
	#define BOOM_API __declspec(dllimport)
#endif

//-------------ASSERTIONS---------------

//runtime assertion
#define BOOM_ASSERT assert

//static assertion
#define BOOM_STATIC_ASSERT static_assert

// function inlining
#if defined(_MSC_VER)
	#define BOOM_INLINE __forceinline
	#define BOOM_NOINLINE __declspec(noinline)
#else
	#define BOOM_INLINE inline
	#define BOOM_NOINLINE
#endif


//CONSOLE LOGGING
#ifdef BOOM_ENABLE_LOG

	namespace Boom
	{
		// ----------------------------------------------------------------
		// Returns a process-wide, thread-safe spdlog logger instance.
		// Initialized on first call.
		// ----------------------------------------------------------------
		std::shared_ptr<spdlog::logger>& GetLogger();
	}

	// Convenience macros — expand to no-ops when logging is disabled:
	#define BOOM_TRACE(...) Boom::GetLogger()->trace(__VA_ARGS__)
	#define BOOM_DEBUG(...) Boom::GetLogger()->debug(__VA_ARGS__)
	#define BOOM_INFO(...)  Boom::GetLogger()->info(__VA_ARGS__)
	#define BOOM_WARN(...)  Boom::GetLogger()->warn(__VA_ARGS__)
	#define BOOM_ERROR(...) Boom::GetLogger()->error(__VA_ARGS__)
	#define BOOM_FATAL(...) Boom::GetLogger()->critical(__VA_ARGS__)

#else
	#define BOOM_TRACE
	#define BOOM_DEBUG
	#define BOOM_ERROR
	#define BOOM_FATAL
	#define BOOM_INFO
	#define BOOM_WARN



#endif //BOOM_ENABLE LOG



#endif //CORE_H
