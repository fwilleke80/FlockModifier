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

	const BaseContainer& dataRef = op->GetDataInstanceRef();

	BaseDocument *doc = op->GetDocument();
	if (!doc)
		return;

	// Current document time
	const Float documentTime = doc->GetTime().Get();

	// Overall weight
	Float modifierWeight = dataRef.GetFloat(OFLOCK_WEIGHT, 1.0);

	// Range of sight
	Float neighborSightRadius = Sqr(dataRef.GetFloat(OFLOCK_SIGHT));

	/* ------------------- Set rule mask & prepare some data --------------------------- */

	// No rule used yet
	Flock::RULEFLAGS rulemask = Flock::RULEFLAGS::NONE;

	// Flock Center
	const Float centerflockWeight = dataRef.GetFloat(OFLOCK_CENTER_WEIGHT) * 0.1;
	if (centerflockWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::CENTER;
	}

	// Neighbor Distance
	const Float neighborWeight = dataRef.GetFloat(OFLOCK_NEIGHBORDIST_WEIGHT) * 0.1;
	const Float neighborMinDist = Sqr(dataRef.GetFloat(OFLOCK_NEIGHBORDIST_DIST));
	if (neighborWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::NEIGHBORDIST;
	}

	// Match Flock Velocity
	const Float matchVelocityWeight = dataRef.GetFloat(OFLOCK_MATCHVEL_WEIGHT) * 0.1;
	if (matchVelocityWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::MATCHVELO;
	}

	// Level Flight
	const Float levelFlightWeight = dataRef.GetFloat(OFLOCK_LEVELFLIGHT_WEIGHT);
	if (levelFlightWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::LEVELFLIGHT;
	}

	// Turbulence
	const Float turbulenceWeight = dataRef.GetFloat(OFLOCK_TURBULENCE_WEIGHT) * 10.0;
	const Float turbulenceScale = 0.1 / ClampValue(dataRef.GetFloat(OFLOCK_TURBULENCE_SCALE, 1.0), EPSILON, MAXVALUE_FLOAT);
	const Float turbulenceTime = documentTime * dataRef.GetFloat(OFLOCK_TURBULENCE_FREQUENCY, 1.0);
	static const Vector vTurbulenceAdd1(PI * 1000.0);
	static const Vector vTurbulenceAdd2(-PI05 * 1000.0);
	if (turbulenceWeight > 0.0)
	{
		rulemask |= Flock::RULEFLAGS::TURBULENCE;
	}

	// Speed Limit
	const Int32 speedMode = dataRef.GetInt32(OFLOCK_SPEED_MODE, OFLOCK_SPEED_MODE_SOFT);
	const Float speedWeight = dataRef.GetFloat(OFLOCK_SPEED_WEIGHT);
	const Float speedMin = Sqr(dataRef.GetFloat(OFLOCK_SPEED_MIN) * diff);
	const Float speedMax = Sqr(dataRef.GetFloat(OFLOCK_SPEED_MAX, 100.0) * diff);
	if ((speedMode == OFLOCK_SPEED_MODE_SOFT && speedWeight > 0.0) || speedMode == OFLOCK_SPEED_MODE_HARD)
	{
		rulemask |= Flock::RULEFLAGS::SPEEDLIMIT;
	}

	// Target
	const Float targetGlobalWeight = dataRef.GetFloat(OFLOCK_TARGET_WEIGHT) * 0.1;
	InExcludeData* targetInExcludeList = (InExcludeData*)dataRef.GetCustomDataType(OFLOCK_TARGET_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 targetCount = 0;
	if (targetInExcludeList)
		targetCount = targetInExcludeList->GetObjectCount();
	maxon::BaseArray<Flock::TargetData> targetData;
	if (targetGlobalWeight > 0.0 && targetInExcludeList && targetCount > 0)
	{
		rulemask |= Flock::RULEFLAGS::TARGET;

		targetData.Flush();
		for (Int32 i = 0; i < targetCount; ++i)
		{
			BaseObject* opListItem = static_cast<BaseObject*>(targetInExcludeList->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				const BaseContainer& targetDataRef = opListItem->GetDataInstanceRef();
				if (targetDataRef.GetBool(OFLOCKTARGET_ENABLED))
				{
					Flock::TargetData targetDataItem(targetDataRef.GetFloat(OFLOCKREPELLER_WEIGHT, 1.0), Sqr(targetDataRef.GetFloat(OFLOCKTARGET_RADIUS)), targetDataRef.GetBool(OFLOCKTARGET_RADIUS_INFINITE, true), opListItem->GetMg().off);
					targetDataItem.radiusI = maxon::Inverse(targetDataItem.radius); // Calculate inverse radius
					iferr (targetData.Append(targetDataItem))
					{
						GePrint(err.GetMessage());
						return;
					}
				}
			}
		}
	}

	// Repell
	Float repellGlobalWeight = dataRef.GetFloat(OFLOCK_REPELL_WEIGHT);
	InExcludeData* repellerInExcludeList = (InExcludeData*)dataRef.GetCustomDataType(OFLOCK_REPELL_LINK, CUSTOMDATATYPE_INEXCLUDE_LIST);
	Int32 repellerCount = 0;
	if (repellerInExcludeList)
		repellerCount = repellerInExcludeList->GetObjectCount();
	maxon::BaseArray<Flock::RepellerData> repellerData;
	if (repellGlobalWeight > 0.0 && repellerInExcludeList && repellerCount > 0)
	{
		rulemask |= Flock::RULEFLAGS::REPELL;

		repellerData.Flush();
		for (Int32 i = 0; i < repellerCount; ++i)
		{
			BaseObject* opListItem = static_cast<BaseObject*>(repellerInExcludeList->ObjectFromIndex(doc, i));
			if (opListItem)
			{
				const BaseContainer& repellerDataRef = opListItem->GetDataInstanceRef();
				if (repellerDataRef.GetBool(OFLOCKREPELLER_ENABLED))
				{
					Flock::RepellerData repellerDataItem(repellerDataRef.GetFloat(OFLOCKREPELLER_WEIGHT, 1.0), Sqr(repellerDataRef.GetFloat(OFLOCKREPELLER_RADIUS)), opListItem->GetMg().off);
					repellerDataItem.radiusI = maxon::Inverse(repellerDataItem.radius); // Calculate inverse radius
					iferr (repellerData.Append(repellerDataItem))
					{
						GePrint(err.GetMessage());
						return;
					}
				}
			}
		}
	}

	// Avoid Geometry
	const Int32 avoidGeoMode = dataRef.GetInt32(OFLOCK_AVOIDGEO_MODE, 1);
	const Float avoidGeoWeight = dataRef.GetFloat(OFLOCK_AVOIDGEO_WEIGHT);
	const Float avoidGeoDist = dataRef.GetFloat(OFLOCK_AVOIDGEO_DIST);
	const Float avoidGeoDistI = maxon::Inverse(avoidGeoDist);
	BaseObject* avoidGeoLink = dataRef.GetObjectLink(OFLOCK_AVOIDGEO_LINK, doc);
	Matrix mAvoidGeo(maxon::DONT_INITIALIZE); // Collision geometry global matrix
	Matrix mAvoidGeoI(maxon::DONT_INITIALIZE); // Collision geometry inverse global matrix
	if (((avoidGeoMode == OFLOCK_AVOIDGEO_MODE_SOFT && avoidGeoWeight > 0.0) || avoidGeoMode == OFLOCK_AVOIDGEO_MODE_HARD) && avoidGeoDist > 0.0 && avoidGeoLink && avoidGeoLink->GetType() == Opolygon && ToPoly(avoidGeoLink)->GetPolygonCount() > 0)
	{
		rulemask |= Flock::RULEFLAGS::AVOIDGEO;

		// Lazy-alloc collider
		if (!_geoAvoidanceCollider)
		{
			_geoAvoidanceCollider.Set(GeRayCollider::Alloc());
			if (!_geoAvoidanceCollider)
				return;
		}
		_geoAvoidanceCollider->Init(avoidGeoLink, false);
		mAvoidGeo = avoidGeoLink->GetMg();
		mAvoidGeoI = ~mAvoidGeo;
	}

	// Iterate particles
	for (Int32 i = 0; i < pcnt; ++i)
	{
		// Store a ref to the current Particle and current BaseParticle, so we don't have to
		// do all the pointer arithmetics for pp[i].
		const Particle &currentParticle = pp[i];
		BaseParticle &currentBaseParticle = ss[i];

		// Skip unwanted particles
		if (!(currentParticle.bits&PARTICLEFLAGS::VISIBLE))
			continue;

		// Reset values
		Vector particleDirection; // New direction of particle
		Vector centerflockPosition; // Position of flock center
		Vector flockVelocity; // Aggregated velocity of considered flockmates
		Int32 consideredFlockmatesCount = 0;

		/* ------------------- Collect particle-particle interaction data --------------------------- */

		// Iterate other particles
		Vector neighborDirection(maxon::DONT_INITIALIZE);
		for (Int32 j = 0; j < pcnt; ++j)
		{
			// Store a ref to the current other particle, so we don't have to
			// do all the pointer arithmetics for pp[j].
			const Particle &currentOtherParticle = pp[j];

			// Skip unwanted particles
			if (!(currentOtherParticle.bits&PARTICLEFLAGS::VISIBLE) || i == j)
				continue;

			// General stuff for particle interaction
			// --------------------------------------

			// Get distance to current particle
			const Vector neighborDiff(currentOtherParticle.off - currentParticle.off);
			const Float neighborDistance = neighborDiff.GetSquaredLength();

			// Wo only consider flockmates within sight radius.
			// Skip if particles are too far away from each other.
			if (neighborDistance > neighborSightRadius)
				continue;

			// Flock Center
			// ------------
			if (rulemask&Flock::RULEFLAGS::CENTER)
			{
				// Add position of flockmate
				centerflockPosition += currentOtherParticle.off;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&Flock::RULEFLAGS::NEIGHBORDIST)
			{
				// If neighbor is too close, move into the opposite direction
				// TODO: (Frank) This doesn't look correct. Shouldn't we take the position of all close neighbors into account? Currently, only the last one counts.
				if (neighborDistance < neighborMinDist)
				{
					neighborDirection = -neighborDiff;
				}
			}

			// Match Velocity
			// --------------
			if (rulemask&Flock::RULEFLAGS::MATCHVELO)
			{
				flockVelocity += currentOtherParticle.v3;
			}

			// Increase counter of considered flockmates
			++consideredFlockmatesCount;
		}

		/* ------------------- Apply particle-particle interaction --------------------------- */

		// If any other particles have been taken into account for the precalculations,
		// apply any particle interaction that took place
		if (consideredFlockmatesCount > 1)
		{
			// Compute inverse of considered flockmates count
			const Float iConsideredFlockmatesCount = maxon::Inverse((Float)consideredFlockmatesCount);

			// Flock Center
			// ------------
			if (rulemask&Flock::RULEFLAGS::CENTER)
			{
				particleDirection += (centerflockPosition * iConsideredFlockmatesCount - currentParticle.off) * centerflockWeight;
			}

			// Neighbor Distance
			// -----------------
			if (rulemask&Flock::RULEFLAGS::NEIGHBORDIST)
			{
				particleDirection += neighborDirection * neighborWeight;
			}

			// Match Velocity
			// --------------
			if (rulemask&Flock::RULEFLAGS::MATCHVELO)
			{
				particleDirection += ((flockVelocity * iConsideredFlockmatesCount) - currentParticle.v3) * matchVelocityWeight;
			}
		}

		// Target
		// ------
		if (rulemask&Flock::RULEFLAGS::TARGET)
		{
			for (maxon::BaseArray<Flock::TargetData>::Iterator target = targetData.Begin(); target != targetData.End(); ++target)
			{
				const Vector distance = target->position - currentParticle.off;
				const Float distLength = distance.GetSquaredLength();
				if (target->infinite)
				{
					particleDirection += distance * target->weight * targetGlobalWeight;
				}
				else if (distLength < target->radius)
				{
					particleDirection += distance * (1.0 - distLength * target->radiusI) * target->weight * targetGlobalWeight;
				}
			}
		}

		// Turbulence
		// ----------
		if (rulemask&Flock::RULEFLAGS::TURBULENCE)
		{
			particleDirection += Vector(SNoise(turbulenceScale * currentParticle.off, turbulenceTime), SNoise(turbulenceScale * currentParticle.off + vTurbulenceAdd1, turbulenceTime), SNoise(turbulenceScale * currentParticle.off + vTurbulenceAdd2, turbulenceTime)) * turbulenceWeight;
		}

		// Repell
		// ------
		if (rulemask&Flock::RULEFLAGS::REPELL)
		{
			for (maxon::BaseArray<Flock::RepellerData>::Iterator repeller = repellerData.Begin(); repeller != repellerData.End(); ++repeller)
			{
				const Vector dist = repeller->position - currentParticle.off;
				const Float distLength = dist.GetSquaredLength();
				if (distLength < repeller->radius)
					particleDirection -= dist * (1.0 - distLength * repeller->radiusI) * repeller->weight * repellGlobalWeight;
			}
		}

		// Add current velocity to resulting direction
		particleDirection += currentParticle.v3;

		// Level Flight
		// ------------
		if (rulemask&Flock::RULEFLAGS::LEVELFLIGHT)
		{
			particleDirection.y -= particleDirection.y * levelFlightWeight;
		}

		// Avoid Geometry
		// --------------
		if (rulemask&Flock::RULEFLAGS::AVOIDGEO)
		{
			if (_geoAvoidanceCollider->Intersect(mAvoidGeoI * currentParticle.off, mAvoidGeoI.sqmat * particleDirection.GetNormalized(), avoidGeoDist))
			{
				GeRayColResult colliderResult;

				if (_geoAvoidanceCollider->GetNearestIntersection(&colliderResult))
				{
					// Mixval: Range of [AvoidGeoDistance -> 0.0] mapped to [0.0 -> 1.0]
					const Float avoidGeoUrgency = 1.0 - colliderResult.distance * avoidGeoDistI;

					// Direction pointing away from surface
					// Just the normalized shading normal in global space
					const Vector awayFromSurface(mAvoidGeo.sqmat * colliderResult.s_normal.GetNormalized());

					switch (avoidGeoMode)
					{
						case OFLOCK_AVOIDGEO_MODE_SOFT:
							// Add the new direction to particle velocity, weighted by mixval and user weight value
							particleDirection += awayFromSurface * particleDirection.GetLength() * avoidGeoUrgency * avoidGeoWeight;
							break;
							
						default:
						case OFLOCK_AVOIDGEO_MODE_HARD:
							// Blend between current velocity and new direction, weighted by mixval
							particleDirection = Blend(awayFromSurface * particleDirection.GetLength(), particleDirection, avoidGeoUrgency);
							break;
					}
				}
			}
		}


		// Speed Limits
		// ------------
		// This is calculated last, because the other rules may lead to very large velocities.
		// The speed limit help keep the boids from reaching warp speed.
		if (rulemask&Flock::RULEFLAGS::SPEEDLIMIT)
		{
			const Float speed = particleDirection.GetSquaredLength() * diff;

			switch (speedMode)
			{
				case OFLOCK_SPEED_MODE_SOFT:
				{
					if (speed < speedMin)
					{
						const Float speedRatio = speedMin / FMax(speed, EPSILON);
						particleDirection *= Blend(1.0, speedRatio, speedWeight);
					}
					else if (speed > speedMax)
					{
						const Float speedRatio = speedMax / FMax(speed, EPSILON);
						particleDirection *= Blend(1.0, speedRatio, speedWeight);
					}
					break;
				}

				case OFLOCK_SPEED_MODE_HARD:
				{
					if (speed < speedMin)
					{
						particleDirection = particleDirection.GetNormalized() * speedMin;
					}
					else if (speed > speedMax)
					{
						particleDirection = particleDirection.GetNormalized() * speedMax;
					}
					break;
				}
			}
		}

		// Add resulting velocity, apply overall weight
		currentBaseParticle.v += Blend(currentParticle.v3, particleDirection, modifierWeight);
		++currentBaseParticle.count;
	}
}


Bool RegisterFlockModifier()
{
	return RegisterObjectPlugin(Flock::ID_OFLOCKMODIFIER, GeLoadString(IDS_OFLOCK), OBJECT_PARTICLEMODIFIER, FlockModifier::Alloc, "Oflock"_s, AutoBitmap("Oflock.tif"_s), 0);
}
