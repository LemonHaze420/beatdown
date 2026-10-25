#include "character/character.h"
#include "character/internal.h"
#include <rw.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

using namespace character_detail;

namespace
{
int32_t g_characterMorphGeometryOffset = -1;

void*
constructCharacterMorphGeometry(void* object, rw::int32 offset, rw::int32)
{
    CharacterMorphGeometryData* data =
        PLUGINOFFSET(CharacterMorphGeometryData, object, offset);
    data->count = 0;
    data->callback = NULL;
    data->interpolators = NULL;
    data->baseVertices = NULL;
    data->baseNormals = NULL;
    data->baseVertexCount = 0;
    return object;
}

void*
destroyCharacterMorphGeometry(void* object, rw::int32 offset, rw::int32)
{
    CharacterMorphGeometryData* data =
        PLUGINOFFSET(CharacterMorphGeometryData, object, offset);
    delete[] data->interpolators;
    delete[] data->baseVertices;
    delete[] data->baseNormals;
    data->interpolators = NULL;
    data->baseVertices = NULL;
    data->baseNormals = NULL;
    data->count = 0;
    data->baseVertexCount = 0;
    return object;
}

void*
copyCharacterMorphGeometry(void* destination, void* source, rw::int32 offset,
                           rw::int32)
{
    CharacterMorphGeometryData* to =
        PLUGINOFFSET(CharacterMorphGeometryData, destination, offset);
    const CharacterMorphGeometryData* from =
        PLUGINOFFSET(CharacterMorphGeometryData, source, offset);
    to->callback = from->callback;
    to->count = from->count;
    if(from->count > 0 && from->interpolators != NULL)
    {
        to->interpolators = new(std::nothrow)
            CharacterMorphInterpolator[from->count];
        if(to->interpolators != NULL)
            memcpy(to->interpolators, from->interpolators,
                   sizeof(CharacterMorphInterpolator) * from->count);
        else
            to->count = 0;
    }
    if(from->baseVertexCount > 0 && from->baseVertices != NULL)
    {
        to->baseVertices = new(std::nothrow) rw::V3d[from->baseVertexCount];
        if(to->baseVertices != NULL)
        {
            memcpy(to->baseVertices, from->baseVertices,
                   sizeof(rw::V3d) * from->baseVertexCount);
            to->baseVertexCount = from->baseVertexCount;
        }
    }
    if(from->baseVertexCount > 0 && from->baseNormals != NULL)
    {
        to->baseNormals = new(std::nothrow) rw::V3d[from->baseVertexCount];
        if(to->baseNormals != NULL)
            memcpy(to->baseNormals, from->baseNormals,
                   sizeof(rw::V3d) * from->baseVertexCount);
    }
    return destination;
}

rw::Stream*
readCharacterMorphGeometry(rw::Stream* stream, rw::int32 length, void* object,
                           rw::int32 offset, rw::int32)
{
    CharacterMorphGeometryData* data =
        PLUGINOFFSET(CharacterMorphGeometryData, object, offset);
    if(length < 4)
        return NULL;
    const int32_t count = stream->readI32();
    if(count < 0 || count > (length - 4) / 20)
        return NULL;
    delete[] data->interpolators;
    data->interpolators = NULL;
    data->count = 0;
    if(count > 0)
    {
        data->interpolators = new(std::nothrow)
            CharacterMorphInterpolator[count];
        if(data->interpolators == NULL)
            return NULL;
    }
    for(int32_t i = 0; i < count; ++i)
    {
        CharacterMorphInterpolator& interpolator = data->interpolators[i];
        interpolator.flags = stream->readI32();
        interpolator.startTarget = stream->readI16();
        interpolator.endTarget = stream->readI16();
        interpolator.duration = stream->readF32();
        interpolator.reciprocalDuration = stream->readF32();
        interpolator.next = stream->readI32();
        if(interpolator.next < 0 || interpolator.next >= count)
            interpolator.next = i;
        if(interpolator.duration > 0.0f)
            interpolator.reciprocalDuration = 1.0f /
                                              interpolator.duration;
        else
            interpolator.reciprocalDuration = 0.0f;
    }
    data->count = count;
    const int32_t remaining = length - 4 - count * 20;
    if(remaining > 0)
        stream->seek(remaining);
    return stream;
}

rw::Stream*
writeCharacterMorphGeometry(rw::Stream* stream, rw::int32, void* object,
                            rw::int32 offset, rw::int32)
{
    const CharacterMorphGeometryData* data =
        PLUGINOFFSET(CharacterMorphGeometryData, object, offset);
    stream->writeI32(data->count);
    for(int32_t i = 0; i < data->count; ++i)
    {
        const CharacterMorphInterpolator& interpolator =
            data->interpolators[i];
        stream->writeI32(interpolator.flags);
        stream->writeI16(interpolator.startTarget);
        stream->writeI16(interpolator.endTarget);
        stream->writeF32(interpolator.duration);
        stream->writeF32(interpolator.reciprocalDuration);
        stream->writeI32(interpolator.next);
    }
    return stream;
}

rw::int32
characterMorphGeometryStreamSize(void* object, rw::int32 offset, rw::int32)
{
    const CharacterMorphGeometryData* data =
        PLUGINOFFSET(CharacterMorphGeometryData, object, offset);
    return data->count > 0 ? 4 + data->count * 20 : 0;
}

}

