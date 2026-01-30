using Boom;
using System;
using System.IO;

namespace GameScripts
{
    public static class SettingsManager
    {
        private static string _path = "audiosettings.json";

        [Serializable]
        class SettingsData
        {
            public float Master = 1.0f;
            public float BGM = 1.0f;
            public float SFX = 1.0f;
        }

        public static void SaveSettings()
        {
            SettingsData data = new SettingsData();
            data.Master = API.GetGroupVolume("Master");
            data.BGM = API.GetGroupVolume("Music");
            data.SFX = API.GetGroupVolume("SFX");

            // Manually construct JSON string to avoid external dependencies
            string json = $"{{\"Master\":{data.Master}, \"BGM\":{data.BGM}, \"SFX\":{data.SFX}}}";

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
            if (!File.Exists(_path)) return;

            try
            {
                string json = File.ReadAllText(_path);

                // Parse values using internal helper to avoid full JSON library requirements
                float m = ParseJsonFloat(json, "Master");
                float b = ParseJsonFloat(json, "BGM");
                float s = ParseJsonFloat(json, "SFX");

                API.SetGroupVolume("Master", m);
                API.SetGroupVolume("Music", b);
                API.SetGroupVolume("SFX", s);

                API.Log($"[Settings] Loaded Successfully | Master: {m:0.00} | BGM: {b:0.00} | SFX: {s:0.00}");
            }
            catch
            {
                API.Log("[Settings] Failed to load settings.");
            }
        }

        // Basic string-based parser for simple JSON key-value pairs.
        private static float ParseJsonFloat(string json, string key)
        {
            int keyIndex = json.IndexOf(key);
            if (keyIndex == -1) return 1.0f;

            int valStart = json.IndexOf(":", keyIndex) + 1;
            int valEnd = json.IndexOf(",", valStart);

            if (valEnd == -1)
                valEnd = json.IndexOf("}", valStart);

            string val = json.Substring(valStart, valEnd - valStart);

            if (float.TryParse(val, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out float result))
            {
                return result;
            }

            return 1.0f;
        }
    }
}