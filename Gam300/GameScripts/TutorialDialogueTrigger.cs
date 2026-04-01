using Boom;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Place on any trigger collider in the scene.
    /// When the player walks in, plays a sequence of dialogue sprite panels
    /// defined by the "Dialogue Names" field (comma-separated entity names).
    ///
    /// Each named entity must exist in the scene with a SpriteComponent and alpha 0.
    /// The player advances through panels with Space / Gamepad A, then the game unpauses.
    /// </summary>
    public class TutorialDialogueTrigger
    {
        public ulong Entity;

        [Boom.EditorExposed("Dialogue Names", "Comma-separated names of dialogue sprite entities to show in order")]
        private string _dialogueNames = "";

        [Boom.EditorExposed("One Shot", "If true, dialogue only triggers once per scene load")]
        private bool _oneShot = true;

        private bool _triggered = false;
        private string[] _nameArray;

        private static readonly Dictionary<ulong, TutorialDialogueTrigger> s_instances =
            new Dictionary<ulong, TutorialDialogueTrigger>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            // Parse and trim the name list
            if (string.IsNullOrEmpty(_dialogueNames))
                _nameArray = new string[0];
            else
            {
                string[] raw = _dialogueNames.Split(',');
                _nameArray = new string[raw.Length];
                for (int i = 0; i < raw.Length; i++)
                    _nameArray[i] = raw[i].Trim();
            }

            if (!API.HasCollider(Entity))
                API.Log($"[TutorialDialogueTrigger] WARNING: Entity {Entity} has no collider.");
            else if (!API.IsTrigger(Entity))
                API.SetTrigger(Entity, true);

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
                s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            if (!s_instances.TryGetValue(triggerEntity, out TutorialDialogueTrigger inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (inst._oneShot && inst._triggered) return;
            if (inst._nameArray.Length == 0) return;
            if (StoryDialogueManager.IsSequenceActive()) return;

            inst._triggered = true;
            StoryDialogueManager.PlayGenericSequence(inst._nameArray);
        }
    }
}
