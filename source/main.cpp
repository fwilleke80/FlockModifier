#include "c4d.h"
#include "main.h"


#define PLUGIN_VERSION	String("0.7")


Bool PluginStart()
{
	GePrint("FlockModifier " + PLUGIN_VERSION);
	
	if (!RegisterFlockModifier()) return false;
	if (!RegisterFlockTarget()) return false;
	if (!RegisterFlockRepeller()) return false;

	return true;
}

void PluginEnd(void)
{
}

Bool PluginMessage(Int32 id, void *data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			if (!resource.Init()) return false; // don't start plugin without resource
			return true;

		case C4DMSG_PRIORITY:
			return true;

		case C4DPL_BUILDMENU:
			break;

		case C4DPL_COMMANDLINEARGS:
			break;

		case C4DPL_EDITIMAGE:
			return false;
	}

	return false;
}
