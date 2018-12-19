#include "c4d.h"
#include "main.h"


#define PLUGIN_VERSION  String("FlockModifier 0.7.6")


Bool PluginStart()
{
	GePrint(PLUGIN_VERSION);
	
	if (!RegisterFlockModifier())
		return false;
	if (!RegisterFlockTarget())
		return false;
	if (!RegisterFlockRepeller())
		return false;

	return true;
}

void PluginEnd()
{
}

Bool PluginMessage(Int32 id, void *data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			return g_resource.Init(); // don't start plugin without resource

		case C4DMSG_PRIORITY:
			return true;
	}

	return false;
}
