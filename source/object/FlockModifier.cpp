#include "customgui_inexclude.h"
#include "lib_collider.h"
#include "lib_description.h"

#include "c4d_particleobject.h"
#include "c4d_objectplugin.h"
#include "c4d_objectdata.h"
#include "c4d_basedocument.h"
#include "c4d_resource.h"
#include "c4d_basebitmap.h"

#include "helpers.h"
#include "ge_prepass.h"
#include "main.h"

#include "oflock.h"
#include "oflocktarget.h"
#include "oflockrepeller.h"
#include "c4d_symbols.h"


///
/// \brief This class implements the actual particle modifier
///
class FlockModifier : public ObjectData
{
	INSTANCEOF(FlockModifier, ObjectData)

public:
	virtual Bool Init(GeListNode* node) override;
	virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) override;
	virtual void ModifyParticles(BaseObject* op, Particle* pp, BaseParticle* ss, Int32 pcnt, Float diff) override;

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
Bool FlockModifier::Init(GeListNode* node)
{
	if (!node)
		return false;

	BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	dataRef.SetFloat(OFLOCK_WEIGHT, 1.0);

	dataRef.SetFloat(OFLOCK_SIGHT, 50.0);

	dataRef.SetFloat(OFLOCK_CENTER_WEIGHT, 0.2);

	dataRef.SetFloat(OFLOCK_NEIGHBORDIST_WEIGHT, 0.1);
	dataRef.SetFloat(OFLOCK_NEIGHBORDIST_DIST, 10.0);

	dataRef.SetFloat(OFLOCK_MATCHVEL_WEIGHT, 0.1);

	dataRef.SetFloat(OFLOCK_LEVELFLIGHT_WEIGHT, 0.3);

	dataRef.SetFloat(OFLOCK_TURBULENCE_WEIGHT, 0.5);
	dataRef.SetFloat(OFLOCK_TURBULENCE_FREQUENCY, 2.0);
	dataRef.SetFloat(OFLOCK_TURBULENCE_SCALE, 2.0);

	dataRef.SetFloat(OFLOCK_SPEED_WEIGHT, 0.2);
	dataRef.SetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	dataRef.SetFloat(OFLOCK_SPEED_MIN, 0.0);
	dataRef.SetFloat(OFLOCK_SPEED_MAX, 200.0);

	dataRef.SetFloat(OFLOCK_TARGET_WEIGHT, 0.2);

	dataRef.SetFloat(OFLOCK_AVOIDGEO_WEIGHT, 1.0);
	dataRef.SetFloat(OFLOCK_AVOIDGEO_DIST, 50.0);
	dataRef.SetFloat(OFLOCK_AVOIDGEO_MODE, OFLOCK_AVOIDGEO_MODE_SOFT);

	dataRef.SetFloat(OFLOCK_REPELL_WEIGHT, 1.0);

	return SUPER::Init(node);
}

