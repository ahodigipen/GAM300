#pragma once
#include "Core.h" //used for variables and error handling
#include "common/Events.h"
#include "GlobalConstants.h"
#include "Input/InputHandler.h"
#include "Graphics/Shaders/LoadingShader.h"
#include "Graphics/Shaders/LoadingVideoShader.h"
#include "Graphics/Video/VideoPlayer.h"
#include <filesystem>
#include <vector>
#include <iostream>

namespace Boom {
	struct AppWindow {
	public:
		AppWindow() = delete;
		AppWindow(AppWindow const&) = delete;

		//example for initializing with reference:
		//std::unique_ptr<AppWindow> w = std::make_unique<AppWindow>(&Dispatcher, 1900, 800, "BOOM");
		BOOM_INLINE AppWindow(EventDispatcher* disp, int32_t w, int32_t h, char const* windowTitle)
			: width{ w }
			, height{ h }
			, refreshRate{ 144 }
			, isFullscreen{ false }
			, monitorPtr{}
			, modePtr{}
			, windowPtr{}
			, dispatcher{ disp }
		{
			if (!glfwInit()) {
				BOOM_FATAL("AppWindow::Init() - glfwInit() failed.");
				std::exit(EXIT_FAILURE);
			}

			// Add common mappings for Bluetooth Xbox, PlayStation, and Switch controllers on Windows
			// GUIDs for controllers via Bluetooth often differ from wired ones.
			const char* mappings =
				"030000005e040000050b000000007801,Xbox One S Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows\n"
				"030000005e040000120b000000007801,Xbox Series Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows\n"
				"030000004c050000cc09000000007801,PS4 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b12,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b3,y:b2,platform:Windows\n"
				"030000004c050000e60c000000007801,PS5 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b12,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b3,y:b2,platform:Windows\n"
				"030000007e0500000920000000007801,Nintendo Switch Pro Controller,a:b1,b:b0,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:b7,rightx:a2,righty:a3,start:b9,x:b2,y:b3,platform:Windows\n"
				"03000000c82d00001260000000000000,8BitDo Controller,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,paddle1:b17,paddle2:b16,paddle3:b2,paddle4:b5,rightshoulder:b7,rightstick:b14,righttrigger:b9,rightx:a3,righty:a4,start:b11,x:b3,y:b4,platform:Windows\n";
			glfwUpdateGamepadMappings(mappings);
			//glfwWindowHint(GLFW_OPENGL_CORE_PROFILE, GLFW_TRUE);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

			monitorPtr = glfwGetPrimaryMonitor();
			modePtr = glfwGetVideoMode(monitorPtr);
			glfwWindowHint(GLFW_REFRESH_RATE, modePtr->refreshRate);
			glfwWindowHint(GLFW_GREEN_BITS, modePtr->greenBits);
			glfwWindowHint(GLFW_BLUE_BITS, modePtr->blueBits);
			glfwWindowHint(GLFW_RED_BITS, modePtr->redBits);
			glfwWindowHint(GLFW_SAMPLES, 4);

			glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
			glfwWindowHint(GLFW_DEPTH_BITS, 24);

			glfwWindowHint(GLFW_MAXIMIZED, GL_FALSE);
			glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

			windowPtr = std::shared_ptr<GLFWwindow>(
				glfwCreateWindow(width, height, windowTitle, NULL, NULL),
				glfwDestroyWindow
			);
			if (windowPtr.get() == nullptr) {
				BOOM_FATAL("AppWindow::Init() - failed to init app window.");
				std::exit(EXIT_FAILURE);
			}

			// ADD THESE LINES:
			BOOM_INFO("AppWindow - Initial window size: {}x{}", width, height);
			glfwShowWindow(windowPtr.get());  // Make sure window is visible
			glfwFocusWindow(windowPtr.get()); // Bring to front

			int actualWidth, actualHeight;
			glfwGetWindowSize(windowPtr.get(), &actualWidth, &actualHeight);
			BOOM_INFO("AppWindow - Actual window size after creation: {}x{}", actualWidth, actualHeight);

			glfwMakeContextCurrent(windowPtr.get());
			GLFWwindow* current = glfwGetCurrentContext();

			if (current != windowPtr.get()) {
				BOOM_ERROR("Failed to make window context current in constructor!");
			}

			//user data
			glfwSetWindowUserPointer(windowPtr.get(), this);
			//enable a-vsync
			glfwSwapInterval(0);
			SetupCallbacks(windowPtr.get());

			//initial window color
			std::apply(glClearColor, CONSTANTS::DEFAULT_BACKGROUND_COLOR);

			glfwGetCursorPos(windowPtr.get(), &prevMousePos.x, &prevMousePos.y);

			//init
			camRegionW = width;
			camRegionH = height;
		}
		BOOM_INLINE ~AppWindow() {}

