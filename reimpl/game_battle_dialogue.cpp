#include "game_battle_dialogue.h"
#include "game_battle_tables.h"
#include "game.h"
#include "character_records.h"

BattleDialogueMessage battleDialogueMessage(int group, int event,
                                            bool fullHealth, uint8_t gender)
{
    BattleDialogueMessage message;
    if(event == 0xc)
    {
        message.voice = 0x9cf;
        message.text = 0xc607;
        return message;
    }
    int offset;
    switch(event)
    {
    case 0: offset = 0x22; break;
    case 1: offset = 0x32; break;
    case 2: offset = 0x42; break;
    case 3: offset = 0x06; break;
    case 4:
    case 0x12: offset = 0x2e; break;
    case 5: offset = 0x2a; break;
    case 6:
    case 7: offset = 0x3a; break;
    case 8: offset = 0x16; break;
    case 9:
    case 0xb: offset = fullHealth ? 0x0a : 0x0e; break;
    case 0xa: offset = 0x26; break;
    case 0xd: offset = 0x4a; break;
    case 0xe: offset = 0x02; break;
    case 0xf: offset = 0x52; break;
    case 0x10: offset = 0x5a; break;
    case 0x11: offset = 0x5e; break;
    case 0x13: offset = 0x3e; break;
    case 0x14: offset = 0x4e; break;
    default:
        message.voice = gender == 0 ? 0x157 : 0x16d;
        return message;
    }
    if(group <= 0 || group > BATTLE_DIALOGUE_GROUP_COUNT)
        return message;
    const uint16_t* row = BATTLE_DIALOGUE_GROUPS[group - 1];
    message.voice = row[offset / 2];
    if(event != 0xe)
        message.text = row[(event == 0x14 ? 0x40 : offset + 2) / 2];
    return message;
}

void Game::queueBattleDialogue(GameSlotRecord& actor, int event)
{
    const CharacterRecord* record = characterRecordForId(
        static_cast<uint16_t>(actor.resourceProfileId));
    if(record == nullptr)
        return;
    const PersistentGameRecord* persistentRecord = characterRecord(actor.persistentRecordId);
    const BattleDialogueMessage message = battleDialogueMessage(record->groupId,
        event, actor.currentHealth == actor.maxHealth,
        persistentRecord != nullptr ? persistentRecord->gender : 0);
    if(message.voice == 0 && message.text == 0)
        return;
    battleController.dialogueActor = actor.slotIndex;
    battleController.dialogueEvent = event;
    ++battleController.dialogueGeneration;
    battleController.eventMessageActive = true;
    battleController.eventMessageText = message.text;
    battleController.eventMessageTimeout = 0;
    battleController.pendingSoundStopMask |= 4;
    battleResult.pendingMessageVoiceTrack = message.voice != 0 ? message.voice : -1;
    gameData.loadTransientText(GAME_TEXT_TABLE_NEGO, true);
}
