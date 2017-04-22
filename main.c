#include "rdp.h"
#include "gfx_1.3.h"
#include "msg.h"
#include "screen.h"

extern const int screen_width = 1024, screen_height = 768;

FILE* zeldainfo = 0;
int ProcessDListShown = 0;
extern int SaveLoaded;
extern UINT32 command_counter;

extern GFX_INFO gfx;

 
EXPORT void CALL CaptureScreen ( char * Directory )
{
}

 
EXPORT void CALL ChangeWindow (void)
{
}

 
EXPORT void CALL CloseDLL (void)
{
	
}

 
EXPORT void CALL DllAbout ( HWND hParent )
{
	msg_warning("angrylion's RDP, unpublished beta. MESS source code used.");
}

 
EXPORT void CALL DllConfig ( HWND hParent )
{
	msg_warning("Nothing to configure");
}

 
EXPORT void CALL DllTest ( HWND hParent )
{
}


EXPORT void CALL ReadScreen(void **dest, long *width, long *height)
{
}

 
EXPORT void CALL DrawScreen (void)
{
}

 
EXPORT void CALL GetDllInfo ( PLUGIN_INFO * PluginInfo )
{
  PluginInfo->Version = 0x0103;
  PluginInfo->Type  = PLUGIN_TYPE_GFX;
  sprintf (PluginInfo->Name, "My little plugin");
  
  
  
  PluginInfo->NormalMemory = TRUE;  
  PluginInfo->MemoryBswaped = TRUE; 
}

 
GFX_INFO gfx;

EXPORT BOOL CALL InitiateGFX (GFX_INFO Gfx_Info)
{
  gfx = Gfx_Info;
  
  return TRUE;
}

 
EXPORT void CALL MoveScreen (int xpos, int ypos)
{
	screen_move();
}

 
EXPORT void CALL ProcessDList(void)
{
	if (!ProcessDListShown)
	{
		msg_warning("ProcessDList");
		ProcessDListShown = 1;
	}
}

 
EXPORT void CALL ProcessRDPList(void)
{
	
	
	
	
	
	
	rdp_process_list();
	return;
}

 
EXPORT void CALL RomClosed (void)
{
	rdp_close();
	screen_close();

	SaveLoaded = 1;
	command_counter = 0;
}

 
EXPORT void CALL RomOpen (void)
{
	screen_init(PRESCALE_WIDTH, PRESCALE_HEIGHT, screen_width, screen_height);
	rdp_init();


}

 
EXPORT void CALL ShowCFB (void)
{
	rdp_update();
}

 

EXPORT void CALL UpdateScreen (void)
{
	rdp_update();
	
	
	
	
}

 
EXPORT void CALL ViStatusChanged (void)
{
}

 
EXPORT void CALL ViWidthChanged (void)
{
}


 
EXPORT void CALL FBWrite(DWORD addr, DWORD val)
{
	
}
 
EXPORT void CALL FBWList(FrameBufferModifyEntry *plist, DWORD size)
{
}

 
EXPORT void CALL FBRead(DWORD addr)
{
}


EXPORT void CALL FBGetFrameBufferInfo(void *pinfo)
{
}

