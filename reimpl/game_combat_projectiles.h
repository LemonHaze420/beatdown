#ifndef GAME_COMBAT_PROJECTILES_H
#define GAME_COMBAT_PROJECTILES_H

#include "game_vector.h"
#include "game_combat_received_damage.h"
#include <stddef.h>
#include <stdint.h>

const int COMBAT_PROJECTILE_CAPACITY = 10;

struct CombatProjectile
{
    int32_t ownerSlot;
    int32_t ignoredSlots[3];
    int32_t group;
    CombatReceivedHitContext hit;
    uint32_t positionMode;
    Vec3 position;
    float radius;
    uint32_t unknown58[13];
    uint16_t damage;
    int16_t hitsRemaining;
    uint32_t unknown90;
    float remainingDistance;
    uint32_t movementFlags;
    Vec3 direction;
    float speed;
    uint32_t unknownac;
};

static_assert(sizeof(CombatProjectile) == 0xb0, "native projectile request size");
static_assert(offsetof(CombatProjectile, hit) == 0x14, "native projectile hit record");
static_assert(offsetof(CombatProjectile, position) == 0x48, "native projectile origin");
static_assert(offsetof(CombatProjectile, damage) == 0x8c, "native projectile damage");
static_assert(offsetof(CombatProjectile, speed) == 0xa8, "native projectile speed");

struct CombatProjectileRuntime
{
    CombatProjectile projectile;
    Vec3 current;
    Vec3 previous;
    Vec3 contact;
    bool active;
};

void initializeCombatProjectile(CombatProjectile& projectile);

bool combatProjectileContact(const CombatProjectileRuntime& projectile,
                              const Vec3& center, float radius, Vec3& contact);

class CombatProjectilePool
{
public:
    CombatProjectilePool();
    void reset();
    int spawn(const CombatProjectile& projectile);
    CombatProjectileRuntime* entry(int index);
    const CombatProjectileRuntime* entry(int index) const;
    void recordImpact(int index, const Vec3& contact);
    void advance(float scaledDeltaSeconds);
    int activeCount() const;
private:
    CombatProjectileRuntime entries[COMBAT_PROJECTILE_CAPACITY];
};

#endif
