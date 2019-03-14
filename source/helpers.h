#ifndef HELPERS_H__
#define HELPERS_H__

#include "c4d.h"

const Int32 ID_OFLOCKMODIFIER = 1029168;
const Int32 ID_OFLOCKREPELLER = 1029184;
const Int32 ID_OFLOCKTARGET = 1029185;

const Vector COLOR_FLOCKTARGET(0.5, 1.0, 0.5);
const Vector COLOR_FLOCKREPELLER(1.0, 0.3, 0.3);


/// Flags to indicate which rules are active
enum RULEFLAGS
{
	RULEFLAGS_NONE          = 0,
	RULEFLAGS_CENTER        = (1<<0),
	RULEFLAGS_NEIGHBORDIST  = (1<<1),
	RULEFLAGS_MATCHVELO     = (1<<2),
	RULEFLAGS_TARGET        = (1<<3),
	RULEFLAGS_LEVELFLIGHT   = (1<<4),
	RULEFLAGS_AVOIDGEO      = (1<<5),
	RULEFLAGS_TURBULENCE    = (1<<6),
	RULEFLAGS_SPEEDLIMIT    = (1<<7),
	RULEFLAGS_REPELL        = (1<<8)
} MAXON_ENUM_FLAGS(RULEFLAGS);


/// Data for a FlockTarget
struct TargetData
{
	Float   _weight;    ///< Weight of this target
	Float   _radius;    ///< Radius of this target
	Float   _radiusI;   ///< Inverted _radius (for performance reasons)
	Bool    _infinite;  ///< Is target radius infinite?
	Vector  _position;  ///< Global position of target

	TargetData() :
		_weight(0.0),
		_radius(0.0),
		_infinite(false),
		_position(0.0)
	{ }

	TargetData(Float weight, Float radius, Bool infinite, const Vector &position) :
		_weight(weight),
		_radius(radius),
		_infinite(infinite),
		_position(position)
	{ }
};


/// Data for a FlockRepeller
struct RepellerData
{
	Float   _weight;    ///< Weight of this repeller
	Float   _radius;    ///< Radius of this repeller
	Float   _radiusI;   ///< Inverted _radius (for performance reasons)
	Vector  _position;  ///< Global position of repeller

	RepellerData() :
		_weight(0.0),
		_radius(0.0),
		_position(0.0)
	{ }

	RepellerData(Float weight, Float radius, const Vector &position) :
		_weight(weight),
		_radius(radius),
		_position(position)
	{ }
};


inline void DrawSphere(BaseDraw *bd, Float radius)
{
	if (!bd)
		return;

	Matrix mc;
	Vector v(DC);

	mc = mc * radius;
	bd->DrawCircle(mc);
	v = mc.sqmat.v3; mc.sqmat.v3 = mc.sqmat.v2; mc.sqmat.v2 = v;
	bd->DrawCircle(mc);
	v = mc.sqmat.v1; mc.sqmat.v1 = mc.sqmat.v3; mc.sqmat.v3 = v;
	bd->DrawCircle(mc);
}


inline void Draw3DCross(BaseDraw *bd, Float length)
{
	if (!bd)
		return;

	Matrix mc;
	Vector v(DC);

	bd->DrawLine(Vector(0.0, length, 0.0), Vector(0.0, -length, 0.0), 0);
	bd->DrawLine(Vector(length, 0.0, 0.0), Vector(-length, 0.0, 0.0), 0);
	bd->DrawLine(Vector(0.0, 0.0, length), Vector(0.0, 0.0, -length), 0);
}

#endif // HELPERS_H__
