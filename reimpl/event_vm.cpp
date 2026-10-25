#include "event_vm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{

bool isHalfwordExternalOperand(EventOperandTypeCode type)
{
    switch(type)
    {
    case EVENT_OPERAND_PAGE_ID:
    case EVENT_OPERAND_TEXT_SLOT:
    case EVENT_OPERAND_TEXTURE_SLOT:
    case EVENT_OPERAND_SPRITE_SLOT:
    case EVENT_OPERAND_POSITION_X_S16:
    case EVENT_OPERAND_POSITION_Y_S16:
    case EVENT_OPERAND_WIDTH:
    case EVENT_OPERAND_HEIGHT:
    case EVENT_OPERAND_REGISTER_ID:
    case EVENT_OPERAND_SOURCE_REGISTER_A:
    case EVENT_OPERAND_SOURCE_REGISTER_B:
    case EVENT_OPERAND_LABEL_SLOT:
    case EVENT_OPERAND_MAILBOX_SLOT:
    case EVENT_OPERAND_SAVE_FLAG_ID:
    case EVENT_OPERAND_STATUS_ID:
        return true;
    default:
        return false;
    }
}

bool isFloatExternalOperand(EventOperandTypeCode type)
{
    switch(type)
    {
    case EVENT_OPERAND_POSITION_X:
    case EVENT_OPERAND_POSITION_Y:
    case EVENT_OPERAND_POSITION_Z:
    case EVENT_OPERAND_SCALE_X:
    case EVENT_OPERAND_SCALE_Y:
    case EVENT_OPERAND_ROTATION:
    case EVENT_OPERAND_WORLD_X:
    case EVENT_OPERAND_WORLD_Y:
    case EVENT_OPERAND_WORLD_Z:
    case EVENT_OPERAND_FLOAT_VALUE:
        return true;
    default:
        return false;
    }
}

bool isUnsignedExternalOperand(EventOperandTypeCode type)
{
    switch(type)
    {
    case EVENT_OPERAND_UNSIGNED_VALUE:
    case EVENT_OPERAND_PACKED_COLOR:
    case EVENT_OPERAND_PAD_PRESSED_MASK:
    case EVENT_OPERAND_PAD_HELD_MASK:
    case EVENT_OPERAND_PAD_RELEASED_MASK:
    case EVENT_OPERAND_PAD_REPEAT_MASK:
    case EVENT_OPERAND_REGISTER_ID:
    case EVENT_OPERAND_SOURCE_REGISTER_A:
    case EVENT_OPERAND_SOURCE_REGISTER_B:
    case EVENT_OPERAND_SAVE_FLAG_ID:
    case EVENT_OPERAND_STATUS_ID:
    case EVENT_OPERAND_PAGE_ID:
        return true;
    default:
        return false;
    }
}

int validIndex(int value, int count)
{
    return value >= 0 && value < count ? value : -1;
}

}

EventVm::EventVm()
    : opcodeSet(EventOpcodeSet::Current),
      story(NULL),
      pc(0),
      pressed(0),
      held(0),
      waitFrames(0),
      waitingMailbox(-1),
      currentEventId(0),
      currentPageId(0xffff),
      randomState(0x53505u),
      hasScript(false),
      isRunning(false),
      hasFailed(false),
      waitingForHost(false)
{
    memset(&host, 0, sizeof(host));
    memset(textSlots, 0, sizeof(textSlots));
    memset(textureSlots, 0, sizeof(textureSlots));
    memset(spriteSlots, 0, sizeof(spriteSlots));
    memset(&audioState, 0, sizeof(audioState));
    memset(mailbox, 0, sizeof(mailbox));
    memset(labels, 0xff, sizeof(labels));
    for(int i = 0; i < 3; ++i)
    {
        audioState.bgmTrack[i] = -1;
        audioState.bgmVolume[i] = 100;
    }
}

void
EventVm::initialize(StoryState* storyState, const EventVmCallbacks& callbacks)
{
    story = storyState;
    host = callbacks;
    stop();
    hasFailed = false;
    errorMessage.clear();
    memset(textSlots, 0, sizeof(textSlots));
    memset(textureSlots, 0, sizeof(textureSlots));
    memset(spriteSlots, 0, sizeof(spriteSlots));
    memset(&audioState, 0, sizeof(audioState));
    memset(mailbox, 0, sizeof(mailbox));
    memset(labels, 0xff, sizeof(labels));
    for(int i = 0; i < 3; ++i)
    {
        audioState.bgmTrack[i] = -1;
        audioState.bgmVolume[i] = 100;
    }
}

bool
EventVm::load(const std::vector<uint8_t>& data, EventLanguage language)
{
    stop();
    hasScript = false;
    hasFailed = false;
    errorMessage.clear();
    code.clear();
    pages.clear();
    strings.clear();

    if(language >= EVENT_LANGUAGE_COUNT || !readEventScript(data, script))
    {
        fail("Could not decode event script");
        return false;
    }

    if(script.format == EventScriptFormat::Localized)
    {
        const LanguageSection* selected = &script.localized.languages[language];
        if(selected->empty)
        {
            for(int i = 0; i < EVENT_LANGUAGE_COUNT; ++i)
            {
                if(!script.localized.languages[i].empty)
                {
                    selected = &script.localized.languages[i];
                    break;
                }
            }
        }
        if(selected->empty || selected->code.empty())
        {
            fail("Event script has no executable language section");
            return false;
        }
        code = selected->code;
        pages = selected->pages;
        strings = selected->text;
        currentEventId = selected->eventId;
        opcodeSet = script.localized.opcodeSet;
    }
    else
    {
        if(script.sms.code.empty())
        {
            fail("SMS event script has no code");
            return false;
        }
        code = script.sms.code;
        pages = script.sms.recordOffsets;
        strings = script.sms.strings;
        currentEventId = script.sms.scriptId;
        opcodeSet = EventOpcodeSet::Current;
    }

    hasScript = true;
    return true;
}

bool
EventVm::start(uint16_t requestedPage)
{
    if(!hasScript || hasFailed)
        return false;

    pc = 0;
    waitFrames = 0;
    waitingMailbox = -1;
    waitingForHost = false;
    currentPageId = 0xffff;
    memset(labels, 0xff, sizeof(labels));
    if(!pages.empty())
    {
        if(requestedPage >= pages.size())
        {
            fail("Event page is out of range");
            return false;
        }
        pc = pages[requestedPage];
        currentPageId = requestedPage;
    }
    else if(requestedPage != 0)
    {
        fail("Headerless event script has no page table");
        return false;
    }
    randomState = 0x53505u ^ currentEventId ^
        (static_cast<uint32_t>(requestedPage) << 16);
    isRunning = true;

    if(story != NULL)
        beginStoryEventSelection(*story, currentEventId);
    return true;
}

void
EventVm::stop(void)
{

    if(isRunning && story != NULL)
        endStoryEventSelection(*story, currentEventId);
    isRunning = false;
    waitingForHost = false;
    waitingMailbox = -1;
    waitFrames = 0;
}

bool
EventVm::update(float deltaSeconds, uint32_t pressedButtons,
                uint32_t heldButtons)
{
    if(!isRunning || hasFailed)
        return false;
    pressed = pressedButtons;
    held = heldButtons;

    if(waitingForHost)
        return true;
    if(waitingMailbox >= 0)
    {
        if(mailbox[waitingMailbox] == 0)
            return true;
        mailbox[waitingMailbox] = 0;
        waitingMailbox = -1;
    }
    if(waitFrames > 0)
    {
        int frames = static_cast<int>(deltaSeconds * 60.0f + 0.5f);
        if(frames < 1)
            frames = 1;
        waitFrames -= frames;
        if(waitFrames > 0)
            return true;
        waitFrames = 0;
    }

    for(int dispatched = 0; dispatched < 4096 && isRunning && !hasFailed;
        ++dispatched)
    {
        if(pc == code.size() * sizeof(uint32_t))
        {
            stop();
            break;
        }
        if(pc + 2 > code.size() * sizeof(uint32_t))
        {
            fail("Event program counter is out of range");
            break;
        }
        const uint16_t serialized = read16(pc);
        const uint16_t decoded = decodeEventOpcode(serialized, opcodeSet);
        if(decoded >= EVENT_OPCODE_DESCRIPTORS.size())
        {
            fail("Event opcode is out of range");
            break;
        }
        const EventOpcodeDescriptor& descriptor =
            EVENT_OPCODE_DESCRIPTORS[decoded];
        if(descriptor.wordCount == 0 ||
           pc + descriptor.byteLength() > code.size() * sizeof(uint32_t))
        {
            fail("Event instruction crosses code boundary");
            break;
        }
        if(!executeInstruction(static_cast<EventOpcode>(decoded), descriptor))
            break;
    }
    if(isRunning && !hasFailed && waitFrames == 0 && waitingMailbox < 0 &&
       !waitingForHost && pc < code.size() * sizeof(uint32_t))
    {
        return true;
    }
    return isRunning && !hasFailed;
}

void EventVm::resumeHostCall(void) { waitingForHost = false; }

void
EventVm::setMailbox(int slot, uint32_t value)
{
    if(validIndex(slot, EVENT_VM_MAILBOX_COUNT) >= 0)
        mailbox[slot] = value;
}

uint8_t
EventVm::read8(uint32_t offset) const
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(code.data());
    return bytes[offset];
}

