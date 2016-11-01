/////////////////////////////////////////////////////////////
// CINEMA 4D SDK                                           //
/////////////////////////////////////////////////////////////
// (c) 1989-2011 MAXON Computer GmbH, all rights reserved  //
/////////////////////////////////////////////////////////////


#include "c4d.h"

Bool RegisterFlockModifier();
Bool RegisterFlockTarget();
Bool RegisterFlockRepeller();

C4D_CrashHandler old_handler;

void SDKCrashHandler(CHAR *crashinfo)
{
	// don't forget to call the original handler!!!
	if (old_handler) (*old_handler)(crashinfo);
}

Bool PluginStart(void)
{
	if (!RegisterFlockModifier()) return FALSE;
	if (!RegisterFlockTarget()) return FALSE;
	if (!RegisterFlockRepeller()) return FALSE;

	return TRUE;
}

void PluginEnd(void)
{
}

Bool PluginMessage(LONG id, void *data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			if (!resource.Init()) return FALSE; // don't start plugin without resource

			return TRUE;

		case C4DMSG_PRIORITY:
			return TRUE;

		case C4DPL_BUILDMENU:
			break;

		case C4DPL_COMMANDLINEARGS:
			break;

		case C4DPL_EDITIMAGE:
			return FALSE;
	}

	return FALSE;
}