Bool FlockModifier::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
	if (!node)
		return false;

	const BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	switch (id[0].id)
	{
		case OFLOCK_AVOIDGEO_WEIGHT:
			return dataRef.GetInt32(OFLOCK_AVOIDGEO_MODE) == OFLOCK_AVOIDGEO_MODE_SOFT;

		case OFLOCK_SPEED_WEIGHT:
			return dataRef.GetInt32(OFLOCK_SPEED_MODE) == OFLOCK_SPEED_MODE_SOFT;
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

void FlockModifier::ModifyParticles(BaseObject* op, Particle* pp, BaseParticle* ss, Int32 pcnt, Float diff)
{
	if (!op || !pp || !ss)
		return;

	BaseDocument *doc = op->GetDocument();
	if (!doc)
		return;

	const BaseContainer& dataRef = op->GetDataInstanceRef();

	// Variables
	Int32 iCount = pcnt - 1;
	Vector vParticleDir(maxon::DONT_INITIALIZE);

	// Overall weight
	Float fWeight = dataRef.GetFloat(OFLOCK_WEIGHT, 1.0);

	// Range of sight
	Float fNeighborDist = 0.0;
	Float fNeighborSightRadius = dataRef.GetFloat(OFLOCK_SIGHT, 0.0);
	fNeighborSightRadius *= fNeighborSightRadius;		// Square

	// Flock Center
	Vector vCenterflockDir(maxon::DONT_INITIALIZE);
	Float fCenterflockWeight = dataRef.GetFloat(OFLOCK_CENTER_WEIGHT, 0.0) * 0.1;

	// Neighbor Distance
	Vector vNeighborDiff(maxon::DONT_INITIALIZE);
	Vector vNeighborDir(maxon::DONT_INITIALIZE);
	Float fNeighborWeight = dataRef.GetFloat(OFLOCK_NEIGHBORDIST_WEIGHT, 0.0) * 0.1;
	Float fNeighborMinDist = dataRef.GetFloat(OFLOCK_NEIGHBORDIST_DIST, 0.0);

	// Match Velocity
	Vector vMatchVelocityDir(maxon::DONT_INITIALIZE);
	Float fMatchVelocityWeight = dataRef.GetFloat(OFLOCK_MATCHVEL_WEIGHT, 0.0) * 0.1;

	// Target
	Float fTargetGlobalWeight = dataRef.GetFloat(OFLOCK_TARGET_WEIGHT, 0.0) * 0.1;
	InExcludeData* inexTarget = (InExcludeData*)dataRef.GetCustomDataType(OFLOCK_TARGET_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 iTargetCount = 0;
	if (inexTarget)
		iTargetCount = inexTarget->GetObjectCount();
	maxon::BaseArray<Flock::TargetData> targetData;

	// Level flight
	Float fLevelFlightWeight = dataRef.GetFloat(OFLOCK_LEVELFLIGHT_WEIGHT, 0.0);

	// Speed limits
	Float fSpeed = 0.0;
	Int32 iSpeedMode = dataRef.GetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	Float fSpeedWeight = dataRef.GetFloat(OFLOCK_SPEED_WEIGHT, 0.0);
	Float fSpeedMin = dataRef.GetFloat(OFLOCK_SPEED_MIN, 0.0) * diff;
	Float fSpeedMax = dataRef.GetFloat(OFLOCK_SPEED_MAX, 100.0) * diff;
	Float fSpeedRatio = 0.0;

	// Geometry avoidance
	Int32 iAvoidGeoMode = dataRef.GetInt32(OFLOCK_AVOIDGEO_MODE, 1);
	Float fAvoidGeoWeight = dataRef.GetFloat(OFLOCK_AVOIDGEO_WEIGHT, 0.0);
	Float fAvoidGeoDist = dataRef.GetFloat(OFLOCK_AVOIDGEO_DIST, 0.0);
	Float fAvoidGeoDistI = maxon::Inverse(fAvoidGeoDist);
	BaseObject* boAvoidGeoLink = dataRef.GetObjectLink(OFLOCK_AVOIDGEO_LINK, doc);
	Matrix mAvoidGeo(maxon::DONT_INITIALIZE);
	Matrix mAvoidGeoI(maxon::DONT_INITIALIZE);
	Float fAvoidGeoMixval = 0.0;

	// Turbulence
	Float fTurbulenceWeight = dataRef.GetFloat(OFLOCK_TURBULENCE_WEIGHT, 0.0) * 10.0;
	Float fTurbulenceTime = doc->GetTime().Get() * dataRef.GetFloat(OFLOCK_TURBULENCE_FREQUENCY, 1.0);
	Float fTurbulenceScale = 0.1 / ClampValue(dataRef.GetFloat(OFLOCK_TURBULENCE_SCALE, 1.0), EPSILON, MAXVALUE_FLOAT);
	static const Vector vTurbulenceAdd1(PI * 1000.0);
	static const Vector vTurbulenceAdd2(-PI05 * 1000.0);

	// Repell
	Float fRepellGlobalWeight = dataRef.GetFloat(OFLOCK_REPELL_WEIGHT, 0.0);
	InExcludeData* inexRepell = (InExcludeData*)dataRef.GetCustomDataType(OFLOCK_REPELL_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 iRepellerCount = 0;
	if (inexRepell)
		iRepellerCount = inexRepell->GetObjectCount();
	maxon::BaseArray<Flock::RepellerData> repellerData;


	/* ------------------- Set rule mask & prepare some data --------------------------- */
	Flock::RULEFLAGS rulemask = Flock::RULEFLAGS::NONE;

	// Center Flock
	if (fCenterflockWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::CENTER;
	}

	// Neighbor Distance
	if (fNeighborWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::NEIGHBORDIST;
		fNeighborMinDist *= fNeighborMinDist; // Square
	}

	// Match Flock Velocity
	if (fMatchVelocityWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::MATCHVELO;
	}

	// Level Flight
	if (fLevelFlightWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::LEVELFLIGHT;
	}

	// Turbulence
	if (fTurbulenceWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::TURBULENCE;
	}

	// Speed Limit
	if (((iSpeedMode == OFLOCK_SPEED_MODE_SOFT && fSpeedWeight > 0.0) || iSpeedMode == OFLOCK_SPEED_MODE_HARD))
	{
		rulemask |= Flock::RULEFLAGS::SPEEDLIMIT;
		fSpeedMin *= fSpeedMin; // Square
		fSpeedMax *= fSpeedMax; // Square
	}

	// Target
	if (fTargetGlobalWeight > 0.0 && inexTarget && iTargetCount > 0)
	{
		rulemask |= Flock::RULEFLAGS::TARGET;

		BaseObject *opListItem = nullptr;
		BaseContainer *tc = nullptr;

		targetData.Flush();
		for (Int32 i = 0; i < iTargetCount; ++i)
		{
			opListItem = static_cast<BaseObject*>(inexTarget->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKTARGET_ENABLED))
				{
					Flock::TargetData tdata(tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0), tc->GetFloat(OFLOCKTARGET_RADIUS, 0.0), tc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, true), opListItem->GetMg().off);
					tdata.radius *= tdata.radius; // Square radius
					tdata.radiusI = maxon::Inverse(tdata.radius); // Calculate inverse radius
					iferr (targetData.Append(tdata))
					{
						DiagnosticOutput("Could not append target to targetData array!");
						return;
					}
				}
			}
		}
	}

	// Avoid Geometry
	if (((iAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_SOFT && fAvoidGeoWeight > 0.0) || iAvoidGeoMode == OFLOCK_AVOIDGEO_MODE_HARD) && fAvoidGeoDist > 0.0 && boAvoidGeoLink && boAvoidGeoLink->GetType() == Opolygon && ToPoly(boAvoidGeoLink)->GetPolygonCount() > 0)
	{
		rulemask |= Flock::RULEFLAGS::AVOIDGEO;

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
	if (fRepellGlobalWeight > 0.0 && inexRepell && iRepellerCount > 0)
	{
		rulemask |= Flock::RULEFLAGS::REPELL;

		BaseObject *opListItem = nullptr;
		BaseContainer *tc = nullptr;

		repellerData.Flush();
		for (Int32 i = 0; i < iRepellerCount; ++i)
		{
			opListItem = static_cast<BaseObject*>(inexRepell->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				tc = opListItem->GetDataInstance();
				if (tc && tc->GetBool(OFLOCKREPELLER_ENABLED))
				{
					Flock::RepellerData rdata(tc->GetFloat(OFLOCKREPELLER_WEIGHT, 1.0), tc->GetFloat(OFLOCKREPELLER_RADIUS, 0.0), opListItem->GetMg().off);
					rdata.radius *= rdata.radius; // Square radius
					rdata.radiusI = maxon::Inverse(rdata.radius); // Calculate inverse radius
					iferr (repellerData.Append(rdata))
					{
						GePrint(err.GetMessage());
						return;
					}
				}
			}
		}
	}


	// Iterate particles
	for (Int32 i = 0; i < pcnt; ++i)
	{
		// Store a ref to the current Particle and current BaseParticle, so we don't have to
		// do all the pointer arithmetics for pp[i].
		Particle &currentParticle = pp[i];
		BaseParticle &currentBaseParticle = ss[i];

		// Skip unwanted particles
		if (!(currentParticle.bits&PARTICLEFLAGS::VISIBLE))
			continue;

		// Reset values
		vParticleDir = vCenterflockDir = vNeighborDir = vMatchVelocityDir = Vector();
		iCount = 0;

		/* ------------------- Collect particle interaction data --------------------------- */

		// Iterate other particles
		for (Int32 j = 0; j < pcnt; ++j)
		{
			// Store a ref to the current other particle, so we don't have to
			// do all the pointer arithmetics for pp[j].
			Particle &currentOtherParticle = pp[j];

			// Skip unwanted particles
			if (!(currentOtherParticle.bits&PARTICLEFLAGS::VISIBLE) || i == j)
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
			if (rulemask&Flock::RULEFLAGS::CENTER)
			{
				vCenterflockDir += currentOtherParticle.off;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&Flock::RULEFLAGS::NEIGHBORDIST)
			{
				if (fNeighborDist < fNeighborMinDist)
					vNeighborDir -= vNeighborDiff;
			}

			// Match Velocity
			// --------------
			if (rulemask&Flock::RULEFLAGS::MATCHVELO)
			{
				vMatchVelocityDir += currentOtherParticle.v3;
			}

			// Increase counter of considered flockmates
			iCount++;
		}

		// If any other particles have been taken into account for the precalculations,
		// apply any particle interaction that took place
		if (iCount > 1)
		{
			/* ------------------- Soft Rules --------------------------- */

			Float fCountI = maxon::Inverse((Float)iCount);

			// Flock Center
			// ------------
			if (rulemask&Flock::RULEFLAGS::CENTER)
			{
				vParticleDir += (vCenterflockDir * fCountI - currentParticle.off) * fCenterflockWeight;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&Flock::RULEFLAGS::NEIGHBORDIST)
			{
				vParticleDir += vNeighborDir * fNeighborWeight;
			}

			// Match Velocity
			// --------------
			if (rulemask&Flock::RULEFLAGS::MATCHVELO)
			{
				vParticleDir += ((vMatchVelocityDir * fCountI) - currentParticle.v3) * fMatchVelocityWeight;
			}
		}

		// Target
		// ------
		if (rulemask&Flock::RULEFLAGS::TARGET)
		{
			for (maxon::BaseArray<Flock::TargetData>::Iterator target = targetData.Begin(); target != targetData.End(); ++target)
			{
				Vector dist = target->position - currentParticle.off;
				Float distLength = dist.GetSquaredLength();
				if (target->infinite)
				{
					vParticleDir += dist * target->weight * fTargetGlobalWeight;
				}
				else if (distLength < target->radius)
				{
					vParticleDir += dist * (1.0 - distLength * target->radiusI) * target->weight * fTargetGlobalWeight;
				}
			}
		}

		// Turbulence
		// ----------
		if (rulemask&Flock::RULEFLAGS::TURBULENCE)
		{
			vParticleDir += Vector(SNoise(fTurbulenceScale * currentParticle.off, fTurbulenceTime), SNoise(fTurbulenceScale * currentParticle.off + vTurbulenceAdd1, fTurbulenceTime), SNoise(fTurbulenceScale * currentParticle.off + vTurbulenceAdd2, fTurbulenceTime)) * fTurbulenceWeight;
		}

		// Repell
		// ------
		if (rulemask&Flock::RULEFLAGS::REPELL)
		{
			for (maxon::BaseArray<Flock::RepellerData>::Iterator repeller = repellerData.Begin(); repeller != repellerData.End(); ++repeller)
			{
				Vector dist = repeller->position - currentParticle.off;
				Float distLength = dist.GetSquaredLength();
				if (distLength < repeller->radius)
					vParticleDir -= dist * (1.0 - distLength * repeller->radiusI) * repeller->weight * fRepellGlobalWeight;
			}
		}

		// Add resulting direction to current velocity
		vParticleDir += currentParticle.v3;


		// Level Flight
		// ------------
		if (rulemask&Flock::RULEFLAGS::LEVELFLIGHT)
		{
			vParticleDir.y -= vParticleDir.y * fLevelFlightWeight;
		}

		// Avoid Geometry
		// --------------
		if (rulemask&Flock::RULEFLAGS::AVOIDGEO)
		{
			if (_geoAvoidanceCollider->Intersect(mAvoidGeoI * currentParticle.off, mAvoidGeoI * !vParticleDir, fAvoidGeoDist))
			{
				GeRayColResult colliderResult;

				if (_geoAvoidanceCollider->GetNearestIntersection(&colliderResult))
				{
					// Mixval: Range of [AvoidGeoDistance -> 0.0] mapped to [0.0 -> 1.0]
					fAvoidGeoMixval = 1.0 - colliderResult.distance * fAvoidGeoDistI;

					// Direction pointing away from surface
					// Just the normalized shading normal in global space
					Vector awayFromSurface(mAvoidGeo.sqmat * !colliderResult.s_normal);

					switch (iAvoidGeoMode)
					{
						case OFLOCK_AVOIDGEO_MODE_SOFT:
							// Add the new direction to particle velocity, weighted by mixval and user weight value
							vParticleDir = vParticleDir + awayFromSurface * vParticleDir.GetLength() * fAvoidGeoMixval * fAvoidGeoWeight;
							break;
							
						default:
						case OFLOCK_AVOIDGEO_MODE_HARD:
							// Blend between current velocity and new direction, weighted by mixval
							vParticleDir = Blend(awayFromSurface * vParticleDir.GetLength(), vParticleDir, fAvoidGeoMixval);
							break;
					}
				}
			}
		}


		// Speed Limits
		// ------------
		if (rulemask&Flock::RULEFLAGS::SPEEDLIMIT)
		{
			fSpeed = vParticleDir.GetSquaredLength() * diff;
			switch (iSpeedMode)
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
					break;
				}

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
					break;
				}
			}
		}

		// Add resulting velocity, apply overall weight
		currentBaseParticle.v += Blend(currentParticle.v3, vParticleDir, fWeight);

		currentBaseParticle.count++;
	}
}


Bool RegisterFlockModifier()
{
	return RegisterObjectPlugin(Flock::ID_OFLOCKMODIFIER, GeLoadString(IDS_OFLOCK), OBJECT_PARTICLEMODIFIER, FlockModifier::Alloc, "Oflock"_s, AutoBitmap("Oflock.tif"_s), 0);
}
