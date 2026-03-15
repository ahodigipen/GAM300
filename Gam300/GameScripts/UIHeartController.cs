using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls the heart UI sprites (Heart_5 to Heart_1) based on player HP
    /// Hearts fade from opaque (alpha = 1) to transparent (alpha = 0) as HP decreases
    /// Heart_5 = 5 HP, Heart_4 = 4 HP, Heart_3 = 3 HP, Heart_2 = 2 HP, Heart_1 = 1 HP
    /// </summary>
    public class UIHeartController
    {
        public ulong Entity;

        // Heart sprite names (ordered from highest to lowest HP)
        private string _heart5Name = "Heart_5";
        private string _heart4Name = "Heart_4";
        private string _heart3Name = "Heart_3";
        private string _heart2Name = "Heart_2";
        private string _heart1Name = "Heart_1";

        // Entity handles for each heart
        private ulong _heart5 = 0;
        private ulong _heart4 = 0;
        private ulong _heart3 = 0;
        private ulong _heart2 = 0;
        private ulong _heart1 = 0;

        // Animation parameters
        private float _fadeSpeed = 5.0f;  // Speed of heart fade animation

        // Current and target alpha values for smooth transitions
        private float[] _currentAlpha = new float[5];  // Index 0 = Heart_5, Index 4 = Heart_1
        private float[] _targetAlpha = new float[5];

        // Track last known HP to detect changes
        private int _lastHP = 5;
        private const int MAX_HP = 5;

        public void OnStart(string jsonParams)
        {
            // Find all heart sprite entities
            _heart5 = API.FindEntity(_heart5Name);
            _heart4 = API.FindEntity(_heart4Name);
            _heart3 = API.FindEntity(_heart3Name);
            _heart2 = API.FindEntity(_heart2Name);
            _heart1 = API.FindEntity(_heart1Name);

            // Get actual current HP from HUD ratio
            int startHP = GetCurrentHPFromRatio();
            _lastHP = startHP;

            // Initialize all hearts to correct starting alpha based on current HP
            InitializeHeart(_heart5, 0, startHP >= 5);
            InitializeHeart(_heart4, 1, startHP >= 4);
            InitializeHeart(_heart3, 2, startHP >= 3);
            InitializeHeart(_heart2, 3, startHP >= 2);
            InitializeHeart(_heart1, 4, startHP >= 1);

            UpdateHeartTargets(startHP);

            API.Log($"[UIHeartController] Initialized heart UI system with {startHP} active hearts");
        }

        private void InitializeHeart(ulong heartEntity, int index, bool visible)
        {
            float alpha = visible ? 1.0f : 0.0f;
            _currentAlpha[index] = alpha;
            _targetAlpha[index] = alpha;

            if (heartEntity != 0 && API.HasSprite(heartEntity))
            {
                API.SetSpriteAlpha(heartEntity, alpha);
            }
            else
            {
                API.Log($"[UIHeartController] Warning: Failed to find heart sprite at index {index} (ID: {heartEntity})");
            }
        }

        public void OnUpdate(float dt)
        {
            // Get current HP from HUD system
            int currentHP = GetCurrentHPFromRatio();

            // Detect HP changes
            if (currentHP != _lastHP)
            {
                API.Log($"[UIHeartController] HP changed: {_lastHP} -> {currentHP}");
                _lastHP = currentHP;
                UpdateHeartTargets(currentHP);
            }

            // Smoothly animate all hearts to their target alpha
            UpdateHeartAlpha(_heart5, 0, dt);
            UpdateHeartAlpha(_heart4, 1, dt);
            UpdateHeartAlpha(_heart3, 2, dt);
            UpdateHeartAlpha(_heart2, 3, dt);
            UpdateHeartAlpha(_heart1, 4, dt);
        }

        /// <summary>
        /// Update target alpha values for all hearts based on current HP
        /// HP 5 = All hearts visible
        /// HP 4 = Heart_5 hidden, others visible
        /// HP 3 = Heart_5 & Heart_4 hidden, others visible
        /// HP 2 = Heart_5, Heart_4 & Heart_3 hidden, Heart_2 & Heart_1 visible
        /// HP 1 = Only Heart_1 visible
        /// HP 0 = All hearts hidden
        /// </summary>
        private void UpdateHeartTargets(int hp)
        {
            // Clamp HP to valid range
            hp = Math.Max(0, Math.Min(MAX_HP, hp));

            // Set target alpha based on HP
            // Each heart corresponds to that HP level
            // Heart_5 visible when HP >= 5
            // Heart_4 visible when HP >= 4
            // Heart_3 visible when HP >= 3
            // Heart_2 visible when HP >= 2
            // Heart_1 visible when HP >= 1

            _targetAlpha[0] = (hp >= 5) ? 1.0f : 0.0f; // Heart_5
            _targetAlpha[1] = (hp >= 4) ? 1.0f : 0.0f; // Heart_4
            _targetAlpha[2] = (hp >= 3) ? 1.0f : 0.0f; // Heart_3
            _targetAlpha[3] = (hp >= 2) ? 1.0f : 0.0f; // Heart_2
            _targetAlpha[4] = (hp >= 1) ? 1.0f : 0.0f; // Heart_1

            API.Log($"[UIHeartController] Updated heart targets for HP={hp}");
        }

        /// <summary>
        /// Smoothly animate a heart sprite to its target alpha
        /// </summary>
        private void UpdateHeartAlpha(ulong heartEntity, int index, float dt)
        {
            if (heartEntity == 0 || !API.HasSprite(heartEntity)) return;

            // Lerp towards target alpha
            float diff = _targetAlpha[index] - _currentAlpha[index];
            float absDiff = Math.Abs(diff);

            if (absDiff > 0.01f)
            {
                // Smooth lerp
                _currentAlpha[index] = Lerp(_currentAlpha[index], _targetAlpha[index], _fadeSpeed * dt);
            }
            else
            {
                // Snap to target when close enough
                _currentAlpha[index] = _targetAlpha[index];
            }

            // Update sprite alpha
            API.SetSpriteAlpha(heartEntity, _currentAlpha[index]);
        }

        /// <summary>
        /// Get current HP from the HUD.HealthRatio
        /// </summary>
        private int GetCurrentHPFromRatio()
        {
            float ratio = HUD.HealthRatio;

            // Convert ratio back to HP value (0.0 to 1.0 -> 0 to 5)
            int hp = (int)Math.Round(ratio * MAX_HP);

            // Clamp to valid range
            return Math.Max(0, Math.Min(MAX_HP, hp));
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}