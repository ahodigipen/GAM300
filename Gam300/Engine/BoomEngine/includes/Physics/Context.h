#pragma once
#include "common/Core.h"
#include "Callback.h"
#include "Utilities.h"
#include "Auxiliaries/Assets.h"
#include <iostream>
#include "PxPhysicsAPI.h"
#include <foundation/PxMath.h>

namespace Boom {
    struct PhysicsContext {

        // Simple line primitive for debug rendering
        struct DebugLine {
            glm::vec3 p0;
            glm::vec3 p1;
            glm::vec4 c0;
            glm::vec4 c1;
        };



        BOOM_INLINE PhysicsContext()
            : m_Foundation(nullptr)
            , m_Physics(nullptr)
            , m_Dispatcher(nullptr)
            , m_Scene(nullptr)
            , m_DebugVisEnabled(false)
            {
            // sinitialize physX SDK
            m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_AllocatorCallback, m_ErrorCallback);
            if (!m_Foundation)
            {
                BOOM_ERROR("Error initializing PhysX m_Foundation");
                return;
            }
            // create context instance
            m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION,
                *m_Foundation, PxTolerancesScale());
            if (!m_Physics)
            {
                BOOM_ERROR("Error initializing PhysX m_Physics");
                m_Foundation->release();
                return;
            }

            // create worker threads
            m_Dispatcher = PxDefaultCpuDispatcherCreate(2);

            // create a scene desciption
            PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
            sceneDesc.simulationEventCallback = &m_EventCallback;
            sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
            sceneDesc.filterShader = CustomFilterShader;
            sceneDesc.cpuDispatcher = m_Dispatcher;

            // create scene instance
            m_Scene = m_Physics->createScene(sceneDesc);


            if (!m_Scene)
            {
                BOOM_ERROR("Error creating PhysX m_Scene");
                m_Physics->release();
                m_Foundation->release();
                return;
            }

