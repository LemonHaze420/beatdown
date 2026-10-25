#ifndef EVENT_VM_H
#define EVENT_VM_H

#include "story_state.h"

#include "script_bin.h"

#include <stdint.h>
#include <string>
#include <vector>

const int EVENT_VM_TEXT_SLOT_COUNT = 32;
const int EVENT_VM_TEXTURE_SLOT_COUNT = 32;
const int EVENT_VM_SPRITE_SLOT_COUNT = 64;
const int EVENT_VM_MAILBOX_COUNT = 32;
const int EVENT_VM_LABEL_COUNT = 32;

struct EventVmOperand
{
    EventOperandTypeCode type;
    uint32_t bits;
};

struct EventVmCommand
{
    EventOpcode opcode;
    EventVmOperand operands[16];
    uint8_t operandCount;
};

enum EventVmHostResult
{
    EVENT_VM_HOST_CONTINUE,
    EVENT_VM_HOST_WAIT,
    EVENT_VM_HOST_FAIL
};

typedef EventVmHostResult EventVmCommandCallback(void* userData,
                                                  const EventVmCommand& command);
typedef EventVmHostResult EventVmCallFunctionCallback(void* userData,
                                                       EventCallFunctionId function);
typedef uint16_t EventVmInventoryCountCallback(
    void* userData, uint16_t itemId, int variant);
typedef bool EventVmInventoryMutationCallback(
    void* userData, uint16_t itemId, uint16_t variant, uint16_t quantity);

struct EventVmCallbacks
{
    EventVmCommandCallback* command;
    EventVmCallFunctionCallback* callFunction;
    EventVmInventoryCountCallback* inventoryCount;
    EventVmInventoryMutationCallback* addInventory;
    EventVmInventoryMutationCallback* removeInventory;
    void* userData;
};

struct EventVmTextState
{
    uint16_t messageId;
    float x;
    float y;
    float scaleX;
    float scaleY;
    uint32_t color;
    int32_t glyphAdvance;
    int32_t lineAdvance;
    uint8_t alignment;
    uint8_t active;
    uint8_t visible;
    uint8_t transitioning;
};

struct EventVmTextureState
{
    int32_t resourceId;
    int32_t value;
    uint16_t width;
    uint16_t height;
    uint8_t loaded;
};

struct EventVmSpriteState
{
    int32_t textureSource;
    float x;
    float y;
    float z;
    float scaleX;
    float scaleY;
    float rotation;
    uint32_t color;
    int16_t width;
    int16_t height;
    uint8_t active;
    uint8_t visible;
    uint8_t transitioning;
};

struct EventVmAudioState
{
    int32_t bgmTrack[3];
    int32_t bgmVolume[3];
    uint8_t bgmPlaying[3];
    uint8_t bgmPaused[3];
    int32_t lastSoundBank;
    int32_t lastSoundCue;
    int32_t lastSoundVolume;
};

class EventVm
{
public:
    EventVm();

    void initialize(StoryState* storyState, const EventVmCallbacks& callbacks);
    bool load(const std::vector<uint8_t>& data, EventLanguage language);
    bool start(uint16_t pageId);
    void stop(void);
    bool update(float deltaSeconds, uint32_t pressedButtons,
                uint32_t heldButtons);
    int pageCount(void) const;
    void resumeHostCall(void);
    void setMailbox(int slot, uint32_t value);

    bool loaded(void) const;
    bool running(void) const;
    bool failed(void) const;
    uint16_t eventId(void) const;
    uint16_t pageId(void) const;
    uint32_t programCounter(void) const;
    const char* activeText(int slot) const;
    const EventVmTextState& text(int slot) const;
    const EventVmSpriteState& sprite(int slot) const;
    const EventVmTextureState* textureState(int slot) const;
    const EventVmAudioState& audio(void) const;
    const char* error(void) const;

private:
    uint8_t read8(uint32_t offset) const;
    uint16_t read16(uint32_t offset) const;
    uint32_t read32(uint32_t offset) const;
    float readFloat(uint32_t offset) const;
    uint32_t pageOffset(uint16_t page) const;
    bool jumpToPage(uint16_t page);
    void fail(const char* message);
    void advance(uint32_t bytes);
    void setRegister(uint16_t index, uint32_t bits, EventVmValueType type);
    int32_t registerSigned(uint16_t index) const;
    uint32_t registerUnsigned(uint16_t index) const;
    float registerFloat(uint16_t index) const;
    bool executeInstruction(EventOpcode opcode,
                            const EventOpcodeDescriptor& descriptor);
    bool executeExternal(EventOpcode opcode,
                         const EventOpcodeDescriptor& descriptor);
    void collectExternalOperands(const EventOpcodeDescriptor& descriptor,
                                 EventVmCommand& command) const;
    int64_t commandValue(const EventVmCommand& command,
                         EventOperandTypeCode type, int occurrence = 0) const;
    float commandFloat(const EventVmCommand& command,
                       EventOperandTypeCode type, int occurrence = 0) const;
    void setCommandRegister(const EventVmCommand& command, uint32_t value);

    EventScript script;
    std::vector<uint32_t> code;
    std::vector<uint32_t> pages;
    std::vector<std::string> strings;
    EventOpcodeSet opcodeSet;
    StoryState* story;
    EventVmCallbacks host;
    EventVmTextState textSlots[EVENT_VM_TEXT_SLOT_COUNT];
    EventVmTextureState textureSlots[EVENT_VM_TEXTURE_SLOT_COUNT];
    EventVmSpriteState spriteSlots[EVENT_VM_SPRITE_SLOT_COUNT];
    EventVmAudioState audioState;
    uint32_t mailbox[EVENT_VM_MAILBOX_COUNT];
    uint32_t labels[EVENT_VM_LABEL_COUNT];
    uint32_t pc;
    uint32_t pressed;
    uint32_t held;
    int32_t waitFrames;
    int16_t waitingMailbox;
    uint16_t currentEventId;
    uint16_t currentPageId;
    uint32_t randomState;
    bool hasScript;
    bool isRunning;
    bool hasFailed;
    bool waitingForHost;
    std::string errorMessage;
};

#endif
