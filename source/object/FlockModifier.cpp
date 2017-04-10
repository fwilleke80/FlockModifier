#include "c4d.h"
#include "c4d_symbols.h"
#include "oflock.h"
#include "lib_collider.h"
#include "customgui_inexclude.h"
#include "Oflocktarget.h"
#include "Oflockrepeller.h"
#include "helpers.h"
#include "main.h"


class FlockModifier : public ObjectData
{
	INSTANCEOF(FlockModifier, ObjectData)
	
public:
	FlockModifier()
	{ }
	
	~FlockModifier()
	{ }

public:
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
	virtual void ModifyParticles(BaseObject *op, Particle *pp, BaseParticle *ss, Int32 pcnt, Float diff);
	
	static NodeData *Alloc(void)
	{
		return NewObjClear(FlockModifier);
	}
	
private:
	AutoFree<GeRayCollider> _collider;
};


/****************************************************************************
 * Initialize attributes
 ****************************************************************************/
Bool FlockModifier::Init(GeListNode *node)
{
	if (!node)
		return false;

	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;

	bc->SetFloat(OFLOCK_WEIGHT, 1.0);
	bc->SetFloat(OFLOCK_CENTER_WEIGHT, 0.2);
	bc->SetFloat(OFLOCK_NEIGHBORDIST_WEIGHT, 0.1);
	bc->SetFloat(OFLOCK_NEIGHBORDIST_DIST, 10.0);
	bc->GetFloat(OFLOCK_MATCHVEL_WEIGHT, 0.1);
	bc->GetFloat(OFLOCK_TARGET_WEIGHT, 0.15);
	bc->GetFloat(OFLOCK_LEVELFLIGHT_WEIGHT, 0.3);
	bc->GetFloat(OFLOCK_SPEED_WEIGHT, 0.2);
	bc->SetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	bc->SetFloat(OFLOCK_SPEED_MIN, 0.0);
	bc->SetFloat(OFLOCK_SPEED_MAX, 200.0);
	bc->SetFloat(OFLOCK_SIGHT, 50.0);
	bc->SetFloat(OFLOCK_AVOIDGEO_WEIGHT, 1.0);
	bc->SetFloat(OFLOCK_AVOIDGEO_DIST, 50.0);
	bc->SetFloat(OFLOCK_AVOIDGEO_MODE, OFLOCK_AVOIDGEO_MODE_SOFT);
	bc->SetFloat(OFLOCK_TURBULENCE_WEIGHT, 0.5);
	bc->SetFloat(OFLOCK_TURBULENCE_FREQUENCY, 2.0);
	bc->SetFloat(OFLOCK_TURBULENCE_SCALE, 2.0);
	bc->SetFloat(OFLOCK_REPELL_WEIGHT, 1.0);

	return SUPER::Init(node);
}

/****************************************************************************
 * Enable/Disable attributes
 ****************************************************************************/