            // Debug visualization disabled by default
            m_DebugVisEnabled = false;
        }

        BOOM_INLINE ~PhysicsContext() {
            if (m_Scene) { m_Scene->release(); }
            if (m_Physics) { m_Physics->release(); }
            if (m_Dispatcher) { m_Dispatcher->release(); }
            if (m_Foundation) { m_Foundation->release(); }
        }

        // Enable/disable PhysX scene debug visualization and which primitives to emit
        BOOM_INLINE void EnableDebugVisualization(bool enable, float scale = 1.0f)
        {
            m_DebugVisEnabled = enable;

            // Global scale. Set to 0.0f to turn off all visualization.
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, enable ? scale : 0.0f);

            if (!enable) return;

            // Keep shapes only; turn off extra clutter that can block view.
            //m_Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
            // Common extras, enable as needed:
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_AABBS, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eBODY_MASS_AXES, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, 1.0f);
        }

        // Convert PhysX' color (ARGB packed) to glm::vec4 RGBA
        BOOM_INLINE static glm::vec4 UnpackPxColor(PxU32 c)
        {
            float a = ((c >> 24) & 0xFF) / 255.0f;
            float r = ((c >> 16) & 0xFF) / 255.0f;
            float g = ((c >> 8) & 0xFF) / 255.0f;
            float b = ((c >> 0) & 0xFF) / 255.0f;
            return { r, g, b, a };
        }

        // Gather current PhysX debug buffer as line segments to feed your renderer
        BOOM_INLINE void CollectDebugLines(std::vector<DebugLine>& outLines) const
        {
            outLines.clear();
            if (!m_DebugVisEnabled || !m_Scene) return;

            const PxRenderBuffer& rb = m_Scene->getRenderBuffer();

            // Lines
            const PxU32 nLines = rb.getNbLines();
            const PxDebugLine* lines = rb.getLines();
            for (PxU32 i = 0; i < nLines; ++i) {
                DebugLine dl;
                dl.p0 = { lines[i].pos0.x, lines[i].pos0.y, lines[i].pos0.z };
                dl.p1 = { lines[i].pos1.x, lines[i].pos1.y, lines[i].pos1.z };
                dl.c0 = UnpackPxColor(lines[i].color0);
                dl.c1 = UnpackPxColor(lines[i].color1);
                outLines.push_back(dl);
            }

            // Triangles (emit 3 edges)
            const PxU32 nTris = rb.getNbTriangles();
            const PxDebugTriangle* tris = rb.getTriangles();
            for (PxU32 i = 0; i < nTris; ++i) {
                glm::vec3 a{ tris[i].pos0.x, tris[i].pos0.y, tris[i].pos0.z };
                glm::vec3 b{ tris[i].pos1.x, tris[i].pos1.y, tris[i].pos1.z };
                glm::vec3 c{ tris[i].pos2.x, tris[i].pos2.y, tris[i].pos2.z };
                glm::vec4 ca = UnpackPxColor(tris[i].color0);
                glm::vec4 cb = UnpackPxColor(tris[i].color1);
                glm::vec4 cc = UnpackPxColor(tris[i].color2);

                outLines.push_back(DebugLine{ a, b, ca, cb });  
                outLines.push_back(DebugLine{ b, c, cb, cc });
                outLines.push_back(DebugLine{ c, a, cc, ca });
            }

            // Points (draw as tiny axis-cross lines)
            const PxU32 nPts = rb.getNbPoints();
            const PxDebugPoint* pts = rb.getPoints();
            const float s = 0.02f; // point cross half-size
            for (PxU32 i = 0; i < nPts; ++i) {
                glm::vec3 p{ pts[i].pos.x, pts[i].pos.y, pts[i].pos.z };
                glm::vec4 c = UnpackPxColor(pts[i].color);

                outLines.push_back(DebugLine{ p + glm::vec3(-s, 0, 0), p + glm::vec3(+s, 0, 0), c, c });
                outLines.push_back(DebugLine{ p + glm::vec3(0, -s, 0), p + glm::vec3(0, +s, 0), c, c });
                outLines.push_back(DebugLine{ p + glm::vec3(0, 0, -s), p + glm::vec3(0, 0, +s), c, c });
            }
        }

        BOOM_INLINE void UpdateColliderShape(Entity& entity, AssetRegistry& assetRegistry) {
            if (!entity.Has<RigidBodyComponent>() || !entity.Has<ColliderComponent>()) {
                return;
            }

            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            auto& collider = entity.Get<ColliderComponent>().Collider;
            PxTransform userLocalPose(ToPxVec3(collider.localPosition), ToPxQuat(collider.localRotation));
            if (!body.actor) return;

            // 1. --- Destroy the old shape ---
            if (collider.Shape) {
                body.actor->detachShape(*collider.Shape);
                collider.Shape->release();
                collider.Shape = nullptr;
            }

            // 2. --- Re-create the shape with the new scale (logic moved from AddRigidBody) ---
            if (collider.type == Collider3D::BOX) {
                PxBoxGeometry box(ToPxVec3((transform.scale * collider.localScale) / 2.0f));
                collider.Shape = m_Physics->createShape(box, *collider.material);
                collider.Shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::SPHERE) {
                PxSphereGeometry sphere((transform.scale.x * collider.localScale.x) / 2.0f);
                collider.Shape = m_Physics->createShape(sphere, *collider.material);
                collider.Shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::CAPSULE) {
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
                Axis majorAxis = AXIS_X;
                if (s.y > s.x && s.y > s.z) majorAxis = AXIS_Y;
                else if (s.z > s.x && s.z > s.y) majorAxis = AXIS_Z;
                float radius, halfHeight;
                if (majorAxis == AXIS_Y) { radius = 0.5f * std::max(s.x, s.z); halfHeight = 0.5f * s.y; }
                else if (majorAxis == AXIS_Z) { radius = 0.5f * std::max(s.x, s.y); halfHeight = 0.5f * s.z; }
                else { radius = 0.5f * std::max(s.y, s.z); halfHeight = 0.5f * s.x; }
                halfHeight = halfHeight - radius;
                const float kMin = 0.01f;
                if (radius <= 0.0f) radius = kMin;
                if (halfHeight <= 0.0f) halfHeight = kMin;
                PxCapsuleGeometry capsule(radius, halfHeight);
                collider.Shape = m_Physics->createShape(capsule, *collider.material);
                PxQuat localQ = PxQuat(PxIdentity);
                if (majorAxis == AXIS_Y) localQ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                else if (majorAxis == AXIS_Z) localQ = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                PxTransform capsuleAxisPose(PxVec3(0.0f), localQ);
                collider.Shape->setLocalPose(userLocalPose * capsuleAxisPose);
            }
            else if (collider.type == Collider3D::CONVEX_MESH)
            {
                if (collider.physicsMeshID == EMPTY_ASSET) return;
                auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                // 1. Load if missing
                if (!asset.mesh) {
                    asset.mesh = LoadCookedMesh(asset.cookedMeshPath);
                }

                // 2. Create Shape
                if (asset.mesh) {
                    PxConvexMeshGeometry convexGeom(asset.mesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to load CONVEX mesh for asset: {}", asset.name);
                }
            }

            // Case 2: TRIANGLE MESH (For Terrain, Stairs, Level Geometry)
            else if (collider.type == Collider3D::TRIANGLE_MESH)
            {
                // Safety Check: PhysX forbids Triangle Meshes on Dynamic actors
                // We check if 'body' exists (AddRigidBody) or assume static (AddColliderOnly)
#ifdef ADD_RIGID_BODY_SCOPE 
                if (body.type != RigidBody3D::STATIC) {
                    BOOM_ERROR("Cannot use TRIANGLE_MESH on Dynamic Rigidbody '{}'. Switch to CONVEX.", entity.Get<InfoComponent>().name);
                    return;
                }
#endif

                if (collider.physicsMeshID == EMPTY_ASSET) return;
                auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                // 1. Load if missing
                if (!asset.triangleMesh) {
                    asset.triangleMesh = LoadCookedTriangleMesh(asset.cookedMeshPath);
                }

                // 2. Create Shape
                if (asset.triangleMesh) {
                    PxTriangleMeshGeometry triGeom(asset.triangleMesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                    collider.Shape = m_Physics->createShape(triGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to load TRIANGLE mesh for asset: {}. Did you cook it as 'Exact'?", asset.name);
                }
            }
            else if (collider.type == Collider3D::PLANE) {
                if (body.type == RigidBody3D::DYNAMIC) {
                    BOOM_WARN("Plane colliders must be STATIC. Forcing body type to STATIC.");
                    body.type = RigidBody3D::STATIC;
                }

                PxPlaneGeometry planeGeom;
                collider.Shape = m_Physics->createShape(planeGeom, *collider.material);

                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                PxQuat planeRot = PxQuat(PxIdentity);

                if (s.y < s.x && s.y < s.z) {
                    planeRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                }
                else if (s.z < s.x && s.z < s.y) {
                    planeRot = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                }

                collider.Shape->setLocalPose(userLocalPose * PxTransform(PxVec3(0.0f), planeRot));
            }
            else if (collider.type == Collider3D::CYLINDER) {
                // 1. Get absolute scale
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);

                // 2. Create Unit Cylinder (Radius 1, HalfHeight 1)
                PxConvexMesh* cylinderMesh = CreateCylinderMesh(1.0f, 1.0f);

                if (cylinderMesh) {
                    PxConvexMeshGeometry convexGeom(cylinderMesh);

                    // 3. Map Scale consistently:
                    //    - Y is always Height
                    //    - Max(X, Z) is Radius
                    //    - Multiply by 0.5f so standard scale 1.0 = size 1.0 (Diameter)
                    float radius = 0.5f * std::max(s.x, s.z);
                    float height = 0.5f * s.y;

                    convexGeom.scale = PxMeshScale(PxVec3(radius, height, radius));
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);

                    // 4. No dynamic rotation based on scale. 
                    //    Use userLocalPose directly (handles manual offsets).
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to create cylinder mesh");
                    return;
                }
            }
            else if (collider.type == Collider3D::TRIANGLE) {
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);

                // Create triangle mesh
                PxConvexMesh* triangleMesh = CreateTriangleMesh(s);
                if (triangleMesh) {
                    PxConvexMeshGeometry convexGeom(triangleMesh);
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to create triangle mesh");
                    return;
                }
            }

            // 3. --- Set all flags BEFORE attaching the shape ---
            if (collider.Shape) {
                // Set visualization flag (for debug rendering)
                collider.Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);

                // Configure trigger vs collision behavior
                if (collider.isTrigger) {
                    // PURE TRIGGER: No physical simulation, only trigger events
                    collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    collider.Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false); // Optional: exclude from scene queries too
                }
                else {
                    // NORMAL COLLIDER: Physical simulation enabled
                    collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    collider.Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                }

                // NOW attach the shape to the actor
                body.actor->attachShape(*collider.Shape);

                if (body.type == RigidBody3D::DYNAMIC) {
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                }
            }
            else {
                BOOM_ERROR("Failed to create collider shape in UpdateColliderShape");
            }
        }

        // Rigid Body
        BOOM_INLINE void AddRigidBody(Entity& entity, AssetRegistry& assetRegistry) {
            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            //bool hasCollider = entity.Has<ColliderComponent>();

        // ============================================================
        // === STEP 1: CLEANUP EXISTING ACTORS (Prevent Duplicates) ===
        // ============================================================

        // A. Check for an existing "Collider-Only" actor (Orphan)
        // If we are adding a RigidBody, the old static collider actor must die.
            if (entity.Has<ColliderComponent>()) {
                auto& collider = entity.Get<ColliderComponent>().Collider;
                if (collider.actor) {
                    if (collider.actor->userData) {
                        delete static_cast<EntityID*>(collider.actor->userData);
                    }
                    m_Scene->removeActor(*collider.actor);
                    collider.actor->release();
                    collider.actor = nullptr;
                }
            }

            // B. Check for an existing RigidBody actor (Re-initialization)
            // If we are re-adding the RB, destroy the old one first.
            if (body.actor) {
                if (body.actor->userData) {
                    delete static_cast<EntityID*>(body.actor->userData);
                }
                m_Scene->removeActor(*body.actor);
                body.actor->release();
                body.actor = nullptr;
            }
            // ============================================================

            // create rigidbody transformation
            PxTransform pose(ToPxVec3(transform.translate));

            glm::vec3 eulerRadians = glm::radians(transform.rotate);
            glm::quat rot = glm::quat(eulerRadians);

            // Normalize the quaternion to ensure it's valid
            rot = glm::normalize(rot);

            pose.q = PxQuat(rot.x, rot.y, rot.z, rot.w);

            // create a rigid body actor
            if (entity.template Has<ColliderComponent>())
            {
                // create collider shape

                auto& collider = entity.Get<ColliderComponent>().Collider;
                PxTransform userLocalPose(ToPxVec3(collider.localPosition), ToPxQuat(collider.localRotation));
                // create collider material
                collider.material = m_Physics->createMaterial(collider.staticFriction,
                    collider.dynamicFriction,
                    collider.restitution);

                if (collider.type == Collider3D::BOX)
                {
                    glm::vec3 halfExtents = (transform.scale * collider.localScale) / 2.0f;

                    // Validate: ensure minimum dimensions to prevent zero-volume geometries
                    const float kMinDimension = 0.01f;
                    halfExtents.x = std::max(halfExtents.x, kMinDimension);
                    halfExtents.y = std::max(halfExtents.y, kMinDimension);
                    halfExtents.z = std::max(halfExtents.z, kMinDimension);

                    PxBoxGeometry box(ToPxVec3(halfExtents));

                    if (!box.isValid()) {
                        BOOM_ERROR("Invalid box geometry for entity. HalfExtents: ({}, {}, {})",
                            halfExtents.x, halfExtents.y, halfExtents.z);
                        return;
                    }

                    collider.Shape = m_Physics->createShape(box, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else if (collider.type == Collider3D::SPHERE) {
                    float radius = (transform.scale.x * collider.localScale.x) / 2.0f;
                    const float kMinRadius = 0.01f;
                    radius = std::max(radius, kMinRadius);

                    PxSphereGeometry sphere(radius);
                    if (!sphere.isValid()) {
                        BOOM_ERROR("Invalid sphere geometry. Radius: {}", radius);
                        return;
                    }

                    collider.Shape = m_Physics->createShape(sphere, *collider.material);
                    PxTransform relativePose(PxQuat(0, PxVec3(0, 0, 1)));
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else if (collider.type == Collider3D::CAPSULE) {
                    if (!m_Physics || !collider.material) {
                        return;
                    }

                    // Decide which axis the capsule should align to based on the largest scale component.
                    const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                    enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
                    Axis majorAxis = AXIS_X;
                    if (s.y > s.x && s.y > s.z) {
                        majorAxis = AXIS_Y;
                    }
                    else if (s.z > s.x && s.z > s.y) {
                        majorAxis = AXIS_Z;
                    }

                    float radius, halfHeight;

                    // Correctly calculate radius and half-height based on the major axis.
                    if (majorAxis == AXIS_Y) { // Y is the longest axis
                        radius = 0.5f * std::max(s.x, s.z);
                        halfHeight = 0.5f * s.y;
                    }
                    else if (majorAxis == AXIS_Z) { // Z is the longest axis
                        radius = 0.5f * std::max(s.x, s.y);
                        halfHeight = 0.5f * s.z;
                    }
                    else { // X is the longest axis (or it's a uniform scale)
                        radius = 0.5f * std::max(s.y, s.z);
                        halfHeight = 0.5f * s.x;
                    }

                    // The halfHeight parameter for PhysX is for the CYLINDRICAL part only.
                    // We must subtract the radius from the half-length of the major axis.
                    halfHeight = halfHeight - radius;

                    // Enforce positive dimensions to prevent invalid geometry.
                    const float kMin = 0.01f;
                    if (radius <= 0.0f)     radius = kMin;
                    if (halfHeight <= 0.0f) halfHeight = kMin; // For a sphere, this will be kMin.

                    PxCapsuleGeometry capsule(radius, halfHeight);
                    PX_ASSERT(capsule.isValid());

                    collider.Shape = m_Physics->createShape(capsule, *collider.material);
                    if (!collider.Shape) {
                        BOOM_ERROR("PxPhysics::createShape failed for capsule");
                        return;
                    }

                    // Rotate the capsule from PhysX's default +X axis to our chosen major axis.
                    PxQuat localQ = PxQuat(PxIdentity); // Default rotation for X-axis alignment
                    if (majorAxis == AXIS_Y) {
                        // Rotate +90 degrees around Z to map X -> Y
                        localQ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                    }
                    else if (majorAxis == AXIS_Z) {
                        // Rotate -90 degrees around Y to map X -> Z
                        localQ = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                    }

                    PxTransform capsuleAxisPose(PxVec3(0.0f), localQ);
                    collider.Shape->setLocalPose(userLocalPose * capsuleAxisPose);
                }

                else if (collider.type == Collider3D::CONVEX_MESH)
                {
                    if (collider.physicsMeshID == EMPTY_ASSET) return;
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                    // 1. Load if missing
                    if (!asset.mesh) {
                        asset.mesh = LoadCookedMesh(asset.cookedMeshPath);
                    }

                    // 2. Create Shape
                    if (asset.mesh) {
                        PxConvexMeshGeometry convexGeom(asset.mesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                        collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                        collider.Shape->setLocalPose(userLocalPose);
                    }
                    else {
                        BOOM_ERROR("Failed to load CONVEX mesh for asset: {}", asset.name);
                    }
                }

                // Case 2: TRIANGLE MESH (For Terrain, Stairs, Level Geometry)
                else if (collider.type == Collider3D::TRIANGLE_MESH)
                {
                    // Safety Check: PhysX forbids Triangle Meshes on Dynamic actors
                    // We check if 'body' exists (AddRigidBody) or assume static (AddColliderOnly)
                    #ifdef ADD_RIGID_BODY_SCOPE 
                    if (body.type != RigidBody3D::STATIC) {
                        BOOM_ERROR("Cannot use TRIANGLE_MESH on Dynamic Rigidbody '{}'. Switch to CONVEX.", entity.Get<InfoComponent>().name);
                        return;
                    }
                    #endif

                    if (collider.physicsMeshID == EMPTY_ASSET) return;
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                    // 1. Load if missing
                    if (!asset.triangleMesh) {
                        asset.triangleMesh = LoadCookedTriangleMesh(asset.cookedMeshPath);
                    }

                    // 2. Create Shape
                    if (asset.triangleMesh) {
                        PxTriangleMeshGeometry triGeom(asset.triangleMesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                        collider.Shape = m_Physics->createShape(triGeom, *collider.material);
                        collider.Shape->setLocalPose(userLocalPose);
                    }
                    else {
                        BOOM_ERROR("Failed to load TRIANGLE mesh for asset: {}. Did you cook it as 'Exact'?", asset.name);
                    }
                }
                else if (collider.type == Collider3D::PLANE)
                {
                    if (body.type == RigidBody3D::DYNAMIC) {
                        BOOM_WARN("Plane colliders must be STATIC. Forcing actor type to STATIC.");
                        body.type = RigidBody3D::STATIC;
                    }

                    // Create the default plane geometry
                    PxPlaneGeometry planeGeom;
                    collider.Shape = m_Physics->createShape(planeGeom, *collider.material);

                    const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                    PxQuat planeRot = PxQuat(PxIdentity); // Default: +X normal (for YZ walls)

                    if (s.y < s.x && s.y < s.z) {
                        // Y is smallest -> Ground plane (+Y normal)
                        planeRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)); // +90 deg around Z
                    }
                    else if (s.z < s.x && s.z < s.y) {
                        // Z is smallest -> XY wall (+Z normal)
                        planeRot = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f)); // -90 deg around Y
                    }
                    // Else: X is smallest, use Identity (default +X normal)

                    // Combine user's local pose with our auto-rotation
                    collider.Shape->setLocalPose(userLocalPose * PxTransform(PxVec3(0.0f), planeRot));
                }
                else if (collider.type == Collider3D::CYLINDER) {
                    // 1. Get absolute scale
                    const glm::vec3 s = glm::abs(transform.scale * collider.localScale);

                    // 2. Create Unit Cylinder (Radius 1, HalfHeight 1)
                    PxConvexMesh* cylinderMesh = CreateCylinderMesh(1.0f, 1.0f);

                    if (cylinderMesh) {
                        PxConvexMeshGeometry convexGeom(cylinderMesh);

                        // 3. Map Scale consistently:
                        //    - Y is always Height
                        //    - Max(X, Z) is Radius
                        //    - Multiply by 0.5f so standard scale 1.0 = size 1.0 (Diameter)
                        float radius = 0.5f * std::max(s.x, s.z);
                        float height = 0.5f * s.y;

                        convexGeom.scale = PxMeshScale(PxVec3(radius, height, radius));
                        collider.Shape = m_Physics->createShape(convexGeom, *collider.material);

                        // 4. No dynamic rotation based on scale. 
                        //    Use userLocalPose directly (handles manual offsets).
                        collider.Shape->setLocalPose(userLocalPose);
                    }
                    else {
                        BOOM_ERROR("Failed to create cylinder mesh");
                        return;
                    }
                }
                else if (collider.type == Collider3D::TRIANGLE) {
                    const glm::vec3 s = glm::abs(transform.scale * collider.localScale);

                    PxConvexMesh* triangleMesh = CreateTriangleMesh(s);
                    if (triangleMesh) {
                        PxConvexMeshGeometry convexGeom(triangleMesh);
                        collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                        collider.Shape->setLocalPose(userLocalPose);
                    }
                    }

                // Ensure shape is included in debug viz and set trigger flags BEFORE attaching
                if (collider.Shape) {
                    collider.Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);

                    // Configure trigger vs collision behavior
                    if (collider.isTrigger) {
                        collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                        collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    }
                    else {
                        collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                        collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    }
                }

                // create actor instanace
                // create actor instanace
                if (body.type == RigidBody3D::DYNAMIC || body.type == RigidBody3D::KINEMATIC)
                {
                    body.actor = PxCreateDynamic(*m_Physics, pose, *collider.Shape, body.density);

                    // Recalculate the mass and inertia tensor.
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                    body.actor->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, true);

                    PxRigidDynamic* dyn = static_cast<PxRigidDynamic*>(body.actor);
                    if (dyn) {
                        dyn->setLinearVelocity(PxVec3(body.initialVelocity.x, body.initialVelocity.y, body.initialVelocity.z));


                        if (body.type == RigidBody3D::KINEMATIC) {
                            dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
                        }
                        dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                        dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                        dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                    }
                }
                else // STATIC
                {
                    body.actor
                        = PxCreateStatic(*m_Physics,
                            pose, *collider.Shape);
                }
            }
            else
            {
                if (body.type == RigidBody3D::DYNAMIC)
                {
                    body.actor = m_Physics->createRigidDynamic(pose);
                }
                else if (body.type == RigidBody3D::STATIC)
                {
                    body.actor = m_Physics->createRigidStatic(pose);


                }
            }

            // check actor
            if (!body.actor)
            {
                BOOM_ERROR("Error creating dynamic actor");
                return;
            }

            // Opt-in the actor to debug visualization
            body.actor->setActorFlag(PxActorFlag::eVISUALIZATION, true);

            // set user data to entt id
            body.actor->userData = new EntityID(entity.ID());

            // add actor to the m_Scene
            m_Scene->addActor(*body.actor);
        }


        BOOM_INLINE void AddColliderOnly(Entity& entity, AssetRegistry& assetRegistry) {
            // ============================================================
                // === STEP 1: ABORT IF RIGIDBODY EXISTS ===
                // ============================================================
                // If this entity has a RigidBody, IT owns the actor. 
                // We should NOT create a separate static actor here.
            if (entity.Has<RigidBodyComponent>()) {
                // Stop. The AddRigidBody function handles actor creation.
                return;
            }

            if (!entity.Has<ColliderComponent>()) {
                BOOM_WARN("AddColliderOnly called on entity without ColliderComponent");
                return;
            }

            auto& transform = entity.Get<TransformComponent>().transform;
            auto& collider = entity.Get<ColliderComponent>().Collider;

            // ============================================================
            // === STEP 2: CLEANUP EXISTING ORPHAN (Re-initialization) ===
            // ============================================================
            if (collider.actor) {
                if (collider.actor->userData) {
                    delete static_cast<EntityID*>(collider.actor->userData);
                }
                m_Scene->removeActor(*collider.actor);
                collider.actor->release();
                collider.actor = nullptr;
            }

            // Create transformation
            PxTransform pose(ToPxVec3(transform.translate));
            glm::vec3 eulerRadians = glm::radians(transform.rotate);
            glm::quat rot = glm::quat(eulerRadians);
            rot = glm::normalize(rot);
            pose.q = PxQuat(rot.x, rot.y, rot.z, rot.w);

            // Create collider material
            collider.material = m_Physics->createMaterial(
                collider.staticFriction,
                collider.dynamicFriction,
                collider.restitution
            );

            PxTransform userLocalPose(ToPxVec3(collider.localPosition), ToPxQuat(collider.localRotation));

            // --- SHAPE CREATION ---

            if (collider.type == Collider3D::BOX) {
                PxBoxGeometry box(ToPxVec3((transform.scale * collider.localScale) / 2.0f));
                collider.Shape = m_Physics->createShape(box, *collider.material);
                collider.Shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::SPHERE) {
                PxSphereGeometry sphere((transform.scale.x * collider.localScale.x) / 2.0f);
                collider.Shape = m_Physics->createShape(sphere, *collider.material);
                collider.Shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::CAPSULE) {
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
                Axis majorAxis = AXIS_X;
                if (s.y > s.x && s.y > s.z) majorAxis = AXIS_Y;
                else if (s.z > s.x && s.z > s.y) majorAxis = AXIS_Z;

                float radius, halfHeight;
                if (majorAxis == AXIS_Y) {
                    radius = 0.5f * std::max(s.x, s.z);
                    halfHeight = 0.5f * s.y;
                }
                else if (majorAxis == AXIS_Z) {
                    radius = 0.5f * std::max(s.x, s.y);
                    halfHeight = 0.5f * s.z;
                }
                else {
                    radius = 0.5f * std::max(s.y, s.z);
                    halfHeight = 0.5f * s.x;
                }

                halfHeight = halfHeight - radius;
                const float kMin = 0.01f;
                if (radius <= 0.0f) radius = kMin;
                if (halfHeight <= 0.0f) halfHeight = kMin;

                PxCapsuleGeometry capsule(radius, halfHeight);
                collider.Shape = m_Physics->createShape(capsule, *collider.material);

                PxQuat localQ = PxQuat(PxIdentity);
                if (majorAxis == AXIS_Y) localQ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                else if (majorAxis == AXIS_Z) localQ = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));

                PxTransform capsuleAxisPose(PxVec3(0.0f), localQ);
                collider.Shape->setLocalPose(userLocalPose * capsuleAxisPose);
            }
            // ----------------------------------------------------------
            // NEW: Explicit CONVEX MESH logic
            // ----------------------------------------------------------
            else if (collider.type == Collider3D::CONVEX_MESH)
            {
                if (collider.physicsMeshID == EMPTY_ASSET) return;
                auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                if (!asset.mesh) {
                    asset.mesh = LoadCookedMesh(asset.cookedMeshPath);
                }

                if (asset.mesh) {
                    PxConvexMeshGeometry convexGeom(asset.mesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to load CONVEX mesh for asset: {}", asset.name);
                }
            }
            // ----------------------------------------------------------
            // NEW: Explicit TRIANGLE MESH logic
            // ----------------------------------------------------------
            else if (collider.type == Collider3D::TRIANGLE_MESH)
            {
                if (collider.physicsMeshID == EMPTY_ASSET) return;
                auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);

                if (!asset.triangleMesh) {
                    asset.triangleMesh = LoadCookedTriangleMesh(asset.cookedMeshPath);
                }

                if (asset.triangleMesh) {
                    PxTriangleMeshGeometry triGeom(asset.triangleMesh, PxMeshScale(ToPxVec3(transform.scale * collider.localScale)));
                    collider.Shape = m_Physics->createShape(triGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
                else {
                    BOOM_ERROR("Failed to load TRIANGLE mesh for asset: {}. Check cooking format.", asset.name);
                }
            }
            else if (collider.type == Collider3D::PLANE) {
                PxPlaneGeometry planeGeom;
                collider.Shape = m_Physics->createShape(planeGeom, *collider.material);

                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                PxQuat planeRot = PxQuat(PxIdentity);

                if (s.y < s.x && s.y < s.z) {
                    planeRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                }
                else if (s.z < s.x && s.z < s.y) {
                    planeRot = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                }

                collider.Shape->setLocalPose(userLocalPose * PxTransform(PxVec3(0.0f), planeRot));
            }
            else if (collider.type == Collider3D::CYLINDER) {
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                PxConvexMesh* cylinderMesh = CreateCylinderMesh(1.0f, 1.0f);

                if (cylinderMesh) {
                    PxConvexMeshGeometry convexGeom(cylinderMesh);
                    float radius = 0.5f * std::max(s.x, s.z);
                    float height = 0.5f * s.y;

                    convexGeom.scale = PxMeshScale(PxVec3(radius, height, radius));
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
            }
            else if (collider.type == Collider3D::TRIANGLE) {
                const glm::vec3 s = glm::abs(transform.scale * collider.localScale);
                PxConvexMesh* triangleMesh = CreateTriangleMesh(s);
                if (triangleMesh) {
                    PxConvexMeshGeometry convexGeom(triangleMesh);
                    collider.Shape = m_Physics->createShape(convexGeom, *collider.material);
                    collider.Shape->setLocalPose(userLocalPose);
                }
            }

            // --- ACTOR CREATION ---

            if (collider.Shape) {
                collider.Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);

                // Configure trigger vs collision behavior
                if (collider.isTrigger) {
                    collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                else {
                    collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                }
            }
            else {
                BOOM_ERROR("Failed to create collider shape in AddColliderOnly");
                return;
            }

            // Create a STATIC actor to hold the shape
            PxRigidStatic* staticActor = m_Physics->createRigidStatic(pose);
            if (!staticActor) {
                BOOM_ERROR("Failed to create static actor for collider-only entity");
                collider.Shape->release();
                collider.Shape = nullptr;
                return;
            }

            // Attach shape to actor
            staticActor->attachShape(*collider.Shape);
            staticActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);

            // Store entity ID
            staticActor->userData = new EntityID(entity.ID());

            // Add to scene
            m_Scene->addActor(*staticActor);

			// Attach the actor to the collider component
            collider.actor = staticActor;

            // Store actor reference in a RigidBodyComponent if it exists
            if (entity.Has<RigidBodyComponent>()) {
                entity.Get<RigidBodyComponent>().RigidBody.actor = staticActor;
                entity.Get<RigidBodyComponent>().RigidBody.type = RigidBody3D::STATIC;
            }

            BOOM_INFO("Created collider-only entity");
        }

        BOOM_INLINE void SetRigidBodyType(Entity& entity, RigidBody3D::Type newType)
        {
            if (!entity.Has<RigidBodyComponent>()) return;

            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            auto* oldActor = body.actor;

            // 1. --- Guard Clause: If the type isn't changing, do nothing ---
            if (!oldActor || body.type == newType) {
                return;
            }

            // 2. --- Preserve all essential properties from the old actor ---
            PxTransform transform = oldActor->getGlobalPose();
            EntityID* userData = static_cast<EntityID*>(oldActor->userData);

            // Get all shapes from the old actor. An actor can have multiple shapes.
            const PxU32 numShapes = oldActor->getNbShapes();
            std::vector<PxShape*> shapes(numShapes);
            oldActor->getShapes(shapes.data(), numShapes);

            // 3. --- Remove and release the old actor ---
            m_Scene->removeActor(*oldActor);
            oldActor->release();

            // 4. --- Create the new actor of the desired type ---
            PxRigidActor* newActor = nullptr;
            if (newType == RigidBody3D::DYNAMIC)
            {
                PxRigidDynamic* dyn = m_Physics->createRigidDynamic(transform);
                PxRigidBodyExt::updateMassAndInertia(*dyn, body.density);
                dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false); // Ensure it's not kinematic
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                newActor = dyn;
            }
            else if (newType == RigidBody3D::KINEMATIC)
            {
                PxRigidDynamic* dyn = m_Physics->createRigidDynamic(transform);
                PxRigidBodyExt::updateMassAndInertia(*dyn, body.density);
                dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true); // Set it to kinematic
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                newActor = dyn;
            }
            else // newType is STATIC
            {
                newActor = m_Physics->createRigidStatic(transform);
            }

            // 5. --- Re-attach all shapes and restore user data ---
            if (newActor) {
                for (PxShape* shape : shapes) {
                    newActor->attachShape(*shape);
                }
                newActor->userData = userData;
                m_Scene->addActor(*newActor);
            }

            // 6. --- IMPORTANT: Update our component to point to the new actor and type ---
            body.actor = newActor;
            body.type = newType;
        }

        BOOM_INLINE void SetColliderType(Entity& entity, Collider3D::Type newType, AssetRegistry& assetRegistry) {
            if (!entity.Has<ColliderComponent>()) return;
            auto& collider = entity.Get<ColliderComponent>().Collider;

            if (collider.type == newType) {
                return;
            }
            collider.type = newType;

            // Pass the asset registry to the update function
            UpdateColliderShape(entity, assetRegistry);
        }

        //raycast functions
        BOOM_INLINE glm::vec3 ResolveThirdPersonCameraPosition(glm::vec3 const& playerEye, glm::vec3 const& idealCamPosition, float minDist = 0.5f)
        {
            PxVec3 targetPos{ ToPxVec3(playerEye) };
            PxVec3 idealCamPos{ ToPxVec3(idealCamPosition) };
            PxVec3 dir = (idealCamPos - targetPos).getNormalized();
            PxReal maxDist = (idealCamPos - targetPos).magnitude();

            PxRaycastBuffer hit;
            if (m_Scene->raycast(targetPos, dir, maxDist, hit))
                return ToGLMVec3(targetPos + dir * PxMax(hit.block.distance - 0.05f, minDist));

            return ToGLMVec3(idealCamPos);
        }
        BOOM_INLINE void SetRotationLock(Boom::Entity entity, bool lockX, bool lockY, bool lockZ)
        {
            {
                if (!entity.Has<Boom::RigidBodyComponent>())
                    return;

                auto& rc = entity.Get<Boom::RigidBodyComponent>();
                if (!rc.RigidBody.actor)
                    return;

                // Get the dynamic actor (constraints only apply to dynamic bodies)
                physx::PxRigidDynamic* dynActor = rc.RigidBody.actor->is<physx::PxRigidDynamic>();
                if (dynActor)
                {
                    // Set each angular lock flag individually
                    dynActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, lockX);
                    dynActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, lockY);
                    dynActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, lockZ);
                }
            }
        }
        // Mesh Colliders
        BOOM_INLINE PxConvexMeshGeometry CookMesh(const MeshData<ShadedVert>& data) {
            // px vertex container
            std::vector<PxVec3> vertices;



            // convert position attributes
            for (auto& vertex : data.vtx)
            {
                vertices.push_back(ToPxVec3(vertex.pos));
            }

            PxConvexMeshDesc meshDesc;
            // vertices
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());
            // indices
            meshDesc.indices.data = data.idx.data();
            meshDesc.indices.count = static_cast<PxU32>(data.idx.size());
            // flags
            meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

            // cooking the mesh
            PxCookingParams cookingParams =
                PxCookingParams(PxTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION,
                *m_Foundation, cookingParams);
            PxConvexMeshCookingResult::Enum result;
            PxConvexMesh* convexMesh = cooking->createConvexMesh(meshDesc,
                m_Physics->getPhysicsInsertionCallback(), &result);
            PxConvexMeshGeometry convexMeshGeometry(convexMesh);

            cooking->release();
            return convexMeshGeometry;

        }

        BOOM_INLINE void UpdatePhysicsMaterial(Entity& ent) {
            // Ensure the entity has the required components
            if (!ent.Has<RigidBodyComponent>() || !ent.Has<ColliderComponent>()) {
                return;
            }

            auto& collider = ent.Get<ColliderComponent>().Collider;
            auto* actor = ent.Get<RigidBodyComponent>().RigidBody.actor;

            if (!actor || !collider.Shape) {
                BOOM_WARN("Attempted to update physics material on an entity with no actor or shape.");
                return;
            }

            // A shape can have multiple materials, but we are using just one.
            // We retrieve the material that is currently attached to the shape.
            PxMaterial* material;
            collider.Shape->getMaterials(&material, 1);

            if (material) {
                // Apply the values from our component to the live PxMaterial.
                material->setDynamicFriction(collider.dynamicFriction);
                material->setStaticFriction(collider.staticFriction);
                material->setRestitution(collider.restitution);
            }
        }

        void UpdateRigidBodyTransform(Entity entity, const Transform3D& transform)
        {
            if (!entity.Has<Boom::RigidBodyComponent>())
                return;

            auto& rc = entity.Get<Boom::RigidBodyComponent>();

            if (!rc.RigidBody.actor)
                return;

            // Convert GLM transform to PhysX pose
            PxTransform pose;
            pose.p = PxVec3(transform.translate.x, transform.translate.y, transform.translate.z);

            // Convert euler angles to quaternion
            glm::quat quat = glm::quat(glm::radians(transform.rotate));
            pose.q = PxQuat(quat.x, quat.y, quat.z, quat.w);

            // Update the actor's global pose
            rc.RigidBody.actor->setGlobalPose(pose);

            // If dynamic, wake it up
            if (rc.RigidBody.type == RigidBody3D::Type::DYNAMIC)
            {
                static_cast<PxRigidDynamic*>(rc.RigidBody.actor)->wakeUp();
            }
        }

        BOOM_INLINE bool CompileAndSaveTriangleMesh(ModelAsset& modelAsset, const std::string& savePath)
        {
            if (!modelAsset.data) return false;

            // Ensure we have the mesh data
            auto staticModel = std::dynamic_pointer_cast<StaticModel>(modelAsset.data);
            if (!staticModel) {
                BOOM_ERROR("Only StaticModels are supported.");
                return false;
            }

            auto physicsMeshData = staticModel->GetMeshData();
            if (physicsMeshData.empty()) return false;
            auto& meshData = physicsMeshData[0];

            // 1. Setup Vertices
            std::vector<PxVec3> vertices;
            vertices.reserve(meshData.vtx.size());
            for (const auto& vertex : meshData.vtx) {
                vertices.push_back(ToPxVec3(vertex.pos));
            }

            // 2. Configure Description
            PxTriangleMeshDesc meshDesc;
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());

            // Assuming your indices are 32-bit (uint32_t)
            meshDesc.triangles.data = meshData.idx.data();
            meshDesc.triangles.stride = 3 * sizeof(uint32_t);
            meshDesc.triangles.count = static_cast<PxU32>(meshData.idx.size() / 3);

            // 3. Cook
            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);

            PxDefaultMemoryOutputStream buf;
            // CRITICAL: We use cookTriangleMesh here, NOT cookConvexMesh
            bool status = cooking->cookTriangleMesh(meshDesc, buf);
            cooking->release();

            if (!status) {
                BOOM_ERROR("Failed to cook triangle mesh.");
                return false;
            }

            // 4. Save
            std::ofstream outFile(savePath, std::ios::binary);
            if (!outFile) return false;
            outFile.write(reinterpret_cast<const char*>(buf.getData()), buf.getSize());
            outFile.close();

            return true;
        }

        BOOM_INLINE physx::PxTriangleMesh* LoadCookedTriangleMesh(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return nullptr;

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            char* buffer = new char[size];
            if (!file.read(buffer, size)) { delete[] buffer; return nullptr; }

            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer), static_cast<PxU32>(size));

            // CRITICAL: createTriangleMesh instead of createConvexMesh
            physx::PxTriangleMesh* mesh = m_Physics->createTriangleMesh(input);

            delete[] buffer;
            return mesh;
        }

        BOOM_INLINE physx::PxConvexMesh* LoadCookedMesh(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                BOOM_ERROR("Failed to open cooked mesh file: {}", path);
                return nullptr;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            char* buffer = new char[size];
            if (!file.read(buffer, size)) {
                BOOM_ERROR("Failed to read cooked mesh file: {}", path);
                delete[] buffer;
                return nullptr;
            }

            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer), static_cast<PxU32>(size));

            physx::PxConvexMesh* convexMesh = m_Physics->createConvexMesh(input);

            delete[] buffer;
            return convexMesh;
        }

        BOOM_INLINE bool CompileAndSavePhysicsMesh(ModelAsset& modelAsset, const std::string& savePath)
        {
            if (!modelAsset.data) return false;

            auto staticModel = std::dynamic_pointer_cast<StaticModel>(modelAsset.data);
            if (!staticModel) {
                BOOM_ERROR("Physics mesh cooking only supports StaticModel.");
                return false;
            }

            auto physicsMeshData = staticModel->GetMeshData();
            if (physicsMeshData.empty()) {
                BOOM_ERROR("Model has no mesh data to cook.");
                return false;
            }

            auto& meshData = physicsMeshData[0]; // Using the first mesh

            std::vector<PxVec3> vertices;
            vertices.reserve(meshData.vtx.size());
            for (const auto& vertex : meshData.vtx) {
                vertices.push_back(ToPxVec3(vertex.pos));
            }

            PxConvexMeshDesc meshDesc;
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());
            meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
            if (!cooking) {
                BOOM_ERROR("Failed to create PhysX cooking");
                return false;
            }

            PxDefaultMemoryOutputStream buf;
            //PxConvexMeshCookingResult::Enum result;
            bool status = cooking->cookConvexMesh(meshDesc, buf);
            cooking->release();

            if (!status) {
                BOOM_ERROR("Failed to cook convex mesh.");
                return false;
            }

            // Save the cooked data to file
            std::ofstream outFile(savePath, std::ios::binary);
            if (!outFile) {
                BOOM_ERROR("Failed to open file for writing cooked mesh: {}", savePath);
                return false;
            }
            outFile.write(reinterpret_cast<const char*>(buf.getData()), buf.getSize());
            outFile.close();

            BOOM_INFO("Successfully cooked and saved physics mesh to {}", savePath);
            return true;
        }

        BOOM_INLINE void Simulate([[maybe_unused]] uint32_t step, float dt)
        {
            const float FIXED_TIMESTEP = 0.016f;

            float accumulatedTime = dt;
            uint32_t substeps = 0;

            while (accumulatedTime > 0.0f && substeps < 4)  // Max 4 substeps
            {
                float timestep = std::min(accumulatedTime, FIXED_TIMESTEP);
                m_Scene->simulate(timestep);
                m_Scene->fetchResults(true);
                accumulatedTime -= timestep;
                substeps++;
            }
        }

        BOOM_INLINE void SetEventCallback(PxCallbackFunction&& callback)
        {
            m_EventCallback.m_Callback = callback;
        }

        BOOM_INLINE PxScene* GetPxScene() const { return m_Scene; }

        // Add this new function inside your PhysicsContext struct in Context.h
