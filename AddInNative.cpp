#include "stdafx.h"
#include "windows.h" // Добавлено явно
#include <UIAutomation.h> // Добавлено для нового режима
#include <string> 
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
#define eMethGetCaretPos 3 // (xOffset, yOffset, UseUIAutomation)
#define eMethMoveWindowToCaretPos 4 // (РазрешитьВыходЗаПределыЭкрана, СделатьПоверхВсех)
#define eMethRun 5

#define BASE_ERRNO     7

static wchar_t *g_MethodNames[] = {L"Sleep", L"PID", L"IsAdmin", L"GetCaretPos", L"MoveWindowToCaretPos", L"Run"};
static wchar_t *g_MethodNamesRu[] = {L"Спать", L"PID", L"ЛиАдмин", L"ПолучитьПозициюКаретки", L"ПереместитьОкноВПозициюКаретки", L"Выполнить"};

static const wchar_t g_kClassNames[] = L"CAddInNative"; //"|OtherClass1|OtherClass2";
static IAddInDefBase *pAsyncEvent = NULL;

// Глобальные переменные для координат
int CaretLeft = 0;
int CaretTop = 0;

// Глобальная переменная для UI Automation
static IUIAutomation *pAutomation = NULL; 

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);
static WcharWrapper s_names(g_kClassNames);

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
	return s_names;
}

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
    
    // Инициализация COM (Multi-threaded Apartment)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); 
    
    // Инициализация IUIAutomation
    if (pAutomation == NULL) {
        CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER, 
                         __uuidof(IUIAutomation), (void**)&pAutomation);
    }
    
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
    // Освобождение IUIAutomation
    if (pAutomation != NULL) {
        pAutomation->Release();
        pAutomation = NULL;
    }
    // Освобождение COM
    CoUninitialize();
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
		return 3; // Увеличено до 3: xOffset, yOffset, UseUIAutomation
	case eMethMoveWindowToCaretPos:
		return 2;
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
        if (lParamNum == 2) // Значение по умолчанию для UseUIAutomation = Ложь
        {
            TV_VT(pvarParamDefValue) = VTYPE_BOOL;
            pvarParamDefValue->bVal = false;
            return true;
        }
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

// =========================================================================
// СТАРЫЙ МЕТОД: Использует GetGUIThreadInfo (для 8.2/8.3)
// =========================================================================
void StoreCaretPos(int xOffset, int yOffset)
{
	HWND hWindow = NULL;
	DWORD remoteThreadId = 0;
	hWindow = GetForegroundWindow();
	//EnableWindow(hWindow, true); // Вроде не нужно
	remoteThreadId = GetWindowThreadProcessId(hWindow, 0);
	POINT point;
	point.x = 0;
	point.y = 0;
	GUITHREADINFO guiInfo;
	guiInfo.cbSize = sizeof(GUITHREADINFO);
	CaretLeft = 0;
	CaretTop = 0;
	if (GetGUIThreadInfo(remoteThreadId, &guiInfo))
	{
		ClientToScreen(guiInfo.hwndCaret, &point);
		CaretLeft = 0;
		CaretTop = 0;
		if (point.y >= 0)
		{
			CaretLeft = guiInfo.rcCaret.right + point.x;
			CaretTop = guiInfo.rcCaret.bottom + point.y;
		}
	}
	CaretLeft += xOffset;
	CaretTop += yOffset;
}

