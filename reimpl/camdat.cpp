#include "camdat.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{

struct CamdatDescriptor
{
    uint8_t count;
    uint8_t records[4];
};

const CamdatDescriptor CAMDAT_DESCRIPTORS[CAMDAT_DESCRIPTOR_COUNT] =
{
    { 1, { 0x00, 0x00, 0x00, 0x00 } },
    { 1, { 0x01, 0x00, 0x00, 0x00 } },
    { 3, { 0x02, 0x17, 0x18, 0x00 } },
    { 4, { 0x03, 0x04, 0x05, 0x06 } },
    { 4, { 0x07, 0x08, 0x09, 0x0a } },
    { 1, { 0x0b, 0x00, 0x00, 0x00 } },
    { 2, { 0x0c, 0x0d, 0x00, 0x00 } },
    { 3, { 0x0e, 0x3b, 0x3c, 0x00 } },
    { 3, { 0x0f, 0x10, 0x11, 0x00 } },
    { 3, { 0x12, 0x3d, 0x3e, 0x00 } },
    { 3, { 0x13, 0x3f, 0x40, 0x00 } },
    { 2, { 0x14, 0x19, 0x00, 0x00 } },
    { 1, { 0x15, 0x00, 0x00, 0x00 } },
    { 1, { 0x16, 0x00, 0x00, 0x00 } },
    { 4, { 0x00, 0x1a, 0x1b, 0x1c } },
    { 2, { 0x1d, 0x27, 0x00, 0x00 } },
    { 4, { 0x00, 0x1e, 0x1f, 0x20 } },
    { 1, { 0x21, 0x00, 0x00, 0x00 } },
    { 3, { 0x22, 0x23, 0x24, 0x00 } },
    { 1, { 0x25, 0x00, 0x00, 0x00 } },
    { 1, { 0x26, 0x00, 0x00, 0x00 } },
    { 2, { 0x28, 0x29, 0x00, 0x00 } },
    { 2, { 0x2a, 0x2b, 0x00, 0x00 } },
    { 2, { 0x2c, 0x9a, 0x00, 0x00 } },
    { 3, { 0x10, 0x11, 0x12, 0x00 } },
    { 1, { 0x6c, 0x00, 0x00, 0x00 } },
    { 2, { 0x2e, 0x2f, 0x00, 0x00 } },
    { 2, { 0x30, 0x31, 0x00, 0x00 } },
    { 1, { 0x36, 0x00, 0x00, 0x00 } },
    { 2, { 0x37, 0x38, 0x00, 0x00 } },
    { 1, { 0x39, 0x00, 0x00, 0x00 } },
    { 2, { 0x41, 0x42, 0x00, 0x00 } },
    { 2, { 0x43, 0x44, 0x00, 0x00 } },
    { 2, { 0x45, 0x46, 0x00, 0x00 } },
    { 2, { 0x47, 0x48, 0x00, 0x00 } },
    { 2, { 0x49, 0x4a, 0x00, 0x00 } },
    { 2, { 0x4b, 0x4c, 0x00, 0x00 } },
    { 2, { 0x4d, 0x4e, 0x00, 0x00 } },
    { 2, { 0x4f, 0x50, 0x00, 0x00 } },
    { 1, { 0x53, 0x00, 0x00, 0x00 } },
    { 1, { 0x54, 0x00, 0x00, 0x00 } },
    { 1, { 0x55, 0x00, 0x00, 0x00 } },
    { 1, { 0x56, 0x00, 0x00, 0x00 } },
    { 1, { 0x57, 0x00, 0x00, 0x00 } },
    { 1, { 0x58, 0x00, 0x00, 0x00 } },
    { 1, { 0x59, 0x00, 0x00, 0x00 } },
    { 2, { 0x51, 0x52, 0x00, 0x00 } },
    { 2, { 0x5a, 0x5b, 0x00, 0x00 } },
    { 2, { 0x5c, 0x5d, 0x00, 0x00 } },
    { 3, { 0x5e, 0x5f, 0x60, 0x00 } },
    { 3, { 0x61, 0x62, 0x63, 0x00 } },
    { 2, { 0x64, 0x65, 0x00, 0x00 } },
    { 2, { 0x66, 0x67, 0x00, 0x00 } },
    { 2, { 0x68, 0x69, 0x00, 0x00 } },
    { 2, { 0x6a, 0x6b, 0x00, 0x00 } },
    { 1, { 0x6d, 0x00, 0x00, 0x00 } },
    { 1, { 0x6e, 0x00, 0x00, 0x00 } },
    { 1, { 0x6f, 0x00, 0x00, 0x00 } },
    { 1, { 0x71, 0x00, 0x00, 0x00 } },
    { 1, { 0x72, 0x00, 0x00, 0x00 } },
    { 1, { 0x73, 0x00, 0x00, 0x00 } },
    { 1, { 0x74, 0x00, 0x00, 0x00 } },
    { 1, { 0x75, 0x00, 0x00, 0x00 } },
    { 1, { 0x76, 0x00, 0x00, 0x00 } },
    { 1, { 0x70, 0x00, 0x00, 0x00 } },
    { 2, { 0x77, 0x78, 0x00, 0x00 } },
    { 3, { 0x79, 0x7a, 0x7b, 0x00 } },
    { 2, { 0x7c, 0x7d, 0x00, 0x00 } },
    { 4, { 0x7e, 0x7f, 0x80, 0x81 } },
    { 4, { 0x82, 0x83, 0x84, 0x85 } },
    { 2, { 0x86, 0x87, 0x00, 0x00 } },
    { 2, { 0x88, 0x89, 0x00, 0x00 } },
    { 2, { 0x8a, 0x8b, 0x00, 0x00 } },
    { 1, { 0x7b, 0x00, 0x00, 0x00 } },
    { 1, { 0x8c, 0x00, 0x00, 0x00 } },
    { 1, { 0x8d, 0x00, 0x00, 0x00 } },
    { 1, { 0x8e, 0x00, 0x00, 0x00 } },
    { 1, { 0x8f, 0x00, 0x00, 0x00 } },
    { 2, { 0x11, 0x12, 0x00, 0x00 } },
    { 1, { 0x10, 0x00, 0x00, 0x00 } },
    { 1, { 0x90, 0x00, 0x00, 0x00 } },
    { 1, { 0x91, 0x00, 0x00, 0x00 } },
    { 1, { 0x92, 0x00, 0x00, 0x00 } },
    { 1, { 0x93, 0x00, 0x00, 0x00 } },
    { 1, { 0x94, 0x00, 0x00, 0x00 } },
    { 1, { 0x95, 0x00, 0x00, 0x00 } },
    { 1, { 0x96, 0x00, 0x00, 0x00 } },
    { 1, { 0x97, 0x00, 0x00, 0x00 } },
    { 2, { 0x98, 0x9b, 0x00, 0x00 } },
    { 1, { 0x9c, 0x00, 0x00, 0x00 } },
    { 1, { 0x9d, 0x00, 0x00, 0x00 } },
    { 1, { 0x9e, 0x00, 0x00, 0x00 } },
    { 1, { 0x9f, 0x00, 0x00, 0x00 } },
    { 2, { 0xa0, 0xa1, 0x00, 0x00 } },
    { 2, { 0xa2, 0xa3, 0x00, 0x00 } },
    { 2, { 0xa4, 0xa5, 0x00, 0x00 } },
    { 2, { 0xa6, 0xa7, 0x00, 0x00 } },
    { 2, { 0xa8, 0xa9, 0x00, 0x00 } },
    { 2, { 0xaa, 0xab, 0x00, 0x00 } },
    { 2, { 0xac, 0xad, 0x00, 0x00 } },
    { 2, { 0xae, 0xaf, 0x00, 0x00 } },
    { 2, { 0xb0, 0xb1, 0x00, 0x00 } },
    { 1, { 0xaa, 0x00, 0x00, 0x00 } },
    { 1, { 0xab, 0x00, 0x00, 0x00 } },
    { 1, { 0xab, 0x00, 0x00, 0x00 } },
    { 1, { 0xb5, 0x00, 0x00, 0x00 } },
    { 1, { 0xb7, 0x00, 0x00, 0x00 } },
    { 1, { 0xb6, 0x00, 0x00, 0x00 } },
    { 1, { 0xb8, 0x00, 0x00, 0x00 } },
    { 1, { 0xb9, 0x00, 0x00, 0x00 } },
    { 1, { 0x9a, 0x00, 0x00, 0x00 } },
    { 1, { 0x05, 0x00, 0x00, 0x00 } },
    { 2, { 0x5f, 0x60, 0x00, 0x00 } },
    { 3, { 0x5e, 0x5f, 0x60, 0x00 } },
    { 3, { 0x61, 0x62, 0x63, 0x00 } },
    { 3, { 0x0e, 0x3b, 0x3c, 0x00 } },
    { 3, { 0x0f, 0x10, 0x11, 0x00 } },
    { 3, { 0x12, 0x3d, 0x3e, 0x00 } },
    { 3, { 0x13, 0x3f, 0x40, 0x00 } }
};

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