//
        BOOM_INLINE void RemoveRigidBody(Entity& entity)
        {
            PxRigidActor* actor = nullptr;

            // 1. Try to get actor from RigidBodyComponent
            if (entity.Has<RigidBodyComponent>()) {
                auto& rb = entity.Get<RigidBodyComponent>();
                actor = rb.RigidBody.actor;
                rb.RigidBody.actor = nullptr; // Clear the reference
            }
            // 2. If no RigidBody but has Collider, the actor might be stored elsewhere
            // For collider-only entities created via AddColliderOnly, we need to find it differently
            else if (entity.Has<ColliderComponent>()) {
                // The AddColliderOnly function doesn't store the actor anywhere accessible
                // We need to search the PhysX scene for it
                auto& collider = entity.Get<ColliderComponent>();

                // If we have a shape, we can get the actor from it
                if (collider.Collider.Shape) {
                    actor = collider.Collider.Shape->getActor();
                }
            }

            if (!actor) {
                return; // Nothing to remove
            }

            // 3. Clean up Collider pointers (if they exist)
            if (entity.Has<ColliderComponent>())
            {
                auto& collider = entity.Get<ColliderComponent>().Collider;

                if (collider.material) {
                    collider.material->release();
                    collider.material = nullptr;
                }

                if (collider.Shape) {
                    collider.Shape->release();
                    collider.Shape = nullptr;
                }
            }

            // 4. Clean up user data
            if (actor->userData) {
                EntityID* owner = static_cast<EntityID*>(actor->userData);
                delete owner;
                actor->userData = nullptr;
            }

            // 5. Remove actor from scene (THIS IS THE KEY STEP!)
            m_Scene->removeActor(*actor);

            // 6. Release actor memory
            actor->release();
        }

        BOOM_INLINE void ForceRemoveActor(uint32_t entityID)
        {
            if (!m_Scene) return;

            // 1. Get all actors
            PxU32 nbActors = m_Scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
            std::vector<PxActor*> actors(nbActors);
            m_Scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, actors.data(), nbActors);

            BOOM_INFO("[Physics] ForceRemoveActor checking {} actors for EntityID: {}", nbActors, entityID);

            int removedCount = 0;
            for (PxActor* actor : actors)
            {
                if (actor->userData)
                {
                    // Cast userData back to EntityID*
                    EntityID* ownerID = static_cast<EntityID*>(actor->userData);

                    // DEBUG: Log every actor's ID to see what's in the scene
                    // BOOM_INFO(" - Found Actor with OwnerID: {}", *ownerID); 

                    if (*ownerID == static_cast<EntityID>(entityID))
                    {
                        BOOM_INFO("[Physics] MATCH FOUND! Removing actor for EntityID: {}", entityID);
                        m_Scene->removeActor(*actor);
                        actor->release();
                        BOOM_DELETE(ownerID);
                        removedCount++;
                        // Continue in case duplicates exist
                    }
                }
                else
                {
                    // BOOM_WARN("[Physics] Found actor with NULL userData");
                }
            }

            if (removedCount == 0) {
                BOOM_WARN("[Physics] Failed to find any actor for EntityID: {}", entityID);
            }
        }

        // Helper to clean up actors for entities that ONLY have a Collider (Triggers)
        // Helper to clean up actors for entities that ONLY have a Collider (Triggers)
        BOOM_INLINE void RemoveColliderActor(Entity& entity)
        {
            if (!entity.Has<ColliderComponent>()) return;

            auto& collider = entity.Get<ColliderComponent>().Collider;

            // *** UPDATED: Use the stored actor pointer directly ***
            PxRigidActor* actor = collider.actor;

            if (actor)
            {
                // Clean up User Data to prevent memory leaks
                if (actor->userData) {
                    EntityID* ownerID = static_cast<EntityID*>(actor->userData);
                    BOOM_DELETE(ownerID);
                    actor->userData = nullptr;
                }

                // Remove from scene and release memory
                m_Scene->removeActor(*actor);
                actor->release();

                BOOM_INFO("Cleaned up Collider-Only Actor.");

                // Clear the pointer
                collider.actor = nullptr;
            }

            // Release the shape itself
            if (collider.Shape) {
                collider.Shape->release();
                collider.Shape = nullptr;
            }

            // Cleanup Material
            if (collider.material) {
                collider.material->release();
                collider.material = nullptr;
            }
        }

    private:
        // custom collision filter shader callback
        static PxFilterFlags CustomFilterShader
        (
            [[maybe_unused]] PxFilterObjectAttributes attributes0,
            [[maybe_unused]] PxFilterData filterData0,
            [[maybe_unused]] PxFilterObjectAttributes attributes1,
            [[maybe_unused]] PxFilterData filterData1,
            [[maybe_unused]] PxPairFlags& pairFlags,
            [[maybe_unused]] const void* constantBlock,
            [[maybe_unused]] PxU32 constantBlockSize
        )
        {
            (void)constantBlock;
            (void)constantBlockSize;

            // Check if either object is a trigger
            bool isTriggerPair = PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1);

            if (isTriggerPair) {
                // For trigger pairs: ONLY trigger events, no physical simulation
                pairFlags = PxPairFlag::eNOTIFY_TOUCH_FOUND |
                    PxPairFlag::eNOTIFY_TOUCH_LOST |
                    PxPairFlag::eDETECT_DISCRETE_CONTACT;
                // DO NOT include eSIMULATION_SHAPE or eCONTACT_DEFAULT
            }
            else {
                // For regular collision pairs: physical simulation + contact events
                pairFlags = PxPairFlag::eCONTACT_DEFAULT |
                    PxPairFlag::eNOTIFY_TOUCH_FOUND |
                    PxPairFlag::eNOTIFY_TOUCH_PERSISTS |
                    PxPairFlag::eNOTIFY_TOUCH_LOST;
            }

            return PxFilterFlag::eDEFAULT;
        }


        // Add these helper functions in the private section of PhysicsContext

