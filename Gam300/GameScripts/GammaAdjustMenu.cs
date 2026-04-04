using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Controls the dedicated Gamma Adjust scene.
    /// Shows a GammaSlider and a MainMenuLogo preview so the player can see the
    /// effect of the gamma setting in real time. Return button fades back to Settings.
    /// </summary>
    public class GammaAdjustMenu
    {
        public ulong Entity;

        private const int MOUSE_LEFT = 0;

        private const string RETURN_TEX_NORMAL = "Resources/Textures/MenusUI/ConfirmButton.png";
        private const string RETURN_TEX_CLICKED = "Resources/Textures/MenusUI/ConfirmButton_Clicked.png";

        private ulong _returnButtonID;
        private ButtonFX _buttonFX;
        private GammaSlider _gammaSlider;

        // Controller navigation: 0 = return button, 1 = gamma slider
        private int _selectedIndex = 1;
        private bool _wasDpadUp = false;
        private bool _wasDpadDown = false;
        private bool _wasStickUp = false;
        private bool _wasStickDown = false;
        private bool _wasAButton = false;

        private float _buttonDelayTimer = 0f;
        private const float CLICK_DELAY_DURATION = 0.1f;

        private float _fadeTimer = 0f;
        private const float FADE_DURATION = 1.0f;
        private bool _isFadingIn = false;

        private const float SLIDER_HIGHLIGHT_ALPHA = 1.0f;
        private const float SLIDER_NORMAL_ALPHA = 0.6f;

        private enum State { WaitingForMouseUp, Idle, ButtonDelay, FadingOut }
        private State _state = State.WaitingForMouseUp;

        public void OnStart(string jsonParams)
        {
            API.Log("GammaAdjustMenu OnStart Running...");

            SettingsManager.LoadSettings();

            _returnButtonID = API.FindEntity("GammaAdjust_ConfirmButton");
            _buttonFX = new ButtonFX(_returnButtonID);
            _gammaSlider = new GammaSlider("GammaAdjust_Gamma_BG", "GammaAdjust_Gamma_Fill", "GammaAdjust_Gamma_Handle");

            _selectedIndex = 1;
            _state = State.WaitingForMouseUp;

            API.SetScreenFadeAlpha(1f);
            _isFadingIn = true;
            _fadeTimer = 0f;

            UpdateVisuals();
        }

        public void OnUpdate(float dt)
        {
            _buttonFX?.Update(dt);

            if (_isFadingIn)
            {
                _fadeTimer += dt;
                float a = 1f - Clamp01(_fadeTimer / FADE_DURATION);
                API.SetScreenFadeAlpha(a);
                if (_fadeTimer >= FADE_DURATION)
                {
                    API.SetScreenFadeAlpha(0f);
                    _isFadingIn = false;
                }
            }

            if (_state == State.FadingOut) { UpdateFadeOut(dt); return; }

            // Slider update
            bool dragging = false;
            if (_gammaSlider != null && _gammaSlider.IsDragging)
            {
                _gammaSlider.Update();
                dragging = true;
            }
            else if (_gammaSlider != null)
            {
                _gammaSlider.Update();
                if (_gammaSlider.IsDragging) dragging = true;
            }

            if (dragging) return;

            UpdateControllerNav(dt);

            switch (_state)
            {
                case State.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT)) _state = State.Idle;
                    break;
                case State.Idle:
                    UpdateIdle();
                    break;
                case State.ButtonDelay:
                    _buttonDelayTimer += dt;
                    if (_buttonDelayTimer >= CLICK_DELAY_DURATION) ExecuteClick();
                    break;
            }
        }

        private void UpdateControllerNav(float dt)
        {
            if (!API.IsGamepadConnected()) return;

            bool dpadUp = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_UP);
            bool dpadDown = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_DOWN);
            float stickY = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_Y);
            bool stickUp = stickY < -0.5f;
            bool stickDown = stickY > 0.5f;
            bool aPressed = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            if ((dpadUp && !_wasDpadUp) || (stickUp && !_wasStickUp))
            {
                _selectedIndex = (_selectedIndex - 1 + 2) % 2;
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                _selectedIndex = (_selectedIndex + 1) % 2;
                UpdateVisuals();
            }

            // Horizontal for gamma slider
            if (_selectedIndex == 1 && _gammaSlider != null)
            {
                float stickX = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_X);
                bool dpadLeft = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_LEFT);
                bool dpadRight = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_RIGHT);

                float move = 0f;
                if (Math.Abs(stickX) > 0.2f) move = stickX * dt * 0.5f;
                else if (dpadLeft) move = -dt * 0.5f;
                else if (dpadRight) move = dt * 0.5f;

                if (Math.Abs(move) > 0.0001f)
                    _gammaSlider.SetNormDelta(move);
            }

            if (aPressed && !_wasAButton && _selectedIndex == 0)
                StartClickDelay();

            _wasDpadUp = dpadUp;
            _wasDpadDown = dpadDown;
            _wasStickUp = stickUp;
            _wasStickDown = stickDown;
            _wasAButton = aPressed;
        }

        private void UpdateVisuals()
        {
            API.SetSpriteTexture(_returnButtonID, _selectedIndex == 0 ? RETURN_TEX_CLICKED : RETURN_TEX_NORMAL);
            SetSliderHighlight("GammaAdjust_Gamma_BG", _selectedIndex == 1);
        }

        private void SetSliderHighlight(string bgName, bool highlight)
        {
            ulong id = API.FindEntity(bgName);
            if (id != 0) API.SetSpriteAlpha(id, highlight ? SLIDER_HIGHLIGHT_ALPHA : SLIDER_NORMAL_ALPHA);
        }

        private void UpdateIdle()
        {
            if (!API.IsMouseDown(MOUSE_LEFT)) return;
            if (!API.GetMousePosInViewport(out Vec2 mousePos)) return;

            if (API.Check2DViewportClick(_returnButtonID, mousePos.X, mousePos.Y))
            {
                _selectedIndex = 0;
                StartClickDelay();
            }
        }

        private void StartClickDelay()
        {
            _state = State.ButtonDelay;
            _buttonDelayTimer = 0f;
            ButtonFX.PlayClickSound();
            API.SetSpriteTexture(_returnButtonID, RETURN_TEX_CLICKED);
        }

        private void ExecuteClick()
        {
            API.Log(">> GammaAdjust: Return to Settings");
            _state = State.FadingOut;
            _fadeTimer = 0f;
        }

        private void UpdateFadeOut(float dt)
        {
            _fadeTimer += dt;
            float alpha = Clamp01(_fadeTimer / FADE_DURATION);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= FADE_DURATION)
            {
                API.SetScreenFadeAlpha(1f);
                API.LoadScene("SettingsMenu");
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}
