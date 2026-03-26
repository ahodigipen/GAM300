#version 450 core
layout(local_size_x = 256) in;

// Particle state: 12 floats per particle
// [posX, posY, posZ, life, velX, velY, velZ, maxLife, size, alive, _pad0, _pad1]
layout(std430, binding = 0) buffer ParticleState {
    vec4 particles[];  // 3 vec4s per particle (12 floats)
};

// Render output: 8 floats per alive particle [posX, posY, posZ, size, r, g, b, a]
layout(std430, binding = 1) buffer RenderOutput {
    vec4 renderData[];  // 2 vec4s per alive particle
};

// Alive particle counter (atomic)
layout(std430, binding = 2) buffer Counters {
    uint uAliveCounter;
    uint uSpawnCounter;
};

// Emitter parameters
uniform float uDt;
uniform int   uMaxParticles;
uniform int   uSpawnCount;
uniform vec3  uEmitterPos;
uniform float uGravity;
uniform float uLifetimeMin;
uniform float uLifetimeMax;
uniform float uSpeedMin;
uniform float uSpeedMax;
uniform int   uShapeType;
uniform float uShapeRadius;
uniform float uShapeAngle;
uniform vec3  uShapeSize;
uniform float uShapeRange;
uniform vec3  uDirection;
uniform float uStartSizeMin;
uniform float uStartSizeMax;
uniform float uEndSize;
uniform vec4  uStartColor;
uniform vec4  uEndColor;
uniform uint  uFrameSeed;

// ─── GPU-friendly hash-based RNG ───────────────────────────────────
uint rngState;