	private: //GLFW callbacks
		BOOM_INLINE static void SetupCallbacks(GLFWwindow* win) {
			glfwSetErrorCallback(AppWindow::OnError);
			glfwSetWindowMaximizeCallback(win, AppWindow::OnMaximized);
			glfwSetFramebufferSizeCallback(win, AppWindow::OnResize);
			glfwSetWindowIconifyCallback(win, AppWindow::OnIconify);
			glfwSetWindowCloseCallback(win, AppWindow::OnClose);
			glfwSetWindowFocusCallback(win, AppWindow::OnFocus);

			glfwSetScrollCallback(win, AppWindow::OnWheel);
			glfwSetMouseButtonCallback(win, AppWindow::OnMouse);
			glfwSetCursorPosCallback(win, AppWindow::OnMotion);
			glfwSetKeyCallback(win, AppWindow::OnKey);
		}
		BOOM_INLINE static void OnError(int32_t errorNo, char const* description) {
			BOOM_ERROR("[GLFW]: [{}] {}", errorNo, description);
		}
		BOOM_INLINE static void OnMaximized(GLFWwindow* win, int32_t action) {
			AppWindow* self{ GetUserData(win) };
			if (action) {
				self->isFullscreen = true;
				//GetUserData(win)->dispatcher->PostEvent<WindowMaximizeEvent>();
			}
			else {
				self->isFullscreen = false;
				//enable input + unpause system + change to non fullscreen or not + etc...
				//GetUserData(win)->dispatcher->PostEvent<WindowRestoreEvent>();
			}
		}
		BOOM_INLINE static void OnResize(GLFWwindow* win, int32_t w, int32_t h) {
			AppWindow* self{ GetUserData(win) };
			if (!self->isEditor) {
				self->dispatcher->PostEvent<WindowResizeEvent>(w, h);
				self->camRegionW = w; 
				self->camRegionH = h;
			}
		}
		BOOM_INLINE static void OnIconify(GLFWwindow* win, int32_t action) {
			(void)win;
			//AppWindow* self{ GetUserData(win) };
			if (action) {
				//disable input + pause system + etc...
				//self->dispatcher->PostEvent<WindowIconifyEvent>();
			}
			else {
				//enable input + unpause system + check fullscreen or not + etc...
				//self->dispatcher->PostEvent<WindowRestoreEvent>();
			}
		}
		BOOM_INLINE static void OnClose(GLFWwindow* win) {
			(void)win;
			//AppWindow* self{ GetUserData(win) };
			//self->dispatcher->PostEvent<WindowCloseEvent>();
			//this event struct can be implemented to contain other needed system logic upon closing window
			//GetUserData(win)->dispatcher->PostEvent<WindowCloseEvent>();
		}
		BOOM_INLINE static void OnFocus(GLFWwindow* win, int32_t focused) {
			(void)win; (void)focused;
			//implemented due to gam250 requirement of pausing the game when unfocused
			//assuming might carry over
		}

		// Scroll
		BOOM_INLINE static void OnWheel(GLFWwindow* win, double sx, double sy) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onScroll(sx, sy);
			// Optional: also emit your event type
			if (self->dispatcher) self->dispatcher->PostEvent<MouseWheelEvent>(sx, sy);
		}

		// Mouse buttons
		BOOM_INLINE static void OnMouse(GLFWwindow* win, int32_t button, int32_t action, int32_t mods) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onMouseButton(button, action, mods);

