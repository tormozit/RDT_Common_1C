
#include "stdafx.h"
//#include "windows.h"
//#include <string> 
//#include <sstream>

#ifdef __linux__
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif

//#include <stdio.h>
//#include <wchar.h>
#include "AddInNative.h"
#include "Shlobj.h"
#include <string>

// https://habrahabr.ru/post/191014/
#define TIME_LEN 34
#define ePropLast 0 // !!! Количество свойств !!!
#define eMethLast 6 // !!! Количество методов !!!

#define eMethSleep 0 // (КоличествоМилисекунд)
#define eMethPID 1
#define eMethIsAdmin 2
#define eMethGetCaretPos 3
#define eMethMoveWindowToCaretPos 4 // (РазрешитьВыходЗаПределыЭкрана)
#define eMethRun 5

#define BASE_ERRNO     7

static wchar_t *g_MethodNames[] = {L"Sleep", L"PID", L"IsAdmin", L"GetCaretPos", L"MoveWindowToCaretPos", L"Run"};
static wchar_t *g_MethodNamesRu[] = {L"Спать", L"PID", L"ЛиАдмин", L"ПолучитьПозициюКаретки", L"ПереместитьОкноВПозициюКаретки", L"Выполнить"};

static const wchar_t g_kClassNames[] = L"CAddInNative"; //"|OtherClass1|OtherClass2";
static IAddInDefBase *pAsyncEvent = NULL;
int CaretLeft = 0;
int CaretTop = 0;

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);

//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
    if(!*pInterface)
    {
        *pInterface= new CAddInNative;
        return (long)*pInterface;
    }
    return 0;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
   if(!*pIntf)
      return -1;

   delete *pIntf;
   *pIntf = 0;
   return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
    static WCHAR_T* names = 0;
    if (!names)
        ::convToShortWchar(&names, g_kClassNames);
    return names;
}
//---------------------------------------------------------------------------//
#ifndef __linux__
VOID CALLBACK MyTimerProc(
        HWND hwnd, // handle of window for timer messages
        UINT uMsg, // WM_TIMER message
        UINT idEvent, // timer identifier
        DWORD dwTime // current system time
);
#else
static void MyTimerProc(int sig);
#endif //__linux__

