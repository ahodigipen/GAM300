using Boom;
using System;
using System.IO;

namespace GameScripts
{
    public static class SettingsManager
    {
        private static string _path = "PauseSettings.json";

        private const float DEFAULT_GAMMA = 2.2f;

        [Serializable]
        class SettingsData
        {
            public float Master = 1.0f;
            public float BGM = 1.0f;
            public float SFX = 1.0f;
            public float Gamma = 2.2f;
        }

        public static void SaveSettings()
        {
            SettingsData data = new SettingsData();
            data.Master = API.GetGroupVolume("Master");
            data.BGM = API.GetGroupVolume("Music");
            data.SFX = API.GetGroupVolume("SFX");
            data.Gamma = API.GetGamma();

            // Manually construct JSON string to avoid external dependencies
            string json = $"{{\"Master\":{data.Master}, \"BGM\":{data.BGM}, \"SFX\":{data.SFX}, \"Gamma\":{data.Gamma}}}";

            try
            {
                File.WriteAllText(_path, json);
            }
            catch
            {
                API.Log("[Settings] Failed to save settings to disk.");
            }
        }

        public static void LoadSettings()
        {
            float m = 1.0f;
            float b = 1.0f;
            float s = 1.0f;
            float g = DEFAULT_GAMMA;

            if (File.Exists(_path))
            {
                try
                {
                    string json = File.ReadAllText(_path);

                    // Parse values using internal helper to avoid full JSON library requirements
                    m = ParseJsonFloat(json, "Master", 1.0f);
                    b = ParseJsonFloat(json, "BGM", 1.0f);
                    s = ParseJsonFloat(json, "SFX", 1.0f);
                    g = ParseJsonFloat(json, "Gamma", DEFAULT_GAMMA);

                    API.Log($"[Settings] Loaded Successfully | Master: {m:0.00} | BGM: {b:0.00} | SFX: {s:0.00} | Gamma: {g:0.00}");
                }
                catch
                {
                    API.Log("[Settings] Failed to load settings. Using defaults.");
                }
            }
            else
            {
                API.Log("[Settings] No save file found. Applying defaults.");
            }

            // Always apply values (defaults or loaded) to ensure audio isn't left at 0 from a previous fade-out
            API.SetGroupVolume("Master", m);
            API.SetGroupVolume("Music", b);
            API.SetGroupVolume("SFX", s);
            API.SetGamma(g);
        }

        // Basic string-based parser for simple JSON key-value pairs.
        private static float ParseJsonFloat(string json, string key, float defaultValue = 1.0f)
        {
            int keyIndex = json.IndexOf(key);
            if (keyIndex == -1) return defaultValue;

            int valStart = json.IndexOf(":", keyIndex) + 1;
            int valEnd = json.IndexOf(",", valStart);

            if (valEnd == -1)
                valEnd = json.IndexOf("}", valStart);

            string val = json.Substring(valStart, valEnd - valStart);

            if (float.TryParse(val, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out float result))
            {
                return result;
            }

            return defaultValue;
        }
    }
}