			// Optional: your event types
			if (!self->dispatcher) return;
			if (action == GLFW_PRESS)   self->dispatcher->PostEvent<MouseDownEvent>(button);
			if (action == GLFW_RELEASE) self->dispatcher->PostEvent<MouseReleaseEvent>(button);
		}

		// Mouse motion
		BOOM_INLINE static void OnMotion(GLFWwindow* win, double x, double y) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onCursorPos(x, y);
			// Optional: motion/drag events
			self->dispatcher->PostEvent<MouseMotionEvent>(x, y);
			if (self->input.current().Mouse.any()) {
				// per-event delta (since last motion). If you want exact deltas, track last x/y here.
				self->dispatcher->PostEvent<MouseDragEvent>(0.0, 0.0);
			}
		}

		// Keys
		BOOM_INLINE static void OnKey(GLFWwindow* win, int32_t key, int32_t sc, int32_t action, int32_t mods) {
			auto* self = GetUserData(win);
			if (!self) return;

			//if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
				//glfwSetWindowShouldClose(win, GLFW_TRUE);
				//return;
			//}

			self->input.onKey(key, sc, action, mods);

			// Optional: keep your key events
			if (!self->dispatcher) return;
			switch (action) {
			case GLFW_PRESS:   self->dispatcher->PostEvent<KeyPressEvent>(key);   break;
			case GLFW_RELEASE: self->dispatcher->PostEvent<KeyReleaseEvent>(key); break;
			case GLFW_REPEAT:  self->dispatcher->PostEvent<KeyRepeatEvent>(key);  break;
			}
		}

		//to be called explicitly to reference window from application
		//example: GetUserData(win)->isFullscreen 
		BOOM_INLINE static AppWindow* GetUserData(GLFWwindow* window) {
			return static_cast<AppWindow*>(glfwGetWindowUserPointer(window));
		}
	public:
		BOOM_INLINE void SetWindowTitle(std::string const& title) {
			glfwSetWindowTitle(windowPtr.get(), title.c_str());
		}

		[[nodiscard]] BOOM_INLINE int32_t& Width() noexcept {
			return width;
		}
		[[nodiscard]] BOOM_INLINE int32_t& Height() noexcept {
			return height;
		}
		BOOM_INLINE std::shared_ptr<GLFWwindow> Handle() const {
			return windowPtr;
		}

		BOOM_INLINE bool PollEvents() {
			input.beginFrame();
			glfwMakeContextCurrent(windowPtr.get());
			glfwPollEvents();
			dispatcher->PollEvents();
			glfwSwapBuffers(windowPtr.get());
			return !glfwWindowShouldClose(windowPtr.get());
		}
		BOOM_INLINE bool IsKey(int32_t key) const {
			return key >= 0 && key <= GLFW_KEY_LAST
				&& inputs.Keys.test(static_cast<size_t>(key));
		}
		BOOM_INLINE bool IsMouse(int32_t button) const {
			if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
				return true; //inputs.Mouse.test(button);
			return false;
		}
		BOOM_INLINE int IsExit() const {
			return glfwWindowShouldClose(windowPtr.get());
		}
		
		BOOM_INLINE void SetCameraInputRegion(double x, double y, double w, double h, bool enabled) {
			camRegionX = x; camRegionY = y; camRegionW = w; camRegionH = h; camInputEnabled = enabled;
		}

		BOOM_INLINE float GetViewportX() const { return (float)camRegionX; }
		BOOM_INLINE float GetViewportY() const { return (float)camRegionY; }
		BOOM_INLINE float GetViewportW() const { return (float)camRegionW; }
		BOOM_INLINE float GetViewportH() const { return (float)camRegionH; }

		BOOM_INLINE bool IsMouseInCameraRegion(GLFWwindow* win) const {
			double mx, my; glfwGetCursorPos(win, &mx, &my); // client-area coords
			return (mx >= camRegionX && mx <= camRegionX + camRegionW &&
				my >= camRegionY && my <= camRegionY + camRegionH);
		}
		BOOM_INLINE void SetViewportKeyboardFocus(bool allow) { allowViewportKeyboard = allow; }

		// NEW: point-in-rect test (window client coords)
		BOOM_INLINE bool IsPointInCameraRect(double x, double y) const {
			return (x >= camRegionX && x <= camRegionX + camRegionW &&
				y >= camRegionY && y <= camRegionY + camRegionH);
		}

		// NEW: allow mouse even if ImGui wants it (used by bridge callbacks)
		BOOM_INLINE bool AllowCameraMouseNow(double x, double y) const {
			return camInputEnabled && IsPointInCameraRect(x, y);
		}
		BOOM_INLINE int getWidth() {
			return width;
		}
		BOOM_INLINE int getHeight() {
			return height;
		}	
		//accesssors
		BOOM_INLINE EventDispatcher* GetDispatcher() const { return dispatcher; }
		BOOM_INLINE InputSystem& GetInputSystem() { return input; }

		/**
		 * Set the video to play during loading screens
		 * @param videoPath Path to MPEG1 video file (relative to Resources/Videos/)
		 * @param loop Whether the video should loop
		 * @return true if video loaded successfully
		 */
		BOOM_INLINE static bool SetLoadingVideo(const std::string& videoPath, bool loop = true) {
			// Create video player if not exists
			if (!s_LoadingVideoPlayer) {
				s_LoadingVideoPlayer = std::make_unique<VideoPlayer>();
			}

			// Try multiple possible paths
			std::vector<std::string> possiblePaths = {
				"Resources/Videos/" + videoPath,
				"../Resources/Videos/" + videoPath,
				"../../Resources/Videos/" + videoPath,
				"../Editor/Resources/Videos/" + videoPath,
				videoPath  // Try direct path as fallback
			};

			std::string fullPath;
			bool found = false;

			// Log current working directory for debugging
			std::string cwd = std::filesystem::current_path().string();
			BOOM_INFO("[LoadingScreen] Current working directory: {}", cwd);

			for (const auto& path : possiblePaths) {
				if (std::filesystem::exists(path)) {
					fullPath = path;
					found = true;
					BOOM_INFO("[LoadingScreen] Found video at: {}", fullPath);
					break;
				}
			}

			if (!found) {
				BOOM_ERROR("[LoadingScreen] Video file not found. Tried paths:");
				for (const auto& path : possiblePaths) {
					BOOM_ERROR("[LoadingScreen]   - {}", path);
				}
				return false;
			}

			// Load the video
			if (!s_LoadingVideoPlayer->Load(fullPath)) {
				BOOM_ERROR("[LoadingScreen] Failed to load video: {}", fullPath);
				return false;
			}

			s_LoadingVideoPlayer->SetLoop(loop);
			s_LoadingVideoPlayer->SetVolume(1.0f);
			s_LoadingVideoEnabled = true;

			BOOM_INFO("[LoadingScreen] Loading video successfully loaded: {}", fullPath);
			return true;
		}

		/**
		 * Enable or disable the loading video
		 */
		BOOM_INLINE static void EnableLoadingVideo(bool enable) {
			s_LoadingVideoEnabled = enable;
		}

		/**
		 * Clear the loading video and free resources
		 */
		BOOM_INLINE static void ClearLoadingVideo() {
			if (s_LoadingVideoPlayer) {
				s_LoadingVideoPlayer->Stop();
				s_LoadingVideoPlayer->Unload();
				s_LoadingVideoPlayer.reset();
			}
			s_LoadingVideoEnabled = false;
		}

		/**
		 * Set whether to show the loading bar on top of the video
		 */
		BOOM_INLINE static void SetShowLoadingBar(bool show) {
			s_ShowLoadingBar = show;
		}

		BOOM_INLINE static void RenderLoading(GLFWwindow* win, float percentProgress) {
			(void)percentProgress; // Unused - video only, no progress bar

			// Static resources - initialized once on first call
			static std::unique_ptr<LoadingVideoShader> videoShader{ nullptr };
			static std::unique_ptr<VideoPlayer> videoPlayer{ nullptr };
			static bool videoInitialized = false;
			static double lastTime = glfwGetTime();

			// Initialize video shader on first call
			if (!videoShader) {
				videoShader = std::make_unique<LoadingVideoShader>("loading_video.glsl");
			}

			// Try to load video on first call
			if (!videoInitialized) {
				videoInitialized = true;
				videoPlayer = std::make_unique<VideoPlayer>();

				// Use the correct path
				std::string videoPath = "Resources/Videos/Deathless_LoadingScreen.mpeg";

				std::cout << "[LoadingVideo] Loading video: " << videoPath << std::endl;
				std::cout.flush();

				if (std::filesystem::exists(videoPath) && videoPlayer->Load(videoPath)) {
					videoPlayer->SetLoop(true);  // Loop until loading completes
					videoPlayer->SetVolume(1.0f);
					videoPlayer->Play();
					std::cout << "[LoadingVideo] Video loaded and looping until resources are ready!" << std::endl;
					std::cout.flush();
				} else {
					std::cout << "[LoadingVideo] Failed to load video - check file format (must be MPEG1)" << std::endl;
					std::cout.flush();
					videoPlayer.reset();
				}
			}

			// Calculate delta time
			double currentTime = glfwGetTime();
			double deltaTime = currentTime - lastTime;
			
			// Safety: Limit deltaTime to 100ms to prevent massive time jumps 
			// (e.g. after long synchronous asset loads) that could hang the video player loop
			if (deltaTime > 0.1) deltaTime = 0.1;
			if (deltaTime < 0.0) deltaTime = 0.0;
			
			lastTime = currentTime;

			// Setup rendering
			int w, h;
			glfwGetFramebufferSize(win, &w, &h);
			glViewport(0, 0, w, h);

			// Clear to black
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDisable(GL_DEPTH_TEST);

			// Render video if loaded
			if (videoPlayer && videoPlayer->IsLoaded()) {
				videoPlayer->Update(deltaTime);

				if (videoPlayer->HasNewFrame()) {
					videoPlayer->UpdateTexture();
				}

				uint32_t textureId = videoPlayer->GetTextureID();
				if (textureId != 0) {
					auto proj = glm::ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);

					// Scale to fit window while maintaining aspect ratio
					// Note: Quad vertices go from -1 to 1, so actual size = scale * 2
					// We divide by 2 to get the correct pixel dimensions
					float videoAspect = (float)videoPlayer->GetWidth() / (float)videoPlayer->GetHeight();
					float screenAspect = (float)w / (float)h;

					float scaleX, scaleY;
					if (screenAspect > videoAspect) {
						// Screen is wider than video - fit to height
						scaleY = (float)h / 2.0f;
						scaleX = ((float)h * videoAspect) / 2.0f;
					} else {
						// Screen is taller than video - fit to width
						scaleX = (float)w / 2.0f;
						scaleY = ((float)w / videoAspect) / 2.0f;
					}

					videoShader->SetTintColor({ 1.0f, 1.0f, 1.0f, 1.0f });
					videoShader->SetTransform({ w * 0.5f, h * 0.5f }, { scaleX, scaleY }, 0.f);
					videoShader->Show(proj, textureId);
				}
			}
			// If no video, just shows black screen (no loading bar)

			glEnable(GL_DEPTH_TEST);
			glfwSwapBuffers(win);
			glfwPollEvents();
		}

	private:
		// Static members for loading video
		inline static std::unique_ptr<VideoPlayer> s_LoadingVideoPlayer{ nullptr };
		inline static bool s_LoadingVideoEnabled{ false };
		inline static bool s_ShowLoadingBar{ true };

		int32_t width{};
		int32_t height{};
		int32_t refreshRate;
		bool isFullscreen;

		//these are kept raw as they are lightweight and non-owning
		GLFWmonitor* monitorPtr;
		GLFWvidmode const* modePtr;
		std::shared_ptr<GLFWwindow> windowPtr;
		EventDispatcher* dispatcher;
		WindowInputs inputs;

	public: //usage for basic glew input to move camera in editor
		bool isRightClickDown{};   // optional legacy flags if you still want them
		bool isMiddleClickDown{};
		bool isShiftDown{};
		bool allowViewportKeyboard{ false };
		// x - strafe right/left, y - hover up/down, z - forward/back
		glm::vec3  camMoveDir{};

		// Kept for completeness; not needed if you use InputSystem mouse deltas
		glm::dvec2 prevMousePos{};

		// around x/y (x=pitch, y=yaw) in degrees
		glm::vec2  camRot{};


		float camMoveMultiplier{ 0.05f };
		// Making sure that the rotation on happens in the viewport
		double camRegionX{ 0.0 }, camRegionY{ 0.0 }, camRegionW{ 0.0 }, camRegionH{ 0.0 };
		bool   camInputEnabled{ false };
		bool isEditor{}; //needed due to imgui's weird resizing bug

		InputSystem input;
	};
}
