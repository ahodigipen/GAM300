using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Generates a maze with a two-phase animation:
    /// Phase 1: All 100 pillars rise up in a diagonal wave and stay up
    /// Phase 2: Path pillars sink down layer by layer (walls 1-10, 11-20, ..., 91-100)
    /// Walls labeled 1-100 are already placed in the scene.
    /// When the player enters the trigger, the maze generates and animates.
    /// </summary>
    public class MazeGeneration
    {
        public ulong Entity;

        // ===== EXPOSED PARAMETERS =====
        [EditorExposed("Maze Width", "Width of the maze grid (must be odd)", 3, 21, true)]
        private int _mazeWidth = 11;

        [EditorExposed("Maze Height", "Height of the maze grid (must be odd)", 3, 21, true)]
        private int _mazeHeight = 11;

        [EditorExposed("Wave Animation Speed", "Speed of the wave construction animation", 1f, 20f, true)]
        private float _waveSpeed = 15f;

        [EditorExposed("Wave Height", "How high walls rise during initial wave", 0f, 10f, true)]
        private float _waveHeight = 3f;

        [EditorExposed("Layer Sink Speed", "Speed at which layers sink down", 1f, 30f, true)]
        private float _layerSinkSpeed = 15f;

        [EditorExposed("Layer Sink Delay", "Delay between each layer sinking (seconds)", 0f, 2f, true)]
        private float _layerSinkDelay = 0.2f;

        [EditorExposed("Auto Generate", "Generate maze on start (ignore trigger)")]
        private bool _autoGenerate = false;

        [EditorExposed("Wall Name Prefix", "Prefix for wall entity names (e.g., 'Wall_')")]
        private string _wallNamePrefix = "Wall_";

        [EditorExposed("Allow Retrigger", "Allow maze to regenerate if player re-enters trigger")]
        private bool _allowRetrigger = false;

        // ===== PRIVATE FIELDS =====
        private bool[,] _mazeGrid; // true = wall, false = path
        private Dictionary<int, WallState> _walls = new Dictionary<int, WallState>();
        private bool _mazeGenerated = false;
        private bool _isAnimating = false;
        private List<int> _pathWallIndices = new List<int>();
        private Random _random = new Random();
        private int _animatingFrameCount = 0;

        // Animation phases
        private enum AnimPhase { WaveUp, LayerSink, Complete }
        private AnimPhase _currentPhase = AnimPhase.WaveUp;
        private float _waveProgress = 0f;
        private int _currentSinkingLayer = 0;
        private float _layerSinkProgress = 0f;

        // Static instance tracking for trigger callbacks
        private static readonly Dictionary<ulong, MazeGeneration> s_instances = new Dictionary<ulong, MazeGeneration>();
        private static MazeGeneration s_primaryInstance = null; // Primary instance for external triggering

        // Directions for maze generation (N, S, E, W)
        private static readonly int[] DX = { 0, 0, 1, -1 };
        private static readonly int[] DY = { -1, 1, 0, 0 };

        /// <summary>
        /// Represents the state of a wall entity
        /// </summary>
        private class WallState
        {
            public ulong EntityHandle;
            public Vec3 OriginalPosition;
            public Vec3 CurrentPosition;
            public Vec3 TargetPosition;
            public bool IsPath; // true if this wall should be lowered
            public bool IsAnimating;
            public bool IsLowered;

            public WallState(ulong handle, Vec3 pos)
            {
                EntityHandle = handle;
                OriginalPosition = pos;
                CurrentPosition = pos;
                TargetPosition = pos;
                IsPath = false;
                IsAnimating = false;
                IsLowered = false;
            }
        }

        public void OnStart(string jsonParams)
        {
            // Entity field is set by the engine before OnStart is called
            API.Log($"[MazeGeneration] OnStart called! Entity ID: {Entity}");

            if (Entity == 0)
            {
                API.Log($"[MazeGeneration] ERROR: Entity is 0! Script not properly attached.");
                return;
            }

            // Register this instance for trigger callbacks
            s_instances[Entity] = this;

            // Set as primary instance if it's the first one
            if (s_primaryInstance == null)
            {
                s_primaryInstance = this;
                API.Log($"[MazeGeneration] Set as primary instance for external triggering");
            }

            API.Log($"[MazeGeneration] Registered instance. Total instances: {s_instances.Count}");

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[MazeGeneration] WARNING: Entity has no collider!");
            }
            else if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[MazeGeneration] Set entity as trigger.");
            }
            else
            {
                API.Log("[MazeGeneration] Entity is already a trigger.");
            }

            // Register trigger callbacks
            API.Log($"[MazeGeneration] Registering callbacks for entity {Entity}...");
            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
            API.Log("[MazeGeneration] Callbacks registered!");

            // Ensure dimensions are odd (required for proper maze generation)
            if (_mazeWidth % 2 == 0) _mazeWidth++;
            if (_mazeHeight % 2 == 0) _mazeHeight++;
            API.Log($"[MazeGeneration] Maze dimensions: {_mazeWidth}x{_mazeHeight}");

            // Initialize walls from scene
            InitializeWalls();

            // Auto-generate if enabled
            if (_autoGenerate)
            {
                API.Log("[MazeGeneration] Auto-generate enabled, starting now...");
                GenerateMaze();
            }
            else
            {
                API.Log("[MazeGeneration] Waiting for player trigger...");
            }

            API.Log("[MazeGeneration] OnStart complete!");
        }

        public void OnUpdate(float deltaTime)
        {
            // Update wall animations
            if (_isAnimating)
            {
                if (_animatingFrameCount == 0) // First frame of animation
                {
                    API.Log("[MazeGeneration] Starting animation updates in OnUpdate...");
                }
                UpdateWallAnimations(deltaTime);
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
            {
                s_instances.Remove(Entity);
            }

            // Clear primary instance if this was it
            if (s_primaryInstance == this)
            {
                s_primaryInstance = null;
            }

            API.UnregisterTriggerCallbacks(Entity);
            API.Log("[MazeGeneration] Destroyed");
        }

        /// <summary>
        /// PUBLIC STATIC: Trigger maze generation from external scripts (e.g., cutscenes)
        /// This allows other scripts to activate the maze without needing a trigger volume
        /// </summary>
        public static void TriggerMazeFromExternal()
        {
            API.Log("[MazeGeneration] TriggerMazeFromExternal called!");

            if (s_primaryInstance == null)
            {
                API.Log("[MazeGeneration] WARNING: No maze instance available to trigger!");
                return;
            }

            if (s_primaryInstance._mazeGenerated && !s_primaryInstance._allowRetrigger)
            {
                API.Log("[MazeGeneration] Maze already generated. Enable 'Allow Retrigger' to regenerate.");
                return;
            }

            // Reset if retriggering
            if (s_primaryInstance._mazeGenerated && s_primaryInstance._allowRetrigger)
            {
                API.Log("[MazeGeneration] Retriggering maze generation...");
                s_primaryInstance.ResetMaze();
            }

            // Generate the maze
            API.Log("[MazeGeneration] Generating maze from external trigger...");
            s_primaryInstance.GenerateMaze();
        }

        /// <summary>
        /// PUBLIC STATIC: Trigger a specific maze instance by entity name
        /// </summary>
        public static void TriggerMazeByName(string entityName)
        {
            API.Log($"[MazeGeneration] TriggerMazeByName called for '{entityName}'");

            ulong mazeEntity = API.FindEntity(entityName);
            if (mazeEntity == 0)
            {
                API.Log($"[MazeGeneration] ERROR: Could not find entity '{entityName}'");
                return;
            }

            MazeGeneration instance;
            if (!s_instances.TryGetValue(mazeEntity, out instance))
            {
                API.Log($"[MazeGeneration] ERROR: Entity '{entityName}' does not have a MazeGeneration instance");
                return;
            }

            if (instance._mazeGenerated && !instance._allowRetrigger)
            {
                API.Log("[MazeGeneration] Maze already generated. Enable 'Allow Retrigger' to regenerate.");
                return;
            }

            // Reset if retriggering
            if (instance._mazeGenerated && instance._allowRetrigger)
            {
                API.Log("[MazeGeneration] Retriggering maze generation...");
                instance.ResetMaze();
            }

            // Generate the maze
            API.Log("[MazeGeneration] Generating maze from external trigger...");
            instance.GenerateMaze();
        }

        // ===== TRIGGER CALLBACKS =====

        /// <summary>
        /// Static callback when player enters the trigger
        /// </summary>
        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            API.Log($"[MazeGeneration] OnTriggerEnterCallback fired! Trigger={triggerEntity}, Other={otherEntity}");

            MazeGeneration inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst))
            {
                API.Log($"[MazeGeneration] ERROR: No instance found for trigger entity {triggerEntity}");
                return;
            }

            API.Log($"[MazeGeneration] Instance found! Checking if entity is player...");

            // Get player entity ID for comparison
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            API.Log($"[MazeGeneration] PlayerMovement.GetPlayerEntity() returned: {playerEntity}");
            API.Log($"[MazeGeneration] Entering entity ID: {otherEntity}");

            // Only player triggers this
            if (otherEntity != playerEntity)
            {
                API.Log($"[MazeGeneration] Non-player entity entered trigger (Expected: {playerEntity}, Got: {otherEntity})");
                return;
            }

            API.Log("[MazeGeneration] *** PLAYER ENTERED TRIGGER! ***");

            // Check if maze already generated
            if (inst._mazeGenerated && !inst._allowRetrigger)
            {
                API.Log("[MazeGeneration] Maze already generated. Set 'Allow Retrigger' to true to regenerate.");
                return;
            }

            // Reset if retriggering
            if (inst._mazeGenerated && inst._allowRetrigger)
            {
                API.Log("[MazeGeneration] Retriggering maze generation...");
                inst.ResetMaze();
            }

            // Generate the maze
            API.Log("[MazeGeneration] About to call GenerateMaze()...");
            inst.GenerateMaze();
        }

        /// <summary>
        /// Static callback when player exits the trigger
        /// </summary>
        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            MazeGeneration inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            API.Log("[MazeGeneration] Player left trigger");
        }

        // ===== MAZE GENERATION =====

        /// <summary>
        /// Initialize wall entities from the scene (Wall_1 through Wall_100)
        /// </summary>
        private void InitializeWalls()
        {
            API.Log("[MazeGeneration] Initializing walls...");

            int foundCount = 0;
            for (int i = 1; i <= 100; i++)
            {
                string wallName = _wallNamePrefix + i;
                ulong wallEntity = API.FindEntity(wallName);

                if (wallEntity != 0)
                {
                    Vec3 pos = API.GetPosition(wallEntity);
                    _walls[i] = new WallState(wallEntity, pos);
                    foundCount++;
                }
            }

            API.Log($"[MazeGeneration] Initialized {foundCount} walls");

            if (foundCount == 0)
            {
                API.Log("[MazeGeneration] ERROR: No walls found! Make sure walls are named with the correct prefix.");
            }
        }

        /// <summary>
        /// Generate the maze using Depth-First Search algorithm
        /// </summary>
        public void GenerateMaze()
        {
            if (_mazeGenerated)
            {
                API.Log("[MazeGeneration] Maze already generated!");
                return;
            }

            API.Log($"[MazeGeneration] Generating maze ({_mazeWidth}x{_mazeHeight})...");

            // Initialize grid (all walls)
            _mazeGrid = new bool[_mazeWidth, _mazeHeight];
            for (int x = 0; x < _mazeWidth; x++)
            {
                for (int y = 0; y < _mazeHeight; y++)
                {
                    _mazeGrid[x, y] = true; // true = wall
                }
            }

            // Generate maze using recursive backtracking (DFS)
            GenerateMazeDFS(1, 1);

            // Create entrance and exit
            _mazeGrid[0, 1] = false; // Entrance
            _mazeGrid[_mazeWidth - 1, _mazeHeight - 2] = false; // Exit

            // Map maze grid to wall entities
            MapMazeToWalls();

            // Start animation
            StartWallAnimation();

            _mazeGenerated = true;
            API.Log($"[MazeGeneration] Maze generated! {_pathWallIndices.Count} walls marked as paths.");
        }

        /// <summary>
        /// Recursive Depth-First Search maze generation
        /// </summary>
        private void GenerateMazeDFS(int x, int y)
        {
            _mazeGrid[x, y] = false; // Carve path

            // Create random direction order
            List<int> directions = new List<int> { 0, 1, 2, 3 };
            ShuffleList(directions);

            foreach (int dir in directions)
            {
                int nx = x + DX[dir] * 2;
                int ny = y + DY[dir] * 2;

                if (IsValidCell(nx, ny) && _mazeGrid[nx, ny])
                {
                    // Remove wall between current and next cell
                    _mazeGrid[x + DX[dir], y + DY[dir]] = false;

                    // Recursively carve from next cell
                    GenerateMazeDFS(nx, ny);
                }
            }
        }

        /// <summary>
        /// Check if cell coordinates are valid
        /// </summary>
        private bool IsValidCell(int x, int y)
        {
            return x > 0 && x < _mazeWidth - 1 && y > 0 && y < _mazeHeight - 1;
        }

        /// <summary>
        /// Shuffle a list randomly
        /// </summary>
        private void ShuffleList<T>(List<T> list)
        {
            for (int i = list.Count - 1; i > 0; i--)
            {
                int j = _random.Next(i + 1);
                T temp = list[i];
                list[i] = list[j];
                list[j] = temp;
            }
        }

        /// <summary>
        /// Map the generated maze grid to actual wall entities
        /// This determines which walls should be lowered (paths)
        /// Entrance uses walls 1-10, exit uses walls 91-100
        /// </summary>
        private void MapMazeToWalls()
        {
            _pathWallIndices.Clear();

            int wallCount = _walls.Count;
            if (wallCount == 0)
            {
                API.Log("[MazeGeneration] ERROR: No walls found in scene!");
                return;
            }

            // Group walls into 10 layers (1-10, 11-20, ..., 91-100)
            int layerSize = 10;
            int totalLayers = wallCount / layerSize;

            API.Log($"[MazeGeneration] Mapping maze to {totalLayers} layers of {layerSize} walls each");

            // Map each row of the maze to a layer of walls
            List<int> pathIndices = new List<int>();

            for (int y = 0; y < _mazeHeight; y++)
            {
                // Map maze row to wall layer (0-based to 0-based)
                int layer = (y * totalLayers) / _mazeHeight;
                int layerStartWall = (layer * layerSize) + 1; // Wall index starts at 1

                for (int x = 0; x < _mazeWidth; x++)
                {
                    if (!_mazeGrid[x, y]) // false = path
                    {
                        // Map maze column to wall within layer
                        int wallInLayer = (x * layerSize) / _mazeWidth;
                        int wallIndex = layerStartWall + wallInLayer;

                        if (wallIndex >= 1 && wallIndex <= wallCount && _walls.ContainsKey(wallIndex))
                        {
                            if (!pathIndices.Contains(wallIndex))
                            {
                                pathIndices.Add(wallIndex);
                            }
                        }
                    }
                }
            }

            // Ensure entrance (layer 0: walls 1-10) has at least one path
            bool hasEntrance = pathIndices.Exists(w => w >= 1 && w <= 10);
            if (!hasEntrance && _walls.ContainsKey(5))
            {
                API.Log("[MazeGeneration] Adding entrance at wall 5");
                pathIndices.Add(5);
            }

            // Ensure exit (layer 9: walls 91-100) has at least one path
            bool hasExit = pathIndices.Exists(w => w >= 91 && w <= 100);
            if (!hasExit && _walls.ContainsKey(95))
            {
                API.Log("[MazeGeneration] Adding exit at wall 95");
                pathIndices.Add(95);
            }

            // Mark walls as paths
            foreach (int idx in pathIndices)
            {
                if (_walls.ContainsKey(idx))
                {
                    _walls[idx].IsPath = true;
                    _pathWallIndices.Add(idx);
                }
            }

            API.Log($"[MazeGeneration] Mapped {_pathWallIndices.Count} walls as paths");
            API.Log($"[MazeGeneration] Entrance walls: {string.Join(", ", pathIndices.FindAll(w => w <= 10))}");
            API.Log($"[MazeGeneration] Exit walls: {string.Join(", ", pathIndices.FindAll(w => w >= 91))}");
        }

        /// <summary>
        /// Start the two-phase construction animation:
        /// Phase 1: Diagonal wave rises across all walls and stays up
        /// Phase 2: Path walls sink down layer by layer (10 at a time)
        /// </summary>
        private void StartWallAnimation()
        {
            API.Log("[MazeGeneration] Starting two-phase construction animation...");

            _currentPhase = AnimPhase.WaveUp;
            _waveProgress = 0f;
            _currentSinkingLayer = 0;
            _layerSinkProgress = 0f;
            _animatingFrameCount = 0;
            _isAnimating = true;

            // Set target positions
            foreach (var kvp in _walls)
            {
                WallState wall = kvp.Value;
                if (!wall.IsPath) // Non-path walls stay at wave height
                {
                    wall.TargetPosition = new Vec3(
                        wall.OriginalPosition.X,
                        wall.OriginalPosition.Y + _waveHeight,
                        wall.OriginalPosition.Z
                    );
                }
                else // Path walls will sink back to original
                {
                    wall.TargetPosition = wall.OriginalPosition;
                }
            }

            API.Log($"[MazeGeneration] Will keep {_walls.Count - _pathWallIndices.Count} walls up, sinking {_pathWallIndices.Count} path walls in layers");
        }

        /// <summary>
        /// Update wall animations each frame with two phases:
        /// Phase 1: Wave rises (walls go up and stay up)
        /// Phase 2: Layer sink (path walls sink down layer by layer)
        /// </summary>
        private void UpdateWallAnimations(float deltaTime)
        {
            _animatingFrameCount++;

            switch (_currentPhase)
            {
                case AnimPhase.WaveUp:
                    UpdateWaveUpPhase(deltaTime);
                    break;

                case AnimPhase.LayerSink:
                    UpdateLayerSinkPhase(deltaTime);
                    break;

                case AnimPhase.Complete:
                    _isAnimating = false;
                    API.Log("[MazeGeneration] All animation phases complete!");
                    break;
            }
        }

        /// <summary>
        /// Phase 1: Diagonal wave animation - walls rise in diagonal pattern and stay up
        /// Starting from wall 1, spreading to 2+11, then 3+12+21, etc.
        /// Once touched by the wave, each pillar rises to full height and stays there.
        /// </summary>
        private void UpdateWaveUpPhase(float deltaTime)
        {
            _waveProgress += _waveSpeed * deltaTime;

            int wallCount = _walls.Count;
            int layerSize = 10; // Walls per layer
            int maxDiagonal = 18; // Max diagonal index (9+9 for 10x10 grid)

            foreach (var kvp in _walls)
            {
                int wallIndex = kvp.Key;
                WallState wall = kvp.Value;

                // Convert wall index to grid position (assuming 10x10 grid)
                int row = (wallIndex - 1) / layerSize; // 0-9
                int col = (wallIndex - 1) % layerSize; // 0-9

                // Calculate diagonal index (walls on same diagonal have same value)
                int diagonalIndex = row + col; // 0 to 18

                // Full target height (same for all walls)
                float fullHeight = wall.OriginalPosition.Y + _waveHeight;

                // If wave has reached or passed this diagonal, wall should rise to full height
                if (diagonalIndex <= _waveProgress)
                {
                    // This wall has been touched by the wave - rise to full height and stay
                    if (wall.CurrentPosition.Y < fullHeight)
                    {
                        float newY = Mathf.MoveTowards(wall.CurrentPosition.Y, fullHeight, _waveSpeed * 2f * deltaTime);
                        Vec3 newPos = new Vec3(wall.CurrentPosition.X, newY, wall.CurrentPosition.Z);
                        API.TeleportRigidBody(wall.EntityHandle, newPos);
                        wall.CurrentPosition = newPos;
                    }
                    else
                    {
                        // Already at full height - ensure it stays there
                        if (Math.Abs(wall.CurrentPosition.Y - fullHeight) > 0.01f)
                        {
                            Vec3 newPos = new Vec3(wall.CurrentPosition.X, fullHeight, wall.CurrentPosition.Z);
                            API.TeleportRigidBody(wall.EntityHandle, newPos);
                            wall.CurrentPosition = newPos;
                        }
                    }
                }
                // Walls ahead of the wave stay at original position
            }

            // Move to layer sink phase when wave has passed all diagonals
            if (_waveProgress > maxDiagonal + 2f)
            {
                // Ensure all walls are at exact full height before moving to next phase
                foreach (var kvp in _walls)
                {
                    WallState wall = kvp.Value;
                    float fullHeight = wall.OriginalPosition.Y + _waveHeight;
                    Vec3 targetPos = new Vec3(wall.CurrentPosition.X, fullHeight, wall.CurrentPosition.Z);
                    API.TeleportRigidBody(wall.EntityHandle, targetPos);
                    wall.CurrentPosition = targetPos;
                }

                API.Log("[MazeGeneration] Diagonal wave complete, all walls are at same height. Starting layer sink phase...");
                _currentPhase = AnimPhase.LayerSink;
                _layerSinkProgress = 0f;
            }

            if (_animatingFrameCount % 30 == 0)
            {
                API.Log($"[MazeGeneration] Diagonal wave phase: diagonal={_waveProgress:F1}");
            }
        }

        /// <summary>
        /// Phase 2: Layer Sink - path walls sink down layer by layer (walls 1-10, 11-20, etc.)
        /// </summary>
        private void UpdateLayerSinkPhase(float deltaTime)
        {
            _layerSinkProgress += deltaTime;

            int layerSize = 10;
            int totalLayers = 10; // 100 walls / 10 per layer

            // Check if we should move to next layer
            if (_layerSinkProgress > _layerSinkDelay && _currentSinkingLayer < totalLayers)
            {
                _layerSinkProgress = 0f;
                _currentSinkingLayer++;

                if (_currentSinkingLayer <= totalLayers)
                {
                    API.Log($"[MazeGeneration] Sinking layer {_currentSinkingLayer} (walls {(_currentSinkingLayer - 1) * layerSize + 1} to {_currentSinkingLayer * layerSize})");

                    // Mark walls in this layer for sinking
                    int layerStartWall = (_currentSinkingLayer - 1) * layerSize + 1;
                    int layerEndWall = _currentSinkingLayer * layerSize;

                    foreach (var kvp in _walls)
                    {
                        int wallIndex = kvp.Key;
                        WallState wall = kvp.Value;

                        // If this wall is in the current layer and is a path, mark it for animation
                        if (wallIndex >= layerStartWall && wallIndex <= layerEndWall && wall.IsPath)
                        {
                            wall.IsAnimating = true;
                        }
                    }
                }
            }

            // Animate sinking walls
            bool anyAnimating = false;
            foreach (var kvp in _walls)
            {
                WallState wall = kvp.Value;

                if (wall.IsAnimating && wall.IsPath)
                {
                    float targetY = wall.TargetPosition.Y; // Original position (lowered)
                    float newY = Mathf.MoveTowards(wall.CurrentPosition.Y, targetY, _layerSinkSpeed * deltaTime);

                    Vec3 newPos = new Vec3(wall.CurrentPosition.X, newY, wall.CurrentPosition.Z);
                    API.TeleportRigidBody(wall.EntityHandle, newPos);
                    wall.CurrentPosition = newPos;

                    // Check if reached target
                    if (Math.Abs(newY - targetY) < 0.01f)
                    {
                        wall.CurrentPosition = new Vec3(wall.CurrentPosition.X, targetY, wall.CurrentPosition.Z);
                        API.TeleportRigidBody(wall.EntityHandle, wall.CurrentPosition);
                        wall.IsAnimating = false;
                        wall.IsLowered = true;
                    }
                    else
                    {
                        anyAnimating = true;
                    }
                }
            }

            // Complete when all layers have been processed
            if (_currentSinkingLayer >= totalLayers && !anyAnimating)
            {
                API.Log("[MazeGeneration] Layer sink complete! Maze is ready.");
                _currentPhase = AnimPhase.Complete;
            }

            if (_animatingFrameCount % 30 == 0 && (_currentSinkingLayer > 0 || anyAnimating))
            {
                API.Log($"[MazeGeneration] Layer sink phase: layer {_currentSinkingLayer} of {totalLayers}");
            }
        }

        /// <summary>
        /// Reset the maze (return all walls to original position)
        /// </summary>
        public void ResetMaze()
        {
            API.Log("[MazeGeneration] Resetting maze...");

            foreach (var kvp in _walls)
            {
                WallState wall = kvp.Value;
                API.TeleportRigidBody(wall.EntityHandle, wall.OriginalPosition);
                wall.CurrentPosition = wall.OriginalPosition;
                wall.IsLowered = false;
                wall.IsPath = false;
                wall.IsAnimating = false;
            }

            _pathWallIndices.Clear();
            _mazeGenerated = false;
            _isAnimating = false;
            _currentPhase = AnimPhase.WaveUp;
            _waveProgress = 0f;
            _currentSinkingLayer = 0;
            _layerSinkProgress = 0f;
            _animatingFrameCount = 0;

            API.Log("[MazeGeneration] Maze reset complete!");
        }

        /// <summary>
        /// Helper math class for smooth movement
        /// </summary>
        private static class Mathf
        {
            public static float MoveTowards(float current, float target, float maxDelta)
            {
                if (Math.Abs(target - current) <= maxDelta)
                {
                    return target;
                }
                return current + Math.Sign(target - current) * maxDelta;
            }
        }
    }
}
