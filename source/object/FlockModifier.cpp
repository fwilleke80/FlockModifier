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
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
	virtual void ModifyParticles(BaseObject *op, Particle *pp, BaseParticle *ss, Int32 pcnt, Float diff);
	
	static NodeData *Alloc()
	{
		return NewObjClear(FlockModifier);
	}
	
private:
	AutoFree<GeRayCollider> _geoAvoidanceCollider; ///< GeRayCollider for "Avoid Geometry" rule
};


//
// Initialize attributes
//
Bool FlockModifier::Init(GeListNode *node)
{
	if (!node)
		return false;

	BaseContainer *bc = static_cast<BaseObject*>(node)->GetDataInstance();
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

//
// Enable/Disable attributes
//
Bool FlockModifier::GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc)
{
	if (!node)
		return false;
	
	BaseContainer *bc = static_cast<BaseObject*>(node)->GetDataInstance();
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

//
// Simulate flock
//
void FlockModifier::ModifyParticles(BaseObject *op, Particle *pp, BaseParticle *ss, Int32 pcnt, Float diff)
{
	if (!op || !pp || !ss)
		return;

	Int32 i,j;

	BaseContainer *bc = op->GetDataInstance();
	BaseDocument *doc = op->GetDocument();
	
	if (!bc || !doc)
		return;

	// Variables
	Int32 lCount = pcnt - 1;
	Vector vParticleDir(DC);

	// Overall weight
	Float fWeight = bc->GetFloat(OFLOCK_WEIGHT, 1.0);

	// Range of sight
	Float fNeighborDist = 0.0;
	Float fNeighborSightRadius = bc->GetFloat(OFLOCK_SIGHT, 0.0);
	fNeighborSightRadius *= fNeighborSightRadius;		// Square

	// Flock Center
	Vector vCenterflockDir(DC);
	Float fCenterflockWeight = bc->GetFloat(OFLOCK_CENTER_WEIGHT, 0.0) * 0.1;

	// Neighbor Distance
	Vector vNeighborDiff(DC);
	Vector vNeighborDir(DC);
	Float fNeighborWeight = bc->GetFloat(OFLOCK_NEIGHBORDIST_WEIGHT, 0.0) * 0.1;
	Float fNeighborMinDist = bc->GetFloat(OFLOCK_NEIGHBORDIST_DIST, 0.0);

	// Match Velocity
	Vector vMatchVelocityDir(DC);
	Float fMatchVelocityWeight = bc->GetFloat(OFLOCK_MATCHVEL_WEIGHT, 0.0) * 0.1;

	// Target
	Float fTargetGlobalWeight = bc->GetFloat(OFLOCK_TARGET_WEIGHT, 0.0) * 0.1;
	InExcludeData* inexTarget = (InExcludeData*)bc->GetCustomDataType(OFLOCK_TARGET_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 lTargetCount = 0;
	if (inexTarget)
		lTargetCount = inexTarget->GetObjectCount();
	maxon::BaseArray<TargetData> targetData;

	// Level flight
	Float fLevelFlightWeight = bc->GetFloat(OFLOCK_LEVELFLIGHT_WEIGHT, 0.0);

	// Speed limits
	Float fSpeed = 0.0;
	Int32 lSpeedMode = bc->GetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	Float fSpeedWeight = bc->GetFloat(OFLOCK_SPEED_WEIGHT, 0.0);
	Float fSpeedMin = bc->GetFloat(OFLOCK_SPEED_MIN, 0.0) * diff;
	Float fSpeedMax = bc->GetFloat(OFLOCK_SPEED_MAX, 100.0) * diff;
	Float fSpeedRatio = 0.0;

	// Geometry avoidance
	Int32 lAvoidGeoMode = bc->GetInt32(OFLOCK_AVOIDGEO_MODE, 1);
	Float fAvoidGeoWeight = bc->GetFloat(OFLOCK_AVOIDGEO_WEIGHT, 0.0);
	Float fAvoidGeoDist = bc->GetFloat(OFLOCK_AVOIDGEO_DIST, 0.0);
	Float fAvoidGeoDistI = 1.0 / FMax(fAvoidGeoDist, EPSILON);
	BaseObject* boAvoidGeoLink = bc->GetObjectLink(OFLOCK_AVOIDGEO_LINK, doc);
	Matrix mAvoidGeo(DC), mAvoidGeoI(DC);
	Float fAvoidGeoMixval = 0.0;

	// Turbulence
	Float fTurbulenceWeight = bc->GetFloat(OFLOCK_TURBULENCE_WEIGHT, 0.0) * 10.0;
	Float fTurbulenceTime = doc->GetTime().Get() * bc->GetFloat(OFLOCK_TURBULENCE_FREQUENCY, 1.0);
	Float fTurbulenceScale = 0.1 / ClampValue(bc->GetFloat(OFLOCK_TURBULENCE_SCALE, 1.0), 0.0001, MAXRANGE_FLOAT);
	Vector vTurbulenceAdd1(DC);
	Vector vTurbulenceAdd2(DC);

	// Repell
	Float fRepellGlobalWeight = bc->GetFloat(OFLOCK_REPELL_WEIGHT, 0.0);
	InExcludeData* inexRepell = (InExcludeData*)bc->GetCustomDataType(OFLOCK_REPELL_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 lRepellCount = 0;
	if (inexRepell)
		lRepellCount = inexRepell->GetObjectCount();
	maxon::BaseArray<RepellerData> repellerData;


	/* ------------------- Set rule mask & prepare some data --------------------------- */ 
	RULEFLAGS rulemask = RULEFLAGS_NONE;

	// Center Flock
	if (fCenterflockWeight > 0.0)
	{
		rulemask |= RULEFLAGS_CENTER;
	}

	// Neighbor Distance
	if (fNeighborWeight > 0.0)
	{
		rulemask |= RULEFLAGS_NEIGHBORDIST;
		fNeighborMinDist *= fNeighborMinDist; // Square
	}

	// Match Flock Velocity
	if (fMatchVelocityWeight > 0.0)
	{
		rulemask |= RULEFLAGS_MATCHVELO;
	}

	// Level Flight
	if (fLevelFlightWeight > 0.0)
	{
		rulemask |= RULEFLAGS_LEVELFLIGHT;
	}

	// Turbulence
	if (fTurbulenceWeight > 0.0)
	{
		rulemask |= RULEFLAGS_TURBULENCE;
		vTurbulenceAdd1 = Vector(PI * 1000.0);
		vTurbulenceAdd2 = Vector(-PI05 * 1000.0);
	}

	// Speed Limit
	if (((lSpeedMode == OFLOCK_SPEED_MODE_SOFT && fSpeedWeight > 0.0) || lSpeedMode == OFLOCK_SPEED_MODE_HARD))
	{
		rulemask |= RULEFLAGS_SPEEDLIMIT;
		fSpeedMin *= fSpeedMin; // Square
		fSpeedMax *= fSpeedMax; // Square
	}

	// Target
	if (fTargetGlobalWeight > 0.0 && inexTarget && lTargetCount > 0)
	{
		rulemask |= RULEFLAGS_TARGET;

		BaseObject *opListItem = nullptr;
		BaseContainer *tc = nullptr;

		targetData.Flush();
		for (i = 0; i < lTargetCount; ++i)
		{
			opListItem = static_cast<BaseObject*>(inexTarget->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKTARGET_ENABLED))
				{
					TargetData tdata(tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0),
					                 tc->GetFloat(OFLOCKTARGET_RADIUS, 0.0),
					                 tc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, true),
					                 opListItem->GetMg().off);
					tdata._radius *= tdata._radius; // Square radius
					tdata._radiusI = 1.0 / FMax(tdata._radius, EPSILON); // Calculate inverse radius
					targetData.Append(tdata);
				}
			}
		}
	}

	// Avoid Geometry
	if (((lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_SOFT && fAvoidGeoWeight > 0.0) || lAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_HARD) && fAvoidGeoDist > 0.0 && boAvoidGeoLink && boAvoidGeoLink->GetType() == Opolygon && ToPoly(boAvoidGeoLink)->GetPolygonCount() > 0)
	{
		rulemask |= RULEFLAGS_AVOIDGEO;
		
		// Lazy-alloc collider
		if (!_geoAvoidanceCollider)
		{
			_geoAvoidanceCollider.Set(GeRayCollider::Alloc());
			if (!_geoAvoidanceCollider)
				return;
		}
		
		_geoAvoidanceCollider->Init(boAvoidGeoLink);
		mAvoidGeo = boAvoidGeoLink->GetMg();
		mAvoidGeoI = ~mAvoidGeo;
	}

	// Repelling
	if (fRepellGlobalWeight > 0.0 && inexRepell && lRepellCount > 0)
	{
		rulemask |= RULEFLAGS_REPELL;

		BaseObject *opListItem = nullptr;
		BaseContainer *tc = nullptr;

		repellerData.Flush();
		for (i = 0; i < lRepellCount; ++i)
		{
			opListItem = static_cast<BaseObject*>(inexRepell->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKREPELLER_ENABLED))
				{
					RepellerData rdata(tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0),
					                   tc->GetFloat(OFLOCKREPELLER_RADIUS, 0.0),
					                   opListItem->GetMg().off);
					rdata._radius *= rdata._radius; // Square radius
					rdata._radiusI = 1.0 / FMax(rdata._radius, EPSILON); // Calculate inverse radius
					repellerData.Append(rdata);
				}
			}
		}
	}


	// Iterate particles
	for (i = 0; i < pcnt; ++i)
	{
		// Store a ref to the current Particle and current BaseParticle, so we don't have to
		// do all the pointer arithmetics for pp[i].
		Particle &currentParticle = pp[i];
		BaseParticle &currentBaseParticle = ss[i];
		
		// Skip unwanted particles
		if (!(currentParticle.bits&PARTICLEFLAGS_VISIBLE))
			continue;

		// Reset values
		vParticleDir = vCenterflockDir = vNeighborDir = vMatchVelocityDir = Vector();
		lCount = 0;

		/* ------------------- Collect particle interaction data --------------------------- */ 

		// Iterate other particles
		for (j = 0; j < pcnt; ++j)
		{
			// Store a ref to the current other particle, so we don't have to
			// do all the pointer arithmetics for pp[j].
			Particle &currentOtherParticle = pp[j];

			// Skip unwanted particles
			if (!(currentOtherParticle.bits&PARTICLEFLAGS_VISIBLE) || i == j)
				continue;

			// General stuff for particle interaction
			// --------------------------------------

			// Get distance to current particle
			vNeighborDiff = currentOtherParticle.off - currentParticle.off;
			fNeighborDist = vNeighborDiff.GetSquaredLength();

			// Skip if particles too far away from each other
			if (fNeighborDist > fNeighborSightRadius)
				continue;
			
			// Flock Center
			// ------------
			if (rulemask&RULEFLAGS_CENTER)
			{
				vCenterflockDir += currentOtherParticle.off;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&RULEFLAGS_NEIGHBORDIST)
			{
				if (fNeighborDist < fNeighborMinDist)
					vNeighborDir -= vNeighborDiff;
			}

			// Match Velocity
			// --------------
			if (rulemask&RULEFLAGS_MATCHVELO)
			{
				vMatchVelocityDir += currentOtherParticle.v3;
			}

			// Increase counter of considered flockmates
			lCount++;
		}

		// Apply any particle interaction that took place
		if (lCount > 1)
		{
			/* ------------------- Soft Rules --------------------------- */
			
			Float fCountI = 1.0 / FMax(lCount, EPSILON);

			// Flock Center
			// ------------
			if (rulemask&RULEFLAGS_CENTER)
			{
				vParticleDir += ((vCenterflockDir * fCountI) - currentParticle.off) * fCenterflockWeight;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&RULEFLAGS_NEIGHBORDIST)
			{
				vParticleDir += vNeighborDir * fNeighborWeight;
			}

			// Target
			// ------
			if (rulemask & RULEFLAGS_TARGET)
			{
				for (maxon::BaseArray<TargetData>::Iterator target = targetData.Begin(); target != targetData.End(); ++target)
				{
					Vector dist = target->_position - currentParticle.off;
					Float distLength = dist.GetSquaredLength();
					if (target->_infinite)
					{
						vParticleDir += dist * target->_weight * fTargetGlobalWeight;
					}
					else if (distLength < target->_radius)
					{
						vParticleDir += dist * (1.0 - distLength * target->_radiusI) * target->_weight * fTargetGlobalWeight;
					}
				}
			}

			// Match Velocity
			// --------------
			if (rulemask&RULEFLAGS_MATCHVELO)
			{
				vParticleDir += ((vMatchVelocityDir * fCountI) - currentParticle.v3) * fMatchVelocityWeight;
			}

			// Turbulence
			// ----------
			if (rulemask&RULEFLAGS_TURBULENCE)
			{
				vParticleDir += Vector(SNoise(fTurbulenceScale * currentParticle.off, fTurbulenceTime), SNoise(fTurbulenceScale * currentParticle.off + vTurbulenceAdd1, fTurbulenceTime), SNoise(fTurbulenceScale * currentParticle.off + vTurbulenceAdd2, fTurbulenceTime)) * fTurbulenceWeight;
			}

			// Repell
			// ------
			if (rulemask&RULEFLAGS_REPELL)
			{
				for (maxon::BaseArray<RepellerData>::Iterator repeller = repellerData.Begin(); repeller != repellerData.End(); ++repeller)
				{
					Vector dist = repeller->_position - currentParticle.off;
					Float distLength = dist.GetSquaredLength();
					if (distLength < repeller->_radius)
						vParticleDir -= dist * (1.0 - distLength * repeller->_radiusI) * repeller->_weight * fRepellGlobalWeight;
				}
			}

			// Add resulting direction to current velocity
			vParticleDir += currentParticle.v3;


			// Level Flight
			// ------------
			if (rulemask&RULEFLAGS_LEVELFLIGHT)
			{
				vParticleDir.y -= vParticleDir.y * fLevelFlightWeight;
			}

			// Avoid Geometry
			// --------------
			if (rulemask&RULEFLAGS_AVOIDGEO)
			{
				if (_geoAvoidanceCollider->Intersect(mAvoidGeoI * currentParticle.off, mAvoidGeoI.TransformVector(!vParticleDir), fAvoidGeoDist))
				{
					GeRayColResult colliderResult;

					if (_geoAvoidanceCollider->GetNearestIntersection(&colliderResult))
					{
						fAvoidGeoMixval = 1.0 - colliderResult.distance * fAvoidGeoDistI;
						switch (lAvoidGeoMode)
						{
							case OFLOCK_AVOIDGEO_MODE_SOFT:
								vParticleDir = vParticleDir + mAvoidGeo.TransformVector(!colliderResult.s_normal) * vParticleDir.GetLength() * fAvoidGeoMixval * fAvoidGeoWeight;
								break;

							default:
							case OFLOCK_AVOIDGEO_MODE_HARD:
								vParticleDir = Blend(mAvoidGeo.TransformVector((!colliderResult.s_normal * vParticleDir.GetLength())), vParticleDir, fAvoidGeoMixval);
								break;
						}
					}
				}
			}


			// Speed Limits
			// ------------
			if (rulemask&RULEFLAGS_SPEEDLIMIT)
			{
				fSpeed = vParticleDir.GetSquaredLength() * diff;
				switch (lSpeedMode)
				{
					case OFLOCK_SPEED_MODE_SOFT:
						{
							if (fSpeed < fSpeedMin)
							{
								fSpeedRatio = fSpeedMin / FMax(fSpeed, EPSILON);
								vParticleDir *= Blend(1.0, fSpeedRatio, fSpeedWeight);
							}
							else if (fSpeed > fSpeedMax)
							{
								fSpeedRatio = fSpeedMax / FMax(fSpeed, EPSILON);
								vParticleDir *= Blend(1.0, fSpeedRatio, fSpeedWeight);
							}
						}
						break;

					case OFLOCK_SPEED_MODE_HARD:
						{
							if (fSpeed < fSpeedMin)
							{
								vParticleDir = !vParticleDir * fSpeedMin;
							}
							else if (fSpeed > fSpeedMax)
							{
								vParticleDir = !vParticleDir * fSpeedMax;
							}
						}
						break;
				}
			}

			// Add resulting velocity, apply overall weight
			currentBaseParticle.v += Blend(currentParticle.v3, vParticleDir, fWeight);
		}

		currentBaseParticle.count++;
	}
}


//
// Register Plugin Object
//
Bool RegisterFlockModifier()
{
	return RegisterObjectPlugin(ID_OFLOCKMODIFIER, GeLoadString(IDS_OFLOCK), OBJECT_PARTICLEMODIFIER, FlockModifier::Alloc, "Oflock", AutoBitmap("Oflock.tif"), 0);
}
