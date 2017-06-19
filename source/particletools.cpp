#include "particletools.h"

Bool BuildParticleAssignmentMap(BaseObject *op, ParticleAssignmentMap &map)
{
	if (!op)
		return false;
	
	map.Reset();
	
	UInt count = 0;
	
	ParticleInfo newInfo;
	
	newInfo.role = PARTICLEROLE_PREDATOR;

	map.Put(0, ParticleInfo());
	map.Put(1, ParticleInfo());
	map.Put(2, ParticleInfo());
	map.Put(3, ParticleInfo());
	map.Put(4, newInfo);
	
//	for (BaseObject *currentObject = op->GetDown(); currentObject; currentObject = currentObject->GetNext(), ++count)
//	{
//		ParticleInfo newInfo;
//		
//		newInfo.role = (count % 3 == 0) ? PARTICLEROLE_PREDATOR : PARTICLEROLE_NORMAL;
//		
//		map.Put(count, newInfo);
//	}
	
	return true;
}

ParticleInfo &GetParticleInfo(UInt particleNumber, ParticleAssignmentMap &map)
{
	UInt pid = particleNumber % map.GetCount();
	
	return map.FindEntry(pid)->GetValue();
}