private:
    // Helper: Create a cylinder convex mesh
    BOOM_INLINE PxConvexMesh* CreateCylinderMesh(float radius, float halfHeight, int segments = 16)
    {
        std::vector<PxVec3> vertices;
        vertices.reserve(segments * 2 + 2); // top and bottom circles + centers

        // Top circle
        for (int i = 0; i < segments; ++i) {
            float angle = (float)i / segments * PxTwoPi;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);
            vertices.push_back(PxVec3(x, halfHeight, z));
        }

        // Bottom circle
        for (int i = 0; i < segments; ++i) {
            float angle = (float)i / segments * PxTwoPi;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);
            vertices.push_back(PxVec3(x, -halfHeight, z));
        }

        // Top and bottom center points
        vertices.push_back(PxVec3(0, halfHeight, 0));
        vertices.push_back(PxVec3(0, -halfHeight, 0));

        PxConvexMeshDesc meshDesc;
        meshDesc.points.data = vertices.data();
        meshDesc.points.stride = sizeof(PxVec3);
        meshDesc.points.count = static_cast<PxU32>(vertices.size());
        meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxCookingParams params(m_Physics->getTolerancesScale());
        PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
        if (!cooking) {
            BOOM_ERROR("Failed to create PhysX cooking for cylinder");
            return nullptr;
        }

        PxDefaultMemoryOutputStream buf;
        bool status = cooking->cookConvexMesh(meshDesc, buf);
        cooking->release();

        if (!status) {
            BOOM_ERROR("Failed to cook cylinder mesh");
            return nullptr;
        }

        PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
        return m_Physics->createConvexMesh(input);
    }

    // Helper: Create a triangle convex mesh
    BOOM_INLINE PxConvexMesh* CreateTriangleMesh(const glm::vec3& scale)
    {
        // Create an equilateral triangle in XZ plane
        std::vector<PxVec3> vertices;
        vertices.reserve(6); // 3 top vertices + 3 bottom vertices for thickness

        float height = scale.y * 0.5f; // Half-height for thickness
        float sizeX = scale.x;
        float sizeZ = scale.z;

        // Top triangle (Y = +height)
        vertices.push_back(PxVec3(0.0f, height, sizeZ * 0.5f));           // front vertex
        vertices.push_back(PxVec3(-sizeX * 0.5f, height, -sizeZ * 0.5f)); // back-left
        vertices.push_back(PxVec3(sizeX * 0.5f, height, -sizeZ * 0.5f));  // back-right

        // Bottom triangle (Y = -height) - for thickness
        vertices.push_back(PxVec3(0.0f, -height, sizeZ * 0.5f));
        vertices.push_back(PxVec3(-sizeX * 0.5f, -height, -sizeZ * 0.5f));
        vertices.push_back(PxVec3(sizeX * 0.5f, -height, -sizeZ * 0.5f));

        PxConvexMeshDesc meshDesc;
        meshDesc.points.data = vertices.data();
        meshDesc.points.stride = sizeof(PxVec3);
        meshDesc.points.count = static_cast<PxU32>(vertices.size());
        meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxCookingParams params(m_Physics->getTolerancesScale());
        PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
        if (!cooking) {
            BOOM_ERROR("Failed to create PhysX cooking for triangle");
            return nullptr;
        }

        PxDefaultMemoryOutputStream buf;
        bool status = cooking->cookConvexMesh(meshDesc, buf);
        cooking->release();

        if (!status) {
            BOOM_ERROR("Failed to cook triangle mesh");
            return nullptr;
        }

        PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
        return m_Physics->createConvexMesh(input);
    }

    private:
        PxDefaultErrorCallback m_ErrorCallback;
        PxDefaultAllocator m_AllocatorCallback;
        PxDefaultCpuDispatcher* m_Dispatcher;
        PxEventCallback m_EventCallback;
        PxFoundation* m_Foundation;
        PxPhysics* m_Physics;
        PxScene* m_Scene;

        bool m_DebugVisEnabled; // new
    };
}