int16_t read16(const uint8_t* data)
{
    int16_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

CameraFramingVector add(const CameraFramingVector& a,
                        const CameraFramingVector& b)
{
    CameraFramingVector result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

CameraFramingVector subtract(const CameraFramingVector& a,
                             const CameraFramingVector& b)
{
    CameraFramingVector result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}

CameraFramingVector scale(const CameraFramingVector& value, float amount)
{
    CameraFramingVector result = {
        value.x * amount, value.y * amount, value.z * amount
    };
    return result;
}

float length(const CameraFramingVector& value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

void normalize(CameraFramingVector& value)
{
    const float magnitude = length(value);
    if(magnitude > 0.0f)
        value = scale(value, 1.0f / magnitude);
}

CameraFramingVector cross(const CameraFramingVector& a,
                          const CameraFramingVector& b)
{
    CameraFramingVector result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}

CamdatFramingBasis directedBasis(const CameraFramingVector& origin,
                                 const CameraFramingVector& destination)
{
    CamdatFramingBasis basis;
    basis.target = origin;
    basis.forward = subtract(destination, origin);
    normalize(basis.forward);
    basis.right = { basis.forward.z, 0.0f, -basis.forward.x };
    normalize(basis.right);
    basis.up = cross(basis.forward, basis.right);
    normalize(basis.up);
    return basis;
}

void applyOffset(CamdatFramingBasis& basis,
                 const CameraFramingVector& offset)
{
    basis.target = add(
        add(add(basis.target, scale(basis.right, offset.x)),
            scale(basis.up, offset.y)),
        scale(basis.forward, offset.z));
}

void mix(float& value, float target, float amount)
{
    value += (target - value) * amount;
}

void mix(CamdatFraming& value, const CamdatFraming& target, float amount)
{
    mix(value.target.x, target.target.x, amount);
    mix(value.target.y, target.target.y, amount);
    mix(value.target.z, target.target.z, amount);
    mix(value.eye.x, target.eye.x, amount);
    mix(value.eye.y, target.eye.y, amount);
    mix(value.eye.z, target.eye.z, amount);
}

}

bool
CamdatDatabase::parse(const uint8_t* data, size_t size)
{
    clear();
    if(data == NULL || size < 8 || (size & 1) != 0)
        return false;

    const uint32_t count = read32(data);
    const uint64_t tableEnd = 4ull + static_cast<uint64_t>(count) * 4ull;
    if(count == 0 || count > 0x10000 || tableEnd > size)
        return false;

    records_.reserve(count);
    uint32_t previousOffset = static_cast<uint32_t>(tableEnd);
    for(uint32_t i = 0; i < count; ++i)
    {
        const uint32_t offset = read32(data + 4 + i * 4);
        const uint32_t end = i + 1 < count ?
            read32(data + 4 + (i + 1) * 4) : static_cast<uint32_t>(size);
        if((offset & 1) != 0 || (end & 1) != 0 ||
           offset < tableEnd || offset < previousOffset ||
           end <= offset || end > size)
        {
            clear();
            return false;
        }
        CamdatRecord record;
        record.firstWord = offset / 2;
        record.wordCount = (end - offset) / 2;
        records_.push_back(record);
        previousOffset = offset;
    }

    words_.resize(size / 2);
    for(size_t i = 0; i < words_.size(); ++i)
        words_[i] = read16(data + i * 2);
    return true;
}

bool
CamdatDatabase::load(const char* path)
{
    clear();
    if(path == NULL)
        return false;
    FILE* file = fopen(path, "rb");
    if(file == NULL)
        return false;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    const long length = ftell(file);
    if(length <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(length));
    const bool read = fread(&data[0], 1, data.size(), file) == data.size();
    fclose(file);
    return read && parse(&data[0], data.size());
}

void CamdatDatabase::clear(void)
{
    words_.clear();
    records_.clear();
}

int CamdatDatabase::recordCount(void) const
{
    return static_cast<int>(records_.size());
}

const int16_t* CamdatDatabase::recordWords(int index) const
{
    if(index < 0 || index >= recordCount())
        return NULL;
    return &words_[records_[index].firstWord];
}

int CamdatDatabase::recordWordCount(int index) const
{
    return index >= 0 && index < recordCount() ?
        static_cast<int>(records_[index].wordCount) : 0;
}

int
selectCamdatRecordIndex(uint16_t descriptorIndex,
                        uint32_t randomValue,
                        uint32_t repeatRandomValue,
                        uint16_t* previousVariant)
{
    if(descriptorIndex >= CAMDAT_DESCRIPTOR_COUNT || previousVariant == NULL)
        return -1;
    const CamdatDescriptor& descriptor = CAMDAT_DESCRIPTORS[descriptorIndex];
    if(descriptor.count == 0 || descriptor.count > 4)
        return -1;

    uint32_t variant = 0;
    if(descriptor.count > 1)
    {
        variant = randomValue % descriptor.count;
        if(variant == *previousVariant)
        {
            if((repeatRandomValue & 1) != 0)
                variant = (variant + 1) % descriptor.count;
            else
                variant = variant == 0 ? descriptor.count - 1 : variant - 1;
        }
    }
    *previousVariant = static_cast<uint16_t>(variant);
    return descriptor.records[variant];
}

int
selectCamdatRecordIndexWithRandom(uint16_t descriptorIndex,
                        CamdatRandomProvider* randomProvider,
                        void* userData,
                        uint16_t* previousVariant)
{
    if(descriptorIndex >= CAMDAT_DESCRIPTOR_COUNT ||
       randomProvider == NULL || previousVariant == NULL)
    {
        return -1;
    }
    const CamdatDescriptor& descriptor = CAMDAT_DESCRIPTORS[descriptorIndex];
    if(descriptor.count == 0 || descriptor.count > 4)
        return -1;

    uint32_t variant = 0;
    if(descriptor.count > 1)
    {
        variant = randomProvider(userData) % descriptor.count;
        if(variant == *previousVariant)
        {
            const uint32_t repeatRandomValue = randomProvider(userData);
            if((repeatRandomValue & 1) != 0)
                variant = (variant + 1) % descriptor.count;
            else
                variant = variant == 0 ? descriptor.count - 1 : variant - 1;
        }
    }
    *previousVariant = static_cast<uint16_t>(variant);
    return descriptor.records[variant];
}

CamdatRuntime::CamdatRuntime()
    : database_(NULL), words_(NULL), wordCount_(0), recordIndex_(-1),
      cursor_(0), cameraOpcode_(0), subjectMode_(0), orientationMode_(0),
      boneIndex_(0), startRoll_(0.0f), endRoll_(0.0f),
      actorActionFlagCommand_(-1),
      waitRemaining_(0.0f), waitDuration_(0.0f), fieldOfView_(39.0f),
      flags_(0), timeScale_(1.0f), slowMotionEnabled_(false),
      active_(false), finished_(false), desiredInitialized_(false),
      endingPending_(false), dynamicYawPending_(false)
{
    offset_ = { 0.0f, 0.0f, 0.0f };
    start_ = { 0.0f, 0.0f, 0.0f };
    end_ = { 0.0f, 0.0f, 0.0f };
    current_.target = current_.eye = offset_;
    desired_ = current_;
}

bool
CamdatRuntime::start(const CamdatDatabase* database, int recordIndex,
                     const CamdatFraming& currentFraming)
{
    stop();
    if(database == NULL)
        return false;
    const int16_t* const record = database->recordWords(recordIndex);
    const int count = database->recordWordCount(recordIndex);
    if(record == NULL || count < 2 || record[0] < 0 || record[0] > 4)
        return false;

    database_ = database;
    words_ = record;
    wordCount_ = count;
    recordIndex_ = recordIndex;
    cursor_ = 1;
    cameraOpcode_ = static_cast<uint8_t>(record[0]);
    current_ = currentFraming;
    desired_ = currentFraming;
    active_ = cameraOpcode_ != 0;
    finished_ = false;
    return true;
}

void CamdatRuntime::stop(void)
{
    database_ = NULL;
    words_ = NULL;
    wordCount_ = 0;
    recordIndex_ = -1;
    cursor_ = 0;
    cameraOpcode_ = 0;
    subjectMode_ = 0;
    orientationMode_ = 0;
    boneIndex_ = 0;
    actorActionFlagCommand_ = -1;
    offset_ = { 0.0f, 0.0f, 0.0f };
    start_ = { 0.0f, 0.0f, 0.0f };
    end_ = { 0.0f, 0.0f, 0.0f };
    startRoll_ = 0.0f;
    endRoll_ = 0.0f;
    waitRemaining_ = 0.0f;
    waitDuration_ = 0.0f;
    fieldOfView_ = 39.0f;
    flags_ = 0;
    timeScale_ = 1.0f;
    slowMotionEnabled_ = false;
    active_ = false;
    finished_ = false;
    desiredInitialized_ = false;
    endingPending_ = false;
    dynamicYawPending_ = false;
}

bool
CamdatRuntime::execute(float scaledDeltaSeconds)
{
    if(finished_)
        return true;
    bool yield = false;
    while(!yield)
    {
        if(waitRemaining_ > 0.0f)
        {
            waitRemaining_ -= scaledDeltaSeconds;
            if(waitRemaining_ > 0.0f)
                return true;
        }
        if(cursor_ < 0 || cursor_ >= wordCount_)
        {
            finished_ = true;
            return false;
        }

        const int opcode = words_[cursor_];
        static const int OPERAND_COUNTS[24] =
        {
            1, 1, 5, 1, 3, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 0, 1, 1, 1, -1, 1, 1, 0
        };
        if(opcode < 0 || opcode >= 24)
            return false;
        const int operandCount = OPERAND_COUNTS[opcode];
        if(operandCount >= 0 &&
           cursor_ + operandCount >= wordCount_)
        {
            return false;
        }
        switch(opcode)
        {
        case 0:
            if(cursor_ + 1 >= wordCount_)
                return false;
            waitDuration_ = static_cast<float>(words_[cursor_ + 1]) / 60.0f;
            waitRemaining_ = waitDuration_;
            cursor_ += 2;
            yield = true;
            break;
        case 1:
            if(cursor_ + 1 >= wordCount_)
                return false;
            boneIndex_ = static_cast<uint16_t>(words_[cursor_ + 1]);
            cursor_ += 2;
            break;
        case 2:
            if(cursor_ + 5 >= wordCount_)
                return false;
            waitRemaining_ = 0.00001f;
            cursor_ += 6;
            yield = true;
            break;
        case 3:
            cursor_ += 2;
            break;
        case 4:
            if(cursor_ + 3 >= wordCount_)
                return false;
            offset_.x = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            offset_.y = static_cast<float>(words_[cursor_ + 2]) * 0.01f;
            offset_.z = static_cast<float>(words_[cursor_ + 3]) * 0.01f;
            cursor_ += 4;
            break;
        case 5:
            subjectMode_ = static_cast<uint8_t>(words_[cursor_ + 1]);
            cursor_ += 2;
            break;
        case 6:
            orientationMode_ = static_cast<uint8_t>(words_[cursor_ + 1]);
            cursor_ += 2;
            break;
        case 7:
            start_.distance = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 8:
            start_.pitchDegrees = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 9:
            start_.yawDegrees = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 10:
            startRoll_ = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 11:
            end_.distance = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            endingPending_ = true;
            cursor_ += 2;
            break;
        case 12:
            end_.pitchDegrees = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            endingPending_ = true;
            cursor_ += 2;
            break;
        case 13:
            end_.yawDegrees = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            endingPending_ = true;
            cursor_ += 2;
            break;
        case 14:
            endRoll_ = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            endingPending_ = true;
            cursor_ += 2;
            break;
        case 15:

            cursor_ += 2;
            break;
        case 16:

            dynamicYawPending_ = true;
            ++cursor_;
            break;
        case 17:
            fieldOfView_ = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 18:
            actorActionFlagCommand_ = words_[cursor_ + 1] == 0 ? 1 : 0;
            cursor_ += 2;
            break;
        case 19:
            cursor_ += 2;
            break;
        case 20:
            ++cursor_;
            while(cursor_ < wordCount_ && words_[cursor_] != -1)
            {
                flags_ |= 1u <<
                    (static_cast<uint32_t>(words_[cursor_]) & 31u);
                ++cursor_;
            }
            if(cursor_ >= wordCount_)
                return false;
            ++cursor_;
            break;
        case 21:
            slowMotionEnabled_ = words_[cursor_ + 1] != 0;
            cursor_ += 2;
            break;
        case 22:
            timeScale_ = static_cast<float>(words_[cursor_ + 1]) * 0.01f;
            cursor_ += 2;
            break;
        case 23:
            finished_ = true;
            yield = true;
            break;
        default:
            return false;
        }
        if(cursor_ > wordCount_)
            return false;
    }
    return true;
}

bool
CamdatRuntime::resolveSubjectBasis(CamdatBasisProvider* provider,
                                   void* userData,
                                   CamdatFramingBasis* basis) const
{
    if(provider == NULL || basis == NULL)
        return false;
    CamdatFramingBasis actor;
    CamdatFramingBasis linked;
    if(!provider(userData, false, 0, &actor))
        return false;

    if(subjectMode_ == 2 || orientationMode_ != 0 || subjectMode_ == 1)
    {
        if(!provider(userData, true, 0, &linked))
            return false;
    }

    if(subjectMode_ == 2)
    {
        *basis = computeCamdatPairFramingBasis(
            actor.target, linked.target, offset_);
        return true;
    }
    if(subjectMode_ == 3)
        return false;

    const bool linkedSubject = subjectMode_ == 1;
    const CamdatFramingBasis& subject = linkedSubject ? linked : actor;
    if(orientationMode_ == 0)
        *basis = subject;
    else
    {
        const CamdatFramingBasis& other = linkedSubject ? actor : linked;
        *basis = directedBasis(subject.target, other.target);
    }
    applyOffset(*basis, offset_);
    return true;
}

bool
CamdatRuntime::computeDesired(CamdatBasisProvider* provider, void* userData,
                              bool ending, CamdatFraming* framing) const
{
    if(framing == NULL)
        return false;
    if(cameraOpcode_ == 1 || cameraOpcode_ == 4)
    {
        CamdatFramingBasis basis;
        if(!resolveSubjectBasis(provider, userData, &basis))
            return false;
        *framing = computeCamdatOpcode1And4Framing(
            basis, ending ? end_ : start_);
        return true;
    }
    if(cameraOpcode_ == 2)
    {
        CamdatFramingBasis bone;
        if(provider == NULL ||
           !provider(userData, false, boneIndex_, &bone))
        {
            return false;
        }
        const float elapsed = waitDuration_ == 0.0f ? 1.0f :
            (waitDuration_ - waitRemaining_) / waitDuration_;
        *framing = computeCamdatOpcode2Framing(bone, start_, end_, elapsed);
        return true;
    }
    return false;
}

bool
CamdatRuntime::update(float scaledDeltaSeconds,
                      CamdatBasisProvider* basisProvider, void* userData,
                      CamdatFraming* framing)
{
    if(!active_ || framing == NULL)
        return false;
    if(scaledDeltaSeconds < 0.0f)
        scaledDeltaSeconds = 0.0f;
    if(!execute(scaledDeltaSeconds))
    {
        stop();
        return false;
    }

    if(dynamicYawPending_ && basisProvider != NULL)
    {
        CamdatFramingBasis actor;
        CamdatFramingBasis linked;
        if(basisProvider(userData, false, 0, &actor) &&
           basisProvider(userData, true, 0, &linked))
        {
            float yaw = (atan2f(actor.target.x - current_.eye.x,
                                actor.target.z - current_.eye.z) -
                         atan2f(actor.target.x - linked.target.x,
                                actor.target.z - linked.target.z)) *
                        (180.0f / 3.1415927f);
            if(yaw > 180.0f)
                yaw -= 360.0f;
            else if(yaw < -180.0f)
                yaw += 360.0f;
            if(yaw >= 0.0f && yaw < 70.0f)
                yaw = 70.0f;
            else if(yaw < 0.0f && yaw > -70.0f)
                yaw = -70.0f;
            end_.yawDegrees = yaw;
            dynamicYawPending_ = false;
        }
    }

    if(cameraOpcode_ == 2)
    {
        if(computeDesired(basisProvider, userData, false, &current_))
        {
            desired_ = current_;
            desiredInitialized_ = true;
        }
    }
    else if(!desiredInitialized_)
    {
        if(computeDesired(basisProvider, userData, false, &desired_))
        {
            current_ = desired_;
            desiredInitialized_ = true;
        }
    }
    else if(endingPending_)
    {
        if(computeDesired(basisProvider, userData, true, &desired_))
            endingPending_ = false;
    }

    if(!desiredInitialized_)
        return false;
    if(cameraOpcode_ != 2 && waitDuration_ != 0.0f)
        mix(current_, desired_, scaledDeltaSeconds / waitDuration_);
    else
        current_ = desired_;
    *framing = current_;
    return true;
}

bool CamdatRuntime::active(void) const { return active_; }
bool CamdatRuntime::finished(void) const { return finished_; }
int CamdatRuntime::recordIndex(void) const { return recordIndex_; }
uint8_t CamdatRuntime::cameraOpcode(void) const { return cameraOpcode_; }
float CamdatRuntime::fieldOfView(void) const { return fieldOfView_; }
uint32_t CamdatRuntime::stateFlags(void) const { return flags_; }

bool CamdatRuntime::consumeActorActionFlag(bool* enabled)
{
    if(enabled == NULL || actorActionFlagCommand_ < 0)
        return false;
    *enabled = actorActionFlagCommand_ != 0;
    actorActionFlagCommand_ = -1;
    return true;
}
float CamdatRuntime::timeScale(void) const { return timeScale_; }
bool CamdatRuntime::slowMotionEnabled(void) const
{
    return slowMotionEnabled_;
}
