#include "camera/camera.h"
#include "rwworld.h"

#include <math.h>

namespace
{

float clampDistance(float value)
{
    if(value < COMBAT_FRAMING_MIN_DISTANCE)
        return COMBAT_FRAMING_MIN_DISTANCE;
    if(value > COMBAT_FRAMING_MAX_DISTANCE)
        return COMBAT_FRAMING_MAX_DISTANCE;
    return value;
}

CameraFramingVector
framingVector(float x, float y, float z)
{
    CameraFramingVector value;
    value.x = x;
    value.y = y;
    value.z = z;
    return value;
}

CameraFramingVector
addFramingVector(const CameraFramingVector& a,
                 const CameraFramingVector& b)
{
    return framingVector(a.x + b.x, a.y + b.y, a.z + b.z);
}

CameraFramingVector
subtractFramingVector(const CameraFramingVector& a,
                      const CameraFramingVector& b)
{
    return framingVector(a.x - b.x, a.y - b.y, a.z - b.z);
}

CameraFramingVector
scaleFramingVector(const CameraFramingVector& value, float scale)
{
    return framingVector(value.x * scale, value.y * scale, value.z * scale);
}

float
dotFramingVector(const CameraFramingVector& a,
                 const CameraFramingVector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

CameraFramingVector
crossFramingVector(const CameraFramingVector& a,
                   const CameraFramingVector& b)
{
    return framingVector(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

float
normalizeFramingVector(CameraFramingVector& value)
{
    const float length = sqrtf(dotFramingVector(value, value));
    if(length > 0.0f)
        value = scaleFramingVector(value, 1.0f / length);
    return length;
}

CameraFramingVector
rotateFramingVector(const CameraFramingVector& value,
                    CameraFramingVector axis,
                    float angleDegrees)
{

    normalizeFramingVector(axis);
    const float radians = angleDegrees * 0.01745329251994329577f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    const CameraFramingVector crossed = crossFramingVector(axis, value);
    return addFramingVector(
        addFramingVector(scaleFramingVector(value, cosine),
                         scaleFramingVector(crossed, sine)),
        scaleFramingVector(
            axis, dotFramingVector(axis, value) * (1.0f - cosine)));
}

void
armCombatCameraSettle(
    CombatCameraState& state,
    float initialFrames, float followupFrames, float eyeScale,
    const CameraFramingVector& cameraForward,
    float currentCameraDistance)
{

    state.settleRemainingSeconds =
        initialFrames / COMBAT_CAMERA_SETTLE_FRAME_RATE;
    state.blendFrames = initialFrames;
    state.initialBlendFrames = initialFrames;
    state.followupBlendFrames = followupFrames;
    if(currentCameraDistance < COMBAT_CAMERA_MIN_ARM_DISTANCE)
        currentCameraDistance = COMBAT_CAMERA_MIN_ARM_DISTANCE;
    state.framing.distance = currentCameraDistance * eyeScale;
    state.framing.eyeX = state.framing.targetX -
        cameraForward.x * state.framing.distance;
    state.framing.eyeY = state.framing.targetY -
        cameraForward.y * state.framing.distance;
    state.framing.eyeZ = state.framing.targetZ -
        cameraForward.z * state.framing.distance;
}

CombatFraming
scaleSpecialCombatFraming(const CombatFraming& framing)
{
    CombatFraming scaled = framing;
    scaled.distance *= COMBAT_CAMERA_MOVE_EYE_SCALE;
    const float horizontalScale = scaled.distance / framing.distance;
    scaled.eyeX = scaled.targetX +
        (framing.eyeX - framing.targetX) * horizontalScale;
    scaled.eyeZ = scaled.targetZ +
        (framing.eyeZ - framing.targetZ) * horizontalScale;
    scaled.eyeY = scaled.targetY +
        (scaled.distance - COMBAT_FRAMING_MIN_DISTANCE) *
            COMBAT_FRAMING_EYE_HEIGHT_RATE +
        COMBAT_FRAMING_EYE_BASE_HEIGHT;
    return scaled;
}

const float MANUAL_CAMERA_AXIS_EPSILON = 1.1920929e-7f;
const float MANUAL_CAMERA_FIRST_RECENTER_PER_FRAME = 4.0f;
const float MANUAL_CAMERA_RECENTER_PER_FRAME = 0.3f;
const float DEGREES_TO_RADIANS = 0.01745329251994329577f;

float moveTowards(float value, float target, float amount)
{
    if(value < target)
    {
        value += amount;
        return value < target ? value : target;
    }
    if(value > target)
    {
        value -= amount;
        return value > target ? value : target;
    }
    return value;
}

float wrapDegrees(float degrees)
{
    if(degrees >= -180.0f && degrees <= 180.0f)
        return degrees;
    degrees = fmodf(degrees + 180.0f, 360.0f);
    if(degrees < 0.0f)
        degrees += 360.0f;
    return degrees - 180.0f;
}

const float CAMERA_COLLISION_EPSILON = 1.0e-6f;
const float CAMERA_COLLISION_LINE_EPSILON = 0.01f;
const float CAMERA_COLLISION_NORMAL_PUSH = 1.001f;
const float CAMERA_COLLISION_SURFACE_OFFSET = 0.1f;
const float CAMERA_COLLISION_ORBIT_RADIANS = 0.69813174f;
const float CAMERA_COLLISION_ORBIT_SCALE = 0.999f;
const float CAMERA_COLLISION_ORBIT_SHIFT =
    GAMEPLAY_CAMERA_COLLISION_RADIUS * 0.9f;
const float CAMERA_COLLISION_SLIDE_DISTANCE = 8.0f;
const float CAMERA_COLLISION_TRACKED_DISTANCE = 15.0f;
const float CAMERA_COLLISION_TRACKED_SURFACE_OFFSET = 1.0f;
const uint8_t CAMERA_COLLISION_CLEAR_FRAMES = 2;

struct CollisionContact
{
    CameraCollisionVector point;
    CameraCollisionVector normal;
    float fraction;
    float penetration;
};

CameraCollisionVector
collisionVector(float x, float y, float z)
{
    CameraCollisionVector value;
    value.x = x;
    value.y = y;
    value.z = z;
    return value;
}

CameraCollisionVector
subtractCollisionVector(const CameraCollisionVector& a,
                        const CameraCollisionVector& b)
{
    return collisionVector(a.x - b.x, a.y - b.y, a.z - b.z);
}

CameraCollisionVector
addCollisionVector(const CameraCollisionVector& a,
                   const CameraCollisionVector& b)
{
    return collisionVector(a.x + b.x, a.y + b.y, a.z + b.z);
}

CameraCollisionVector
scaleCollisionVector(const CameraCollisionVector& value, float scale)
{
    return collisionVector(value.x * scale, value.y * scale,
                           value.z * scale);
}

float
dotCollisionVector(const CameraCollisionVector& a,
                   const CameraCollisionVector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float
lengthSquaredCollisionVector(const CameraCollisionVector& value)
{
    return dotCollisionVector(value, value);
}

float
normalizeCollisionVector(CameraCollisionVector& value)
{
    const float lengthSquared = lengthSquaredCollisionVector(value);
    if(lengthSquared <= CAMERA_COLLISION_EPSILON *
                        CAMERA_COLLISION_EPSILON)
    {
        value = collisionVector(0.0f, 0.0f, 0.0f);
        return 0.0f;
    }
    const float length = sqrtf(lengthSquared);
    value = scaleCollisionVector(value, 1.0f / length);
    return length;
}

CameraCollisionVector
worldVertex(const RwWorldVertex& vertex)
{
    return collisionVector(vertex.x, vertex.y, vertex.z);
}

bool
segmentCollision(const RwWorldGeometry& geometry,
                 const CameraCollisionVector& start,
                 const CameraCollisionVector& end,
                 CollisionContact& contact)
{
    CameraCollisionVector direction = subtractCollisionVector(end, start);
    const float segmentLength = normalizeCollisionVector(direction);
    if(segmentLength <= CAMERA_COLLISION_LINE_EPSILON)
        return false;

    bool found = false;
    float bestDistance = segmentLength;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(triangle.v[0] >= geometry.vertices.size() ||
           triangle.v[1] >= geometry.vertices.size() ||
           triangle.v[2] >= geometry.vertices.size())
        {
            continue;
        }
        const CameraCollisionVector a =
            worldVertex(geometry.vertices[triangle.v[0]]);
        const CameraCollisionVector b =
            worldVertex(geometry.vertices[triangle.v[1]]);
        const CameraCollisionVector c =
            worldVertex(geometry.vertices[triangle.v[2]]);
        const CameraCollisionVector edge1 = subtractCollisionVector(b, a);
        const CameraCollisionVector edge2 = subtractCollisionVector(c, a);
        const CameraCollisionVector p = collisionVector(
            direction.y * edge2.z - direction.z * edge2.y,
            direction.z * edge2.x - direction.x * edge2.z,
            direction.x * edge2.y - direction.y * edge2.x);
        const float determinant = dotCollisionVector(edge1, p);
        if(fabsf(determinant) < CAMERA_COLLISION_EPSILON)
            continue;
        const float inverseDeterminant = 1.0f / determinant;
        const CameraCollisionVector fromA =
            subtractCollisionVector(start, a);
        const float u = dotCollisionVector(fromA, p) * inverseDeterminant;
        if(u < 0.0f || u > 1.0f)
            continue;
        const CameraCollisionVector q = collisionVector(
            fromA.y * edge1.z - fromA.z * edge1.y,
            fromA.z * edge1.x - fromA.x * edge1.z,
            fromA.x * edge1.y - fromA.y * edge1.x);
        const float v = dotCollisionVector(direction, q) *
            inverseDeterminant;
        if(v < 0.0f || u + v > 1.0f)
            continue;
        const float distance = dotCollisionVector(edge2, q) *
            inverseDeterminant;
        if(distance <= CAMERA_COLLISION_LINE_EPSILON ||
           distance >= bestDistance)
        {
            continue;
        }

        CameraCollisionVector normal = collisionVector(
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x);
        if(normalizeCollisionVector(normal) == 0.0f)
            continue;
        if(dotCollisionVector(normal, direction) > 0.0f)
            normal = scaleCollisionVector(normal, -1.0f);
        bestDistance = distance;
        contact.point = addCollisionVector(
            start, scaleCollisionVector(direction, distance));
        contact.normal = normal;
        contact.fraction = distance / segmentLength;
        contact.penetration = 0.0f;
        found = true;
    }
    return found;
}

bool
segmentCollision(const RwWorldGeometry* const* geometry, size_t geometryCount,
                 const CameraCollisionVector& start,
                 const CameraCollisionVector& end,
                 CollisionContact& contact)
{

    for(size_t i = 0; i < geometryCount; ++i)
    {
        if(geometry[i] != NULL &&
           segmentCollision(*geometry[i], start, end, contact))
            return true;
    }
    return false;
}

CameraCollisionVector
trackedCollisionAnchor(const RwWorldGeometry* const* geometry,
                       size_t geometryCount,
                       const CameraCollisionVector& trackedPosition,
                       float trackedHeadingDegrees)
{
    const float radians = trackedHeadingDegrees * DEGREES_TO_RADIANS;
    CameraCollisionVector anchor = collisionVector(
        trackedPosition.x -
            sinf(radians) * CAMERA_COLLISION_TRACKED_DISTANCE,
        trackedPosition.y,
        trackedPosition.z -
            cosf(radians) * CAMERA_COLLISION_TRACKED_DISTANCE);
    CollisionContact contact;
    if(segmentCollision(geometry, geometryCount,
                        trackedPosition, anchor, contact))
    {
        anchor = addCollisionVector(contact.point, scaleCollisionVector(
            contact.normal, CAMERA_COLLISION_TRACKED_SURFACE_OFFSET));
    }
    return anchor;
}

CameraCollisionVector
closestPointOnTriangle(const CameraCollisionVector& point,
                       const CameraCollisionVector& a,
                       const CameraCollisionVector& b,
                       const CameraCollisionVector& c)
{
    const CameraCollisionVector ab = subtractCollisionVector(b, a);
    const CameraCollisionVector ac = subtractCollisionVector(c, a);
    const CameraCollisionVector ap = subtractCollisionVector(point, a);
    const float d1 = dotCollisionVector(ab, ap);
    const float d2 = dotCollisionVector(ac, ap);
    if(d1 <= 0.0f && d2 <= 0.0f)
        return a;

    const CameraCollisionVector bp = subtractCollisionVector(point, b);
    const float d3 = dotCollisionVector(ab, bp);
    const float d4 = dotCollisionVector(ac, bp);
    if(d3 >= 0.0f && d4 <= d3)
        return b;

    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return addCollisionVector(a, scaleCollisionVector(
            ab, d1 / (d1 - d3)));

    const CameraCollisionVector cp = subtractCollisionVector(point, c);
    const float d5 = dotCollisionVector(ab, cp);
    const float d6 = dotCollisionVector(ac, cp);
    if(d6 >= 0.0f && d5 <= d6)
        return c;

    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return addCollisionVector(a, scaleCollisionVector(
            ac, d2 / (d2 - d6)));

    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
    {
        const CameraCollisionVector bc = subtractCollisionVector(c, b);
        return addCollisionVector(b, scaleCollisionVector(
            bc, (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    }

    const float inverse = 1.0f / (va + vb + vc);
    return addCollisionVector(
        a, addCollisionVector(scaleCollisionVector(ab, vb * inverse),
                              scaleCollisionVector(ac, vc * inverse)));
}

bool
sphereCollision(const RwWorldGeometry& geometry,
                const CameraCollisionVector& centre, float radius,
                CollisionContact& contact)
{
    const float radiusSquared = radius * radius;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(triangle.v[0] >= geometry.vertices.size() ||
           triangle.v[1] >= geometry.vertices.size() ||
           triangle.v[2] >= geometry.vertices.size())
        {
            continue;
        }
        const CameraCollisionVector a =
            worldVertex(geometry.vertices[triangle.v[0]]);
        const CameraCollisionVector b =
            worldVertex(geometry.vertices[triangle.v[1]]);
        const CameraCollisionVector c =
            worldVertex(geometry.vertices[triangle.v[2]]);
        const CameraCollisionVector closest =
            closestPointOnTriangle(centre, a, b, c);
        CameraCollisionVector normal =
            subtractCollisionVector(centre, closest);
        const float distanceSquared = lengthSquaredCollisionVector(normal);
        if(distanceSquared >= radiusSquared)
            continue;

        const float distance = normalizeCollisionVector(normal);
        if(distance == 0.0f)
        {
            const CameraCollisionVector ab = subtractCollisionVector(b, a);
            const CameraCollisionVector ac = subtractCollisionVector(c, a);
            normal = collisionVector(
                ab.y * ac.z - ab.z * ac.y,
                ab.z * ac.x - ab.x * ac.z,
                ab.x * ac.y - ab.y * ac.x);
            if(normalizeCollisionVector(normal) == 0.0f)
                continue;
        }

        contact.point = closest;
        contact.normal = normal;
        contact.fraction = distance / radius;
        contact.penetration = radius - distance;
        return true;
    }
    return false;
}

bool
sphereCollision(const RwWorldGeometry* const* geometry, size_t geometryCount,
                const CameraCollisionVector& centre, float radius,
                CollisionContact& contact)
{

    for(size_t i = 0; i < geometryCount; ++i)
    {
        if(geometry[i] != NULL &&
           sphereCollision(*geometry[i], centre, radius, contact))
            return true;
    }
    return false;
}

CameraCollisionVector
orbitCollisionEye(const CameraCollisionVector& eye,
                  const CameraCollisionVector& target, float radians)
{
    const float x = (eye.x - target.x) * CAMERA_COLLISION_ORBIT_SCALE;
    const float z = (eye.z - target.z) * CAMERA_COLLISION_ORBIT_SCALE;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    return collisionVector(target.x + x * cosine - z * sine, eye.y,
                           target.z + x * sine + z * cosine);
}

void
acceptOrbitCollisionEye(const CameraCollisionVector& pushedEye,
                        const CameraCollisionVector& candidate,
                        CameraCollisionVector& eye)
{
    CameraCollisionVector movement =
        subtractCollisionVector(candidate, pushedEye);
    const float distance = normalizeCollisionVector(movement);
    if(distance > CAMERA_COLLISION_ORBIT_SHIFT)
    {
        movement = scaleCollisionVector(
            movement, CAMERA_COLLISION_ORBIT_SHIFT);
        eye = addCollisionVector(pushedEye, movement);
    }
    else
        eye = candidate;
}

bool
updateObstructedCollision(
    GameplayCameraCollisionState& state,
    const RwWorldGeometry* const* geometry, size_t geometryCount,
    const CameraCollisionVector& trackedAnchor,
    const CameraCollisionVector& target,
    CameraCollisionVector& eye)
{
    const CameraCollisionVector originalEye = eye;
    CameraCollisionVector actorPlaneEye =
        collisionVector(trackedAnchor.x, eye.y, trackedAnchor.z);
    CollisionContact contact;
    if(segmentCollision(geometry, geometryCount,
                        originalEye, actorPlaneEye, contact))
    {
        eye = addCollisionVector(contact.point, scaleCollisionVector(
            contact.normal, CAMERA_COLLISION_SURFACE_OFFSET));
        const CameraCollisionVector actorFromHit =
            subtractCollisionVector(trackedAnchor, contact.point);
        const float normalDistance =
            dotCollisionVector(actorFromHit, contact.normal);
        const CameraCollisionVector pointOnPlane = addCollisionVector(
            contact.point,
            scaleCollisionVector(contact.normal, normalDistance));
        CameraCollisionVector slide =
            subtractCollisionVector(trackedAnchor, pointOnPlane);
        if(normalizeCollisionVector(slide) > 0.0f)
        {
            eye = addCollisionVector(eye, scaleCollisionVector(
                slide, CAMERA_COLLISION_SLIDE_DISTANCE));
        }
    }

    if(segmentCollision(geometry, geometryCount, target, eye, contact))
    {
        state.clearFramesRemaining = CAMERA_COLLISION_CLEAR_FRAMES;
        return true;
    }
    if(state.clearFramesRemaining != 0)
    {
        --state.clearFramesRemaining;
        return true;
    }
    state.obstructionMode = 0;
    state.lastClearEye = eye;
    return false;
}

}

void
resetGameplayCameraCollision(GameplayCameraCollisionState& state)
{
    state.lastClearEye = collisionVector(0.0f, 0.0f, 0.0f);
    state.trackedAnchor = collisionVector(0.0f, 0.0f, 0.0f);
    state.obstructionMode = 0;
    state.clearFramesRemaining = 0;
    state.initialized = false;
}

bool
resolveGameplayCameraCollision(
    GameplayCameraCollisionState& state,
    const RwWorldGeometry* const* geometry, size_t geometryCount,
    const CameraCollisionVector& trackedPosition,
    float trackedHeadingDegrees,
    const CameraCollisionVector& target,
    CameraCollisionVector& eye)
{
    if(geometry == NULL || geometryCount == 0)
    {
        resetGameplayCameraCollision(state);
        return false;
    }

    CollisionContact contact;
    if(!state.initialized)
    {
        state.initialized = true;
        state.obstructionMode = 0;
        state.clearFramesRemaining = 0;
        state.lastClearEye = eye;

        if(segmentCollision(geometry, geometryCount, target, eye, contact))
        {
            eye = contact.point;
            state.lastClearEye = eye;
            return true;
        }
        return false;
    }

    if(state.obstructionMode != 0)
    {
        return updateObstructedCollision(
            state, geometry, geometryCount,
            state.trackedAnchor, target, eye);
    }

    bool sphereHit = sphereCollision(
        geometry, geometryCount, eye,
        GAMEPLAY_CAMERA_COLLISION_RADIUS, contact);
    if(sphereHit)
    {

        eye = addCollisionVector(eye, scaleCollisionVector(
            contact.normal,
            contact.penetration * CAMERA_COLLISION_NORMAL_PUSH));
        const CameraCollisionVector pushedEye = eye;

        CollisionContact orbitContact;
        CameraCollisionVector candidate = orbitCollisionEye(
            pushedEye, target, CAMERA_COLLISION_ORBIT_RADIANS);
        if(!sphereCollision(geometry, geometryCount, candidate,
                            GAMEPLAY_CAMERA_COLLISION_RADIUS, orbitContact))
        {
            acceptOrbitCollisionEye(pushedEye, candidate, eye);
        }
        else
        {
            candidate = orbitCollisionEye(
                pushedEye, target, -CAMERA_COLLISION_ORBIT_RADIANS);
            if(!sphereCollision(geometry, geometryCount, candidate,
                                GAMEPLAY_CAMERA_COLLISION_RADIUS,
                                orbitContact))
            {
                acceptOrbitCollisionEye(pushedEye, candidate, eye);
            }
            else
                eye = state.lastClearEye;
        }
    }

    state.trackedAnchor = trackedCollisionAnchor(
        geometry, geometryCount, trackedPosition, trackedHeadingDegrees);

    if(segmentCollision(geometry, geometryCount, target, eye, contact))
    {
        state.obstructionMode = 1;
        state.clearFramesRemaining = 0;
        return true;
    }

    if(!sphereHit)
        state.lastClearEye = eye;
    return sphereHit;
}

CombatFraming
computeCombatFraming(float ax, float az, float bx, float bz,
                     float fallbackDirectionX, float fallbackDirectionZ,
                     float fieldOfViewDegrees)
{
    const float deltaX = ax - bx;
    const float deltaZ = az - bz;
    float length = deltaX * deltaX + deltaZ * deltaZ;

    float unitX = fallbackDirectionX;
    float unitZ = fallbackDirectionZ;
    if(length > 0.0f)
    {
        length = sqrtf(length);
        unitX = deltaX / length;
        unitZ = deltaZ / length;
    }
    else
        length = 0.0f;

    const float halfSeparation = length * 0.5f;
    const float halfFieldOfView = fieldOfViewDegrees * 0.5f;
    const float spread = halfFieldOfView > 0.0f ?
        halfSeparation * ((90.0f - halfFieldOfView) / halfFieldOfView) : 0.0f;

    CombatFraming framing;
    framing.distance = clampDistance(spread * 0.8f);
    framing.targetX = halfSeparation * unitX + bx;
    framing.targetY = COMBAT_FRAMING_TARGET_HEIGHT;
    framing.targetZ = halfSeparation * unitZ + bz;

    const float perpendicularX = unitZ;
    const float perpendicularZ = -unitX;
    framing.eyeX = framing.targetX + perpendicularX * framing.distance;
    framing.eyeZ = framing.targetZ + perpendicularZ * framing.distance;
    framing.eyeY = framing.targetY +
        (framing.distance - COMBAT_FRAMING_MIN_DISTANCE) *
            COMBAT_FRAMING_EYE_HEIGHT_RATE +
        COMBAT_FRAMING_EYE_BASE_HEIGHT;
    return framing;
}

bool
updateBattleCameraSubject(
    BattleCameraSubject& tracked,
    float actualX, float actualY, float actualZ)
{
    float deltaX = actualX - tracked.x;
    float deltaY = actualY - tracked.y;
    float deltaZ = actualZ - tracked.z;
    const float distanceSquared =
        deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
    if(distanceSquared <= 2.5f * 2.5f)
        return false;

    const float distance = sqrtf(distanceSquared);
    deltaX /= distance;
    deltaY /= distance;
    deltaZ /= distance;
    if(distance > BATTLE_CAMERA_SUBJECT_CLEARANCE)
    {
        tracked.x = actualX -
            deltaX * BATTLE_CAMERA_SUBJECT_CLEARANCE;
        tracked.y = actualY -
            deltaY * BATTLE_CAMERA_SUBJECT_CLEARANCE;
        tracked.z = actualZ -
            deltaZ * BATTLE_CAMERA_SUBJECT_CLEARANCE;
        return true;
    }

    const float outerFraction =
        (distance - 2.5f) / 2.5f;
    const float step =
        BATTLE_CAMERA_SUBJECT_CLEARANCE * outerFraction * 0.05f;
    tracked.x += deltaX * step;
    tracked.y += deltaY * step;
    tracked.z += deltaZ * step;
    return true;
}

CombatFraming
computeBattleCameraFraming(
    float ax, float ay, float az, float bx, float by, float bz,
    float previousEyeX, float previousEyeZ, float maxDistance)
{
    const float deltaX = ax - bx;
    const float deltaY = ay - by;
    const float deltaZ = az - bz;
    const float separation = sqrtf(
        deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    const float horizontal = sqrtf(deltaX * deltaX + deltaZ * deltaZ);

    float unitX = 0.0f;
    float unitZ = 1.0f;
    if(horizontal > 0.0001f)
    {
        unitX = deltaX / horizontal;
        unitZ = deltaZ / horizontal;
    }

    CombatFraming framing;
    framing.targetX = (ax + bx) * 0.5f;
    framing.targetY = (ay + by) * 0.5f;
    framing.targetZ = (az + bz) * 0.5f;

    const float angleRadians =
        BATTLE_CAMERA_FRAMING_ANGLE_DEGREES *
        0.01745329251994329577f;
    const float halfSpan =
        (separation + BATTLE_CAMERA_SUBJECT_CLEARANCE * 2.0f) * 0.5f;
    float distance = halfSpan / sinf(angleRadians);
    if(distance < BATTLE_CAMERA_MIN_DISTANCE)
        distance = BATTLE_CAMERA_MIN_DISTANCE;
    if(maxDistance < BATTLE_CAMERA_MIN_DISTANCE)
        maxDistance = BATTLE_CAMERA_MAX_DISTANCE;
    if(distance > maxDistance)
        distance = maxDistance;
    framing.distance = distance;

    float perpendicularX = unitZ;
    float perpendicularZ = -unitX;
    const float previousX = previousEyeX - framing.targetX;
    const float previousZ = previousEyeZ - framing.targetZ;
    if(previousX * perpendicularX + previousZ * perpendicularZ < 0.0f)
    {
        perpendicularX = -perpendicularX;
        perpendicularZ = -perpendicularZ;
    }

    framing.eyeX = framing.targetX + perpendicularX * distance;
    framing.eyeZ = framing.targetZ + perpendicularZ * distance;
    const float heightRatio = distance / maxDistance;
    framing.eyeY = framing.targetY +
        BATTLE_CAMERA_EYE_HEIGHT * heightRatio;
    framing.targetY +=
        BATTLE_CAMERA_EYE_HEIGHT * (1.0f - heightRatio);
    return framing;
}

CamdatFramingBasis
computeCamdatPairFramingBasis(
    const CameraFramingVector& first,
    const CameraFramingVector& second,
    const CameraFramingVector& offset)
{

    CamdatFramingBasis basis;
    basis.forward = subtractFramingVector(second, first);
    const float separation = normalizeFramingVector(basis.forward);
    basis.right = framingVector(
        basis.forward.z, 0.0f, -basis.forward.x);
    normalizeFramingVector(basis.right);
    basis.up = crossFramingVector(basis.forward, basis.right);
    normalizeFramingVector(basis.up);
    basis.target = addFramingVector(
        addFramingVector(
            addFramingVector(
                addFramingVector(
                    first,
                    scaleFramingVector(basis.forward, separation * 0.5f)),
                scaleFramingVector(basis.right, offset.x)),
            scaleFramingVector(basis.up, offset.y)),
        scaleFramingVector(basis.forward, offset.z));
    return basis;
}

CamdatFraming
computeCamdatOpcode1And4Framing(
    const CamdatFramingBasis& basis,
    const CamdatFramingParameters& parameters)
{

    const CameraFramingVector yawedRight = rotateFramingVector(
        basis.right, basis.up, parameters.yawDegrees);
    const CameraFramingVector yawedForward = rotateFramingVector(
        basis.forward, basis.up, parameters.yawDegrees);
    const CameraFramingVector direction = rotateFramingVector(
        yawedForward, yawedRight, parameters.pitchDegrees);
    CamdatFraming framing;
    framing.target = basis.target;
    framing.eye = addFramingVector(
        basis.target, scaleFramingVector(direction, parameters.distance));
    return framing;
}

CamdatFraming
computeCamdatOpcode2Framing(
    const CamdatFramingBasis& boneBasis,
    const CamdatFramingParameters& start,
    const CamdatFramingParameters& end,
    float elapsedFraction)
{

    CamdatFramingParameters parameters;
    parameters.distance =
        (end.distance - start.distance) * elapsedFraction + start.distance;
    parameters.pitchDegrees =
        (end.pitchDegrees - start.pitchDegrees) * elapsedFraction +
        start.pitchDegrees;
    parameters.yawDegrees =
        (end.yawDegrees - start.yawDegrees) * elapsedFraction +
        start.yawDegrees;
    CameraFramingVector direction = rotateFramingVector(
        boneBasis.forward, boneBasis.right, parameters.pitchDegrees);
    direction = rotateFramingVector(
        direction, boneBasis.up, parameters.yawDegrees);
    CamdatFraming framing;
    framing.target = boneBasis.target;
    framing.eye = addFramingVector(
        boneBasis.target,
        scaleFramingVector(direction, parameters.distance));
    return framing;
}

void
resetCombatCameraState(CombatCameraState& state,
                       const CombatFraming& currentFraming)
{

    state.framing = currentFraming;
    state.settleRemainingSeconds = 0.0f;
    state.blendFrames = 0.0f;
    state.initialBlendFrames = 0.0f;
    state.followupBlendFrames = 0.0f;
    state.specialRemainingSeconds = 0.0f;
    state.specialDistanceScale = false;
}

float
combatCameraBlendFraction(
    const CombatCameraState& state,
    float scaledFrameDeltaSeconds,
    float nominalFrameRateHz)
{

    const float blendFrames = state.blendFrames == 0.0f ?
        COMBAT_CAMERA_DEFAULT_BLEND_FRAMES : state.blendFrames;
    return scaledFrameDeltaSeconds /
        (blendFrames / nominalFrameRateHz);
}

float
computeCombatCameraField0x439c(float slotX, float slotZ,
                              float linkedModelX, float linkedModelZ)
{

    const float deltaX = slotX - linkedModelX;
    const float deltaZ = slotZ - linkedModelZ;
    return sqrtf(deltaX * deltaX + deltaZ * deltaZ);
}

bool
armCombatDamageCameraSettle(
    CombatCameraState& state, float damage,
    const CameraFramingVector& cameraForward,
    float currentCameraDistance)
{

    if(damage <= COMBAT_CAMERA_DAMAGE_THRESHOLD)
        return false;
    armCombatCameraSettle(
        state,
        COMBAT_CAMERA_DAMAGE_SETTLE_FRAMES,
        COMBAT_CAMERA_DAMAGE_FOLLOWUP_FRAMES,
        COMBAT_CAMERA_DAMAGE_EYE_SCALE,
        cameraForward,
        currentCameraDistance);
    return true;
}

void
armCombatMoveCameraSettle(
    CombatCameraState& state, float closeUpDurationFrames,
    const CameraFramingVector& cameraForward,
    float currentCameraDistance)
{

    state.specialDistanceScale = true;
    armCombatCameraSettle(
        state,
        COMBAT_CAMERA_MOVE_SETTLE_FRAMES,
        COMBAT_CAMERA_MOVE_FOLLOWUP_FRAMES,
        COMBAT_CAMERA_MOVE_EYE_SCALE,
        cameraForward,
        currentCameraDistance);
    state.specialRemainingSeconds =
        closeUpDurationFrames / COMBAT_CAMERA_SETTLE_FRAME_RATE;
}

bool
updateCombatCameraState(
    CombatCameraState& state,
    float ax, float az, float bx, float bz,
    float fallbackDirectionX, float fallbackDirectionZ,
    float fieldOfViewDegrees,
    float field0x439c,
    float scaledFrameDeltaSeconds)
{
    CombatFraming next = computeCombatFraming(
        ax, az, bx, bz,
        fallbackDirectionX, fallbackDirectionZ,
        fieldOfViewDegrees);

    state.framing.targetX = next.targetX;
    state.framing.targetY = next.targetY;
    state.framing.targetZ = next.targetZ;

    if(state.settleRemainingSeconds > 0.0f)
    {
        state.settleRemainingSeconds -= scaledFrameDeltaSeconds;
        if(state.settleRemainingSeconds > 0.0f)
            return false;

        if(state.blendFrames == state.initialBlendFrames)
        {
            if(!state.specialDistanceScale)
            {
                state.blendFrames = state.followupBlendFrames;
                state.settleRemainingSeconds =
                    state.followupBlendFrames /
                    COMBAT_CAMERA_SETTLE_FRAME_RATE;
            }
            else
                state.blendFrames = 5.0f;
        }
        else if(state.blendFrames == state.followupBlendFrames)
        {
            state.blendFrames = 0.0f;
            state.settleRemainingSeconds = 0.0f;
        }
    }

    if(field0x439c < COMBAT_CAMERA_FIELD_0X439C_MINIMUM)
        return false;

    if(state.specialDistanceScale)
        next = scaleSpecialCombatFraming(next);

    if(state.specialDistanceScale)
    {

        state.specialRemainingSeconds -= scaledFrameDeltaSeconds;
        if(state.specialRemainingSeconds < 0.0f)
        {
            state.specialRemainingSeconds = 0.0f;
            state.specialDistanceScale = false;
            state.blendFrames = state.followupBlendFrames;
            state.settleRemainingSeconds =
                state.followupBlendFrames /
                COMBAT_CAMERA_SETTLE_FRAME_RATE;
        }
    }

    state.framing = next;
    return true;
}

namespace
{

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590020[7] =
{
    { 0x0080, 0, -220.0f, 80.0f, -125.0f, -295.0f, -229.0f, -160.0f, -133.0f, 1000.0f },
    { 0x0080, 0, -335.0f, 80.0f, -125.0f, -295.0f, -229.0f, -160.0f, -133.0f, 1000.0f },
    { 0x0080, 0, 311.0f, 27.0f, -29.0f, 235.0f, 270.0f, -45.0f, -30.0f, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590120[7] =
{
    { 0x0080, 0, -149.0f, 20.23f, 26.95f, -155.0f, -110.0f, 26.0f, 58.0f, 30.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590220[7] =
{
    { 1, 0, -140.0f, 20.0f, 40.0f, -170.0f, -110.0f, -50.0f, 30.0f, 1000.0f },
    { 0x0081, 0, -160.0f, 20.0f, 70.0f, -300.0f, -185.0f, -50.0f, 93.0f, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590320[7] =
{
    { 1, 0, -78.0f, 25.0f, -15.0f, -100.0f, -30.0f, -50.0f, 50.0f, 1000.0f },
    { 1, 0, -15.0f, 25.0f, -28.0f, -40.0f, 100.0f, -50.0f, 50.0f, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590420[7] =
{
    { 1, 0, -20.0f, 32.0f, 40.0f, -25.0f, 100.0f, -55.0f, 200.0f, 1000.0f },
    { 1, 0, -25.0f, 32.0f, -55.0f, -60.0f, 50.0f, -200.0f, -50.0f, 1000.0f },
    { 1, 0, -25.0f, 32.0f, -55.0f, -200.0f, -45.0f, -100.0f, -50.0f, 1000.0f },
    { 1, 0, -60.0f, 32.0f, -55.0f, -200.0f, -45.0f, -200.0f, -100.0f, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590520[7] =
{
    { 1, 0, -20.0f, 32.0f, 40.0f, -25.0f, 100.0f, -55.0f, 200.0f, 1000.0f },
    { 1, 0, -25.0f, 32.0f, -55.0f, -60.0f, 50.0f, -200.0f, -50.0f, 1000.0f },
    { 1, 0, -25.0f, 32.0f, -55.0f, -200.0f, -45.0f, -100.0f, -50.0f, 1000.0f },
    { 1, 0, -59.0f, 21.0f, -100.0f, -200.0f, -45.0f, -200.0f, -100.0f, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590620[7] =
{
    { 1, 0, 60.0f, 30.0f, -20.0f, 20.0f, 200.0f, -150.0f, 150.0f, 1000.0f },
    { 1, 0, 10.0f, 25.0f, 70.0f, -20.0f, 20.0f, 25.0f, 150.0f, 1000.0f },
    { 1, 0, -18.0f, 33.0f, 55.0f, -20.0f, 20.0f, -150.0f, 30.0f, 1000.0f },
    { 1, 0, 10.0f, 25.0f, 70.0f, -200.0f, 20.0f, 40.0f, 150.0f, 1000.0f },
    { 1, 0, 10.0f, 25.0f, -82.0f, -20.0f, 20.0f, -150.0f, -10.0f, 1000.0f },
    { 1, 0, -20.0f, 33.0f, -33.0f, -20.0f, 20.0f, -15.0f, 150.0f, 1000.0f },
    { 1, 0, 10.0f, 25.0f, -82.0f, -200.0f, 20.0f, -150.0f, -50.0f, 1000.0f }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590720[7] =
{
    { 3, 1, 4.7f, 22.9f, 34.2f, 0, 0, 0, 0, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590820[7] =
{
    { 5, 0, -105.82f, 20.62f, 80.96f, 0, 0, 0, 0, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590920[7] =
{
    { 5, 0, 284.06f, 23.34f, 87.97f, 0, 0, 0, 0, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590a20[7] =
{
    { 5, 0, 0.19f, 27.05f, -42.26f, 0, 0, 0, 0, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord FIELD_CAMERA_TABLE_590b20[7] =
{
    { 6, 0, -284.21f, 18.11f, 222.99f, 0, 0, 0, 0, 1000.0f },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const FieldCameraRegionRecord*
fieldCameraTableForDispatch(uint16_t stageId, float playerZ, int storyPhase)
{
    switch(stageId)
    {
    case 0x35:
    case 0x29: return FIELD_CAMERA_TABLE_590620;
    case 0x33: return storyPhase > 3 ?
        FIELD_CAMERA_TABLE_590520 : FIELD_CAMERA_TABLE_590420;
    case 0x32: return FIELD_CAMERA_TABLE_590320;
    case 0x30: return FIELD_CAMERA_TABLE_590220;
    case 0x2f: return playerZ > 100.0f ?
        FIELD_CAMERA_TABLE_590b20 : FIELD_CAMERA_TABLE_590a20;
    case 0x26: return FIELD_CAMERA_TABLE_590120;
    case 0x1d: return FIELD_CAMERA_TABLE_590920;
    case 0x19: return FIELD_CAMERA_TABLE_590720;
    case 0x13: return FIELD_CAMERA_TABLE_590020;
    case 0x0d: return FIELD_CAMERA_TABLE_590820;
    default: return NULL;
    }
}

const FieldCameraRegionRecord*
fieldCameraTableForEntry(uint16_t stageId, float playerZ, int storyPhase,
                         bool eventFlag031e)
{
    if(stageId == 0x33 && (storyPhase > 3 || eventFlag031e))
        return FIELD_CAMERA_TABLE_590520;
    return fieldCameraTableForDispatch(stageId, playerZ, storyPhase);
}

bool
fieldCameraRegionCondition(const FieldCameraRegionRecord& record,
                           const FieldCameraRegionInputs& inputs,
                           bool replacement)
{
    switch(record.behavior & 0x7f)
    {
    case 0:
    case 1:
        if(!replacement && !(record.minX < inputs.player.x &&
                             inputs.player.x < record.maxX &&
                             record.minZ < inputs.player.z &&
                             inputs.player.z < record.maxZ &&
                             inputs.player.y < record.maxY))
            return false;
        return replacement || (record.minX < inputs.player.x &&
                               inputs.player.x < record.maxX &&
                               record.minZ < inputs.player.z &&
                               inputs.player.z < record.maxZ &&
                               inputs.player.y < record.maxY);
    case 2:
        return (inputs.sceneControlFlags & 0x00010000u) != 0;
    case 3:
        return (inputs.sceneControlFlags &
                (replacement ? 0x00010000u : 0x00020000u)) != 0;
    case 4:
        return (inputs.sceneControlFlags & 0x00020000u) != 0;
    case 5:
        return inputs.groundSurface == 4;
    case 6:
        return inputs.groundSurface == 5;
    default:
        return false;
    }
}

int
fieldCameraReplacementIndex(const FieldCameraRegionRecord* table,
                            const FieldCameraRegionInputs& inputs)
{
    bool found = false;
    float nearest = 0.0f;
    int selected = -1;
    for(int i = 0; i < 7; ++i)
    {
        const FieldCameraRegionRecord& record = table[i];
        const uint16_t condition = record.behavior & 0x7f;
        if(condition == 0 || condition == 1)
        {
            if(!(record.minX < inputs.player.x &&
                 inputs.player.x < record.maxX &&
                 record.minZ < inputs.player.z &&
                 inputs.player.z < record.maxZ &&
                 inputs.player.y < record.maxY))
                continue;
            const CameraFramingVector& center =
                condition == 0 ? inputs.currentEye : inputs.player;
            const float dx = center.x - record.eyeX;
            const float dy = center.y - record.eyeY;
            const float dz = center.z - record.eyeZ;
            const float distance = dx * dx + dy * dy + dz * dz;
            if(!found || distance < nearest)
            {
                found = true;
                nearest = distance;
                selected = i;
            }
            continue;
        }
        if(fieldCameraRegionCondition(record, inputs, true))
        {
            found = true;
            selected = i;
        }
    }
    return selected;
}

}

const FieldCameraRegionRecord*
fieldCameraRegionTable(uint16_t stageId, float playerZ, int storyPhase,
                       bool eventFlag031e, uint16_t* recordCount)
{
    if(recordCount != NULL)
        *recordCount = 7;
    return fieldCameraTableForEntry(
        stageId, playerZ, storyPhase, eventFlag031e);
}

bool
resolveFieldCameraRegion(const FieldCameraRegionInputs& inputs,
                         FieldCameraRegionState& state)
{
    const FieldCameraRegionRecord* table = fieldCameraTableForDispatch(
        inputs.stageId, inputs.player.z, inputs.storyPhase);
    const FieldCameraRegionRecord* entryTable = fieldCameraTableForEntry(
        inputs.stageId, inputs.player.z, inputs.storyPhase,
        inputs.eventFlag031e);
    state.stageId = inputs.stageId;
    state.table = table;
    state.tableRecordCount = table == NULL ? 0 : 7;
    if(table == NULL || entryTable == NULL)
    {
        state.active = false;
        state.recordIndex = -1;
        state.smoothingRow = 0;
        return false;
    }

    int selected = -1;
    if(state.active && state.table == table && state.recordIndex >= 0 &&
       state.recordIndex < 7 &&
       fieldCameraRegionCondition(
           table[state.recordIndex], inputs, false))
    {
        selected = state.recordIndex;
    }
    else
    {
        FieldCameraRegionInputs entryInputs = inputs;
        selected = fieldCameraReplacementIndex(entryTable, entryInputs);

        if(selected >= 0 && entryTable != table &&
           selected >= 7)
            selected = -1;
    }
    if(selected < 0)
    {
        state.active = false;
        state.recordIndex = -1;
        state.smoothingRow = 0;
        return false;
    }

    const FieldCameraRegionRecord& record = table[selected];
    state.active = true;
    state.recordIndex = static_cast<int16_t>(selected);
    state.preserveEyeY = (record.behavior & 0x80) != 0;
    state.smoothingRow = record.smoothingRow;
    if(record.smoothingRow == 1)
    {
        state.targetDeadband = 1.0f;
        state.eyeDeadband = 0.001f;
        state.smoothingAlpha = 1.0f;
    }
    else if(record.smoothingRow == 2)
    {
        state.targetDeadband = 1.0f;
        state.eyeDeadband = 0.5f;
        state.smoothingAlpha = 0.074f;
    }
    else
    {
        state.targetDeadband = 1.0f;
        state.eyeDeadband = 1.0f;
        state.smoothingAlpha = 0.074f;
    }
    return true;
}

PlayerFollowFraming
computePlayerFollowFraming(float x, float y, float z,
                           float headingDegrees, float distance,
                           float targetX, float targetY, float targetZ)
{
    const float polar =
        MANUAL_CAMERA_DEFAULT_POLAR_DEGREES * DEGREES_TO_RADIANS;
    const float azimuth =
        (90.0f - headingDegrees) * DEGREES_TO_RADIANS;
    const float polarCosine = cosf(polar);
    PlayerFollowFraming framing;
    framing.centerX = x;
    framing.centerY = y;
    framing.centerZ = z;
    framing.targetX = targetX;
    framing.targetY = targetY - 1.0f;
    framing.targetZ = targetZ;
    framing.eyeX =
        x + distance * polarCosine * cosf(azimuth);
    framing.eyeY = y + 2.0f + distance * sinf(polar);
    framing.eyeZ =
        z + distance * polarCosine * sinf(azimuth);
    return framing;
}

void
resetManualCameraControl(ManualCameraControl& control)
{
    control.yawOffsetDegrees = 0.0f;
    control.polarDegrees = MANUAL_CAMERA_DEFAULT_POLAR_DEGREES;
    control.verticalInputIdle = false;
    control.enabled = false;
}

void
updateManualCameraControl(ManualCameraControl& control, bool enabled,
                          float horizontalAxis, float verticalAxis,
                          float frameStep)
{
    control.enabled = enabled;
    if(frameStep <= 0.0f)
        return;

    if(!enabled)
    {
        horizontalAxis = 0.0f;
        verticalAxis = 0.0f;
    }

    control.yawOffsetDegrees = wrapDegrees(
        control.yawOffsetDegrees +
        horizontalAxis * MANUAL_CAMERA_YAW_PER_FRAME * frameStep);

    if(fabsf(verticalAxis) > MANUAL_CAMERA_AXIS_EPSILON)
    {
        control.polarDegrees -=
            verticalAxis * MANUAL_CAMERA_POLAR_PER_FRAME * frameStep;
        control.verticalInputIdle = false;
    }
    else
    {
        const float rate = control.verticalInputIdle ?
            MANUAL_CAMERA_RECENTER_PER_FRAME :
            MANUAL_CAMERA_FIRST_RECENTER_PER_FRAME;
        control.polarDegrees = moveTowards(
            control.polarDegrees, MANUAL_CAMERA_DEFAULT_POLAR_DEGREES,
            rate * frameStep);
        control.verticalInputIdle = true;
    }

    if(control.polarDegrees < MANUAL_CAMERA_MIN_POLAR_DEGREES)
        control.polarDegrees = MANUAL_CAMERA_MIN_POLAR_DEGREES;
    else if(control.polarDegrees > MANUAL_CAMERA_MAX_POLAR_DEGREES)
        control.polarDegrees = MANUAL_CAMERA_MAX_POLAR_DEGREES;
}

PlayerFollowFraming
applyManualCameraControl(const ManualCameraControl& control,
                         const PlayerFollowFraming& automaticFraming)
{
    if(fabsf(control.yawOffsetDegrees) <= MANUAL_CAMERA_AXIS_EPSILON &&
       fabsf(control.polarDegrees - MANUAL_CAMERA_DEFAULT_POLAR_DEGREES) <=
           MANUAL_CAMERA_AXIS_EPSILON)
        return automaticFraming;

    const float offsetX =
        automaticFraming.eyeX - automaticFraming.centerX;
    const float offsetY =
        automaticFraming.eyeY - automaticFraming.centerY - 2.0f;
    const float offsetZ =
        automaticFraming.eyeZ - automaticFraming.centerZ;
    const float horizontalLength =
        sqrtf(offsetX * offsetX + offsetZ * offsetZ);
    if(horizontalLength <= MANUAL_CAMERA_AXIS_EPSILON)
        return automaticFraming;

    const float radius =
        sqrtf(horizontalLength * horizontalLength + offsetY * offsetY);
    const float baseElevation = atan2f(offsetY, horizontalLength);
    const float elevation = baseElevation +
        (MANUAL_CAMERA_DEFAULT_POLAR_DEGREES - control.polarDegrees) *
            DEGREES_TO_RADIANS;
    const float horizontal = cosf(elevation) * radius;
    const float yaw = control.yawOffsetDegrees * DEGREES_TO_RADIANS;
    const float cosine = cosf(yaw);
    const float sine = sinf(yaw);
    const float unitX = offsetX / horizontalLength;
    const float unitZ = offsetZ / horizontalLength;

    const float rotatedX = unitX * cosine - unitZ * sine;
    const float rotatedZ = unitX * sine + unitZ * cosine;

    PlayerFollowFraming framing = automaticFraming;
    framing.eyeX = framing.centerX + rotatedX * horizontal;
    framing.eyeY = framing.centerY + 2.0f + sinf(elevation) * radius;
    framing.eyeZ = framing.centerZ + rotatedZ * horizontal;
    return framing;
}

const CameraPathSegment TITLE_CAMERA_PATH =
{
    { 18.38f, 5.0f, 51.0f },
    { -665.25f, 15.91f, 77.04f },
    695.0f,
    1,
    { 74.70f, 10.40f, 62.20f },
    { 75.70f, 10.40f, 58.0f },
    695.0f,
    1
};

void
cameraPathStart(CameraPathPlayer& player, const CameraPathSegment& segment)
{
    player.segment = segment;
    player.eyeT = 0.0f;
    player.targetT = 0.0f;
    player.finished = false;
}

namespace
{

float pathLerp(float start, float end, float t)
{
    return start + (end - start) * t;
}

bool advancePathChannel(float& t, float duration, uint16_t stop, float frameStep)
{
    if(duration <= 0.0f)
    {
        t = 1.0f;
        return stop == 0;
    }
    t += frameStep / duration;
    if(t <= 1.0f)
        return true;
    if(stop != 0)
    {
        t = 1.0f;
        return false;
    }
    t -= 1.0f;
    if(t > 1.0f)
        t = fmodf(t, 1.0f);
    return true;
}

}

bool
cameraPathSample(CameraPathPlayer& player, float frameStep,
                 float* eyeX, float* eyeY, float* eyeZ,
                 float* targetX, float* targetY, float* targetZ)
{
    const CameraPathSegment& segment = player.segment;
    const bool eyeRunning = advancePathChannel(
        player.eyeT, segment.eyeDuration, segment.eyeStop, frameStep);
    const bool targetRunning = advancePathChannel(
        player.targetT, segment.targetDuration, segment.targetStop, frameStep);
    player.finished = !eyeRunning && !targetRunning;

    if(eyeX != 0)
        *eyeX = pathLerp(segment.eyeStart[0], segment.eyeEnd[0], player.eyeT);
    if(eyeY != 0)
        *eyeY = pathLerp(segment.eyeStart[1], segment.eyeEnd[1], player.eyeT);
    if(eyeZ != 0)
        *eyeZ = pathLerp(segment.eyeStart[2], segment.eyeEnd[2], player.eyeT);
    if(targetX != 0)
        *targetX = pathLerp(segment.targetStart[0], segment.targetEnd[0],
                            player.targetT);
    if(targetY != 0)
        *targetY = pathLerp(segment.targetStart[1], segment.targetEnd[1],
                            player.targetT);
    if(targetZ != 0)
        *targetZ = pathLerp(segment.targetStart[2], segment.targetEnd[2],
                            player.targetT);
    return !player.finished;
}
