#ifndef GAME_VECTOR_H
#define GAME_VECTOR_H

#include <math.h>

struct Vec3
{
    float x;
    float y;
    float z;
};

namespace game_detail
{
inline Vec3 vectorDifference(const Vec3& first, const Vec3& second)
{
    return { first.x - second.x, first.y - second.y, first.z - second.z };
}

inline float vectorDot(const Vec3& first, const Vec3& second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

inline Vec3 normalizedVector(const Vec3& vector)
{
    const float length = sqrtf(vectorDot(vector, vector));
    if(length == 0.0f)
        return vector;
    return { vector.x / length, vector.y / length, vector.z / length };
}
}

#endif
