#include "game_combat_projectiles.h"
#include <math.h>
#include <string.h>

using namespace game_detail;

void initializeCombatProjectile(CombatProjectile& projectile)
{

    memset(&projectile, 0, sizeof(projectile));
    projectile.ownerSlot = -1;
    for(int& ignored : projectile.ignoredSlots)
        ignored = -1;
    projectile.hit.ownerSlot = -1;
    projectile.hitsRemaining = -1;
    projectile.group = 2;
}

CombatProjectilePool::CombatProjectilePool()
{
    reset();
}

void CombatProjectilePool::reset()
{
    memset(entries, 0, sizeof(entries));
    for(CombatProjectileRuntime& entry : entries)
        initializeCombatProjectile(entry.projectile);
}

int CombatProjectilePool::spawn(const CombatProjectile& projectile)
{
    for(int index = 0; index < COMBAT_PROJECTILE_CAPACITY; ++index)
    {
        CombatProjectileRuntime& entry = entries[index];
        if(entry.active)
            continue;
        entry = {};
        entry.projectile = projectile;
        entry.active = true;

        if(projectile.positionMode == 1)
            entry.current = projectile.position;
        return index;
    }
    return -1;
}

CombatProjectileRuntime* CombatProjectilePool::entry(int index)
{
    return index >= 0 && index < COMBAT_PROJECTILE_CAPACITY ? &entries[index] : nullptr;
}

const CombatProjectileRuntime* CombatProjectilePool::entry(int index) const
{
    return index >= 0 && index < COMBAT_PROJECTILE_CAPACITY ? &entries[index] : nullptr;
}

void CombatProjectilePool::recordImpact(int index, const Vec3& contact)
{
    CombatProjectileRuntime* entry = this->entry(index);
    if(entry == nullptr || !entry->active)
        return;
    entry->contact = contact;
    if(entry->projectile.hitsRemaining > 0 && --entry->projectile.hitsRemaining == 0)
        entry->active = false;
}

void CombatProjectilePool::advance(float scaledDeltaSeconds)
{
    for(CombatProjectileRuntime& entry : entries)
    {
        if(!entry.active)
            continue;
        CombatProjectile& projectile = entry.projectile;
        if(projectile.movementFlags == 0)
        {
            entry.active = false;
            continue;
        }
        entry.previous = entry.current;
        entry.current = projectile.position;
        const float distance = projectile.speed * scaledDeltaSeconds;
        projectile.remainingDistance -= distance;
        if(projectile.positionMode == 1)
        {
            projectile.position.x += projectile.direction.x * distance;
            projectile.position.y += projectile.direction.y * distance;
            projectile.position.z += projectile.direction.z * distance;
        }
        if(projectile.remainingDistance <= 0.0f)
            entry.active = false;
    }
}

int CombatProjectilePool::activeCount() const
{
    int count = 0;
    for(const CombatProjectileRuntime& entry : entries)
        count += entry.active ? 1 : 0;
    return count;
}

bool combatProjectileContact(const CombatProjectileRuntime& runtime,
                              const Vec3& center, float radius, Vec3& contact)
{
    const CombatProjectile& projectile = runtime.projectile;
    const float reach = radius + projectile.radius;
    const float reachSquared = reach * reach;
    if(projectile.movementFlags == 0)
    {
        const Vec3 difference = vectorDifference(projectile.position, center);
        if(vectorDot(difference, difference) > reachSquared)
            return false;

        contact = { projectile.position.x + difference.x * 0.5f,
                    projectile.position.y + difference.y * 0.5f,
                    projectile.position.z + difference.z * 0.5f };
        return true;
    }
    const Vec3 first = vectorDifference(runtime.current, center);
    const Vec3 second = vectorDifference(projectile.position, center);
    const bool endpoint = vectorDot(first, first) < reachSquared ||
        vectorDot(second, second) < reachSquared;
    const Vec3 segment = vectorDifference(projectile.position, runtime.current);
    const float lengthSquared = vectorDot(segment, segment);
    if(lengthSquared == 0.0f)
        return false;
    const Vec3 toCenter = vectorDifference(center, runtime.current);
    const float fraction = vectorDot(toCenter, segment) / lengthSquared;
    if(!endpoint && (fraction < 0.0f || fraction > 1.0f))
        return false;
    const Vec3 projected = { runtime.current.x + segment.x * fraction,
                             runtime.current.y + segment.y * fraction,
                             runtime.current.z + segment.z * fraction };
    const Vec3 difference = vectorDifference(projected, center);
    const float distanceSquared = vectorDot(difference, difference);
    if(!endpoint && distanceSquared >= reachSquared)
        return false;
    const Vec3 direction = normalizedVector(segment);
    const float chord = sqrtf(reachSquared - distanceSquared);
    const Vec3 intersection = { projected.x - direction.x * chord,
                                projected.y - direction.y * chord,
                                projected.z - direction.z * chord };
    const Vec3 normal = normalizedVector(vectorDifference(intersection, center));
    contact = { center.x + normal.x * radius, center.y + normal.y * radius,
                center.z + normal.z * radius };
    return true;
}