uint pcgHash(uint val) {
    uint state = val * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void seedRng(uint id) {
    rngState = pcgHash(id + uFrameSeed * 196613u);
}

float rand01() {
    rngState = pcgHash(rngState);
    return float(rngState) / 4294967295.0;
}

float randRange(float lo, float hi) {
    return lo + rand01() * (hi - lo);
}

vec3 randDirection() {
    float theta = rand01() * 6.2831853;
    float phi   = acos(rand01() * 2.0 - 1.0);
    float sp = sin(phi);
    return vec3(sp * cos(theta), sp * sin(theta), cos(phi));
}

vec3 randInSphere(float radius) {
    float r = radius * pow(rand01(), 1.0 / 3.0);
    return randDirection() * r;
}

vec3 randInCone(vec3 dir, float halfAngleDeg) {
    float halfAngleRad = radians(halfAngleDeg);
    float cosAngle = cos(halfAngleRad);
    float z = randRange(cosAngle, 1.0);
    float phi = rand01() * 6.2831853;
    float sinTheta = sqrt(1.0 - z * z);

    vec3 localDir = vec3(sinTheta * cos(phi), sinTheta * sin(phi), z);

    vec3 d = normalize(dir);
    vec3 up = vec3(0, 0, 1);
    if (abs(dot(d, up)) > 0.999) up = vec3(1, 0, 0);
    vec3 right = normalize(cross(d, up));
    vec3 newUp = cross(right, d);

    return right * localDir.x + newUp * localDir.y + d * localDir.z;
}

vec3 randInBox(vec3 halfExtents) {
    return vec3(
        randRange(-halfExtents.x, halfExtents.x),
        randRange(-halfExtents.y, halfExtents.y),
        randRange(-halfExtents.z, halfExtents.z)
    );
}

// Returns a random position inside a cone volume (for spotlight dust).
// dir = cone axis, halfAngleDeg = outer half-angle, range = cone length.
// Uses cube-root distribution along the axis for uniform volume density.
vec3 randInConeVolume(vec3 dir, float halfAngleDeg, float range) {
    // Distance along cone axis (cube root for uniform volume distribution)
    float d = range * pow(rand01(), 1.0 / 3.0);

    // Radius of the cone cross-section at distance d
    float coneRadius = d * tan(radians(halfAngleDeg));

    // Random point within the circular cross-section (square-root for uniform disk)
    float r   = coneRadius * sqrt(rand01());
    float phi = rand01() * 6.2831853;

    // Build a local coordinate frame around the cone direction
    vec3 ax = normalize(dir);
    vec3 up = vec3(0, 0, 1);
    if (abs(dot(ax, up)) > 0.999) up = vec3(1, 0, 0);
    vec3 right = normalize(cross(ax, up));
    vec3 newUp = cross(right, ax);

    return ax * d + right * (r * cos(phi)) + newUp * (r * sin(phi));
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(uMaxParticles)) return;

    // Each particle = 3 vec4s in the buffer
    uint base = idx * 3u;

    vec4 slot0 = particles[base + 0u]; // posX, posY, posZ, life
    vec4 slot1 = particles[base + 1u]; // velX, velY, velZ, maxLife
    vec4 slot2 = particles[base + 2u]; // size, alive, _pad, _pad

    vec3  pos     = slot0.xyz;
    float life    = slot0.w;
    vec3  vel     = slot1.xyz;
    float maxLife = slot1.w;
    float size    = slot2.x;
    float alive   = slot2.y;

    seedRng(idx);

    // ─── SPAWNING ───────────────────────────────────────────────────
    // If this particle is dead and we still have particles to spawn
    if (alive < 0.5) {
        uint prevSpawn = atomicAdd(uSpawnCounter, 0xFFFFFFFFu);
        if (prevSpawn > 0u && prevSpawn < 0x80000000u) {
            alive   = 1.0;
        maxLife = randRange(uLifetimeMin, uLifetimeMax);
        life    = maxLife;
        size    = randRange(uStartSizeMin, uStartSizeMax);

        // Position based on shape
        if (uShapeType == 1) {            // sphere
            pos = uEmitterPos + randInSphere(uShapeRadius);
        } else if (uShapeType == 3) {     // box
            pos = uEmitterPos + randInBox(uShapeSize);
        } else if (uShapeType == 4) {     // spotlight volume
            pos = uEmitterPos + randInConeVolume(uDirection, uShapeAngle, uShapeRange);
        } else {                          // point or cone
            pos = uEmitterPos;
        }

        // Velocity direction based on shape
        vec3 dir;
        if (uShapeType == 1) {            // sphere: outward
            vec3 offset = pos - uEmitterPos;
            float len = length(offset);
            dir = (len > 0.001) ? offset / len : randDirection();
        } else if (uShapeType == 2) {     // cone
            dir = randInCone(uDirection, uShapeAngle);
        } else if (uShapeType == 4) {     // spotlight volume: slow random drift
            dir = randDirection();
        } else {                          // point: slight spread
            dir = randInCone(uDirection, 15.0);
        }

        float speed = randRange(uSpeedMin, uSpeedMax);
        vel = dir * speed;
        } // End of spawn branch
    }

    // ─── SIMULATION (only for alive particles) ──────────────────────
    if (alive > 0.5) {
        life -= uDt;

        if (life <= 0.0) {
            alive = 0.0;
        } else {
            // Physics
            vel.y += uGravity * uDt;
            pos += vel * uDt;

            // Interpolation factor (0 = just born, 1 = about to die)
            float t = 1.0 - (life / maxLife);

            // Size over lifetime
            float startSize = (uStartSizeMin + uStartSizeMax) * 0.5;
            size = startSize + (uEndSize - startSize) * t;

            // Color over lifetime
            vec4 color = mix(uStartColor, uEndColor, t);

            // Spotlight volume: fade alpha based on position within cone
            if (uShapeType == 4) {
                vec3 offset = pos - uEmitterPos;
                vec3 ax = normalize(uDirection);
                float along = dot(offset, ax);                     // distance along cone axis
                vec3 perp = offset - ax * along;                   // perpendicular offset
                float coneR = max(along, 0.0) * tan(radians(uShapeAngle)); // cone radius at this depth
                float edgeFade = (coneR > 0.001) ? 1.0 - smoothstep(0.5, 1.0, length(perp) / coneR) : 1.0;
                float depthFade = 1.0 - smoothstep(0.7, 1.0, along / uShapeRange); // fade near end
                color.a *= edgeFade * depthFade;
            }

            // Write to render output (compact)
            uint renderIdx = atomicAdd(uAliveCounter, 1u);
            uint rBase = renderIdx * 2u;
            renderData[rBase + 0u] = vec4(pos, size);
            renderData[rBase + 1u] = color;
        }
    }

    // ─── WRITE BACK particle state ──────────────────────────────────
    particles[base + 0u] = vec4(pos, life);
    particles[base + 1u] = vec4(vel, maxLife);
    particles[base + 2u] = vec4(size, alive, 0.0, 0.0);
}