// =========================================================================
// НОВЫЙ МЕТОД: Использует UI Automation (для 8.5+)
// НОВЫЙ МЕТОД: Использует UI Automation (для 8.5+)
// НОВЫЙ МЕТОД: Использует UI Automation (для 8.5+)
void StoreCaretPosUIA(int xOffset, int yOffset)
{
	if (pAutomation == NULL) {
		CaretLeft = 0;
		CaretTop = 0;
		return;
	}

	IUIAutomationElement *pFocusedElement = NULL;
	IUIAutomationTextPattern *pTextPattern = NULL;
	IUIAutomationTextRangeArray *pTextRangeArray = NULL;
	IUIAutomationTextRange *pTextRange = NULL;
	IUIAutomationTextRange *pExpandedRange = NULL;

	CaretLeft = 0;
	CaretTop = 0;

	RECT docRect = { 0 }; // Bounding box самого Document-элемента для смещения

						  // 1. Получаем элемент, находящийся в фокусе ввода
	HRESULT hr = pAutomation->GetFocusedElement(&pFocusedElement);
	if (FAILED(hr) || pFocusedElement == NULL) {
		return;
	}

	// 2. Проверяем, что это элемент Document (50030)
	CONTROLTYPEID controlType;
	pFocusedElement->get_CurrentControlType(&controlType);

	if (controlType == UIA_DocumentControlTypeId)
	{
		// 2.1. Получаем экранные координаты Document-элемента
		// Они будут использоваться как OFFSET, если GetBoundingRectangles вернет относительные координаты.
		pFocusedElement->get_CurrentBoundingRectangle(&docRect);

		// 3. Получаем TextPattern
		hr = pFocusedElement->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern), (void**)&pTextPattern);

		if (SUCCEEDED(hr) && pTextPattern != NULL)
		{
			// 4. Получаем выделение (каретку)
			hr = pTextPattern->GetSelection(&pTextRangeArray);

			if (SUCCEEDED(hr) && pTextRangeArray != NULL)
			{
				int rangeCount = 0;
				pTextRangeArray->get_Length(&rangeCount);

				if (rangeCount > 0)
				{
					// Берем первый (единственный) диапазон выделения/каретки
					hr = pTextRangeArray->GetElement(0, &pTextRange);

					if (SUCCEEDED(hr) && pTextRange != NULL)
					{
						// 5. РАБОТА С НУЛЕВЫМ ДИАПАЗОНОМ (КАРЕТКОЙ)

						// Создаем копию диапазона
						pTextRange->Clone(&pExpandedRange);

						if (pExpandedRange != NULL)
						{
							// Расширяем диапазон до ближайшего символа для получения физических границ
							pExpandedRange->ExpandToEnclosingUnit(TextUnit_Character);

							SAFEARRAY *rectArray = NULL;
							hr = pExpandedRange->GetBoundingRectangles(&rectArray);

							if (SUCCEEDED(hr) && rectArray != NULL && rectArray->rgsabound[0].cElements > 0)
							{
								// Берем первый прямоугольник
								double* pRect = NULL;
								hr = SafeArrayAccessData(rectArray, (void**)&pRect);

								if (SUCCEEDED(hr) && pRect != NULL)
								{
									// pRect[0]=Left (относительно Document), pRect[3]=Bottom (относительно Document)

									// *** КОРРЕКЦИЯ: Добавляем смещение Document-элемента ***
									CaretLeft = (int)pRect[0]; // X-координата + смещение X документа
									CaretTop = (int)pRect[3] + docRect.top;  // Y-координата (Bottom) + смещение Y документа

									SafeArrayUnaccessData(rectArray);
								}
								SafeArrayDestroy(rectArray);
							}
							pExpandedRange->Release(); // Освобождаем расширенный диапазон
						}
					}
					if (pTextRange != NULL) pTextRange->Release(); // Освобождаем оригинальный диапазон
				}
				pTextRangeArray->Release(); // Освобождаем массив диапазонов
			}
		}
	}

	// Освобождение ресурсов (остальные)
	if (pTextPattern != NULL) pTextPattern->Release();
	if (pFocusedElement != NULL) pFocusedElement->Release();

	// Применяем смещения, заданные пользователем
	CaretLeft += xOffset;
	CaretTop += yOffset;
}

void MoveWindowToCaret(bool AllowOutScreen, bool MakeAlwaysOnTop)
{
	HWND hWindow = GetForegroundWindow();
	RECT rect;
	GetWindowRect(hWindow, &rect);
	if (CaretTop > 0)
	{
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
	if (MakeAlwaysOnTop)
		SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
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
		int yOffset, xOffset;
		bool UseUIAutomation;
		
		xOffset = 0;
		yOffset = 0;
		UseUIAutomation = false; // Значение по умолчанию

		// Чтение параметров
		if (lSizeArray > 0) xOffset = TV_INT(paParams);
		if (lSizeArray > 1) yOffset = TV_INT(paParams + 1);
		// Параметр UseUIAutomation доступен только если передано 3 параметра
		if (lSizeArray > 2) UseUIAutomation = TV_BOOL(paParams + 2); 

		if (UseUIAutomation)
		{
			StoreCaretPosUIA(xOffset, yOffset); // Новый режим для 8.5+
		}
		else
		{
			StoreCaretPos(xOffset, yOffset); // Старый режим для 8.2/8.3
		}
		return true;
	case eMethMoveWindowToCaretPos:
		bool AllowOutScreen;
		AllowOutScreen = false;
		bool MakeAlwaysOnTop;
		MakeAlwaysOnTop = false;
		if (lSizeArray)
		{
			AllowOutScreen = TV_BOOL(paParams);
			MakeAlwaysOnTop = TV_BOOL(paParams + 1);
		}
		MoveWindowToCaret(AllowOutScreen, MakeAlwaysOnTop);
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
//---------------------------------------------------------------------------//

#ifdef LINUX_OR_MACOS
WcharWrapper::WcharWrapper(const WCHAR_T* str) : m_str_WCHAR(NULL),
m_str_wchar(NULL)
{
	if (str)
	{
		int len = getLenShortWcharStr(str);
		m_str_WCHAR = new WCHAR_T[len + 1];
		memset(m_str_WCHAR, 0, sizeof(WCHAR_T) * (len + 1));
		memcpy(m_str_WCHAR, str, sizeof(WCHAR_T) * len);
		::convFromShortWchar(&m_str_wchar, m_str_WCHAR);
	}
}
#endif
//---------------------------------------------------------------------------//
WcharWrapper::WcharWrapper(const wchar_t* str) :
#ifdef LINUX_OR_MACOS
	m_str_WCHAR(NULL),
#endif 
	m_str_wchar(NULL)
{
	if (str)
	{
		size_t len = wcslen(str);
		m_str_wchar = new wchar_t[len + 1];
		memset(m_str_wchar, 0, sizeof(wchar_t) * (len + 1));
		memcpy(m_str_wchar, str, sizeof(wchar_t) * len);
#ifdef LINUX_OR_MACOS
		::convToShortWchar(&m_str_WCHAR, m_str_wchar);
#endif
	}

}
//---------------------------------------------------------------------------//
WcharWrapper::~WcharWrapper()
{
#ifdef LINUX_OR_MACOS
	if (m_str_WCHAR)
	{
		delete[] m_str_WCHAR;
		m_str_WCHAR = NULL;
	}
#endif
	if (m_str_wchar)
	{
		delete[] m_str_wchar;
		m_str_wchar = NULL;
	}
}
//---------------------------------------------------------------------------//