uint16_t
EventVm::read16(uint32_t offset) const
{
    uint16_t value;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(code.data());
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

uint32_t
EventVm::read32(uint32_t offset) const
{
    uint32_t value;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(code.data());
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

float
EventVm::readFloat(uint32_t offset) const
{
    float value;
    const uint32_t bits = read32(offset);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t
EventVm::pageOffset(uint16_t page) const
{
    if(page >= pages.size())
        return UINT32_MAX;
    return pages[page];
}

bool
EventVm::jumpToPage(uint16_t page)
{
    const uint32_t offset = pageOffset(page);
    if(offset == UINT32_MAX)
    {
        fail("Event jump page is out of range");
        return false;
    }
    pc = offset;
    currentPageId = page;
    return true;
}

void
EventVm::fail(const char* message)
{
    hasFailed = true;
    isRunning = false;
    errorMessage = message == NULL ? "Event VM failure" : message;
}

void EventVm::advance(uint32_t bytes) { pc += bytes; }

void
EventVm::setRegister(uint16_t index, uint32_t bits, EventVmValueType type)
{
    if(story == NULL || index >= STORY_REGISTER_COUNT)
        return;
    story->registers[index] = bits;
    story->registerTypes[index] = static_cast<uint8_t>(type);
}

int32_t
EventVm::registerSigned(uint16_t index) const
{
    if(story == NULL || index >= STORY_REGISTER_COUNT)
        return 0;
    switch(story->registerTypes[index])
    {
    case EVENT_VM_S16: return static_cast<int16_t>(story->registers[index]);
    case EVENT_VM_U16: return static_cast<uint16_t>(story->registers[index]);
    case EVENT_VM_S8: return static_cast<int8_t>(story->registers[index]);
    case EVENT_VM_U8: return static_cast<uint8_t>(story->registers[index]);
    default: return static_cast<int32_t>(story->registers[index]);
    }
}

uint32_t
EventVm::registerUnsigned(uint16_t index) const
{
    return static_cast<uint32_t>(registerSigned(index));
}

float
EventVm::registerFloat(uint16_t index) const
{
    if(story == NULL || index >= STORY_REGISTER_COUNT)
        return 0.0f;
    if(story->registerTypes[index] != EVENT_VM_F32)
        return static_cast<float>(registerSigned(index));
    float value;
    const uint32_t bits = story->registers[index];
    memcpy(&value, &bits, sizeof(value));
    return value;
}

bool
EventVm::executeInstruction(EventOpcode opcode,
                            const EventOpcodeDescriptor& descriptor)
{
    const uint32_t start = pc;
    const uint32_t next = pc + descriptor.byteLength();

    if(opcode >= EVENT_OP_EXF_SET_POS && opcode <= EVENT_OP_EXF_MESTURN)
        return executeExternal(opcode, descriptor);

    switch(opcode)
    {
    case EVENT_OP_NONE:
        fail("Invalid zero Event opcode");
        return false;
    case EVENT_OP_EVENT:
        currentEventId = read16(start + 2);
        pc = next;
        break;
    case EVENT_OP_END_EVENT:
    case EVENT_OP_EVENT_END:
        pc = next;
        stop();
        return false;
    case EVENT_OP_PAGESET:
        currentPageId = read16(start + 2);
        pc = next;
        break;
    case EVENT_OP_END_PAGESET:
    {
        const int16_t page = static_cast<int16_t>(read16(start + 2));
        pc = next;
        if(page >= 0)
            return jumpToPage(static_cast<uint16_t>(page));
        stop();
        return false;
    }
    case EVENT_OP_PAGE_JUMP:
        return jumpToPage(read16(start + 2));

    case EVENT_OP_JUMP_BEQI:
    case EVENT_OP_JUMP_BEQUI:
    case EVENT_OP_JUMP_BEQS:
    case EVENT_OP_JUMP_BEQUS:
    case EVENT_OP_JUMP_BEQB:
    case EVENT_OP_JUMP_BEQUB:
    case EVENT_OP_JUMP_BEQF:
    case EVENT_OP_JUMP_BNEI:
    case EVENT_OP_JUMP_BNEUI:
    case EVENT_OP_JUMP_BNES:
    case EVENT_OP_JUMP_BNEUS:
    case EVENT_OP_JUMP_BNEB:
    case EVENT_OP_JUMP_BNEUB:
    case EVENT_OP_JUMP_BNEF:
    {
        const uint16_t page = read16(start + 2);
        const uint16_t reg = read16(start + 0x0a);
        bool equal = false;
        switch(opcode)
        {
        case EVENT_OP_JUMP_BEQI: case EVENT_OP_JUMP_BNEI:
            equal = registerSigned(reg) == static_cast<int32_t>(read32(start + 4));
            break;
        case EVENT_OP_JUMP_BEQUI: case EVENT_OP_JUMP_BNEUI:
            equal = registerUnsigned(reg) == read32(start + 4);
            break;
        case EVENT_OP_JUMP_BEQS: case EVENT_OP_JUMP_BNES:
            equal = registerSigned(reg) == static_cast<int16_t>(read16(start + 4));
            break;
        case EVENT_OP_JUMP_BEQUS: case EVENT_OP_JUMP_BNEUS:
            equal = registerUnsigned(reg) == read16(start + 4);
            break;
        case EVENT_OP_JUMP_BEQB: case EVENT_OP_JUMP_BNEB:
            equal = registerSigned(reg) == static_cast<int8_t>(read8(start + 4));
            break;
        case EVENT_OP_JUMP_BEQUB: case EVENT_OP_JUMP_BNEUB:
            equal = registerUnsigned(reg) == read8(start + 4);
            break;
        default:
            equal = fabsf(registerFloat(reg) - readFloat(start + 4)) < 0.00001f;
            break;
        }
        const bool notEqualOpcode = opcode >= EVENT_OP_JUMP_BNEI &&
            opcode <= EVENT_OP_JUMP_BNEF;
        pc = next;
        if(equal != notEqualOpcode)
            return jumpToPage(page);
        break;
    }
    case EVENT_OP_JZE:
    case EVENT_OP_JNZ:
    case EVENT_OP_JMI:
    case EVENT_OP_JPZ:
    {
        const uint16_t page = read16(start + 2);
        const int32_t value = registerSigned(read16(start + 6));
        bool take = opcode == EVENT_OP_JZE ? value == 0 :
                    opcode == EVENT_OP_JNZ ? value != 0 :
                    opcode == EVENT_OP_JMI ? value < 0 : value >= 0;
        pc = next;
        if(take)
            return jumpToPage(page);
        break;
    }

    case EVENT_OP_TEX_LOAD:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_TEXTURE_SLOT_COUNT) < 0)
        {
            fail("Texture slot is out of range");
            return false;
        }
        textureSlots[slot].resourceId = static_cast<int32_t>(read32(start + 4));
        textureSlots[slot].value = static_cast<int32_t>(read32(start + 8));
        textureSlots[slot].width = read16(start + 0x0c);
        textureSlots[slot].height = read16(start + 0x0e);
        textureSlots[slot].loaded = 1;
        pc = next;
        break;
    }
    case EVENT_OP_TEX_RELEASE:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_TEXTURE_SLOT_COUNT) >= 0)
            memset(&textureSlots[slot], 0, sizeof(textureSlots[slot]));
        pc = next;
        break;
    }
    case EVENT_OP_TEX_CHECK:
        pc = next;
        break;
    case EVENT_OP_SPRITE:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0)
        {
            fail("Sprite slot is out of range");
            return false;
        }
        EventVmSpriteState& spriteState = spriteSlots[slot];
        memset(&spriteState, 0, sizeof(spriteState));
        spriteState.width = static_cast<int16_t>(read16(start + 4));
        spriteState.height = static_cast<int16_t>(read16(start + 6));
        spriteState.x = static_cast<float>(static_cast<int16_t>(read16(start + 8)));
        spriteState.y = static_cast<float>(static_cast<int16_t>(read16(start + 0x0a)));
        spriteState.textureSource = static_cast<int32_t>(read32(start + 0x0c));
        spriteState.scaleX = 1.0f;
        spriteState.scaleY = 1.0f;
        spriteState.color = 0xffffffffu;
        spriteState.active = 1;
        pc = next;
        break;
    }
    case EVENT_OP_SPRSET_XYZ:
    case EVENT_OP_SPRCNG_XYZ:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0)
        {
            fail("Sprite position slot is out of range");
            return false;
        }
        EventVmSpriteState& spriteState = spriteSlots[slot];
        spriteState.x = readFloat(start + 4);
        spriteState.y = readFloat(start + 8);
        spriteState.z = opcode == EVENT_OP_SPRSET_XYZ ?
            static_cast<float>(static_cast<int32_t>(read32(start + 0x0c))) :
            readFloat(start + 0x0c);
        if(opcode == EVENT_OP_SPRCNG_XYZ)
        {
            waitFrames = static_cast<int32_t>(read32(start + 0x10));
            spriteState.transitioning = waitFrames > 0;
        }
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_SPRSET_SCALE:
    case EVENT_OP_SPRCNG_SCALE:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0)
        {
            fail("Sprite scale slot is out of range");
            return false;
        }
        spriteSlots[slot].scaleX = readFloat(start + 4);
        spriteSlots[slot].scaleY = readFloat(start + 8);
        if(opcode == EVENT_OP_SPRCNG_SCALE)
            waitFrames = static_cast<int32_t>(read32(start + 0x0c));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_SPRSET_COLOR:
    case EVENT_OP_SPRCNG_COLOR:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0)
        {
            fail("Sprite color slot is out of range");
            return false;
        }
        spriteSlots[slot].color = read32(start + 4);
        if(opcode == EVENT_OP_SPRCNG_COLOR)
            waitFrames = static_cast<int32_t>(read32(start + 8));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_SPRCNG_TURN:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0)
        {
            fail("Sprite rotation slot is out of range");
            return false;
        }
        spriteSlots[slot].rotation = readFloat(start + 4);
        waitFrames = static_cast<int32_t>(read32(start + 8));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_SPRDISP_ON:
    case EVENT_OP_SPRDISP_OFF:
    case EVENT_OP_SPRCLEAR:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) >= 0)
        {
            spriteSlots[slot].visible = opcode == EVENT_OP_SPRDISP_ON;
            if(opcode == EVENT_OP_SPRCLEAR)
                memset(&spriteSlots[slot], 0, sizeof(spriteSlots[slot]));
        }
        pc = next;
        break;
    }
    case EVENT_OP_SPRCALCSTART:
    case EVENT_OP_SPRALLCALCSTART:
        pc = next;
        break;
    case EVENT_OP_WAIT_SPR:
        waitFrames = static_cast<int32_t>(read32(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_PADBTN_SET:
        pc = next;
        break;

    case EVENT_OP_BGM_PLAY:
    {
        const int channel = read8(start + 2);
        if(validIndex(channel, 3) >= 0)
        {
            audioState.bgmTrack[channel] = static_cast<int32_t>(read32(start + 4));
            audioState.bgmPlaying[channel] = 1;
            audioState.bgmPaused[channel] = 0;
        }
        pc = next;
        break;
    }
    case EVENT_OP_BGM_STOP:
    {
        const int channel = read8(start + 2);
        if(validIndex(channel, 3) >= 0)
            audioState.bgmPlaying[channel] = 0;
        pc = next;
        break;
    }
    case EVENT_OP_BGM_FADE:
    {
        const int channel = read8(start + 2);
        if(validIndex(channel, 3) >= 0)
            audioState.bgmVolume[channel] = static_cast<int32_t>(read32(start + 8));
        waitFrames = static_cast<int32_t>(read32(start + 0x0c));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_BGM_SETVOLUME:
    {
        const int channel = read8(start + 2);
        if(validIndex(channel, 3) >= 0)
            audioState.bgmVolume[channel] = static_cast<int32_t>(read32(start + 4));
        pc = next;
        break;
    }
    case EVENT_OP_BGM_PAUSE:
    {
        const int channel = read8(start + 2);
        if(validIndex(channel, 3) >= 0)
            audioState.bgmPaused[channel] = read32(start + 4) != 0;
        pc = next;
        break;
    }
    case EVENT_OP_SE_PLAY:
    {
        audioState.lastSoundBank = static_cast<int32_t>(read32(start + 4));
        audioState.lastSoundVolume = static_cast<int32_t>(read32(start + 8));
        audioState.lastSoundCue = static_cast<int32_t>(read32(start + 0x0c));
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_MAILBOX_COUNT) >= 0)
            mailbox[slot] = static_cast<uint32_t>(audioState.lastSoundCue);
        pc = next;
        break;
    }
    case EVENT_OP_SE_STOP:
        audioState.lastSoundCue = -1;
        pc = next;
        break;
    case EVENT_OP_PAD_PRESS:
    {
        const uint32_t released = ~(held) & read32(start + 0x0c);
        const uint32_t repeat = held & read32(start + 0x10);
        const uint32_t result = (pressed & read32(start + 4)) |
            (held & read32(start + 8)) | released | repeat;
        setRegister(read16(start + 2), result, EVENT_VM_U32);
        pc = next;
        break;
    }

    case EVENT_OP_TEXTSET:
    {
        const int slot = read16(start + 2);
        if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) < 0)
        {
            fail("Text slot is out of range");
            return false;
        }
        EventVmTextState& state = textSlots[slot];
        memset(&state, 0, sizeof(state));
        state.x = readFloat(start + 4);
        state.y = readFloat(start + 8);
        state.scaleX = readFloat(start + 0x0c);
        state.scaleY = readFloat(start + 0x10);
        state.color = read32(start + 0x14);
        state.messageId = static_cast<uint16_t>(read32(start + 0x18));
        state.alignment = read8(start + 0x1c);
        state.glyphAdvance = static_cast<int32_t>(read32(start + 0x20));
        state.lineAdvance = static_cast<int32_t>(read32(start + 0x24));
        state.active = 1;
        pc = next;
        break;
    }
    case EVENT_OP_TEXTDISP_ON:
    case EVENT_OP_TEXTDISP_OFF:
    case EVENT_OP_TEXTCLEAR:
    {
        const int slot = read16(start + 2);
        if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) >= 0)
        {
            textSlots[slot].visible = opcode == EVENT_OP_TEXTDISP_ON;
            if(opcode == EVENT_OP_TEXTCLEAR)
                memset(&textSlots[slot], 0, sizeof(textSlots[slot]));
        }
        pc = next;
        break;
    }
    case EVENT_OP_TEXT_FADEIN:
    case EVENT_OP_TEXT_FADEOUT:
    {
        const int slot = read16(start + 2);
        if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) >= 0)
        {
            if(opcode == EVENT_OP_TEXT_FADEOUT)
                textSlots[slot].color = read32(start + 8);
            textSlots[slot].visible = opcode == EVENT_OP_TEXT_FADEIN;
            textSlots[slot].transitioning = 1;
        }
        waitFrames = static_cast<int32_t>(read32(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_TEXT_CNG_XY:
    {
        const int slot = read16(start + 2);
        if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) >= 0)
        {
            textSlots[slot].x = readFloat(start + 4);
            textSlots[slot].y = readFloat(start + 8);
        }
        waitFrames = static_cast<int32_t>(read32(start + 0x0c));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_TEXT_CNG_COLOR:
    {
        const int slot = read16(start + 2);
        if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) >= 0)
            textSlots[slot].color = read32(start + 4);
        waitFrames = static_cast<int32_t>(read32(start + 8));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    }
    case EVENT_OP_PROPORTIONAL:
        pc = next;
        break;
    case EVENT_OP_REG2STRBOX:
    {
        const int box = static_cast<int32_t>(read32(start + 4));
        if(validIndex(box, EVENT_VM_MAILBOX_COUNT) >= 0)
            mailbox[box] = story == NULL ? 0 :
                story->registers[read16(start + 2) % STORY_REGISTER_COUNT];
        pc = next;
        break;
    }

    case EVENT_OP_SVFLAG_ON:
    case EVENT_OP_SVFLAG_OFF:
        if(story != NULL)
            setStorySaveFlag(*story, read16(start + 2),
                             opcode == EVENT_OP_SVFLAG_ON);
        pc = next;
        break;
    case EVENT_OP_SVFLAG_BRON:
    case EVENT_OP_SVFLAG_BROFF:
    {
        const bool value = story != NULL &&
            getStorySaveFlag(*story, read16(start + 2));
        const bool take = opcode == EVENT_OP_SVFLAG_BRON ? value : !value;
        pc = next;
        if(take)
            return jumpToPage(read16(start + 6));
        break;
    }
    case EVENT_OP_GET_STATUS:
    {
        const uint16_t status = read16(start + 2);
        const uint16_t reg = read16(start + 6);
        const int32_t fallback = static_cast<int32_t>(read32(start + 8));
        const int32_t value = story != NULL && status < STORY_STATUS_COUNT ?
            story->statuses[status] : fallback;
        setRegister(reg, static_cast<uint32_t>(value), EVENT_VM_S32);
        pc = next;
        break;
    }
    case EVENT_OP_SET_STATUS:
    {
        const uint16_t status = read16(start + 2);
        const uint16_t reg = read16(start + 6);
        const int32_t immediate = static_cast<int32_t>(read32(start + 8));
        if(story != NULL && status < STORY_STATUS_COUNT)
            story->statuses[status] = reg < STORY_REGISTER_COUNT ?
                registerSigned(reg) : immediate;
        pc = next;
        break;
    }
    case EVENT_OP_WAIT_TEXT:
        waitFrames = static_cast<int32_t>(read32(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_WAIT_BGM:
        pc = next;
        break;
    case EVENT_OP_WAIT_EXEC:
        waitFrames = static_cast<int32_t>(read32(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_STOP_EXEC:
        waitFrames = static_cast<int32_t>(readFloat(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_FADEIN:
        waitFrames = static_cast<int32_t>(read32(start + 4));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_FADEOUT:
        waitFrames = static_cast<int16_t>(read16(start + 2));
        pc = next;
        if(waitFrames > 0)
            return false;
        break;
    case EVENT_OP_FADEIN_CHK:
    case EVENT_OP_FADEOUT_CHK:
        pc = next;
        break;
    case EVENT_OP_WAIT_BT:
        if((pressed & read16(start + 2)) == 0)
            return false;
        pc = next;
        break;
    case EVENT_OP_LABEL_SET:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_LABEL_COUNT) >= 0)
            labels[slot] = next;
        pc = next;
        break;
    }
    case EVENT_OP_LABEL_JUMP:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_LABEL_COUNT) < 0 || labels[slot] == UINT32_MAX)
        {
            fail("Event label is unset");
            return false;
        }
        pc = labels[slot];
        break;
    }
    case EVENT_OP_FLAG_INT:
    case EVENT_OP_FLAG_UINT:
    case EVENT_OP_FLAG_FLOAT:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        if(validIndex(slot, EVENT_VM_MAILBOX_COUNT) >= 0)
            mailbox[slot] = read32(start + 4);
        pc = next;
        break;
    }
    case EVENT_OP_PAUSE:
    {
        const int slot = static_cast<int16_t>(read16(start + 2));
        pc = next;
        if(validIndex(slot, EVENT_VM_MAILBOX_COUNT) >= 0 && mailbox[slot] == 0)
        {
            waitingMailbox = static_cast<int16_t>(slot);
            return false;
        }
        if(validIndex(slot, EVENT_VM_MAILBOX_COUNT) >= 0)
            mailbox[slot] = 0;
        break;
    }
    case EVENT_OP_DOOR_OPEN:
    case EVENT_OP_DOOR_CLOSE:
        if(story != NULL)
            story->eventValue = opcode == EVENT_OP_DOOR_OPEN ? 1u : 0u;
        pc = next;
        break;

    case EVENT_OP_SETREG_I:
        setRegister(read16(start + 2), read32(start + 4), EVENT_VM_S32);
        pc = next;
        break;
    case EVENT_OP_SETREG_UI:
        setRegister(read16(start + 2), read32(start + 4), EVENT_VM_U32);
        pc = next;
        break;
    case EVENT_OP_SETREG_S:
        setRegister(read16(start + 2),
                    static_cast<uint32_t>(static_cast<int16_t>(read16(start + 4))),
                    EVENT_VM_S16);
        pc = next;
        break;
    case EVENT_OP_SETREG_US:
        setRegister(read16(start + 2), read16(start + 4), EVENT_VM_U16);
        pc = next;
        break;
    case EVENT_OP_SETREG_B:
        setRegister(read16(start + 2),
                    static_cast<uint32_t>(static_cast<int8_t>(read8(start + 4))),
                    EVENT_VM_S8);
        pc = next;
        break;
    case EVENT_OP_SETREG_UB:
        setRegister(read16(start + 2), read8(start + 4), EVENT_VM_U8);
        pc = next;
        break;
    case EVENT_OP_SETREG_F:
        setRegister(read16(start + 2), read32(start + 4), EVENT_VM_F32);
        pc = next;
        break;
    case EVENT_OP_SETTYPE:
        if(story != NULL && read16(start + 2) < STORY_REGISTER_COUNT)
            story->registerTypes[read16(start + 2)] =
                static_cast<uint8_t>(read32(start + 4));
        pc = next;
        break;
    case EVENT_OP_REGADDNUM_S:
    case EVENT_OP_REGSUBNUM_S:
    {
        const uint16_t reg = read16(start + 2);
        const int32_t value = static_cast<int32_t>(read32(start + 4));
        const int32_t result = registerSigned(reg) +
            (opcode == EVENT_OP_REGADDNUM_S ? value : -value);
        setRegister(reg, static_cast<uint32_t>(result), EVENT_VM_S32);
        pc = next;
        break;
    }
    case EVENT_OP_REGADDNUM_U:
    case EVENT_OP_REGSUBNUM_U:
    {
        const uint16_t reg = read16(start + 2);
        const uint32_t value = read32(start + 4);
        const uint32_t result = opcode == EVENT_OP_REGADDNUM_U ?
            registerUnsigned(reg) + value : registerUnsigned(reg) - value;
        setRegister(reg, result, EVENT_VM_U32);
        pc = next;
        break;
    }
    case EVENT_OP_REGADDNUM_F:
    case EVENT_OP_REGSUBNUM_F:
    {
        const uint16_t reg = read16(start + 2);
        float result = registerFloat(reg);
        result += (opcode == EVENT_OP_REGADDNUM_F ? 1.0f : -1.0f) *
            readFloat(start + 4);
        uint32_t bits;
        memcpy(&bits, &result, sizeof(bits));
        setRegister(reg, bits, EVENT_VM_F32);
        pc = next;
        break;
    }
    case EVENT_OP_REGADDREG:
    case EVENT_OP_REGSUBREG:
    case EVENT_OP_REGMULREG:
    case EVENT_OP_REGDIVREG_Q:
    case EVENT_OP_REGDIVREG_R:
    {
        const uint16_t destination = read16(start + 2);
        const int32_t a = registerSigned(read16(start + 4));
        const int32_t b = registerSigned(read16(start + 6));
        int32_t result = 0;
        if(opcode == EVENT_OP_REGADDREG) result = a + b;
        else if(opcode == EVENT_OP_REGSUBREG) result = a - b;
        else if(opcode == EVENT_OP_REGMULREG) result = a * b;
        else if(b != 0 && opcode == EVENT_OP_REGDIVREG_Q) result = a / b;
        else if(b != 0) result = a % b;
        setRegister(destination, static_cast<uint32_t>(result), EVENT_VM_S32);
        pc = next;
        break;
    }
    case EVENT_OP_REGCPYREG:
    {
        const uint16_t source = read16(start + 4);
        const uint16_t destination = read16(start + 6);
        if(story != NULL && source < STORY_REGISTER_COUNT &&
           destination < STORY_REGISTER_COUNT)
        {
            story->registers[destination] = story->registers[source];
            story->registerTypes[destination] = story->registerTypes[source];
        }
        pc = next;
        break;
    }
    case EVENT_OP_RANDAM:
        randomState = randomState * 1664525u + 1013904223u;
        setRegister(read16(start + 2), randomState, EVENT_VM_U32);
        pc = next;
        break;
    case EVENT_OP_CALLFUNC:
    {
        const uint32_t functionValue = read32(start + 4);
        if(functionValue >= EVENT_CALLFUNC_COUNT)
        {
            fail("Event call function is out of range");
            return false;
        }
        pc = next;
        if(host.callFunction != NULL)
        {
            const EventVmHostResult result = host.callFunction(
                host.userData, static_cast<EventCallFunctionId>(functionValue));
            if(result == EVENT_VM_HOST_FAIL)
            {
                fail("Event host call failed");
                return false;
            }
            if(result == EVENT_VM_HOST_WAIT)
            {
                waitingForHost = true;
                return false;
            }
        }
        break;
    }
    default:
        fail("Unhandled Event VM opcode");
        return false;
    }
    return true;
}

