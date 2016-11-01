#ifndef _FLOCKHELPERS_H_
#define _FLOCKHELPERS_H_


#define ID_OFLOCKMODIFIER	1029168
#define ID_OFLOCKREPELLER	1029184
#define ID_OFLOCKTARGET		1029185


#define COLOR_FLOCKTARGET		Vector(RCO 0.5, RCO 1.0, RCO 0.5)
#define COLOR_FLOCKREPELLER	Vector(RCO 1.0, RCO 0.3, RCO 0.3)


enum RULEFLAGS
{
	RULEFLAGS_0							=	0,
	RULEFLAGS_CENTER				= (1<<0),
	RULEFLAGS_NEIGHBORDIST	= (1<<1),
	RULEFLAGS_MATCHVELO			= (1<<2),
	RULEFLAGS_TARGET				= (1<<3),
	RULEFLAGS_LEVELFLIGHT		= (1<<4),
	RULEFLAGS_AVOIDGEO			= (1<<5),
	RULEFLAGS_TURBULENCE		= (1<<6),
	RULEFLAGS_SPEEDLIMIT		= (1<<7),
	RULEFLAGS_REPELL				= (1<<8)
};


struct TargetData
{
	Real		_weight;
	Real		_radius;
	Bool		_infinite;
	Vector	_position;

	TargetData() : 
		_weight(RCO 0.0),
		_radius(RCO 0.0),
		_infinite(FALSE),
		_position(RCO 0.0)
	{	}
};


struct RepellerData
{
	Real	_weight;
	Real	_radius;
	Vector	_position;

	RepellerData() : 
		_weight(RCO 0.0),
		_radius(RCO 0.0),
		_position(RCO 0.0)
	{	}
};


inline void DrawSphere(BaseDraw* bd, Real radius)
{
	if (!bd) return;

	Matrix mc;
	Vector v(DC);

	mc.Scale(radius);
	bd->DrawCircle(mc);
	v = mc.v3; mc.v3 = mc.v2; mc.v2 = v;
	bd->DrawCircle(mc);
	v = mc.v1; mc.v1 = mc.v3; mc.v3 = v;
	bd->DrawCircle(mc);
}


inline void Draw3DCross(BaseDraw* bd, Real length)
{
	if (!bd) return;

	Matrix mc;
	Vector v(DC);

	bd->DrawLine(Vector(RCO 0.0, RCO length, RCO 0.0), Vector(RCO 0.0, -length, RCO 0.0), 0);
	bd->DrawLine(Vector(length, RCO 0.0, RCO 0.0), Vector(-length, RCO 0.0, RCO 0.0), 0);
	bd->DrawLine(Vector(RCO 0.0, RCO 0.0, RCO length), Vector(RCO 0.0, RCO 0.0, -length), 0);
}


#endif
