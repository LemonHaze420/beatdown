#ifndef CHARACTER_SHADOW_H
#define CHARACTER_SHADOW_H

#include <stddef.h>

struct CharacterScene;
struct RwWorldGeometry;
namespace rw { struct World; }

struct CharacterShadowActor
{
    const CharacterScene* scene;
    float x;
    float y;
    float z;
};

bool initializeCharacterShadows(const char* dataArchivePath, rw::World* world);
void terminateCharacterShadows(void);
void captureCharacterShadows(const CharacterShadowActor* actors,
                             size_t actorCount);
void renderCharacterShadows(const RwWorldGeometry& geometry,
                            const CharacterShadowActor* actors,
                            size_t actorCount);

#endif
