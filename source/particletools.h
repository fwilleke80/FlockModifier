#ifndef PARTICLETOOLS_H__
#define PARTICLETOOLS_H__

#include "c4d.h"

enum PARTICLEROLE
{
	PARTICLEROLE_NORMAL,
	PARTICLEROLE_PREDATOR
} ENUM_END_LIST(PARTICLEROLE);

struct ParticleInfo
{
	PARTICLEROLE role;
	
	ParticleInfo() : role(PARTICLEROLE_NORMAL)
	{}
};

typedef maxon::HashMap<UInt, ParticleInfo> ParticleAssignmentMap;

Bool BuildParticleAssignmentMap(BaseObject *op, ParticleAssignmentMap &map);
ParticleInfo &GetParticleInfo(UInt particleNumber, ParticleAssignmentMap &map);

#endif // PARTICLETOOLS_H__
