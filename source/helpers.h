#ifndef HELPERS_H__
#define HELPERS_H__

#include "c4d_basedraw.h"
#include "ge_prepass.h"


namespace Flock
{

	// Plugin IDs
	const Int32 ID_OFLOCKMODIFIER = 1029168;
	const Int32 ID_OFLOCKREPELLER = 1029184;
	const Int32 ID_OFLOCKTARGET = 1029185;

	// Color constants
	const Vector COLOR_FLOCKTARGET = Vector(0.5, 1.0, 0.5);
	const Vector COLOR_FLOCKREPELLER = Vector(1.0, 0.3, 0.3);

	///
	/// \brief Flags to indicate which rules are active
	///
	enum class RULEFLAGS
	{
		NONE         = 0,
		CENTER       = (1<<0),
		NEIGHBORDIST = (1<<1),
		MATCHVELO    = (1<<2),
		TARGET       = (1<<3),
		LEVELFLIGHT  = (1<<4),
		AVOIDGEO     = (1<<5),
		TURBULENCE   = (1<<6),
		SPEEDLIMIT   = (1<<7),
		REPELL       = (1<<8)
	} MAXON_ENUM_FLAGS(RULEFLAGS);

	///
	/// \brief Data for a FlockTarget
	///
	struct TargetData
	{
		Float weight; ///< Weight of this target
		Float radius; ///< Radius of this target
		Float radiusI; ///< Inverted _radius (for performance reasons)
		Bool infinite; ///< Is target radius infinite?
		Vector position; ///< Global position of target

		TargetData() :
		weight(0.0),
		radius(0.0),
		infinite(false),
		position(0.0)
		{}

		TargetData(Float t_weight, Float t_radius, Bool t_infinite, const Vector& t_position) :
		weight(t_weight),
		radius(t_radius),
		infinite(t_infinite),
		position(t_position)
		{}
	};

	///
	/// \brief Data for a FlockRepeller
	///
	struct RepellerData
	{
		Float weight;    ///< Weight of this repeller
		Float radius;    ///< Radius of this repeller
		Float radiusI;   ///< Inverted _radius (for performance reasons)
		Vector position;  ///< Global position of repeller

		RepellerData() :
		weight(0.0),
		radius(0.0),
		position(0.0)
		{}

		RepellerData(Float t_weight, Float t_radius, const Vector& t_position) :
		weight(t_weight),
		radius(t_radius),
		position(t_position)
		{}
	};

	///
	/// \brief Draws a sphere into the viewport
	///
	/// \param[in] bd The BaseDraw to draw in
	/// \param[in] radius The radius of the sphere
	///
	MAXON_ATTRIBUTE_FORCE_INLINE void DrawSphere(BaseDraw* bd, Float radius)
	{
		if (!bd)
			return;

		Matrix circleMatrix;

		circleMatrix = circleMatrix * radius;
		bd->DrawCircle(circleMatrix);

		Vector tmpVector(circleMatrix.sqmat.v3);
		circleMatrix.sqmat.v3 = circleMatrix.sqmat.v2;
		circleMatrix.sqmat.v2 = tmpVector;
		bd->DrawCircle(circleMatrix);

		tmpVector = circleMatrix.sqmat.v1;
		circleMatrix.sqmat.v1 = circleMatrix.sqmat.v3;
		circleMatrix.sqmat.v3 = tmpVector;
		bd->DrawCircle(circleMatrix);
	}

	///
	/// \brief Draws a 3D cross into the viewport
	///
	/// \param[in] bd The BaseDraw to draw in
	/// \param[in] length The length of the cross lines
	///
	MAXON_ATTRIBUTE_FORCE_INLINE void Draw3dCross(BaseDraw* bd, Float length)
	{
		if (!bd)
			return;

		bd->DrawLine(Vector(0.0, length, 0.0), Vector(0.0, -length, 0.0), 0);
		bd->DrawLine(Vector(length, 0.0, 0.0), Vector(-length, 0.0, 0.0), 0);
		bd->DrawLine(Vector(0.0, 0.0, length), Vector(0.0, 0.0, -length), 0);
	}

} // namespace Flock

#endif // HELPERS_H__
