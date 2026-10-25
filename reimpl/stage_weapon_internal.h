#ifndef STAGE_WEAPON_INTERNAL_H
#define STAGE_WEAPON_INTERNAL_H

#include "stage.h"

int16_t cloneFstageWeaponInstance(FstageScene* scene, uint16_t selector,
                                  const rw::Matrix& transform);
rw::Frame* findFstageWeaponFrame(FstageScene* scene, int16_t weaponId);
rw::Matrix detachFstageWeaponFrame(rw::Frame* frame);
void fstageWeaponAngularDelta(const rw::Matrix& current,
                              const rw::Matrix& previous, rw::V3d& angular);

struct FstageWeaponDescriptor
{
    uint16_t selector;
    uint32_t radius;
    uint32_t groundOffset;
    uint8_t breakClass;
    uint32_t position[6];
    uint32_t rotation[6];
};

const FstageWeaponDescriptor FSTAGE_WEAPON_DESCRIPTORS[] =
{
    { 8, 0x40733333, 0x3dcccccd, 2,
      { 0x3f400000, 0xbf59999a, 0x3dcccccd, 0, 0x80000000, 0x80000000 },
      { 0x42b40000, 0x43340000, 0, 0, 0, 0 } },
    { 10, 0x40866666, 0x3e23d70a, 2,
      { 0x3f4ccccd, 0xc0500000, 0x3dcccccd, 0, 0x80000000, 0x80000000 },
      { 0x42b40000, 0x43340000, 0, 0, 0, 0 } },
    { 11, 0x40866666, 0x3e23d70a, 0,
      { 0x3f4ccccd, 0xbf933333, 0xbc23d70a, 0, 0x80000000, 0x80000000 },
      { 0xc2ac0000, 0x43340000, 0xc3310000, 0, 0, 0 } },
    { 13, 0x41280000, 0x3e4ccccd, 2,
      { 0x3f4ccccd, 0xc01ccccd, 0x3dcccccd,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0x42b40000, 0x43340000, 0,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 15, 0x41280000, 0x3dcccccd, 1,
      { 0x3f4ccccd, 0xc00f5c29, 0x3e800000,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0x42b40000, 0x43340000, 0,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 17, 0x41280000, 0x3e4ccccd, 3,
      { 0x3f70a3d7, 0xc0566666, 0xbe75c28f,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0xc2a70000, 0xc3340000, 0x43358000,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 18, 0x40c33333, 0x3e4ccccd, 2,
      { 0x3f19999a, 0xc02ccccd, 0xbe4ccccd,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0xc2a80000, 0xc33c0000, 0xc3340000,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 19, 0x40c33333, 0x3e800000, 3,
      { 0x3f19999a, 0xc02ccccd, 0xbe4ccccd,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0xc2a80000, 0xc33c0000, 0xc3340000,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 21, 0, 0x3e800000, 2,
      { 0x3f800000, 0xbfe66666, 0x3dcccccd,
        0x3f99999a, 0xc0733333, 0xbe4ccccd },
      { 0x42b40000, 0x43340000, 0,
        0xc2bf999a, 0xc27a0000, 0x4341999a } },
    { 22, 0x41100000, 0x3f400000, 0,
      { 0x3dcccccd, 0xc0951eb8, 0x3f99999a, 0, 0, 0 },
      { 0x42b40000, 0x43340000, 0, 0, 0, 0 } },
    { 23, 0x41100000, 0x3f400000, 0,
      { 0xbdcccccd, 0xc0900000, 0x3f99999a, 0, 0, 0 },
      { 0x42b40000, 0x43340000, 0, 0, 0, 0 } }
};

#endif
