#include "c4d.h"
#include "c4d_symbols.h"
#include "Oflockrepeller.h"
#include "helpers.h"


class FlockRepeller : public ObjectData
{
	INSTANCEOF(FlockRepeller, ObjectData)

public:
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc);
	virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);

	static NodeData *Alloc(void) { return gNew FlockRepeller; }
};


Bool FlockRepeller::Init( GeListNode *node )
{
	BaseObject		*op   = (BaseObject*)node;
	BaseContainer *bc = op->GetDataInstance();
	if (!bc) return FALSE;

	bc->SetBool(OFLOCKREPELLER_ENABLED, TRUE);
	bc->SetReal(OFLOCKREPELLER_WEIGHT, RCO 1.0);
	bc->SetReal(OFLOCKREPELLER_RADIUS, RCO 75.0);

	return SUPER::Init(node);
}

Bool FlockRepeller::GetDEnabling( GeListNode *node, const DescID &id,const GeData &t_data,DESCFLAGS_ENABLE flags,const BaseContainer *itemdesc )
{
	BaseContainer *bc = ((BaseObject*)node)->GetDataInstance();
	if (!bc) return FALSE;

	switch (id[0].id)
	{
	case OFLOCKREPELLER_RADIUS:
	case OFLOCKREPELLER_WEIGHT:
		return bc->GetBool(OFLOCKREPELLER_ENABLED, FALSE);
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

DRAWRESULT FlockRepeller::Draw( BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh )
{
	if (drawpass != DRAWPASS_OBJECT) return DRAWRESULT_OK;
	if (!op || !bd || !bh) return DRAWRESULT_ERROR;

	BaseContainer* bc = op->GetDataInstance();
	if (!bc) return DRAWRESULT_ERROR;

	bd->SetPen(COLOR_FLOCKREPELLER * bc->GetReal(OFLOCKREPELLER_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	DrawSphere(bd, bc->GetReal(OFLOCKREPELLER_RADIUS));
	bd->SetMatrix_Matrix(NULL, Matrix());

	return DRAWRESULT_OK;
}


/****************************************************************************
 * Register Plugin Object
 ****************************************************************************/
Bool RegisterFlockRepeller(void)
{
	return RegisterObjectPlugin(ID_OFLOCKREPELLER, GeLoadString(IDS_OFLOCKREPELLER), 0, FlockRepeller::Alloc, "Oflockrepeller", AutoBitmap("Oflockrepeller.tif"), 0);
}
