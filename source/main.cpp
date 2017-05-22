#include "c4d.h"
#include "main.h"


#define PLUGIN_VERSION  String("0.8")


Bool PluginStart()
{
	GePrint("FlockModifier " + PLUGIN_VERSION);
	
	if (!RegisterFlockModifier())
		return false;
	if (!RegisterFlockTarget())
		return false;
	if (!RegisterFlockRepeller())
		return false;

	return true;
}

void PluginEnd()
{ }

Bool PluginMessage(Int32 id, void *data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			return resource.Init(); // don't start plugin without resource

		case C4DMSG_PRIORITY:
			return true;
	}

	return false;
}
