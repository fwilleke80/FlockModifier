#include "lib_description.h"

#include "c4d_baseobject.h"
#include "c4d_objectdata.h"
#include "c4d_resource.h"
#include "c4d_basecontainer.h"
#include "c4d_basebitmap.h"

#include "helpers.h"
#include "main.h"

#include "c4d_symbols.h"
#include "oflocktarget.h"


///
/// \brief This class implements a simple object with some viewport drawing. The FlockModifier will recognize it as an attractor.
///
class FlockTarget : public ObjectData
{
	INSTANCEOF(FlockTarget, ObjectData)

public:
	virtual Bool Init(GeListNode* node) override;
	virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) override;
	virtual DRAWRESULT Draw(BaseObject* op, DRAWPASS drawpass, BaseDraw* bd, BaseDrawHelp* bh) override;

	static NodeData *Alloc()
	{
		return NewObjClear(FlockTarget);
	}
};


Bool FlockTarget::Init(GeListNode* node)
{
	if (!node)
		return false;

	BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	dataRef.SetBool(OFLOCKTARGET_ENABLED, true);
	dataRef.SetFloat(OFLOCKTARGET_WEIGHT, 1.0);
	dataRef.SetFloat(OFLOCKTARGET_RADIUS, 50.0);
	dataRef.SetBool(OFLOCKTARGET_RADIUS_INFINITE, true);

	return SUPER::Init(node);
}

Bool FlockTarget::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc)
{
	if (!node)
		return false;

	const BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	switch (id[0].id)
	{
		case OFLOCKTARGET_RADIUS_INFINITE:
		case OFLOCKTARGET_WEIGHT:
			return dataRef.GetBool(OFLOCKTARGET_ENABLED, false);
			break;

		case OFLOCKTARGET_RADIUS:
			return !dataRef.GetBool(OFLOCKTARGET_RADIUS_INFINITE, true) && dataRef.GetBool(OFLOCKTARGET_ENABLED, false);
			break;
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

DRAWRESULT FlockTarget::Draw(BaseObject* op, DRAWPASS drawpass, BaseDraw* bd, BaseDrawHelp* bh)
{
	if (drawpass != DRAWPASS::OBJECT)
		return DRAWRESULT::SKIP;

	if (!op || !bd || !bh)
		return DRAWRESULT::FAILURE;

	const BaseContainer& dataRef = op->GetDataInstanceRef();

	bd->SetPen(Flock::COLOR_FLOCKTARGET * dataRef.GetFloat(OFLOCKTARGET_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	if (dataRef.GetBool(OFLOCKTARGET_RADIUS_INFINITE))
		Flock::Draw3dCross(bd, 50.0);
	else
		Flock::DrawSphere(bd, dataRef.GetFloat(OFLOCKTARGET_RADIUS, 50.0));

	bd->SetMatrix_Matrix(nullptr, Matrix());

	return DRAWRESULT::OK;
}


Bool RegisterFlockTarget()
{
	return RegisterObjectPlugin(Flock::ID_OFLOCKTARGET, GeLoadString(IDS_OFLOCKTARGET), 0, FlockTarget::Alloc, "oflocktarget"_s, AutoBitmap("oflocktarget.tif"_s), 0);
}
