#include "c4d.h"
#include "c4d_symbols.h"
#include "Oflocktarget.h"
#include "helpers.h"


class FlockTarget : public ObjectData
{
	INSTANCEOF(FlockTarget, ObjectData)

public:
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc);
	virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);

	static NodeData *Alloc(void) { return gNew FlockTarget; }
};


Bool FlockTarget::Init( GeListNode *node )
{
	BaseObject		*op   = (BaseObject*)node;
	BaseContainer *bc = op->GetDataInstance();
	if (!bc) return FALSE;

	bc->SetBool(OFLOCKTARGET_ENABLED, TRUE);
	bc->SetReal(OFLOCKTARGET_WEIGHT, RCO 1.0);
	bc->SetReal(OFLOCKTARGET_RADIUS, RCO 50.0);
	bc->SetBool(OFLOCKTARGET_RADIUS_INFINITE, TRUE);

	return SUPER::Init(node);
}

Bool FlockTarget::GetDEnabling( GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc )
{
	BaseContainer *bc = ((BaseObject*)node)->GetDataInstance();
	if (!bc) return FALSE;

	switch (id[0].id)
	{
		case OFLOCKTARGET_RADIUS_INFINITE:
		case OFLOCKTARGET_WEIGHT:
			return bc->GetBool(OFLOCKTARGET_ENABLED, FALSE);
			break;

		case OFLOCKTARGET_RADIUS:
			return !bc->GetBool(OFLOCKTARGET_RADIUS_INFINITE, TRUE) && bc->GetBool(OFLOCKTARGET_ENABLED, FALSE);
			break;
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

DRAWRESULT FlockTarget::Draw( BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh )
{
	if (drawpass != DRAWPASS_OBJECT) return DRAWRESULT_OK;
	if (!op || !bd || !bh) return DRAWRESULT_ERROR;

	BaseContainer* bc = op->GetDataInstance();
	if (!bc) return DRAWRESULT_ERROR;

	bd->SetPen(COLOR_FLOCKTARGET * bc->GetReal(OFLOCKTARGET_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	if (bc->GetBool(OFLOCKTARGET_RADIUS_INFINITE))
	{
		Draw3DCross(bd, RCO 50.0);
	}
	else
	{
		DrawSphere(bd, bc->GetReal(OFLOCKTARGET_RADIUS, RCO 50.0));
	}
	bd->SetMatrix_Matrix(NULL, Matrix());

	return DRAWRESULT_OK;
}


/****************************************************************************
 * Register Plugin Object
 ****************************************************************************/
Bool RegisterFlockTarget(void)
{
	return RegisterObjectPlugin(ID_OFLOCKTARGET, GeLoadString(IDS_OFLOCKTARGET), 0, FlockTarget::Alloc, "Oflocktarget", AutoBitmap("Oflocktarget.tif"), 0);
}
