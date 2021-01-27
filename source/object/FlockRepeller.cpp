#include "lib_description.h"

#include "c4d_baseobject.h"
#include "c4d_objectdata.h"
#include "c4d_resource.h"
#include "c4d_basecontainer.h"
#include "c4d_basebitmap.h"

#include "helpers.h"
#include "main.h"

#include "c4d_symbols.h"
#include "oflockrepeller.h"


///
/// \brief This class implements a imple object with some viewport drawing. The FlockModifier will recognize it as a repeller.
///
class FlockRepeller : public ObjectData
{
	INSTANCEOF(FlockRepeller, ObjectData)

public:
	virtual Bool Init(GeListNode* node);
	virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);
	virtual DRAWRESULT Draw(BaseObject* op, DRAWPASS drawpass, BaseDraw* bd, BaseDrawHelp* bh);

	static NodeData* Alloc()
	{
		return NewObjClear(FlockRepeller);
	}
};


Bool FlockRepeller::Init(GeListNode* node)
{
	if (!node)
		return false;

	BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	dataRef.SetBool(OFLOCKREPELLER_ENABLED, true);
	dataRef.SetFloat(OFLOCKREPELLER_WEIGHT, 1.0);
	dataRef.SetFloat(OFLOCKREPELLER_RADIUS, 75.0);

	return SUPER::Init(node);
}

Bool FlockRepeller::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc)
{
	if (!node)
		return false;

	const BaseContainer& dataRef = static_cast<BaseObject*>(node)->GetDataInstanceRef();

	switch (id[0].id)
	{
		case OFLOCKREPELLER_RADIUS:
		case OFLOCKREPELLER_WEIGHT:
			return dataRef.GetBool(OFLOCKREPELLER_ENABLED, false);
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

DRAWRESULT FlockRepeller::Draw(BaseObject* op, DRAWPASS drawpass, BaseDraw* bd, BaseDrawHelp* bh)
{
	if (drawpass != DRAWPASS::OBJECT)
		return DRAWRESULT::SKIP;

	if (!op || !bd || !bh)
		return DRAWRESULT::FAILURE;

	const BaseContainer& dataRef = op->GetDataInstanceRef();

	bd->SetPen(Flock::COLOR_FLOCKREPELLER * dataRef.GetFloat(OFLOCKREPELLER_WEIGHT));

	bd->SetMatrix_Matrix(op, bh->GetMg());
	Flock::DrawSphere(bd, dataRef.GetFloat(OFLOCKREPELLER_RADIUS));
	bd->SetMatrix_Matrix(nullptr, Matrix());

	return DRAWRESULT::OK;
}


Bool RegisterFlockRepeller()
{
	return RegisterObjectPlugin(Flock::ID_OFLOCKREPELLER, GeLoadString(IDS_OFLOCKREPELLER), 0, FlockRepeller::Alloc, "oflockrepeller"_s, AutoBitmap("oflockrepeller.tif"_s), 0);
}
