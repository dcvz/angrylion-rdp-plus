#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "wgl_ext.h"

void* IntGetProcAddress(const char *name);

int wgl_ext_EXT_swap_control = 0;
int wgl_ext_ARB_create_context = 0;
int wgl_ext_ARB_create_context_profile = 0;

/* Extension: EXT_swap_control*/
typedef int (CODEGEN_FUNCPTR *PFN_PTRC_WGLGETSWAPINTERVALEXTPROC)(void);
static int CODEGEN_FUNCPTR Switch_GetSwapIntervalEXT(void);
typedef BOOL (CODEGEN_FUNCPTR *PFN_PTRC_WGLSWAPINTERVALEXTPROC)(int);
static BOOL CODEGEN_FUNCPTR Switch_SwapIntervalEXT(int interval);

/* Extension: ARB_create_context*/
typedef HGLRC (CODEGEN_FUNCPTR *PFN_PTRC_WGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
static HGLRC CODEGEN_FUNCPTR Switch_CreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int * attribList);


/* Extension: EXT_swap_control*/
PFN_PTRC_WGLGETSWAPINTERVALEXTPROC _ptrc_wglGetSwapIntervalEXT = Switch_GetSwapIntervalEXT;
PFN_PTRC_WGLSWAPINTERVALEXTPROC _ptrc_wglSwapIntervalEXT = Switch_SwapIntervalEXT;

/* Extension: ARB_create_context*/
PFN_PTRC_WGLCREATECONTEXTATTRIBSARBPROC _ptrc_wglCreateContextAttribsARB = Switch_CreateContextAttribsARB;


/* Extension: EXT_swap_control*/
static int CODEGEN_FUNCPTR Switch_GetSwapIntervalEXT(void)
{
	_ptrc_wglGetSwapIntervalEXT = (PFN_PTRC_WGLGETSWAPINTERVALEXTPROC)IntGetProcAddress("wglGetSwapIntervalEXT");
	return _ptrc_wglGetSwapIntervalEXT();
}

static BOOL CODEGEN_FUNCPTR Switch_SwapIntervalEXT(int interval)
{
	_ptrc_wglSwapIntervalEXT = (PFN_PTRC_WGLSWAPINTERVALEXTPROC)IntGetProcAddress("wglSwapIntervalEXT");
	return _ptrc_wglSwapIntervalEXT(interval);
}


/* Extension: ARB_create_context*/
static HGLRC CODEGEN_FUNCPTR Switch_CreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int * attribList)
{
	_ptrc_wglCreateContextAttribsARB = (PFN_PTRC_WGLCREATECONTEXTATTRIBSARBPROC)IntGetProcAddress("wglCreateContextAttribsARB");
	return _ptrc_wglCreateContextAttribsARB(hDC, hShareContext, attribList);
}



static void ClearExtensionVariables(void)
{
	wgl_ext_EXT_swap_control = 0;
	wgl_ext_ARB_create_context = 0;
	wgl_ext_ARB_create_context_profile = 0;
}

typedef struct wgl_MapTable_s
{
	char *extName;
	int *extVariable;
}wgl_MapTable;

static wgl_MapTable g_mappingTable[3] = 
{
	{"WGL_EXT_swap_control", &wgl_ext_EXT_swap_control},
	{"WGL_ARB_create_context", &wgl_ext_ARB_create_context},
	{"WGL_ARB_create_context_profile", &wgl_ext_ARB_create_context_profile},
};

static void LoadExtByName(const char *extensionName)
{
	wgl_MapTable *tableEnd = &g_mappingTable[3];
	wgl_MapTable *entry = &g_mappingTable[0];
	for(; entry != tableEnd; ++entry)
	{
		if(strcmp(entry->extName, extensionName) == 0)
			break;
	}
	
	if(entry != tableEnd)
		*(entry->extVariable) = 1;
}

static void ProcExtsFromExtString(const char *strExtList)
{
	size_t iExtListLen = strlen(strExtList);
	const char *strExtListEnd = strExtList + iExtListLen;
	const char *strCurrPos = strExtList;
	char strWorkBuff[256];

	while(*strCurrPos)
	{
		/*Get the extension at our position.*/
		int iStrLen = 0;
		const char *strEndStr = strchr(strCurrPos, ' ');
		int iStop = 0;
		if(strEndStr == NULL)
		{
			strEndStr = strExtListEnd;
			iStop = 1;
		}

		iStrLen = (int)((ptrdiff_t)strEndStr - (ptrdiff_t)strCurrPos);

		if(iStrLen > 255)
			return;

		strncpy(strWorkBuff, strCurrPos, iStrLen);
		strWorkBuff[iStrLen] = '\0';

		LoadExtByName(strWorkBuff);

		strCurrPos = strEndStr + 1;
		if(iStop) break;
	}
}

void wgl_CheckExtensions(HDC hdc)
{
	ClearExtensionVariables();
	
	{
		typedef const char * (CODEGEN_FUNCPTR *MYGETEXTSTRINGPROC)(HDC);
		MYGETEXTSTRINGPROC InternalGetExtensionString = (MYGETEXTSTRINGPROC)IntGetProcAddress("wglGetExtensionsStringARB");
		if(!InternalGetExtensionString) return;
		ProcExtsFromExtString((const char *)InternalGetExtensionString(hdc));
	}
}
