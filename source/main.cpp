#include "c4d_resource.h"
#include "c4d_plugin.h"
#include "c4d_general.h"
#include "main.h"


Bool PluginStart()
{
	GePrint("FlockModifier 0.7.8"_s);

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

Bool PluginMessage(Int32 id, void* data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			return g_resource.Init();
	}

	return false;
}