void
EventVm::collectExternalOperands(const EventOpcodeDescriptor& descriptor,
                                 EventVmCommand& command) const
{
    command.operandCount = 0;
    for(uint8_t i = 0; i < descriptor.operandCount &&
        command.operandCount < 16; ++i)
    {
        const EventOperandMetadata& metadata =
            EVENT_OPERAND_METADATA[descriptor.operandOffset + i];
        if(metadata.typeCode == EVENT_OPERAND_NONE)
            continue;

        uint8_t offset = static_cast<uint8_t>(metadata.wordIndex * 4);
        if(isHalfwordExternalOperand(metadata.typeCode))
        {
            if(metadata.typeCode == EVENT_OPERAND_SOURCE_REGISTER_B)
                offset += 2;
            else if(metadata.typeCode != EVENT_OPERAND_SOURCE_REGISTER_A)
                offset = metadata.wordIndex == 0 ? 2 : offset + 2;
        }

        EventVmOperand& operand = command.operands[command.operandCount++];
        operand.type = metadata.typeCode;
        if(isHalfwordExternalOperand(metadata.typeCode))
            operand.bits = read16(pc + offset);
        else
            operand.bits = read32(pc + offset);
    }
}

int64_t
EventVm::commandValue(const EventVmCommand& command,
                      EventOperandTypeCode type, int occurrence) const
{
    for(uint8_t i = 0; i < command.operandCount; ++i)
    {
        if(command.operands[i].type != type)
            continue;
        if(occurrence-- != 0)
            continue;
        if(isUnsignedExternalOperand(type) || isHalfwordExternalOperand(type))
            return command.operands[i].bits;
        return static_cast<int32_t>(command.operands[i].bits);
    }
    return 0;
}