namespace character_detail
{
CharacterMorphGeometryData*
characterMorphGeometryData(rw::Geometry* geometry)
{
    if(geometry == NULL || g_characterMorphGeometryOffset < 0)
        return NULL;
    return PLUGINOFFSET(CharacterMorphGeometryData, geometry,
                        g_characterMorphGeometryOffset);
}

const CharacterMorphGeometryData*
characterMorphGeometryData(const rw::Geometry* geometry)
{
    return characterMorphGeometryData(
        const_cast<rw::Geometry*>(geometry));
}

}

rw::Image* decodeCharacterImageChunk(const uint8_t* chunk, uint32_t chunkLength)
{
    if(chunkLength < 28 || read32(chunk) != CHUNK_STRUCT || read32(chunk + 4) != 16)
        return NULL;

    const uint32_t width = read32(chunk + 12);
    const uint32_t height = read32(chunk + 16);
    const uint32_t depth = read32(chunk + 20);
    const uint32_t pitch = read32(chunk + 24);
    if(width == 0 || height == 0 || width > 4096 || height > 4096 ||
       (depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32))
        return NULL;

    const uint64_t pixelSize = static_cast<uint64_t>(pitch) * height;
    const uint32_t paletteSize = depth == 4 ? 64 : depth == 8 ? 1024 : 0;
    if(28 + pixelSize + paletteSize != chunkLength)
        return NULL;

    rw::Image* image = rw::Image::create(
        static_cast<int32_t>(width), static_cast<int32_t>(height),
        static_cast<int32_t>(depth));
    if(image == NULL)
        return NULL;
    image->allocate();
    if(image->pixels == NULL || (paletteSize != 0 && image->palette == NULL) ||
       static_cast<uint32_t>(image->stride) != pitch)
    {
        image->destroy();
        return NULL;
    }

    memcpy(image->pixels, chunk + 28, static_cast<size_t>(pixelSize));
    if(paletteSize != 0)
    {
        memcpy(image->palette, chunk + 28 + pixelSize, paletteSize);
        const uint32_t colorCount = depth == 4 ? 16 : 256;
        for(uint32_t i = 0; i < colorCount; i++)
            image->palette[i * 4 + 3] =
                expandPs2Alpha(image->palette[i * 4 + 3]);
    }
    else if(depth == 32)
    {
        for(uint32_t y = 0; y < height; ++y)
        {
            uint8_t* row = image->pixels + y * image->stride;
            for(uint32_t x = 0; x < width; ++x)
                row[x * 4 + 3] = expandPs2Alpha(row[x * 4 + 3]);
        }
    }

    image->convertTo32();

    return image;
}

