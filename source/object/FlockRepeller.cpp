#include "c4d.h"
#include "c4d_symbols.h"
#include "Oflockrepeller.h"
#include "helpers.h"
#include "main.h"


class FlockRepeller : public ObjectData
{
	INSTANCEOF(FlockRepeller, ObjectData)
	
public:
	virtual Bool Init(GeListNode *node);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
	virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);
	
	static NodeData *Alloc()
	{
		return NewObjClear(FlockRepeller);
	}
};


//
// Initialize attributes
//
Bool FlockRepeller::Init(GeListNode *node)
{
	if (!node)
		return false;
	
	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;

	bc->SetBool(OFLOCKREPELLER_ENABLED, true);
	bc->SetFloat(OFLOCKREPELLER_WEIGHT, 1.0);
	bc->SetFloat(OFLOCKREPELLER_RADIUS, 75.0);

	return SUPER::Init(node);
}

//
// Enable/Disable attributes
//
Bool FlockRepeller::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
	if (!node)
		return false;
	
	BaseContainer *bc = (static_cast<BaseObject*>(node))->GetDataInstance();
	if (!bc)
		return false;

	switch (id[0].id)
	{
		case OFLOCKREPELLER_RADIUS:
		case OFLOCKREPELLER_WEIGHT:
			return bc->GetBool(OFLOCKREPELLER_ENABLED, false);
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

//
// Draw viewport representation
//
DRAWRESULT FlockRepeller::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
	if (drawpass != DRAWPASS::OBJECT)
		return DRAWRESULT::SKIP;
	
	if (!op || !bd || !bh)
		return DRAWRESULT::FAILURE;

	BaseContainer* bc = op->GetDataInstance();
	if (!bc)
		return DRAWRESULT::FAILURE;

	bd->SetPen(COLOR_FLOCKREPELLER * bc->GetFloat(OFLOCKREPELLER_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	DrawSphere(bd, (Float32)bc->GetFloat(OFLOCKREPELLER_RADIUS));
	bd->SetMatrix_Matrix(nullptr, Matrix());

	return DRAWRESULT::OK;
}


//
// Register Plugin Object
//
Bool RegisterFlockRepeller()
{
	return RegisterObjectPlugin(ID_OFLOCKREPELLER, GeLoadString(IDS_OFLOCKREPELLER), 0, FlockRepeller::Alloc, "Oflockrepeller"_s, AutoBitmap("Oflockrepeller.tif"_s), 0);
}
