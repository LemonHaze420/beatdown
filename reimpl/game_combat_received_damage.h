#ifndef GAME_COMBAT_RECEIVED_DAMAGE_H
#define GAME_COMBAT_RECEIVED_DAMAGE_H

#include <stddef.h>
#include <stdint.h>

struct CombatReceivedHitContext
{
    int32_t ownerSlot;
    uint32_t attributeAddress;
    int32_t boneIndex;
    int32_t contactIndex;
    uint16_t generation;
    uint16_t pride;
    int32_t reactionFamily;
    int32_t unknown18;
    int32_t unknown1c;
    uint32_t hitFlags;
    uint32_t flagsA;
    uint32_t flagsB;
    uint32_t flagsC;
};

struct CombatReceivedContact
{
    int32_t attackerSlot;
    int32_t family;
    int32_t reaction;
    int32_t direction;
    uint32_t hitFlags;
};

static_assert(sizeof(CombatReceivedHitContext) == 0x30, "native hit context size");
static_assert(offsetof(CombatReceivedHitContext, reactionFamily) == 0x14,
              "native signed reaction-family word");
static_assert(offsetof(CombatReceivedHitContext, flagsC) == 0x2c,
              "native final hit flags word");
static_assert(sizeof(CombatReceivedContact) == 0x14, "native contact size");

struct CombatReceivedReactionInput
{
    CombatReceivedHitContext hit;
    uint32_t physicalState;
    uint32_t linkedTargetFlags;
    uint8_t facing;
    uint8_t linkedCombo;
    int linkedComboLimit;
    int weaponBehavior;
    bool alive;
    bool grounded;
};

struct CombatReceivedReaction
{
    int family;
    int reaction;
    int direction;
    int column;
    int actionRemap;
    bool defeat;
    bool physicalOverride;
};

CombatReceivedReaction selectCombatReceivedReaction(
    const CombatReceivedReactionInput& input);
void clearCombatReceivedHitContext(CombatReceivedHitContext& context);

#endif