float
EventVm::commandFloat(const EventVmCommand& command,
                      EventOperandTypeCode type, int occurrence) const
{
    for(uint8_t i = 0; i < command.operandCount; ++i)
    {
        if(command.operands[i].type != type)
            continue;
        if(occurrence-- != 0)
            continue;
        if(!isFloatExternalOperand(type))
            return static_cast<float>(static_cast<int32_t>(command.operands[i].bits));
        float value;
        memcpy(&value, &command.operands[i].bits, sizeof(value));
        return value;
    }
    return 0.0f;
}

void
EventVm::setCommandRegister(const EventVmCommand& command, uint32_t value)
{
    for(uint8_t i = 0; i < command.operandCount; ++i)
    {
        if(command.operands[i].type == EVENT_OPERAND_REGISTER_ID)
        {
            setRegister(static_cast<uint16_t>(command.operands[i].bits),
                        value, EVENT_VM_S32);
            return;
        }
    }
}

bool
EventVm::executeExternal(EventOpcode opcode,
                         const EventOpcodeDescriptor& descriptor)
{
    EventVmCommand command;
    command.opcode = opcode;
    collectExternalOperands(descriptor, command);
    advance(descriptor.byteLength());

    const uint32_t instructionStart = pc - descriptor.byteLength();
    const int32_t parameter = static_cast<int32_t>(
        commandValue(command, EVENT_OPERAND_EXF_PARAMETER));
    const int32_t value = static_cast<int32_t>(
        commandValue(command, EVENT_OPERAND_EXF_VALUE));

    if(story != NULL)
    {
        const auto setRegisterOperand =
            [this, &command](EventOperandTypeCode type, int occurrence,
                             uint32_t bits)
            {
                for(uint8_t i = 0; i < command.operandCount; ++i)
                {
                    if(command.operands[i].type != type)
                        continue;
                    if(occurrence-- != 0)
                        continue;
                    setRegister(static_cast<uint16_t>(command.operands[i].bits),
                                bits, EVENT_VM_S32);
                    return;
                }
            };
        const auto registerOperandValue =
            [this, &command](EventOperandTypeCode type, int occurrence)
                -> uint32_t
            {
                const int64_t index = commandValue(command, type, occurrence);
                return index >= 0 && index < STORY_REGISTER_COUNT ?
                    registerUnsigned(static_cast<uint16_t>(index)) : 0;
            };
        const auto actorFor =
            [this, &command](StoryState& state, EventOperandTypeCode type,
                             EventOperandTypeCode optionalRegister)
                -> StoryActorState*
            {
                uint16_t actorId = static_cast<uint16_t>(
                    commandValue(command, type));
                const int64_t registerId =
                    commandValue(command, optionalRegister);
                if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
                    actorId = static_cast<uint16_t>(
                        registerUnsigned(static_cast<uint16_t>(registerId)));
                return findStoryActor(state, actorId, true);
            };
        const auto characterFor =
            [](StoryState& state, uint16_t characterId)
                -> StoryCharacterState*
            {
                return characterId < STORY_CHARACTER_COUNT ?
                    &state.characters[characterId] : NULL;
            };

        story->eventValue = static_cast<uint32_t>(value);
        switch(opcode)
        {
        case EVENT_OP_EXF_SET_POS:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_EXF_VALUE,
                                              EVENT_OPERAND_REGISTER_ID);
            if(actor != NULL)
            {
                actor->position[0] =
                    commandFloat(command, EVENT_OPERAND_WORLD_X);
                actor->position[1] =
                    commandFloat(command, EVENT_OPERAND_WORLD_Y);
                actor->position[2] =
                    commandFloat(command, EVENT_OPERAND_WORLD_Z);
            }
            break;
        }
        case EVENT_OP_EXF_SET_BATTLE:
        {
            uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            switch(currentEventId)
            {
            case 0x55:
            case 0x27:
            case 0x28:
            case 0x29:
            case 0x48:
            case 0x54:
            case 0x57:
            case 0x58:
            case 0x85:
            case 0x6e:
            case 0x6a:
            case 0x75:
            case 0x6f:
                characterId = story->callerActorId;
                break;
            default:
                break;
            }
            if(currentEventId == 0x58 || currentEventId == 0x85)
                story->battleFlags |= 4u;
            story->battleId = characterId;
            story->battleEventId = currentEventId;
            story->battleActive = 1;
            story->battlePlural = 0;
            story->battleResultCode = 0;
            break;
        }
        case EVENT_OP_EXF_SET_BATTLE_PLURAL:
        {
            const int64_t registerId =
                commandValue(command, EVENT_OPERAND_EXF_PARAMETER);
            story->battleId = registerId > 0 &&
                              registerId < STORY_REGISTER_COUNT ?
                static_cast<uint16_t>(registerUnsigned(
                    static_cast<uint16_t>(registerId))) :
                story->activeOpponentCharacterId;
            story->battleEventId = currentEventId;
            story->battleActive = 1;
            story->battlePlural = 1;
            story->battleResultCode = 0;
            break;
        }
        case EVENT_OP_EXF_JOIN_PARTY:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_ACTOR_ID,
                                              EVENT_OPERAND_REGISTER_ID);
            if(actor != NULL)
                addStoryPartyMember(*story, actor->actorId, false);
            break;
        }
        case EVENT_OP_EXF_JOIN_FRIEND:
        {
            const uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            StoryCharacterState* character =
                characterFor(*story, characterId);
            if(character != NULL)
            {
                character->characterFlags |= STORY_CHARACTER_ENCOUNTERED |
                                             STORY_CHARACTER_RECRUITED;
                character->profileRevealFlags = 0xffff;
            }
            break;
        }
        case EVENT_OP_EXF_RELEASE_FRIEND:
        {
            const uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            StoryCharacterState* character =
                characterFor(*story, characterId);
            if(character != NULL)
                character->characterFlags &= ~STORY_CHARACTER_RECRUITED;
            break;
        }
        case EVENT_OP_EXF_RELEASE_PARTY:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_ACTOR_ID,
                                              EVENT_OPERAND_REGISTER_ID);
            if(actor != NULL)
                removeStoryPartyMember(*story, actor->actorId);
            break;
        }
        case EVENT_OP_EXF_CALL_TELPHONE:
            story->telephoneEnabled = 1;
            story->messageActorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            break;
        case EVENT_OP_EXF_ADD_REGS2:
        {
            const int32_t left = static_cast<int32_t>(registerOperandValue(
                EVENT_OPERAND_SOURCE_REGISTER_A, 0));
            const int32_t right = static_cast<int32_t>(registerOperandValue(
                EVENT_OPERAND_SOURCE_REGISTER_B, 0));
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(value == 0 ?
                    left + right : left - right));
            break;
        }
        case EVENT_OP_EXF_ADD_REGS:
        {
            const uint16_t registerId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_REGISTER_ID));
            const int32_t immediate = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_SIGNED_VALUE));
            const int32_t current = registerSigned(registerId);
            setRegister(registerId, static_cast<uint32_t>(value == 0 ?
                current + immediate : immediate - current), EVENT_VM_S32);
            break;
        }
        case EVENT_OP_EXF_GAMEOVER:
            story->gameOver = read16(instructionStart + 6) < 2 ? 1 : 0;
            break;
        case EVENT_OP_EXF_SET_COUNTDOWN:
            if(value == 0)
                story->countdownFrames = 0;
            else if(value == 1)
                story->countdownFrames = parameter > 0 ? parameter : 0;
            break;
        case EVENT_OP_EXF_GET_COUNTDOWN:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(story->countdownFrames > 0 ?
                    story->countdownFrames / 60 : -1));
            break;
        case EVENT_OP_EXF_NPC_WALK_TARGET:
        case EVENT_OP_EXF_NPC_RUN_TARGET:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_ACTOR_ID,
                                              EVENT_OPERAND_NONE);
            if(actor != NULL)
            {
                actor->targetActorId = static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_TARGET_ACTOR_ID));
                actor->movementMode =
                    opcode == EVENT_OP_EXF_NPC_WALK_TARGET ? 1 : 2;
                actor->movementDurationFrames = static_cast<float>(
                    commandValue(command, EVENT_OPERAND_DURATION));
            }
            break;
        }
        case EVENT_OP_EXF_NPC_WALK_POS:
        case EVENT_OP_EXF_NPC_RUN_POS:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_ACTOR_ID,
                                              EVENT_OPERAND_REGISTER_ID);
            if(actor != NULL)
            {
                actor->destination[0] =
                    commandFloat(command, EVENT_OPERAND_WORLD_X);
                actor->destination[1] =
                    commandFloat(command, EVENT_OPERAND_WORLD_Y);
                actor->destination[2] =
                    commandFloat(command, EVENT_OPERAND_WORLD_Z);
                actor->targetActorId = 0xffff;
                actor->movementMode =
                    opcode == EVENT_OP_EXF_NPC_WALK_POS ? 1 : 2;
                actor->movementDurationFrames = static_cast<float>(
                    commandValue(command, EVENT_OPERAND_DURATION));
            }
            break;
        }
        case EVENT_OP_EXF_SET_MESNAMES:
        case EVENT_OP_EXF_LOG_MESSAGE:
        case EVENT_OP_EXF_KILL_RECRUITE:
        case EVENT_OP_EXF_CHECK_BUSTED:
        case EVENT_OP_EXF_CHECK_CAPTURE:
            break;
        case EVENT_OP_EXF_COUNT_CONTENT:
            if(getStorySaveFlag(*story, 0x031f))
                setStorySaveFlag(*story, 0x07e4, true);
            break;
        case EVENT_OP_EXF_HEART:
            ++story->eventGeneration;
            break;
        case EVENT_OP_EXF_CHECK_TOUCH_NPC:
        {
            const StoryActorState* player = findStoryActor(*story, 0);
            const StoryActorState* actor = findStoryActor(*story,
                static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_ACTOR_ID)));
            bool touching = false;
            if(player != NULL && actor != NULL &&
               fabsf(player->position[1] - actor->position[1]) < 10.0f)
            {
                const float dx = player->position[0] - actor->position[0];
                const float dz = player->position[2] - actor->position[2];
                touching = dx * dx + dz * dz < 400.0f;
            }
            setRegister(0, touching ? 1u : 0u, EVENT_VM_S32);
            break;
        }
        case EVENT_OP_EXF_ITEM_ADD:
            if(host.addInventory != NULL && parameter > 0)
                host.addInventory(host.userData,
                    static_cast<uint16_t>(commandValue(command,
                        EVENT_OPERAND_ITEM_OR_DRESS_ID)),
                    0, static_cast<uint16_t>(parameter));
            break;
        case EVENT_OP_EXF_ITEM_DEL:
            if(host.removeInventory != NULL && parameter > 0)
                host.removeInventory(host.userData,
                    static_cast<uint16_t>(commandValue(command,
                        EVENT_OPERAND_ITEM_OR_DRESS_ID)),
                    0, static_cast<uint16_t>(parameter));
            break;
        case EVENT_OP_EXF_ITEM_CHECK:
        {
            const uint16_t itemId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ITEM_OR_DRESS_ID));
            const uint16_t quantity = host.inventoryCount == NULL ? 0 :
                host.inventoryCount(host.userData, itemId, -1);
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0, quantity);
            break;
        }
        case EVENT_OP_EXF_CHECK_ST_BATTLE_END:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(story->battleResultCode));
            if(story->battleResultCode != 11)
                story->subBattleId = 0;
            break;
        case EVENT_OP_EXF_SET_CIRCUMSTANCE:
            story->circumstanceId = static_cast<uint16_t>(
                commandValue(command,
                             EVENT_OPERAND_CONTENT_OR_CIRCUMSTANCE_ID));
            if(story->circumstanceId == 0x14)
                story->crime = 0;
            else if(story->circumstanceId == 0x15)
                story->crime = 100;
            break;
        case EVENT_OP_EXF_RESULT:
            story->result = value == 0 ? 2 : 3;
            break;
        case EVENT_OP_EXF_EVENT_END:
            stop();
            break;
        case EVENT_OP_EXF_DISP_RESULT:
            story->resultDisplayActive = 1;
            break;
        case EVENT_OP_EXF_CHECK_TALK_NPC:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                story->targetActorId == static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_ACTOR_ID)));
            break;
        case EVENT_OP_EXF_MESSAGE:
            story->messageActorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            break;
        case EVENT_OP_EXF_SET_NPC:
            story->eventGeneration = static_cast<uint32_t>(parameter);
            break;
        case EVENT_OP_EXF_ADD_MONEY:
        {
            const int32_t amount = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_SIGNED_VALUE));
            const int64_t registerId =
                commandValue(command, EVENT_OPERAND_REGISTER_ID);
            const int32_t resolved = registerId > 0 &&
                                     registerId < STORY_REGISTER_COUNT ?
                registerSigned(static_cast<uint16_t>(registerId)) : amount;
            int64_t cash = static_cast<int64_t>(story->money) + resolved;
            if(cash < 0)
                cash = 0;
            if(cash > 99999999)
                cash = 99999999;
            story->money = static_cast<int32_t>(cash);
            story->characters[0].cash = story->money;
            break;
        }
        case EVENT_OP_EXF_CHECK_STAGE:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                parameter == 0 ? story->stageId : story->eventStageId);
            break;
        case EVENT_OP_EXF_WAIT:
            waitFrames = parameter > 0 ? parameter : 0;
            break;
        case EVENT_OP_EXF_WAIT_ST:
            if(story->stageTransitionPending)
            {
                pc = instructionStart;
                waitFrames = 1;
                return false;
            }
            break;
        case EVENT_OP_EXF_CHECK_DRESS:
        {
            const uint16_t itemId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_PARTY_MEMBER_ID));
            const int32_t variant = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_ITEM_OR_DRESS_ID));
            bool found = false;
            for(int i = 0; i < STORY_DRESS_SLOT_COUNT; ++i)
            {
                const StoryDressEntry& dress = story->dressInventory[i];
                if(!dress.active || dress.quantity == 0)
                    continue;
                if((itemId == 0 || dress.itemId == itemId) &&
                   (variant < 0 || dress.variant == variant))
                {
                    found = true;
                    break;
                }
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                               found ? 1u : 0u);
            break;
        }
        case EVENT_OP_EXF_SET_EMOTION:
        {
            uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const int64_t registerId =
                commandValue(command, EVENT_OPERAND_REGISTER_ID);
            if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
                characterId = static_cast<uint16_t>(
                    registerUnsigned(static_cast<uint16_t>(registerId)));
            StoryCharacterState* character =
                characterFor(*story, characterId);
            if(character != NULL)
            {
                if(value == 0)
                    character->relationshipScore =
                        static_cast<int16_t>(parameter);
                else
                    character->relationshipScore = static_cast<int16_t>(
                        character->relationshipScore + parameter);
                if(character->relationshipScore < -20)
                    character->relationshipState = 1;
                else if(character->relationshipScore > 20)
                    character->relationshipState = 2;
                else
                    character->relationshipState = 0;
            }
            break;
        }
        case EVENT_OP_EXF_SET_ESCAPE:

            story->escapeEnabled = value == 0 ? 1 : 0;
            break;
        case EVENT_OP_EXF_CHECK_LIFE:
        {
            uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const int64_t sourceRegister =
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A);
            if(sourceRegister > 0 && sourceRegister < STORY_REGISTER_COUNT)
                actorId = static_cast<uint16_t>(
                    registerUnsigned(static_cast<uint16_t>(sourceRegister)));
            const StoryActorState* actor = findStoryActor(
                *story, actorId);
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                actor == NULL ? static_cast<uint32_t>(-1) :
                    static_cast<uint32_t>(actor->currentHealth));
            break;
        }
        case EVENT_OP_EXF_CHECK_MONEY:
        {
            const uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const StoryCharacterState* character =
                characterFor(*story, characterId);
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(character == NULL ?
                    story->money : character->cash));
            break;
        }
        case EVENT_OP_EXF_RANDOM:
        {
            const int32_t minimum = parameter;
            const int32_t maximum = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_EXF_OPTION));
            randomState = randomState * 1664525u + 1013904223u;
            const uint32_t range = maximum >= minimum ?
                static_cast<uint32_t>(maximum - minimum) + 1u : 0u;
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                range == 0 ? static_cast<uint32_t>(minimum) :
                    static_cast<uint32_t>(minimum) + randomState % range);
            break;
        }
        case EVENT_OP_EXF_CHECK_ABILITY:
        {
            static const uint32_t masks[7] =
                { 1u, 2u, 4u, 0x80u, 0x10u, 0x20u, 0x80000000u };
            const int ability = parameter;
            uint16_t foundActor = 0;
            bool found = false;
            for(int i = 1; i <= 3; ++i)
            {
                const uint16_t characterId = story->party[i];
                if(characterId >= STORY_CHARACTER_COUNT ||
                   ability < 0 || ability >= 7)
                    continue;
                if((story->characters[characterId].abilityFlags &
                    masks[ability]) != 0)
                {
                    found = true;
                    foundActor = characterId;
                    break;
                }
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                               found ? 1u : 0u);
            if(found)
                setRegisterOperand(EVENT_OPERAND_SOURCE_REGISTER_A, 0,
                                   foundActor);
            break;
        }
        case EVENT_OP_EXF_WAIT_NPC:
        {
            uint32_t characterId = 0;
            for(int i = 1; i <= 3; ++i)
            {
                if(story->party[i] != 0xffff)
                {
                    characterId = story->party[i];
                    break;
                }
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0, characterId);
            break;
        }
        case EVENT_OP_EXF_CHANGE_TARGET:
        {
            const uint16_t sourceRegister = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const uint16_t targetRegister = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_TARGET_ACTOR_ID));
            const uint16_t sourceId = static_cast<uint16_t>(
                registerUnsigned(sourceRegister));
            const uint16_t targetId = static_cast<uint16_t>(
                registerUnsigned(targetRegister));
            StoryActorState* actor =
                findStoryActor(*story, sourceId, true);
            if(actor != NULL)
                actor->targetActorId = targetId;
            story->targetActorId = targetId;
            break;
        }
        case EVENT_OP_EXF_SET_SUBBTL:
            story->subBattleId = value == 0 ? 0 : 1;
            break;
        case EVENT_OP_EXF_CHECK_PUNCHED_NPC:
        {
            const uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            uint32_t other = 0;
            if(actorId == story->lastPunchedActorId)
                other = story->lastPunchingActorId;
            else if(actorId == story->lastPunchingActorId)
                other = story->lastPunchedActorId;
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0, other);
            break;
        }
        case EVENT_OP_EXF_CHECK_NAKAMAWARE:
        {
            const int64_t firstRegister = commandValue(
                command, EVENT_OPERAND_ACTOR_ID);
            const int64_t secondRegister = commandValue(
                command, EVENT_OPERAND_TARGET_ACTOR_ID);
            const uint16_t firstId = firstRegister >= 0 &&
                firstRegister < STORY_REGISTER_COUNT ?
                static_cast<uint16_t>(registerUnsigned(
                    static_cast<uint16_t>(firstRegister))) : 0xffff;
            const uint16_t secondId = secondRegister >= 0 &&
                secondRegister < STORY_REGISTER_COUNT ?
                static_cast<uint16_t>(registerUnsigned(
                    static_cast<uint16_t>(secondRegister))) : 0xffff;
            StoryActorState* first = findStoryActor(*story, firstId, false);
            StoryActorState* second = findStoryActor(*story, secondId, false);
            uint32_t result = 0;
            if(first != NULL && first->currentHealth < 1)
                result |= 1;
            if(second != NULL && second->currentHealth < 1)
                result |= 2;
            if(result != 0)
            {
                if(first != NULL && first->currentHealth < 1)
                {
                    first->currentHealth = 1;
                    first->defeated = 0;
                }
                if(second != NULL && second->currentHealth < 1)
                {
                    second->currentHealth = 1;
                    second->defeated = 0;
                }
                setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                    result == 1 ? 1u : result == 2 ? 0u : 2u);
            }
            break;
        }
        case EVENT_OP_EXF_CHECK_POS:
        {
            const StoryActorState* actor = findStoryActor(*story,
                static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_ACTOR_ID)));
            bool inside = false;
            if(actor != NULL &&
               !getStorySaveFlag(*story, 0x0df0) &&
               story->stageId == static_cast<uint16_t>(
                   commandValue(command, EVENT_OPERAND_MARK_OR_ENTITY_ID)))
            {
                const float dx = actor->position[0] -
                    commandFloat(command, EVENT_OPERAND_POSITION_X);
                const float dy = actor->position[1] -
                    commandFloat(command, EVENT_OPERAND_POSITION_Y);
                const float dz = actor->position[2] -
                    commandFloat(command, EVENT_OPERAND_SCALE_X);
                const float radius =
                    commandFloat(command, EVENT_OPERAND_SCALE_Y);
                inside = dx * dx + dy * dy + dz * dz < radius * radius;
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                               inside ? 1u : 0u);
            break;
        }
        case EVENT_OP_EXF_WAIT_BATTLE_END:
            if(story->battleActive)
            {
                pc = instructionStart;
                waitFrames = 1;
                return false;
            }
            break;
        case EVENT_OP_EXF_SET_SKILL:
        {
            const uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const uint32_t skill = static_cast<uint32_t>(parameter) & 31u;
            StoryCharacterState* character =
                characterFor(*story, characterId);
            if(character != NULL)
            {
                const uint32_t mask = 1u << skill;
                if(value == 0)
                    character->abilityFlags &= ~mask;
                else
                    character->abilityFlags |= mask;
            }
            break;
        }
        case EVENT_OP_EXF_HIDE_NPC:
        {
            const int slot = parameter;
            if(slot >= 0 && slot < STORY_NPC_HIDE_SLOT_COUNT)
            {
                StoryNpcSlot& npc = story->npcSlots[slot];
                if(value == 0)
                    npc.active = 0;
                else
                {
                    npc.active = 1;
                    npc.actorId = static_cast<uint16_t>(
                        commandValue(command, EVENT_OPERAND_ACTOR_ID));
                    npc.option = static_cast<int32_t>(
                        commandValue(command, EVENT_OPERAND_EXF_OPTION));
                }
            }
            break;
        }
        case EVENT_OP_EXF_CHECK_PARTY:
        {
            uint32_t count = 0;
            for(int i = 0; i < 3; ++i)
                count += story->party[i] != 0xffff;
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                               count > 3 ? 3 : count);
            break;
        }
        case EVENT_OP_EXF_CHECK_DRESS_CATE:
        {
            const int category = static_cast<int>(
                commandValue(command, EVENT_OPERAND_PARTY_MEMBER_ID));
            const int variant = static_cast<int>(
                commandValue(command, EVENT_OPERAND_ITEM_OR_DRESS_ID));
            bool found = false;
            for(int i = 0; i < STORY_DRESS_SLOT_COUNT; ++i)
            {
                const StoryDressEntry& dress = story->dressInventory[i];
                if(!dress.active || dress.quantity == 0)
                    continue;
                const int itemCategory = dress.itemId / 100;
                bool categoryMatch = category == 0 ||
                    itemCategory == category;
                if(category == 0x26)
                    categoryMatch = dress.itemId > 0x09e7 &&
                                    dress.itemId < 0x09fd;
                else if(category == 0x27)
                    categoryMatch = (itemCategory == 1 ||
                                     itemCategory == 3 ||
                                     itemCategory == 6 ||
                                     (itemCategory >= 0x15 &&
                                      itemCategory <= 0x18)) &&
                                    dress.itemId % 100 != 1;
                if(categoryMatch &&
                   (variant < 0 || dress.variant == variant))
                {
                    found = true;
                    break;
                }
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                               found ? 1u : 0u);
            break;
        }
        case EVENT_OP_EXF_MOVE_STAGE:
        {
            uint16_t stageId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_STAGE_ID));
            if(value == 0 && storyMoveStageUsesRegister(currentEventId))
            {
                int64_t registerId =
                    commandValue(command, EVENT_OPERAND_REGISTER_ID);

                if(registerId == 0)
                    registerId = 9;
                if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
                {
                    stageId = static_cast<uint16_t>(
                        registerUnsigned(
                            static_cast<uint16_t>(registerId)));
                }
            }

            story->actorActionControlFlags = 0;
            if(currentEventId == 0x07 ||
               currentEventId == 0x75 ||
               currentEventId == 0x92)
                story->scratch005f1964 = 0;

            queueStoryStageTransition(
                *story, stageId, currentEventId, 0);
            break;
        }
        case EVENT_OP_EXF_CNTL_MES:
            if(value == 0)
                story->messageControlFlags |= 1u;
            else if(value == 1)
                story->messageControlFlags &= ~1u;
            else if(value == 2)
                story->messageControlFlags |= 0x80u;
            else if(value == 3)
                story->messageControlFlags &= ~0x80u;
            else if(value == 4)
                story->messageControlFlags |= 0x200u;
            else if(value == 5)
                story->messageControlFlags &= ~0x200u;
            break;
        case EVENT_OP_EXF_SET_TYPE:
            if((static_cast<uint32_t>(parameter) &
                static_cast<uint32_t>(story->eventTypeMask)) == 0)
            {
                const uint16_t page = static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_PAGE_ID));
                if(page > 0)
                    return jumpToPage(page);
                stop();
            }
            break;
        case EVENT_OP_EXF_PAGE_JUMP_EXIT:
            return jumpToPage(static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_PAGE_ID)));
        case EVENT_OP_EXF_CHECK_SEX:
        {
            uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const int64_t registerId =
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A);
            if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
                characterId = static_cast<uint16_t>(
                    registerUnsigned(static_cast<uint16_t>(registerId)));
            const StoryCharacterState* character =
                characterFor(*story, characterId);
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                character == NULL ?
                    story->customization.genderSelector :
                    character->gender);
            break;
        }
        case EVENT_OP_EXF_SET_MOTION:
        {
            uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const int64_t registerId =
                commandValue(command, EVENT_OPERAND_REGISTER_ID);
            if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
                actorId = static_cast<uint16_t>(
                    registerUnsigned(static_cast<uint16_t>(registerId)));
            StoryActorState* actor = findStoryActor(*story, actorId, false);
            if(actor != NULL)
            {
                actor->motionId = static_cast<uint16_t>(parameter);
                if(value < 2)
                    actor->movementMode = 0;
            }
            break;
        }
        case EVENT_OP_EXF_SET_MARK:
        {
            const int16_t objectiveId = static_cast<int16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const int16_t stageId = static_cast<int16_t>(
                commandValue(command, EVENT_OPERAND_MARK_OR_ENTITY_ID));
            setStoryHudObjective(*story, objectiveId, stageId, true);
            break;
        }
        case EVENT_OP_EXF_CHECK_MONEY2:
        {
            const uint16_t characterId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            const StoryCharacterState* character =
                characterFor(*story, characterId);
            const int32_t cash = character == NULL ?
                story->money : character->cash;
            int32_t amount = value;
            const int64_t sourceRegister =
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A);
            if(sourceRegister > 0 && sourceRegister < STORY_REGISTER_COUNT)
                amount = registerSigned(
                    static_cast<uint16_t>(sourceRegister));
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(cash < amount ? -1 :
                    cash > amount ? 1 : 0));
            break;
        }
        case EVENT_OP_EXF_SET_NPC_MARK:
        {
            const uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            story->targetActorId = value == 0 ? actorId : 0;
            StoryActorState* actor =
                findStoryActor(*story, actorId, value == 0);
            if(actor != NULL)
                actor->markId = value == 0 ?
                    static_cast<uint16_t>(parameter) : 0xffff;
            break;
        }
        case EVENT_OP_EXF_KILL_NPC:
        {
            const int slot = value;
            if(slot >= 0 && slot < STORY_NPC_SLOT_COUNT)
            {
                story->npcSlots[slot].markId = static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_MARK_OR_ENTITY_ID));
                story->npcSlots[slot].active = 0;
            }
            break;
        }
        case EVENT_OP_EXF_SET_NPC_BEF:
        {
            const int slot = static_cast<int>(
                commandValue(command, EVENT_OPERAND_EXF_OPTION));
            if(slot >= 0 && slot < STORY_NPC_BEFORE_RECORD_COUNT)
            {

                StoryNpcBeforeRecord& record =
                    story->npcBeforeRecords[slot];
                record.root = static_cast<int16_t>(parameter);
                record.transitionMode = static_cast<uint8_t>(
                    commandValue(command,
                                 EVENT_OPERAND_MARK_OR_ENTITY_ID));
                record.stageId = static_cast<uint8_t>(
                    commandValue(command,
                                 EVENT_OPERAND_NPC_BEFORE_PARAMETER));
            }
            break;
        }
        case EVENT_OP_EXF_SVFLAG_BRON:
        case EVENT_OP_EXF_SVFLAG_BROFF:
        {
            const uint16_t flag = static_cast<uint16_t>(commandValue(
                command, EVENT_OPERAND_SAVE_FLAG_ID));
            const bool enabled = getStorySaveFlag(*story, flag);
            const bool take = opcode == EVENT_OP_EXF_SVFLAG_BRON ?
                enabled : !enabled;
            if(take)
                return jumpToPage(static_cast<uint16_t>(commandValue(
                    command, EVENT_OPERAND_PAGE_ID)));
            break;
        }
        case EVENT_OP_EXF_GEN_INIT:
            memset(story->generators, 0, sizeof(story->generators));
            ++story->eventGeneration;
            break;
        case EVENT_OP_EXF_GEN_SET:
        {
            const int index = static_cast<int>(
                commandValue(command, EVENT_OPERAND_REGISTER_ID));
            if(index >= 0 && index < STORY_GENERATOR_COUNT)
            {
                StoryGeneratorState& generator = story->generators[index];
                generator = {};

                generator.position[0] =
                    commandFloat(command, EVENT_OPERAND_SCALE_X);
                generator.position[1] =
                    commandFloat(command, EVENT_OPERAND_POSITION_X);
                generator.position[2] =
                    commandFloat(command, EVENT_OPERAND_POSITION_Y);
                generator.headingDegrees =
                    commandFloat(command, EVENT_OPERAND_DURATION);
                generator.remainingCount =
                    static_cast<int16_t>(parameter);
                generator.stageId = static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_MARK_OR_ENTITY_ID));
            }
            break;
        }
        case EVENT_OP_EXF_SET_CALLER:
        {
            randomState = randomState * 1664525u + 1013904223u;
            const uint8_t relationship = randomState % 100 < 50 ? 2 :
                randomState % 100 < 80 ? 0 : 1;
            uint16_t candidates[STORY_CHARACTER_COUNT];
            int candidateCount = 0;
            for(uint16_t i = 0; i < STORY_CHARACTER_COUNT; ++i)
            {
                const StoryCharacterState& character =
                    story->characters[i];
                if((character.characterFlags & STORY_CHARACTER_RECRUITED) != 0 &&
                   character.relationshipState == relationship)
                    candidates[candidateCount++] = i;
            }
            if(candidateCount > 0)
            {
                randomState = randomState * 1664525u + 1013904223u;
                story->callerActorId =
                    candidates[randomState % candidateCount];
                setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                                   story->callerActorId);
                randomState = randomState * 1664525u + 1013904223u;
                setRegisterOperand(EVENT_OPERAND_SOURCE_REGISTER_A, 0,
                                   randomState % 6);
            }
            break;
        }
        case EVENT_OP_EXF_TRANS_REG:
        {
            const uint16_t local = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_REGISTER_ID));
            const uint16_t global = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A));
            if(local < STORY_REGISTER_COUNT && global < STORY_REGISTER_COUNT)
            {
                if(value == 0)
                    story->globalRegisters[global] =
                        story->registers[local];
                else
                    setRegister(local, story->globalRegisters[global],
                                EVENT_VM_S32);
            }
            break;
        }
        case EVENT_OP_EXF_SET_OUTLOW:
            story->outlawValues[0] = value;
            story->outlawValues[1] = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_REGISTER_ID));
            story->outlawValues[2] = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A));
            story->outlawValues[3] = static_cast<int32_t>(
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_B));
            break;
        case EVENT_OP_EXF_EXIT_BATTLE:
            story->battleActive = 0;
            break;
        case EVENT_OP_EXF_RECOVER_NPC:
        {
            uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            if(parameter > 0 && parameter < STORY_REGISTER_COUNT)
                actorId = static_cast<uint16_t>(
                    registerUnsigned(static_cast<uint16_t>(parameter)));
            StoryActorState* actor =
                findStoryActor(*story, actorId, true);
            if(actor != NULL)
            {
                actor->defeated = 0;
                actor->active = 1;
                actor->visible = 1;
                if(actor->currentHealth < 1)
                    actor->currentHealth = 1;
            }
            break;
        }
        case EVENT_OP_EXF_GET_CRIME:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                static_cast<uint32_t>(story->crime & 0xff));
            break;
        case EVENT_OP_EXF_MUL_REG:
        {
            const uint16_t registerId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_REGISTER_ID));
            const int32_t immediate = value;
            const int32_t current = registerSigned(registerId);
            if(parameter == 0)
                setRegister(registerId,
                    static_cast<uint32_t>(current * immediate),
                    EVENT_VM_S32);
            else if(immediate != 0)
                setRegister(registerId,
                    static_cast<uint32_t>(current / immediate),
                    EVENT_VM_S32);
            break;
        }
        case EVENT_OP_EXF_SET_GROW:
            story->growDisabled = value == 0;
            break;
        case EVENT_OP_EXF_STOP_PLAYER:
            if(value == 0)
                story->playerStopped = 0;
            else if(value == 1)
                story->playerStopped = 1;
            break;
        case EVENT_OP_EXF_CAM_MODE:
            story->cameraMode = static_cast<uint8_t>(value == 0 ? 8 : 7);
            break;
        case EVENT_OP_EXF_CAM_POS_START:
            story->cameraPosition.start[0] =
                commandFloat(command, EVENT_OPERAND_WORLD_X);
            story->cameraPosition.start[1] =
                commandFloat(command, EVENT_OPERAND_WORLD_Y);
            story->cameraPosition.start[2] =
                commandFloat(command, EVENT_OPERAND_WORLD_Z);
            story->cameraPosition.active = 1;
            break;
        case EVENT_OP_EXF_CAM_POS_END:
            story->cameraPosition.end[0] =
                commandFloat(command, EVENT_OPERAND_WORLD_X);
            story->cameraPosition.end[1] =
                commandFloat(command, EVENT_OPERAND_WORLD_Y);
            story->cameraPosition.end[2] =
                commandFloat(command, EVENT_OPERAND_WORLD_Z);
            story->cameraPosition.durationFrames = parameter;
            story->cameraPosition.interpolate = value == 0;
            story->cameraPosition.active = 1;
            break;
        case EVENT_OP_EXF_CAM_TRGPOS_START:
            story->cameraTarget.start[0] =
                commandFloat(command, EVENT_OPERAND_WORLD_X);
            story->cameraTarget.start[1] =
                commandFloat(command, EVENT_OPERAND_WORLD_Y);
            story->cameraTarget.start[2] =
                commandFloat(command, EVENT_OPERAND_WORLD_Z);
            story->cameraTarget.active = 1;
            break;
        case EVENT_OP_EXF_CAM_TRGPOS_END:
            story->cameraTarget.end[0] =
                commandFloat(command, EVENT_OPERAND_WORLD_X);
            story->cameraTarget.end[1] =
                commandFloat(command, EVENT_OPERAND_WORLD_Y);
            story->cameraTarget.end[2] =
                commandFloat(command, EVENT_OPERAND_WORLD_Z);
            story->cameraTarget.durationFrames = parameter;
            story->cameraTarget.interpolate = value == 0;
            story->cameraTarget.active = 1;
            break;
        case EVENT_OP_EXF_NPC_ROT:
        {
            StoryActorState* actor = actorFor(*story, EVENT_OPERAND_EXF_VALUE,
                                              EVENT_OPERAND_REGISTER_ID);
            if(actor != NULL)
                actor->headingDegrees =
                    commandFloat(command, EVENT_OPERAND_WORLD_X);
            break;
        }
        case EVENT_OP_EXF_REMOVE:
        {
            uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            if(value == 0)
                story->removedActorId = 0;
            else
            {
                if(value == 2 && actorId < STORY_REGISTER_COUNT)
                    actorId = static_cast<uint16_t>(
                        registerUnsigned(actorId));
                story->removedActorId = actorId;
                StoryActorState* actor =
                    findStoryActor(*story, actorId, false);
                if(actor != NULL)
                    actor->active = 0;
            }
            break;
        }
        case EVENT_OP_EXF_EVSTAGE:
            story->eventStageEntityId = value == 0 ? 0 :
                static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_MARK_OR_ENTITY_ID));
            break;
        case EVENT_OP_EXF_CHECK_ARREST:
        {
            const int mode = value;
            const uint16_t actorOrRegister = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_SOURCE_REGISTER_A));
            if(mode == 0)
            {
                uint16_t arrested[STORY_CHARACTER_COUNT];
                int count = 0;
                for(uint16_t i = 1; i < STORY_CHARACTER_COUNT; ++i)
                {
                    const StoryCharacterState& character =
                        story->characters[i];
                    if((character.characterFlags &
                        STORY_CHARACTER_RECRUITED) != 0 &&
                       (character.battleStatusFlags & 8u) != 0)
                        arrested[count++] = i;
                }
                setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                                   count > 0 ? 1u : 0u);
                uint16_t selected = 0;
                if(count > 0)
                {
                    randomState = randomState * 1664525u + 1013904223u;
                    selected = arrested[randomState % count];
                }
                setRegisterOperand(EVENT_OPERAND_SOURCE_REGISTER_A, 0,
                                   selected);
                story->messageActorId = selected;
            }
            else if(mode == 1 && actorOrRegister < STORY_REGISTER_COUNT)
            {
                const uint16_t characterId = static_cast<uint16_t>(
                    registerUnsigned(actorOrRegister));
                StoryCharacterState* character =
                    characterFor(*story, characterId);
                if(character != NULL)
                    character->battleStatusFlags &= ~8u;
            }
            else if(mode == 2)
            {
                StoryCharacterState* character =
                    characterFor(*story, actorOrRegister);
                if(character != NULL)
                    character->battleStatusFlags |= 8u;
            }
            break;
        }
        case EVENT_OP_EXF_KILL_CHR:
        {
            const uint16_t actorId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ACTOR_ID));
            StoryActorState* actor =
                findStoryActor(*story, actorId, false);
            if(actor != NULL)
            {
                actor->currentHealth = 0;
                actor->defeated = 1;
                actor->active = 0;
            }
            StoryCharacterState* character = characterFor(*story, actorId);
            if(character != NULL)
            {
                character->battleStatusFlags |= 1u;
                character->characterFlags &= ~STORY_CHARACTER_RECRUITED;
            }
            removeStoryPartyMember(*story, actorId);
            break;
        }
        case EVENT_OP_EXF_GET_PARTY_ID:
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0,
                story->party[1] == 0xffff ? 0 : story->party[1]);
            setRegisterOperand(EVENT_OPERAND_SOURCE_REGISTER_A, 0,
                story->party[2] == 0xffff ? 0 : story->party[2]);
            break;
        case EVENT_OP_EXF_BAININ:
        {
            static const uint16_t itemIds[10] =
                { 'Z', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'V', 'X' };
            uint32_t selected = 0;
            if(value == 0)
            {
                int available = 0;
                for(int i = 0; i < 10; ++i)
                    available += host.inventoryCount != NULL &&
                        host.inventoryCount(host.userData, itemIds[i], -1) != 0;
                if(available > 0)
                {
                    randomState = randomState * 1664525u + 1013904223u;
                    int choice = randomState % available;
                    for(int i = 0; i < 10; ++i)
                    {
                        if(host.inventoryCount == NULL ||
                           host.inventoryCount(
                               host.userData, itemIds[i], -1) == 0)
                            continue;
                        if(choice-- == 0)
                        {
                            selected = itemIds[i];
                            break;
                        }
                    }
                }
            }
            else
            {
                randomState = randomState * 1664525u + 1013904223u;
                const int roll = randomState % 100;
                if(roll < 70)
                    selected = itemIds[roll / 10 + 1];
                else if(roll < 75)
                    selected = 'V';
            }
            setRegisterOperand(EVENT_OPERAND_REGISTER_ID, 0, selected);
            break;
        }
        case EVENT_OP_EXF_ADD_DRESS:
        {
            const uint16_t itemId = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_PARTY_MEMBER_ID));
            const uint16_t variant = static_cast<uint16_t>(
                commandValue(command, EVENT_OPERAND_ITEM_OR_DRESS_ID));
            const uint32_t quantity = parameter < 0 ? 0 :
                static_cast<uint32_t>(parameter);
            if(host.addInventory == NULL || quantity == 0 ||
               !host.addInventory(host.userData, itemId, variant,
                    static_cast<uint16_t>(quantity)))
                break;
            StoryDressEntry* freeSlot = NULL;
            StoryDressEntry* entry = NULL;
            for(int i = 0; i < STORY_DRESS_SLOT_COUNT; ++i)
            {
                StoryDressEntry& candidate = story->dressInventory[i];
                if(candidate.active && candidate.itemId == itemId &&
                   candidate.variant == variant)
                {
                    entry = &candidate;
                    break;
                }
                if(!candidate.active && freeSlot == NULL)
                    freeSlot = &candidate;
            }
            if(entry == NULL)
                entry = freeSlot;
            if(entry != NULL)
            {
                if(!entry->active)
                {
                    memset(entry, 0, sizeof(*entry));
                    entry->active = 1;
                    entry->itemId = itemId;
                    entry->variant = variant;
                }
                const uint32_t total = entry->quantity + quantity;
                entry->quantity = static_cast<uint16_t>(
                    total > 0xffff ? 0xffff : total);
            }
            break;
        }
        case EVENT_OP_EXF_CNTL_TEL:
            if(value == 0)
                story->telephoneEnabled = 1;
            else if(value == 1)
                story->telephoneEnabled = 0;
            else
            {
                story->messageActorId = static_cast<uint16_t>(
                    commandValue(command, EVENT_OPERAND_ACTOR_ID));
                story->eventValue = static_cast<uint32_t>(parameter);
            }
            break;
        case EVENT_OP_EXF_SURI:
            story->pickpocketMode = static_cast<uint8_t>(value);
            setStorySaveFlag(*story, 0x0df0, value != 0);
            break;
        case EVENT_OP_EXF_MESTURN:
            story->messageTurnEnabled =
                read16(instructionStart + 6) == 0 ? 0 : 1;
            break;
        default:
            break;
        }
    }

    if(host.command != NULL)
    {
        const EventVmHostResult result = host.command(host.userData, command);
        if(result == EVENT_VM_HOST_FAIL)
        {
            fail("Event external command failed");
            return false;
        }
        if(result == EVENT_VM_HOST_WAIT)
        {
            waitingForHost = true;
            return false;
        }
    }
    return isRunning && !hasFailed && waitFrames <= 0;
}

