#include "c4d.h"
#include "c4d_symbols.h"
#include "oflock.h"
#include "lib_collider.h"
#include "customgui_inexclude.h"
#include "Oflocktarget.h"
#include "Oflockrepeller.h"
#include "helpers.h"


class FlockModifier : public ObjectData
{
	INSTANCEOF(FlockModifier, ObjectData)

	private:
		GeRayCollider* collider;
		GeRayColResult collider_result;

	public:
		virtual Bool Init(GeListNode *node);
		virtual Bool GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc);

		virtual void ModifyParticles(BaseObject* op, Particle* pp, BaseParticle* ss, LONG pcnt, Real diff);

		static NodeData *Alloc(void) { return gNew FlockModifier; }

		FlockModifier()
		{ 
			collider = GeRayCollider::Alloc();
		}

		~FlockModifier()
		{
			if (collider) GeRayCollider::Free(collider);
		}
};


/****************************************************************************
 * Initialize attributes
 ****************************************************************************/
Bool FlockModifier::Init(GeListNode *node)
{	
	BaseObject		*op   = (BaseObject*)node;
	BaseContainer *bc = op->GetDataInstance();
	if (!bc) return FALSE;

	bc->SetReal(OFLOCK_WEIGHT, RCO 1.0);
	bc->SetReal(OFLOCK_CENTER_WEIGHT, RCO 0.2);
	bc->SetReal(OFLOCK_NEIGHBORDIST_WEIGHT, RCO 0.1);
	bc->SetReal(OFLOCK_NEIGHBORDIST_DIST, RCO 10.0);
	bc->GetReal(OFLOCK_MATCHVEL_WEIGHT, RCO 0.1);
	bc->SetReal(OFLOCK_TARGET_WEIGHT, RCO 0.15);
	bc->SetReal(OFLOCK_LEVELFLIGHT_WEIGHT, RCO 0.3);
	bc->SetReal(OFLOCK_SPEED_WEIGHT, RCO 0.2);
	bc->SetLong(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	bc->SetReal(OFLOCK_SPEED_MIN, RCO 0.0);
	bc->SetReal(OFLOCK_SPEED_MAX, RCO 200.0);
	bc->SetReal(OFLOCK_SIGHT, RCO 50.0);
	bc->SetReal(OFLOCK_AVOIDGEO_WEIGHT, RCO 1.0);
	bc->SetReal(OFLOCK_AVOIDGEO_DIST, 50.0);
	bc->SetLong(OFLOCK_AVOIDGEO_MODE, OFLOCK_AVOIDGEO_MODE_SOFT);
	bc->SetReal(OFLOCK_TURBULENCE_WEIGHT, RCO 0.5);
	bc->SetReal(OFLOCK_TURBULENCE_FREQUENCY, RCO 2.0);
	bc->SetReal(OFLOCK_TURBULENCE_SCALE, RCO 2.0);
	bc->SetReal(OFLOCK_REPELL_WEIGHT, RCO 1.0);

	return SUPER::Init(node);
}

/****************************************************************************
 * Enable/Disable attributes
 ****************************************************************************/
Bool FlockModifier::GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc)
{
	BaseContainer *bc = ((BaseObject*)node)->GetDataInstance();
	if (!bc) return FALSE;

	switch (id[0].id)
	{
		case OFLOCK_AVOIDGEO_WEIGHT:
			return bc->GetLong(OFLOCK_AVOIDGEO_MODE) == OFLOCK_AVOIDGEO_MODE_SOFT;
			break;
		case OFLOCK_SPEED_WEIGHT:
			return bc->GetLong(OFLOCK_SPEED_MODE) == OFLOCK_SPEED_MODE_SOFT;
			break;
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

/****************************************************************************
 * Simulate flock
 ****************************************************************************/
void FlockModifier::ModifyParticles(BaseObject* op, Particle* pp, BaseParticle* ss, LONG pcnt, Real diff)
{
	if (!op || !pp || !ss || !collider) return;

	LONG i,j;

	BaseContainer* bc = op->GetDataInstance();
	BaseDocument* doc = op->GetDocument();

	// Variables
	LONG lCount = pcnt - 1;
	Vector vParticleDir;

	// Overall weight
	Real rWeight = bc->GetReal(OFLOCK_WEIGHT, RCO 1.0);

	// Range of sight
	Real rNeighborDist = RCO 0.0;
	Real rNeighborSightRadius = bc->GetReal(OFLOCK_SIGHT, RCO 0.0);
	rNeighborSightRadius *= rNeighborSightRadius;		// Square

	// Flock Center
	Vector vCenterflockDir;
	Real rCenterflockWeight = bc->GetReal(OFLOCK_CENTER_WEIGHT, RCO 0.0) * RCO 0.1;

	// Neighbor Distance
	Vector vNeighborDiff(DC);
	Vector vNeighborDir;
	Real rNeighborWeight = bc->GetReal(OFLOCK_NEIGHBORDIST_WEIGHT, RCO 0.0) * RCO 0.1;
	Real rNeighborMinDist = bc->GetReal(OFLOCK_NEIGHBORDIST_DIST, RCO 0.0);

	// Match Velocity
	Vector vMatchVelocityDir;
	Real rMatchVelocityWeight = bc->GetReal(OFLOCK_MATCHVEL_WEIGHT, RCO 0.0) * RCO 0.1;

	// Target
	Real rTargetGlobalWeight = bc->GetReal(OFLOCK_TARGET_WEIGHT, RCO 0.0) * RCO 0.1;
	InExcludeData* inexTarget = (InExcludeData*)bc->GetCustomDataType(OFLOCK_TARGET_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	LONG lTargetCount = 0;
	if (inexTarget) lTargetCount = inexTarget->GetObjectCount();
	c4d_misc::BaseArray<TargetData>targetData;

	// Level flight
	Real rLevelFlightWeight = bc->GetReal(OFLOCK_LEVELFLIGHT_WEIGHT, RCO 0.0);

	// Speed limits
	Real rSpeed = RCO 0.0;
	LONG lSpeedMode = bc->GetLong(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	Real rSpeedWeight = bc->GetReal(OFLOCK_SPEED_WEIGHT, RCO 0.0);
	Real rSpeedMin = bc->GetReal(OFLOCK_SPEED_MIN, RCO 0.0) * diff;
	Real rSpeedMax = bc->GetReal(OFLOCK_SPEED_MAX, RCO 100.0) * diff;
	Real rSpeedRatio;

	// Geometry avoidance
	LONG lAvoidGeoMode = bc->GetLong(OFLOCK_AVOIDGEO_MODE, 1);
	Real rAvoidGeoWeight = bc->GetReal(OFLOCK_AVOIDGEO_WEIGHT, RCO 0.0);
	Real rAvoidGeoDist = bc->GetReal(OFLOCK_AVOIDGEO_DIST, RCO 0.0);
	Vector vAvoidGeoDir;
	BaseObject* boAvoidGeoLink = bc->GetObjectLink(OFLOCK_AVOIDGEO_LINK, doc);
	Matrix mAvoidGeo, mAvoidGeoI;
	Real rAvoidGeoMixval;

	// Turbulence
	Real rTurbulenceWeight = bc->GetReal(OFLOCK_TURBULENCE_WEIGHT, RCO 0.0) * RCO 10.0;
	Real rTurbulenceTime = doc->GetTime().Get() * bc->GetReal(OFLOCK_TURBULENCE_FREQUENCY, RCO 1.0);
	Real rTurbulenceScale = RCO 0.1 / FCut(bc->GetReal(OFLOCK_TURBULENCE_SCALE, RCO 1.0), RCO 0.0001, MAXREALl);
	Vector rTurbulenceAdd1;
	Vector rTurbulenceAdd2;

	// Repell
	Real rRepellGlobalWeight = bc->GetReal(OFLOCK_REPELL_WEIGHT, RCO 0.0);
	InExcludeData* inexRepell = (InExcludeData*)bc->GetCustomDataType(OFLOCK_REPELL_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	LONG lRepellCount = 0;
	if (inexRepell) lRepellCount = inexRepell->GetObjectCount();
	c4d_misc::BaseArray<RepellerData>repellerData;


	/* ------------------- Set rule mask & prepare some data --------------------------- */ 
	LONG rulemask = RULEFLAGS_0;

	// Center Flock
	if (!CompareFloatTolerant(rCenterflockWeight, RCO 0.0))
	{
		rulemask |= RULEFLAGS_CENTER;
	}

	// Neighbor Distance
	if (!CompareFloatTolerant(rNeighborWeight, RCO 0.0))
	{
		rulemask |= RULEFLAGS_NEIGHBORDIST;
		rNeighborMinDist *= rNeighborMinDist;		// Square
	}

	// Match Flock Velocity
	if (!CompareFloatTolerant(rMatchVelocityWeight, RCO 0.0))
	{
		rulemask |= RULEFLAGS_MATCHVELO;
	}

	// Level Flight
	if (!CompareFloatTolerant(rLevelFlightWeight, RCO 0.0))
	{
		rulemask |= RULEFLAGS_LEVELFLIGHT;
	}

	// Turbulence
	if (!CompareFloatTolerant(rTurbulenceWeight, RCO 0.0))
	{
		rulemask |= RULEFLAGS_TURBULENCE;
		rTurbulenceAdd1 = Vector(pi * RCO 1000.0);
		rTurbulenceAdd2 = Vector(-pi05 * RCO 1000.0);
	}

	// Speed Limit
	if (((lSpeedMode == OFLOCK_SPEED_MODE_SOFT && !CompareFloatTolerant(rSpeedWeight, RCO 0.0)) || lSpeedMode == OFLOCK_SPEED_MODE_HARD))
	{
		rulemask |= RULEFLAGS_SPEEDLIMIT;
		rSpeedMin *= rSpeedMin;		// Square
		rSpeedMax *= rSpeedMax;		// Square
	}

	// Target
	if (!CompareFloatTolerant(rTargetGlobalWeight, RCO 0.0) && inexTarget && lTargetCount > 0)
	{
		rulemask |= RULEFLAGS_TARGET;

		BaseObject* opListItem = NULL;
		TargetData tdata;
		BaseContainer* tc = NULL;

		targetData.Flush();
		for (LONG i = 0; i < lTargetCount; i++)
		{
			opListItem = (BaseObject*)inexTarget->ObjectFromIndex(doc, i);
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKTARGET_ENABLED))
				{
					tdata._weight = tc->GetReal(OFLOCKREPELLER_WEIGHT, RCO 1.0);
					tdata._radius = tc->GetReal(OFLOCKTARGET_RADIUS, RCO 0.0);
					tdata._radius *= tdata._radius;		// Square
					tdata._infinite = tc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, TRUE);
					tdata._position = opListItem->GetMg().off;
					targetData.Append(tdata);
				}
			}
		}
	}

	// Avoid Geometry
	if (((lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_SOFT && !CompareFloatTolerant(rAvoidGeoWeight, RCO 0.0)) || lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_HARD) && !CompareFloatTolerant(rAvoidGeoDist, RCO 0.0) && boAvoidGeoLink && boAvoidGeoLink->GetType() == Opolygon && ToPoly(boAvoidGeoLink)->GetPolygonCount() > 0)
	{
		rulemask |= RULEFLAGS_AVOIDGEO;
		collider->Init(boAvoidGeoLink, boAvoidGeoLink->GetDirty(DIRTYFLAGS_CACHE|DIRTYFLAGS_MATRIX|DIRTYFLAGS_DATA));
		mAvoidGeo = boAvoidGeoLink->GetMg();
		mAvoidGeoI = !mAvoidGeo;
	}

	// Repelling
	if (!CompareFloatTolerant(rRepellGlobalWeight, RCO 0.0) && inexRepell && lRepellCount > 0)
	{
		rulemask |= RULEFLAGS_REPELL;

		BaseObject* opListItem = NULL;
		RepellerData rdata;
		BaseContainer* tc = NULL;

		repellerData.Flush();
		for (LONG i = 0; i < lRepellCount; i++)
		{
			opListItem = (BaseObject*)inexRepell->ObjectFromIndex(doc, i);
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKREPELLER_ENABLED))
				{
					rdata._weight = tc->GetReal(OFLOCKREPELLER_WEIGHT, RCO 1.0);
					rdata._radius = tc->GetReal(OFLOCKREPELLER_RADIUS, RCO 0.0);
					rdata._radius *= rdata._radius;		// Square
					rdata._position = opListItem->GetMg().off;
					repellerData.Append(rdata);
				}
			}
		}
	}


	// Iterate particles
	for (i = 0; i < pcnt; i++)
	{
		// Skip unwanted particles
		if (!(pp[i].bits & PARTICLEFLAGS_VISIBLE)) continue;

		// Reset values
		vParticleDir = vCenterflockDir = vNeighborDir = vMatchVelocityDir = Vector();
		lCount = 0;

		/* ------------------- Collect particle interaction data --------------------------- */ 

		// Iterate other particles
		for (j = 0; j < pcnt; j++)
		{
			// Skip unwanted particles
			if (!(pp[j].bits & PARTICLEFLAGS_VISIBLE) || i == j) continue;

			// General stuff for particle interaction
			// --------------------------------------

			// Get distance to current particle
			vNeighborDiff = pp[j].off - pp[i].off;
			rNeighborDist = vNeighborDiff.GetLengthSquared();

			// Skip if particles too far away from each other
			if (rNeighborDist > rNeighborSightRadius) continue;

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
				vParticleDir += ((vCenterflockDir / RCO lCount) - pp[i].off) * rCenterflockWeight;
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
				for (LONG n = 0; n < targetData.GetCount(); n++)
				{
					Vector diff = targetData[n]._position - pp[i].off;
					Real dist = diff.GetLengthSquared();
					if (targetData[n]._infinite)
					{
						vParticleDir += diff * targetData[n]._weight * rTargetGlobalWeight;
					}
					else if (dist < targetData[n]._radius)
					{
						vParticleDir += diff * (RCO 1.0 - dist / targetData[n]._radius) * targetData[n]._weight * rTargetGlobalWeight;
					}
				}
			}

			// Match Velocity
			// --------------
			if (rulemask & RULEFLAGS_MATCHVELO)
			{
				vParticleDir += ((vMatchVelocityDir / RCO lCount) - pp[i].v3) * rMatchVelocityWeight;
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
				for (LONG n = 0; n < repellerData.GetCount(); n++)
				{
					Vector diff = repellerData[n]._position - pp[i].off;
					Real dist = diff.GetLengthSquared();
					if (dist < repellerData[n]._radius)
					{
						vParticleDir -= diff * (RCO 1.0 - dist / repellerData[n]._radius) * repellerData[n]._weight * rRepellGlobalWeight;
					}
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
				if (collider->Intersect(pp[i].off * mAvoidGeoI, !vParticleDir ^ mAvoidGeoI, rAvoidGeoDist))
				{
					if (collider->GetNearestIntersection(&collider_result))
					{
						rAvoidGeoMixval = RCO 1.0 - collider_result.distance / rAvoidGeoDist;
						switch (lAvoidGeoMode)
						{
							case OFLOCK_AVOIDGEO_MODE_SOFT:
								vParticleDir = vParticleDir + (!(collider_result.s_normal) ^ mAvoidGeo) * vParticleDir.GetLength() * rAvoidGeoMixval * rAvoidGeoWeight;
								break;

							default:
							case OFLOCK_AVOIDGEO_MODE_HARD:
								vParticleDir = Mix((!collider_result.s_normal * vParticleDir.GetLength()) ^ mAvoidGeo, vParticleDir, rAvoidGeoMixval);
								break;
						}
					}
				}
			}


			// Speed Limits
			// ------------
			rSpeed = vParticleDir.GetLengthSquared() * diff;
			if (rulemask & RULEFLAGS_SPEEDLIMIT)
			{
				switch (lSpeedMode)
				{
					case OFLOCK_SPEED_MODE_SOFT:
						{
							if (rSpeed < rSpeedMin)
							{
								rSpeedRatio = rSpeedMin / FMax(rSpeed, RCO 0.001);
								vParticleDir *= Mix(RCO 1.0, rSpeedRatio, rSpeedWeight);
							}
							else if (rSpeed > rSpeedMax)
							{
								rSpeedRatio = rSpeedMax / FMax(rSpeed, RCO 0.001);
								vParticleDir *= Mix(RCO 1.0, rSpeedRatio, rSpeedWeight);
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
			ss[i].v += Mix(pp[i].v3, vParticleDir, rWeight);
		}

		ss[i].count++;
	}
}


/****************************************************************************
 * Register Plugin Object
 ****************************************************************************/
Bool RegisterFlockModifier(void)
{
	return RegisterObjectPlugin(ID_OFLOCKMODIFIER, GeLoadString(IDS_OFLOCK), OBJECT_PARTICLEMODIFIER, FlockModifier::Alloc, "Oflock", AutoBitmap("Oflock.tif"), 0);
}