void prepareCharacterRenderClump(rw::Clump* clump)
{
    rw::HAnimHierarchy* hierarchy = rw::HAnimHierarchy::get(clump);
    FORLIST(atomicLink, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
        rw::Geometry* geometry = atomic->geometry;
        if((geometry->flags & rw::Geometry::NATIVE) != 0)
            atomic->uninstance();
        if(rw::Skin::get(geometry) != NULL)
        {
            rw::Skin::setPipeline(atomic, 1);
            if(hierarchy != NULL)
                rw::Skin::setHierarchy(atomic, hierarchy);
            rw::Skin* skin = rw::Skin::get(geometry);
            printf("skin verts=%d bones=%d used=%d limit=%d meshes=%d rle=%d indices=",
                   geometry->numVertices, skin->numBones, skin->numUsedBones,
                   skin->boneLimit, skin->numMeshes, skin->rleSize);
            bool used[256] = {};
            for(int32_t vertex = 0; vertex < geometry->numVertices; ++vertex)
                for(int weight = 0; weight < 4; ++weight)
                    if(skin->weights[vertex * 4 + weight] != 0.0f)
                        used[skin->indices[vertex * 4 + weight]] = true;
            float minimumWeightSum = 1000.0f;
            float maximumWeightSum = -1000.0f;
            for(int32_t vertex = 0; vertex < geometry->numVertices; ++vertex)
            {
                float sum = 0.0f;
                for(int weight = 0; weight < 4; ++weight)
                    sum += skin->weights[vertex * 4 + weight];
                minimumWeightSum = fminf(minimumWeightSum, sum);
                maximumWeightSum = fmaxf(maximumWeightSum, sum);
            }
            printf(" weight-sums=%.6f..%.6f ", minimumWeightSum,
                   maximumWeightSum);
            for(int bone = 0; bone < skin->numBones; ++bone)
                if(used[bone])
                    printf("%d,", bone);
            printf("\n  remap=");
            for(int bone = 0; bone < skin->numBones; ++bone)
                printf("%d,", skin->remapIndices != NULL ?
                       skin->remapIndices[bone] : bone);
            printf("\n  groups=");
            for(int mesh = 0; mesh < skin->numMeshes; ++mesh)
                printf("%d:%d,", skin->rleCount[mesh].start,
                       skin->rleCount[mesh].size);
            printf("\n  runs=");
            for(int run = 0; run < skin->rleSize; ++run)
                printf("%d:%d,", skin->rle[run].startbone,
                       skin->rle[run].n);
            printf("\n");
        }
        else
            atomic->pipeline = NULL;
        atomic->setRenderCB(NULL);
        geometry->flags |= rw::Geometry::LIGHT;
        for(int32_t i = 0; i < geometry->matList.numMaterials; i++)
        {
            rw::Material* material = geometry->matList.materials[i];
            material->color = rw::makeRGBA(255, 255, 255, 255);
            material->surfaceProps.ambient = 1.0f;
            material->surfaceProps.diffuse = 1.0f;
        }
    }
}

void
registerCharacterMorphPlugin(void)
{
    if(g_characterMorphGeometryOffset >= 0)
        return;
    const int32_t offset = rw::Geometry::registerPlugin(
        sizeof(CharacterMorphGeometryData), PLUGIN_MORPH,
        constructCharacterMorphGeometry, destroyCharacterMorphGeometry,
        copyCharacterMorphGeometry);
    if(offset < 0)
    {
        printf("ERROR: Could not register character morph plugin.\n");
        return;
    }
    g_characterMorphGeometryOffset = offset;
    if(rw::Geometry::registerPluginStream(
           PLUGIN_MORPH, readCharacterMorphGeometry,
           writeCharacterMorphGeometry,
           characterMorphGeometryStreamSize) < 0)
        printf("ERROR: Could not register character morph stream.\n");
}