// CAddInNative
//---------------------------------------------------------------------------//
CAddInNative::CAddInNative()
{
    m_iMemory = 0;
    m_iConnect = 0;
}
//---------------------------------------------------------------------------//
CAddInNative::~CAddInNative()
{
}
//---------------------------------------------------------------------------//
bool CAddInNative::Init(void* pConnection)
{ 
    m_iConnect = (IAddInDefBase*)pConnection;
    return m_iConnect != NULL;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetInfo()
{ 
    // Component should put supported component technology version 
    // This component supports 2.0 version
    return 2000; 
}
//---------------------------------------------------------------------------//
void CAddInNative::Done()
{
}
/////////////////////////////////////////////////////////////////////////////
// ILanguageExtenderBase
//---------------------------------------------------------------------------//
bool CAddInNative::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{ 
    wchar_t *wsExtension = L"AddIn";
    int iActualSize = ::wcslen(wsExtension) + 1;
    WCHAR_T* dest = 0;

    if (m_iMemory)
    {
        if(m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
        return true;
    }

    return false; 
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNProps()
{ 
    // You may delete next lines and add your own implementation code here
    return ePropLast;
}
//---------------------------------------------------------------------------//
long CAddInNative::FindProp(const WCHAR_T* wsPropName)
{ 
    long plPropNum = -1;
    return plPropNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetPropName(long lPropNum, long lPropAlias)
{ 
    return NULL;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{ 
    return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::SetPropVal(const long lPropNum, tVariant *varPropVal)
{ 
    return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropReadable(const long lPropNum)
{ 
    return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropWritable(const long lPropNum)
{
    return false;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNMethods()
{ 
    return eMethLast;
}
//---------------------------------------------------------------------------//
long CAddInNative::FindMethod(const WCHAR_T* wsMethodName)
{ 
    long plMethodNum = -1;
    wchar_t* name = 0;

    ::convFromShortWchar(&name, wsMethodName);

    plMethodNum = findName(g_MethodNames, name, eMethLast);

    if (plMethodNum == -1)
        plMethodNum = findName(g_MethodNamesRu, name, eMethLast);

    return plMethodNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetMethodName(const long lMethodNum, const long lMethodAlias)
{ 
    if (lMethodNum >= eMethLast)
        return NULL;

    wchar_t *wsCurrentName = NULL;
    WCHAR_T *wsMethodName = NULL;
    int iActualSize = 0;

    switch(lMethodAlias)
    {
    case 0: // First language
        wsCurrentName = g_MethodNames[lMethodNum];
        break;
    case 1: // Second language
        wsCurrentName = g_MethodNamesRu[lMethodNum];
        break;
    default: 
        return 0;
    }

    iActualSize = wcslen(wsCurrentName)+1;

    if (m_iMemory && wsCurrentName)
    {
        if(m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
    }

    return wsMethodName;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNParams(const long lMethodNum)
{ 
    switch(lMethodNum)
    { 
    case eMethSleep:
        return 1;
	case eMethPID:
		return 0;
	case eMethIsAdmin:
		return 0;
	case eMethGetCaretPos:
		return 0;
	case eMethMoveWindowToCaretPos:
		return 1;
	case eMethRun:
		return 5;
	default:
        return 0;
    }
    
    return 0;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum,
                          tVariant *pvarParamDefValue)
{ 
    TV_VT(pvarParamDefValue)= VTYPE_EMPTY;

    switch(lMethodNum)
    { 
    case eMethSleep:
        // There are no parameter values by default 
        break;
	case eMethPID:
		// There are no parameter values by default 
		break;
 	case eMethIsAdmin:
		// There are no parameter values by default 
		break;
	case eMethGetCaretPos:
		// There are no parameter values by default 
		break;
	case eMethMoveWindowToCaretPos:
		// There are no parameter values by default 
		break;	
	case eMethRun:
		// There are no parameter values by default 
		break;
	default:
        return false;
    }

    return false;
} 
//---------------------------------------------------------------------------//
bool CAddInNative::HasRetVal(const long lMethodNum)
{ 
    switch(lMethodNum)
    { 
    case eMethSleep:
        return false;
	case eMethPID:
		return true;
	case eMethIsAdmin:
		return true;
	case eMethGetCaretPos:
		return false;
	case eMethMoveWindowToCaretPos:
		return false;
	case eMethRun:
		return false;
	default:
        return false;
    }
}

void StoreCaretPos()
{
	// http://stackoverflow.com/questions/31055249/is-it-possible-to-get-caret-position-in-word-to-update-faster
	HWND hWindow = NULL;
	DWORD remoteThreadId = 0;
	hWindow = GetForegroundWindow();
	EnableWindow(hWindow, true);
	remoteThreadId = GetWindowThreadProcessId(hWindow, 0);
	POINT point;
	point.x = 0;
	point.y = 0;
	GUITHREADINFO guiInfo;
	guiInfo.cbSize = sizeof(GUITHREADINFO);
	if (GetGUIThreadInfo(remoteThreadId, &guiInfo))
	{
		ClientToScreen(guiInfo.hwndCaret, &point);
		CaretLeft = 0;
		CaretTop = 0;
		if (point.y > 0)
		{
			CaretLeft = guiInfo.rcCaret.right + point.x;
			CaretTop = guiInfo.rcCaret.bottom + point.y;
		}
	}
}

void MoveWindowToCaret(bool AllowOutScreen)
{
	if (CaretTop > 0)
	{
		HWND hWindow = GetForegroundWindow();
		RECT rect;
		GetWindowRect(hWindow, &rect);
		int WindowWidth = rect.right - rect.left;
		int WindowHeight = rect.bottom - rect.top;
		int NewLeft;
		int NewTop;
		int maxTop = GetSystemMetrics(SM_CYVIRTUALSCREEN) - WindowHeight - 30;
		if (!AllowOutScreen && CaretTop + 1 > maxTop)
			NewTop = maxTop;
		else
			NewTop = CaretTop + 1;
		int maxLeft = GetSystemMetrics(SM_CXVIRTUALSCREEN) - WindowWidth;
		if (CaretLeft > maxLeft)
			NewLeft = maxLeft;
		else
			NewLeft = CaretLeft;
		MoveWindow(hWindow, NewLeft, NewTop, WindowWidth, WindowHeight, true);
	}
}

//---------------------------------------------------------------------------//
bool CAddInNative::CallAsProc(const long lMethodNum,
                    tVariant* paParams, const long lSizeArray)
{ 
	HWND hWindow = NULL;
	wchar_t* wsTmp = 0;
	int Result = 0;
	switch (lMethodNum)
    { 
    case eMethSleep:
        if (lSizeArray)
        {
			int Duration = TV_INT(paParams);
			if (Duration > 0)
				{
					Sleep(Duration);
					return true;
				}
			else
				return false;
		}
		else
			return false;
	case eMethGetCaretPos:
		StoreCaretPos();
		return true;
	case eMethMoveWindowToCaretPos:
		bool AllowOutScreen;
		AllowOutScreen = false;
		if (lSizeArray)
		{
			AllowOutScreen = TV_BOOL(paParams);
		}
		MoveWindowToCaret(AllowOutScreen);
		return true;
	case eMethRun:
		if (lSizeArray)
		{
			LPCWSTR ExeFilename = (paParams)->pwstrVal;
			LPCWSTR ExeParams = (paParams + 1)->pwstrVal;
			LPCWSTR CurrentDirectory = (paParams + 2)->pwstrVal;
			BOOL WaitForComplete = (paParams + 3)->bVal;
			BOOL AdminMode = (paParams + 4)->bVal;
			if (std::wcslen(ExeFilename) > 0)
			{
				wchar_t* param2 = 0;
				if (AdminMode)
					convToShortWchar(&param2, L"runas");
				SHELLEXECUTEINFO shExInfo = { 0 };
				shExInfo.cbSize = sizeof(shExInfo);
				shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				shExInfo.hwnd = 0;
				shExInfo.lpVerb = param2; 
				shExInfo.lpFile = ExeFilename;
				shExInfo.lpParameters = ExeParams;
				shExInfo.lpDirectory = CurrentDirectory;
				shExInfo.nShow = SW_HIDE;
				shExInfo.hInstApp = 0;
				if (ShellExecuteEx(&shExInfo))
				{
					if (WaitForComplete)
					{
						WaitForSingleObject(shExInfo.hProcess, INFINITE);
						CloseHandle(shExInfo.hProcess);
					}
				}
				return true;
			}
			else
				return false;
		}
		else
			return false;
    default:
        return false;
    }

}


//---------------------------------------------------------------------------//
bool CAddInNative::CallAsFunc(const long lMethodNum,
                tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{ 
    bool ret = false;
    FILE *file = 0;
    char *name = 0;
    int size = 0;
    char *mbstr = 0;
    wchar_t* wsTmp = 0;
	switch(lMethodNum)
	{ 
	case eMethPID:
		pvarRetValue->intVal = GetCurrentProcessId();
		pvarRetValue->vt = VTYPE_I4;
		return true;
	case eMethIsAdmin:
		pvarRetValue->bVal = IsUserAnAdmin();
		pvarRetValue->vt = VTYPE_BOOL;
		return true;
	default:
		return false;
	}
    return ret; 
}

//template <typename T>
//std::string toString(T val)
//{
//	std::ostringstream oss;
//	oss << val;
//	return oss.str();
//}
//
//template<typename T>
//T fromString(const std::string& s)
//{
//	std::istringstream iss(s);
//	T res;
//	iss >> res;
//	return res;
//}

//---------------------------------------------------------------------------//
// This code will work only on the client!
#ifndef __linux__
VOID CALLBACK MyTimerProc(
  HWND hwnd,    // handle of window for timer messages
  UINT uMsg,    // WM_TIMER message
  UINT idEvent, // timer identifier
  DWORD dwTime  // current system time
)
{
    if (!pAsyncEvent)
        return;

    wchar_t *who = L"ComponentNative", *what = L"Timer";

    wchar_t *wstime = new wchar_t[TIME_LEN];
    if (wstime)
    {
        wmemset(wstime, 0, TIME_LEN);
        ::_ultow(dwTime, wstime, 10);
        pAsyncEvent->ExternalEvent(who, what, wstime);
        delete[] wstime;
    }
}
#else
void MyTimerProc(int sig)
{
    if (pAsyncEvent)
        return;

    WCHAR_T *who = 0, *what = 0, *wdata = 0;
    wchar_t *data = 0;
    time_t dwTime = time(NULL);

    data = new wchar_t[TIME_LEN];
    
    if (data)
    {
        wmemset(data, 0, TIME_LEN);
        swprintf(data, TIME_LEN, L"%ul", dwTime);
        ::convToShortWchar(&who, L"ComponentNative");
        ::convToShortWchar(&what, L"Timer");
        ::convToShortWchar(&wdata, data);

        pAsyncEvent->ExternalEvent(who, what, wdata);

        delete[] who;
        delete[] what;
        delete[] wdata;
        delete[] data;
    }
}
#endif
//---------------------------------------------------------------------------//
void CAddInNative::SetLocale(const WCHAR_T* loc)
{
#ifndef __linux__
    _wsetlocale(LC_ALL, loc);
#else
    //We convert in char* char_locale
    //also we establish locale
    //setlocale(LC_ALL, char_locale);
#endif
}
/////////////////////////////////////////////////////////////////////////////
// LocaleBase
//---------------------------------------------------------------------------//
bool CAddInNative::setMemManager(void* mem)
{
    m_iMemory = (IMemoryManager*)mem;
    return m_iMemory != 0;
}
//---------------------------------------------------------------------------//
void CAddInNative::addError(uint32_t wcode, const wchar_t* source, 
                        const wchar_t* descriptor, long code)
{
    if (m_iConnect)
    {
        WCHAR_T *err = 0;
        WCHAR_T *descr = 0;
        
        ::convToShortWchar(&err, source);
        ::convToShortWchar(&descr, descriptor);

        m_iConnect->AddError(wcode, err, descr, code);
        delete[] err;
        delete[] descr;
    }
}
//---------------------------------------------------------------------------//
long CAddInNative::findName(wchar_t* names[], const wchar_t* name, 
                         const uint32_t size) const
{
    long ret = -1;
    for (uint32_t i = 0; i < size; i++)
    {
        if (!wcscmp(names[i], name))
        {
            ret = i;
            break;
        }
    }
    return ret;
}
//---------------------------------------------------------------------------//
uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len)
{
    if (!len)
        len = ::wcslen(Source)+1;

    if (!*Dest)
        *Dest = new WCHAR_T[len];

    WCHAR_T* tmpShort = *Dest;
    wchar_t* tmpWChar = (wchar_t*) Source;
    uint32_t res = 0;

    ::memset(*Dest, 0, len*sizeof(WCHAR_T));
    do
    {
        *tmpShort++ = (WCHAR_T)*tmpWChar++;
        ++res;
    }
    while (len-- && *tmpWChar);

    return res;
}
//---------------------------------------------------------------------------//
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len)
{
    if (!len)
        len = getLenShortWcharStr(Source)+1;

    if (!*Dest)
        *Dest = new wchar_t[len];

    wchar_t* tmpWChar = *Dest;
    WCHAR_T* tmpShort = (WCHAR_T*)Source;
    uint32_t res = 0;

    ::memset(*Dest, 0, len*sizeof(wchar_t));
    do
    {
        *tmpWChar++ = (wchar_t)*tmpShort++;
        ++res;
    }
    while (len-- && *tmpShort);

    return res;
}
//---------------------------------------------------------------------------//
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
    uint32_t res = 0;
    WCHAR_T *tmpShort = (WCHAR_T*)Source;

    while (*tmpShort++)
        ++res;

    return res;
}
//---------------------------------------------------------------------------//
