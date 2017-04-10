#include "c4d.h"
#include "c4d_symbols.h"
#include "Oflocktarget.h"
#include "helpers.h"
#include "main.h"


class FlockTarget : public ObjectData
{
	INSTANCEOF(FlockTarget, ObjectData)

public:
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
	virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);

	static NodeData *Alloc(void)
	{
		return NewObjClear(FlockTarget);
	}
};


Bool FlockTarget::Init(GeListNode *node)
{
	if (!node)
		return false;
	
	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;

	bc->SetBool(OFLOCKTARGET_ENABLED, true);
	bc->SetFloat(OFLOCKTARGET_WEIGHT, 1.0);
	bc->SetFloat(OFLOCKTARGET_RADIUS, 50.0);
	bc->SetBool(OFLOCKTARGET_RADIUS_INFINITE, true);

	return SUPER::Init(node);
}

Bool FlockTarget::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
	if (!node)
		return false;
	
	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;

	switch (id[0].id)
	{
		case OFLOCKTARGET_RADIUS_INFINITE:
		case OFLOCKTARGET_WEIGHT:
			return bc->GetBool(OFLOCKTARGET_ENABLED, false);
			break;

		case OFLOCKTARGET_RADIUS:
			return !bc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, true) && bc->GetBool(OFLOCKTARGET_ENABLED, false);
			break;
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

DRAWRESULT FlockTarget::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
	if (drawpass != DRAWPASS_OBJECT)
		return DRAWRESULT_SKIP;
	
	if (!op || !bd || !bh)
		return DRAWRESULT_ERROR;

	BaseContainer* bc = op->GetDataInstance();
	if (!bc)
		return DRAWRESULT_ERROR;

	bd->SetPen(COLOR_FLOCKTARGET * bc->GetFloat(OFLOCKTARGET_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	if (bc->GetBool(OFLOCKTARGET_RADIUS_INFINITE))
	{
		Draw3DCross(bd, 50.0);
	}
	else
	{
		DrawSphere(bd, (Float32)bc->GetFloat(OFLOCKTARGET_RADIUS, 50.0));
	}
	bd->SetMatrix_Matrix(nullptr, Matrix());

	return DRAWRESULT_OK;
}


/****************************************************************************
 * Register Plugin Object
 ****************************************************************************/
Bool RegisterFlockTarget()
{
	return RegisterObjectPlugin(ID_OFLOCKTARGET, GeLoadString(IDS_OFLOCKTARGET), 0, FlockTarget::Alloc, "Oflocktarget", AutoBitmap("Oflocktarget.tif"), 0);
}