bool EventVm::loaded(void) const { return hasScript; }
bool EventVm::running(void) const { return isRunning; }
bool EventVm::failed(void) const { return hasFailed; }
uint16_t EventVm::eventId(void) const { return currentEventId; }
uint16_t EventVm::pageId(void) const { return currentPageId; }
uint32_t EventVm::programCounter(void) const { return pc; }
int EventVm::pageCount(void) const { return static_cast<int>(pages.size()); }

const char*
EventVm::activeText(int slot) const
{
    if(validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) < 0 ||
       !textSlots[slot].active || textSlots[slot].messageId >= strings.size())
        return NULL;
    return strings[textSlots[slot].messageId].c_str();
}

const EventVmTextState&
EventVm::text(int slot) const
{
    static const EventVmTextState empty = {};
    return validIndex(slot, EVENT_VM_TEXT_SLOT_COUNT) < 0 ? empty : textSlots[slot];
}

const EventVmTextureState*
EventVm::textureState(int slot) const
{
    return validIndex(slot, EVENT_VM_TEXTURE_SLOT_COUNT) < 0 ?
        NULL : &textureSlots[slot];
}

const EventVmSpriteState&
EventVm::sprite(int slot) const
{
    static const EventVmSpriteState empty = {};
    return validIndex(slot, EVENT_VM_SPRITE_SLOT_COUNT) < 0 ? empty : spriteSlots[slot];
}

const EventVmAudioState& EventVm::audio(void) const { return audioState; }
const char* EventVm::error(void) const { return errorMessage.c_str(); }