Bool FlockModifier::GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc)
{
	if (!node)
		return false;
	
	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;
	
	switch (id[0].id)
	{
		case OFLOCK_AVOIDGEO_WEIGHT:
			return bc->GetInt32(OFLOCK_AVOIDGEO_MODE) == OFLOCK_AVOIDGEO_MODE_SOFT;
			break;
			
		case OFLOCK_SPEED_WEIGHT:
			return bc->GetInt32(OFLOCK_SPEED_MODE) == OFLOCK_SPEED_MODE_SOFT;
			break;
	}
	
	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

/****************************************************************************
 * Simulate flock
 ****************************************************************************/
void FlockModifier::ModifyParticles(BaseObject *op, Particle *pp, BaseParticle *ss, Int32 pcnt, Float diff)
{
	if (!op || !pp || !ss)
		return;

	Int32 i,j,n;

	BaseContainer* bc = op->GetDataInstance();
	BaseDocument* doc = op->GetDocument();
	
	if (!bc || !doc)
		return;

	// Variables
	Int32 lCount = pcnt - 1;
	Vector vParticleDir;

	// Overall weight
	Float rWeight = bc->GetFloat(OFLOCK_WEIGHT, 1.0);

	// Range of sight
	Float rNeighborDist = 0.0;
	Float rNeighborSightRadius = bc->GetFloat(OFLOCK_SIGHT, 0.0);
	rNeighborSightRadius *= rNeighborSightRadius;		// Square

	// Flock Center
	Vector vCenterflockDir;
	Float rCenterflockWeight = bc->GetFloat(OFLOCK_CENTER_WEIGHT, 0.0) * 0.1;

	// Neighbor Distance
	Vector vNeighborDiff(DC);
	Vector vNeighborDir;
	Float rNeighborWeight = bc->GetFloat(OFLOCK_NEIGHBORDIST_WEIGHT, 0.0) * 0.1;
	Float rNeighborMinDist = bc->GetFloat(OFLOCK_NEIGHBORDIST_DIST, 0.0);

	// Match Velocity
	Vector vMatchVelocityDir;
	Float rMatchVelocityWeight = bc->GetFloat(OFLOCK_MATCHVEL_WEIGHT, 0.0) * 0.1;

	// Target
	Float rTargetGlobalWeight = bc->GetFloat(OFLOCK_TARGET_WEIGHT, 0.0) * 0.1;
	InExcludeData* inexTarget = (InExcludeData*)bc->GetCustomDataType(OFLOCK_TARGET_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 lTargetCount = 0;
	if (inexTarget) lTargetCount = inexTarget->GetObjectCount();
	maxon::BaseArray<TargetData>targetData;

	// Level flight
	Float rLevelFlightWeight = bc->GetFloat(OFLOCK_LEVELFLIGHT_WEIGHT, 0.0);

	// Speed limits
	Float rSpeed = 0.0;
	Int32 lSpeedMode = bc->GetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	Float rSpeedWeight = bc->GetFloat(OFLOCK_SPEED_WEIGHT, 0.0);
	Float rSpeedMin = bc->GetFloat(OFLOCK_SPEED_MIN, 0.0) * diff;
	Float rSpeedMax = bc->GetFloat(OFLOCK_SPEED_MAX, 100.0) * diff;
	Float rSpeedRatio;

	// Geometry avoidance
	Int32 lAvoidGeoMode = bc->GetInt32(OFLOCK_AVOIDGEO_MODE, 1);
	Float rAvoidGeoWeight = bc->GetFloat(OFLOCK_AVOIDGEO_WEIGHT, 0.0);
	Float rAvoidGeoDist = bc->GetFloat(OFLOCK_AVOIDGEO_DIST, 0.0);
	Vector vAvoidGeoDir;
	BaseObject* boAvoidGeoLink = bc->GetObjectLink(OFLOCK_AVOIDGEO_LINK, doc);
	Matrix mAvoidGeo, mAvoidGeoI;
	Float rAvoidGeoMixval;

	// Turbulence
	Float rTurbulenceWeight = bc->GetFloat(OFLOCK_TURBULENCE_WEIGHT, 0.0) * 10.0;
	Float rTurbulenceTime = doc->GetTime().Get() * bc->GetFloat(OFLOCK_TURBULENCE_FREQUENCY, 1.0);
	Float rTurbulenceScale = 0.1 / ClampValue(bc->GetFloat(OFLOCK_TURBULENCE_SCALE, 1.0), 0.0001, MAXRANGE_FLOAT);
	Vector rTurbulenceAdd1;
	Vector rTurbulenceAdd2;

	// Repell
	Float rRepellGlobalWeight = bc->GetFloat(OFLOCK_REPELL_WEIGHT, 0.0);
	InExcludeData* inexRepell = (InExcludeData*)bc->GetCustomDataType(OFLOCK_REPELL_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 lRepellCount = 0;
	if (inexRepell) lRepellCount = inexRepell->GetObjectCount();
	maxon::BaseArray<RepellerData>repellerData;


	/* ------------------- Set rule mask & prepare some data --------------------------- */ 
	RULEFLAGS rulemask = RULEFLAGS_NONE;

	// Center Flock
	if (!CompareFloatTolerant(rCenterflockWeight, 0.0))
	{
		rulemask |= RULEFLAGS_CENTER;
	}

	// Neighbor Distance
	if (!CompareFloatTolerant(rNeighborWeight, 0.0))
	{
		rulemask |= RULEFLAGS_NEIGHBORDIST;
		rNeighborMinDist *= rNeighborMinDist;		// Square
	}

	// Match Flock Velocity
	if (!CompareFloatTolerant(rMatchVelocityWeight, 0.0))
	{
		rulemask |= RULEFLAGS_MATCHVELO;
	}

	// Level Flight
	if (!CompareFloatTolerant(rLevelFlightWeight, 0.0))
	{
		rulemask |= RULEFLAGS_LEVELFLIGHT;
	}

	// Turbulence
	if (!CompareFloatTolerant(rTurbulenceWeight, 0.0))
	{
		rulemask |= RULEFLAGS_TURBULENCE;
		rTurbulenceAdd1 = Vector(PI * 1000.0);
		rTurbulenceAdd2 = Vector(-PI05 * 1000.0);
	}

	// Speed Limit
	if (((lSpeedMode == OFLOCK_SPEED_MODE_SOFT && !CompareFloatTolerant(rSpeedWeight, 0.0)) || lSpeedMode == OFLOCK_SPEED_MODE_HARD))
	{
		rulemask |= RULEFLAGS_SPEEDLIMIT;
		rSpeedMin *= rSpeedMin;   // Square
		rSpeedMax *= rSpeedMax;   // Square
	}

	// Target
	if (!CompareFloatTolerant(rTargetGlobalWeight, 0.0) && inexTarget && lTargetCount > 0)
	{
		rulemask |= RULEFLAGS_TARGET;

		BaseObject *opListItem = nullptr;
		BaseContainer *tc = nullptr;

		targetData.Flush();
		for (i = 0; i < lTargetCount; ++i)
		{
			opListItem = (BaseObject*)inexTarget->ObjectFromIndex(doc, i);
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKTARGET_ENABLED))
				{
					TargetData tdata((Float32)tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0),
					                 (Float32)tc->GetFloat(OFLOCKTARGET_RADIUS, 0.0),
					                 tc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, true),
					                 opListItem->GetMg().off);
					tdata._radius *= tdata._radius;   // Square
					targetData.Append(tdata);
				}
			}
		}
	}

	// Avoid Geometry
	if (((lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_SOFT && !CompareFloatTolerant(rAvoidGeoWeight, 0.0)) || lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_HARD) && !CompareFloatTolerant(rAvoidGeoDist, 0.0) && boAvoidGeoLink && boAvoidGeoLink->GetType() == Opolygon && ToPoly(boAvoidGeoLink)->GetPolygonCount() > 0)
	{
		rulemask |= RULEFLAGS_AVOIDGEO;
		
		// Lazy-alloc collider
		if (!_collider)
		{
			_collider.Set(GeRayCollider::Alloc());
			if (!_collider)
				return;
		}
		
		_collider->Init(boAvoidGeoLink, boAvoidGeoLink->GetDirty(DIRTYFLAGS_CACHE|DIRTYFLAGS_MATRIX|DIRTYFLAGS_DATA));
		mAvoidGeo = boAvoidGeoLink->GetMg();
		mAvoidGeoI = ~mAvoidGeo;
	}

	// Repelling
	if (!CompareFloatTolerant(rRepellGlobalWeight, 0.0) && inexRepell && lRepellCount > 0)
	{
		rulemask |= RULEFLAGS_REPELL;

		BaseObject* opListItem = nullptr;
		BaseContainer* tc = nullptr;

		repellerData.Flush();
		for (i = 0; i < lRepellCount; ++i)
		{
			opListItem = (BaseObject*)inexRepell->ObjectFromIndex(doc, i);
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKREPELLER_ENABLED))
				{
					RepellerData rdata((Float32)tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0),
					                   (Float32)tc->GetFloat(OFLOCKREPELLER_RADIUS, 0.0),
					                   opListItem->GetMg().off);
					rdata._radius *= rdata._radius;   // Square
					repellerData.Append(rdata);
				}
			}
		}
	}


	// Iterate particles
	for (i = 0; i < pcnt; ++i)
	{
		// Skip unwanted particles
		if (!(pp[i].bits & PARTICLEFLAGS_VISIBLE))
			continue;

		// Reset values
		vParticleDir = vCenterflockDir = vNeighborDir = vMatchVelocityDir = Vector();
		lCount = 0;

		/* ------------------- Collect particle interaction data --------------------------- */ 

		// Iterate other particles
		for (j = 0; j < pcnt; ++j)
		{
			// Skip unwanted particles
			if (!(pp[j].bits & PARTICLEFLAGS_VISIBLE) || i == j)
				continue;

			// General stuff for particle interaction
			// --------------------------------------

			// Get distance to current particle
			vNeighborDiff = pp[j].off - pp[i].off;
			rNeighborDist = vNeighborDiff.GetSquaredLength();

			// Skip if particles too far away from each other
			if (rNeighborDist > rNeighborSightRadius)
				continue;
			
			// Flock Center
			// ------------
			if (rulemask & RULEFLAGS_CENTER)
			{
				vCenterflockDir += pp[j].off;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask & RULEFLAGS_NEIGHBORDIST)
			{
				if (rNeighborDist < rNeighborMinDist)
					vNeighborDir -= vNeighborDiff;
			}

			// Match Velocity
			// --------------
			if (rulemask & RULEFLAGS_MATCHVELO)
			{
				vMatchVelocityDir += pp[j].v3;
			}

			// Increase counter of considered flockmates
			lCount++;
		}

		// Apply any particle interaction that took place
		if (lCount > 1)
		{
			/* ------------------- Soft Rules --------------------------- */

			// Flock Center
			// ------------
			if (rulemask & RULEFLAGS_CENTER)
			{
				vParticleDir += ((vCenterflockDir / lCount) - pp[i].off) * rCenterflockWeight;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask & RULEFLAGS_NEIGHBORDIST)
			{
				vParticleDir += vNeighborDir * rNeighborWeight;
			}

			// Target
			// ------
			if (rulemask & RULEFLAGS_TARGET)
			{
				for (n = 0; n < targetData.GetCount(); ++n)
				{
					Vector dist = targetData[n]._position - pp[i].off;
					Float distLength = dist.GetSquaredLength();
					if (targetData[n]._infinite)
					{
						vParticleDir += dist * targetData[n]._weight * rTargetGlobalWeight;
					}
					else if (distLength < targetData[n]._radius)
					{
						vParticleDir += dist * (1.0 - distLength / targetData[n]._radius) * targetData[n]._weight * rTargetGlobalWeight;
					}
				}
			}

			// Match Velocity
			// --------------
			if (rulemask & RULEFLAGS_MATCHVELO)
			{
				vParticleDir += ((vMatchVelocityDir / lCount) - pp[i].v3) * rMatchVelocityWeight;
			}

			// Turbulence
			// ----------
			if (rulemask & RULEFLAGS_TURBULENCE)
			{
				vParticleDir += Vector(SNoise(rTurbulenceScale * pp[i].off, rTurbulenceTime), SNoise(rTurbulenceScale * pp[i].off + rTurbulenceAdd1, rTurbulenceTime), SNoise(rTurbulenceScale * pp[i].off + rTurbulenceAdd2, rTurbulenceTime)) * rTurbulenceWeight;
			}

			// Repell
			// ------
			if (rulemask  & RULEFLAGS_REPELL)
			{
				for (n = 0; n < repellerData.GetCount(); ++n)
				{
					Vector dist = repellerData[n]._position - pp[i].off;
					Float distLength = dist.GetSquaredLength();
					if (distLength < repellerData[n]._radius)
						vParticleDir -= dist * (1.0 - distLength / repellerData[n]._radius) * repellerData[n]._weight * rRepellGlobalWeight;
				}
			}

			// Add resulting to current velocity
			vParticleDir = pp[i].v3 + vParticleDir;


			// Level Flight
			// ------------
			if (rulemask & RULEFLAGS_LEVELFLIGHT)
			{
				vParticleDir.y -= vParticleDir.y * rLevelFlightWeight;
			}

			// Avoid Geometry
			// --------------
			if (rulemask & RULEFLAGS_AVOIDGEO)
			{
				if (_collider->Intersect(mAvoidGeoI * pp[i].off, mAvoidGeoI.TransformVector(!vParticleDir), rAvoidGeoDist))
				{
					GeRayColResult colliderResult;

					if (_collider->GetNearestIntersection(&colliderResult))
					{
						rAvoidGeoMixval = 1.0 - colliderResult.distance / rAvoidGeoDist;
						switch (lAvoidGeoMode)
						{
							case OFLOCK_AVOIDGEO_MODE_SOFT:
								vParticleDir = vParticleDir + mAvoidGeo.TransformVector(!colliderResult.s_normal) * vParticleDir.GetLength() * rAvoidGeoMixval * rAvoidGeoWeight;
								break;

							default:
							case OFLOCK_AVOIDGEO_MODE_HARD:
								vParticleDir = Blend(mAvoidGeo.TransformVector((!colliderResult.s_normal * vParticleDir.GetLength())), vParticleDir, rAvoidGeoMixval);
								break;
						}
					}
				}
			}


			// Speed Limits
			// ------------
			rSpeed = vParticleDir.GetSquaredLength() * diff;
			if (rulemask & RULEFLAGS_SPEEDLIMIT)
			{
				switch (lSpeedMode)
				{
					case OFLOCK_SPEED_MODE_SOFT:
						{
							if (rSpeed < rSpeedMin)
							{
								rSpeedRatio = rSpeedMin / FMax(rSpeed, 0.001);
								vParticleDir *= Blend(1.0, rSpeedRatio, rSpeedWeight);
							}
							else if (rSpeed > rSpeedMax)
							{
								rSpeedRatio = rSpeedMax / FMax(rSpeed, 0.001);
								vParticleDir *= Blend(1.0, rSpeedRatio, rSpeedWeight);
							}
						}
						break;

					case OFLOCK_SPEED_MODE_HARD:
						{
							if (rSpeed < rSpeedMin)
							{
								vParticleDir = !vParticleDir * rSpeedMin;
							}
							else if (rSpeed > rSpeedMax)
							{
								vParticleDir = !vParticleDir * rSpeedMax;
							}
						}
						break;
				}
			}

			// Add resulting velocity, apply overall weight
			ss[i].v += Blend(pp[i].v3, vParticleDir, rWeight);
		}

		ss[i].count++;
	}
}


/****************************************************************************
 * Register Plugin Object
 ****************************************************************************/
Bool RegisterFlockModifier()
{
	return RegisterObjectPlugin(ID_OFLOCKMODIFIER, GeLoadString(IDS_OFLOCK), OBJECT_PARTICLEMODIFIER, FlockModifier::Alloc, "Oflock", AutoBitmap("Oflock.tif"), 0);
}
