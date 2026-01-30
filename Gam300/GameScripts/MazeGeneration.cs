using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Generates a maze by lowering wall pillars that form the path.
    /// Walls labeled 1-100 are already placed in the scene.
    /// When the player enters the trigger, the maze generates and animates path walls downward.
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

        [EditorExposed("Wall Rise Speed", "Speed at which maze walls rise", 1f, 30f, true)]
        private float _wallRiseSpeed = 20f;

        [EditorExposed("Wall Height", "Final height for maze walls", 0f, 15f, true)]
        private float _wallHeight = 5f;

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
        private enum AnimPhase { WaveUp, Flatten, WallRise, Complete }
        private AnimPhase _currentPhase = AnimPhase.WaveUp;
        private float _waveProgress = 0f;
        private float _wallRiseWaveProgress = 0f;

        // Static instance tracking for trigger callbacks
        private static readonly Dictionary<ulong, MazeGeneration> s_instances = new Dictionary<ulong, MazeGeneration>();

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
            API.UnregisterTriggerCallbacks(Entity);
            API.Log("[MazeGeneration] Destroyed");
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
        /// Start the three-phase construction animation:
        /// Phase 1: Wave rises across all walls
        /// Phase 2: All walls flatten back to original
        /// Phase 3: Maze walls rise up (paths stay flat)
        /// </summary>
        private void StartWallAnimation()
        {
            API.Log("[MazeGeneration] Starting three-phase construction animation...");

            _currentPhase = AnimPhase.WaveUp;
            _waveProgress = 0f;
            _animatingFrameCount = 0;
            _isAnimating = true;

            // Set target positions for NON-path walls (they rise up in phase 3)
            foreach (var kvp in _walls)
            {
                WallState wall = kvp.Value;
                if (!wall.IsPath) // Walls (obstacles) rise up
                {
                    wall.TargetPosition = new Vec3(
                        wall.OriginalPosition.X,
                        wall.OriginalPosition.Y + _wallHeight,
                        wall.OriginalPosition.Z
                    );
                }
                else // Paths stay at original position
                {
                    wall.TargetPosition = wall.OriginalPosition;
                }
            }

            API.Log($"[MazeGeneration] Will raise {_walls.Count - _pathWallIndices.Count} walls, leaving {_pathWallIndices.Count} as paths");
        }

        /// <summary>
        /// Update wall animations each frame with three phases:
        /// Phase 1: Wave rises (walls go up in sequence)
        /// Phase 2: Flatten (all walls return to original)
        /// Phase 3: Wall rise wave (maze walls rise up, revealing paths)
        /// </summary>
        private void UpdateWallAnimations(float deltaTime)
        {
            _animatingFrameCount++;

            switch (_currentPhase)
            {
                case AnimPhase.WaveUp:
                    UpdateWaveUpPhase(deltaTime);
                    break;

                case AnimPhase.Flatten:
                    UpdateFlattenPhase(deltaTime);
                    break;

                case AnimPhase.WallRise:
                    UpdateWallRisePhase(deltaTime);
                    break;

                case AnimPhase.Complete:
                    _isAnimating = false;
                    API.Log("[MazeGeneration] All animation phases complete!");
                    break;
            }
        }

        /// <summary>
        /// Phase 1: Diagonal wave animation - walls rise in diagonal pattern from corner
        /// Starting from wall 1, spreading to 2+11, then 3+12+21, etc.
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

                // Current wave diagonal position
                float waveDiagonal = _waveProgress;

                // Wave width (how many diagonals are affected at once)
                float waveWidth = 3f;

                // Calculate how much this wall should be raised based on diagonal
                float distanceFromWave = Math.Abs(diagonalIndex - waveDiagonal);
                float waveInfluence = Math.Max(0f, 1f - (distanceFromWave / waveWidth));

                // Smooth the wave with a sine function
                waveInfluence = (float)Math.Sin(waveInfluence * Math.PI / 2);

                float targetY = wall.OriginalPosition.Y + (_waveHeight * waveInfluence);
                float newY = Mathf.MoveTowards(wall.CurrentPosition.Y, targetY, _waveSpeed * 2f * deltaTime);

                Vec3 newPos = new Vec3(wall.CurrentPosition.X, newY, wall.CurrentPosition.Z);
                API.SetPosition(wall.EntityHandle, newPos);
                wall.CurrentPosition = newPos;
            }

            // Move to flatten phase when wave has passed all diagonals
            if (_waveProgress > maxDiagonal + 3f)
            {
                API.Log("[MazeGeneration] Diagonal wave complete, starting flatten phase...");
                _currentPhase = AnimPhase.Flatten;
            }

            if (_animatingFrameCount % 30 == 0)
            {
                API.Log($"[MazeGeneration] Diagonal wave phase: diagonal={_waveProgress:F1}");
            }
        }

        /// <summary>
        /// Phase 2: Flatten - all walls return to original height
        /// </summary>
        private void UpdateFlattenPhase(float deltaTime)
        {
            bool allFlat = true;

            foreach (var kvp in _walls)
            {
                WallState wall = kvp.Value;

                float targetY = wall.OriginalPosition.Y;
                float newY = Mathf.MoveTowards(wall.CurrentPosition.Y, targetY, _waveSpeed * deltaTime);

                if (Math.Abs(newY - targetY) > 0.01f)
                {
                    allFlat = false;
                }

                Vec3 newPos = new Vec3(wall.CurrentPosition.X, newY, wall.CurrentPosition.Z);
                API.SetPosition(wall.EntityHandle, newPos);
                wall.CurrentPosition = newPos;
            }

            if (allFlat)
            {
                API.Log("[MazeGeneration] Flatten complete, raising maze walls...");
                _currentPhase = AnimPhase.WallRise;
                _wallRiseWaveProgress = 0f;

                // Mark NON-path walls for animation (these will rise)
                foreach (var kvp in _walls)
                {
                    if (!kvp.Value.IsPath)
                    {
                        kvp.Value.IsAnimating = true;
                    }
                }
            }
        }

        /// <summary>
        /// Phase 3: Sequential wall rise - maze walls rise one at a time in quick succession
        /// </summary>
        private void UpdateWallRisePhase(float deltaTime)
        {
            _wallRiseWaveProgress += _wallRiseSpeed * deltaTime;

            int wallCount = _walls.Count;
            int currentWallIndex = (int)_wallRiseWaveProgress;

            foreach (var kvp in _walls)
            {
                int wallIndex = kvp.Key;
                WallState wall = kvp.Value;

                // Skip path walls - they stay flat
                if (wall.IsPath)
                    continue;

                if (!wall.IsAnimating)
                    continue;

                // Only animate the current wall (or walls very close to it)
                float distanceFromCurrent = Math.Abs(wallIndex - _wallRiseWaveProgress);

                // Very tight range - essentially one wall at a time
                if (distanceFromCurrent < 2f)
                {
                    // This wall should be rising
                    float targetY = wall.TargetPosition.Y;
                    float newY = Mathf.MoveTowards(wall.CurrentPosition.Y, targetY, _wallHeight * 10f * deltaTime);

                    Vec3 newPos = new Vec3(wall.CurrentPosition.X, newY, wall.CurrentPosition.Z);
                    API.SetPosition(wall.EntityHandle, newPos);
                    wall.CurrentPosition = newPos;

                    // Check if reached target
                    if (Math.Abs(newY - targetY) < 0.01f)
                    {
                        wall.CurrentPosition = new Vec3(wall.CurrentPosition.X, targetY, wall.CurrentPosition.Z);
                        API.SetPosition(wall.EntityHandle, wall.CurrentPosition);
                        wall.IsAnimating = false;
                    }
                }
                else if (wallIndex < _wallRiseWaveProgress - 2f)
                {
                    // Wave has passed, ensure wall is at final height
                    if (Math.Abs(wall.CurrentPosition.Y - wall.TargetPosition.Y) > 0.01f)
                    {
                        wall.CurrentPosition = new Vec3(wall.CurrentPosition.X, wall.TargetPosition.Y, wall.CurrentPosition.Z);
                        API.SetPosition(wall.EntityHandle, wall.CurrentPosition);
                    }
                    wall.IsAnimating = false;
                }
            }

            // Complete when sequence has passed all walls
            if (_wallRiseWaveProgress > wallCount + 5f)
            {
                API.Log("[MazeGeneration] Wall rise complete! Maze is ready.");
                _currentPhase = AnimPhase.Complete;
            }

            if (_animatingFrameCount % 30 == 0)
            {
                API.Log($"[MazeGeneration] Wall rise phase: wall {currentWallIndex} of {wallCount}");
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
                API.SetPosition(wall.EntityHandle, wall.OriginalPosition);
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
            _wallRiseWaveProgress = 0f;
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
