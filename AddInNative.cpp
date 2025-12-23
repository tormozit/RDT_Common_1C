#include "stdafx.h"
#include "windows.h" 
#include <UIAutomation.h> 
#include <string> 

#ifdef __linux__
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif

#include "AddInNative.h"
#include "Shlobj.h"
#include <string>

// https://habrahabr.ru/post/191014/
#define TIME_LEN 34
#define ePropLast 0 
#define eMethLast 8 // Было 7, стало 8

#define eMethSleep 0 
#define eMethPID 1
#define eMethIsAdmin 2
#define eMethGetCaretPos 3 
#define eMethMoveWindowToCaretPos 4 
#define eMethRun 5
#define eMethSetClipboard 6 
#define eMethGetClipboard 7 // Новый метод

#define BASE_ERRNO     7

// Добавлено имя метода GetClipboard
static wchar_t *g_MethodNames[] = { L"Sleep", L"PID", L"IsAdmin", L"GetCaretPos", L"MoveWindowToCaretPos", L"Run", L"SetClipboard", L"GetClipboard" };
static wchar_t *g_MethodNamesRu[] = { L"Спать", L"PID", L"ЛиАдмин", L"ПолучитьПозициюКаретки", L"ПереместитьОкноВПозициюКаретки", L"Выполнить", L"УстановитьБуферОбмена", L"ПолучитьБуферОбмена" };

static const wchar_t g_kClassNames[] = L"CAddInNative";
static IAddInDefBase *pAsyncEvent = NULL;

// Глобальные переменные для координат
int CaretLeft = 0;
int CaretTop = 0;

IUIAutomation *pAutomation = NULL;
IUIAutomationElement *g_pCachedElement = NULL;
IUIAutomationTextPattern *g_pCachedTextPattern = NULL;

// --------------------------------------------------------------------------
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// --------------------------------------------------------------------------
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
	uint32_t res = 0;
	WCHAR_T *tmpShort = (WCHAR_T*)Source;

	while (*tmpShort++)
		++res;

	return res;
}

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0)
{
	if (!len)
		len = ::wcslen(Source) + 1;

	if (!*Dest)
		*Dest = new WCHAR_T[len];

	WCHAR_T* tmpShort = *Dest;
	wchar_t* tmpWChar = (wchar_t*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(WCHAR_T));
	do
	{
		*tmpShort++ = (WCHAR_T)*tmpWChar++;
		++res;
	} while (len-- && *tmpWChar);

	return res;
}

uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0)
{
	if (!len)
		len = getLenShortWcharStr(Source) + 1;

	if (!*Dest)
		*Dest = new wchar_t[len];

	wchar_t* tmpWChar = *Dest;
	WCHAR_T* tmpShort = (WCHAR_T*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(wchar_t));
	do
	{
		*tmpWChar++ = (wchar_t)*tmpShort++;
		++res;
	} while (len-- && *tmpShort);

	return res;
}

// Структура для поиска окна текущего процесса
struct FindWindowData {
	DWORD processId;
	const wchar_t* title;
	HWND hWndFound;
};

// Callback функция для фильтрации окон
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
	FindWindowData* data = (FindWindowData*)lParam;
	DWORD windowPid = 0;
	GetWindowThreadProcessId(hWnd, &windowPid);

	if (windowPid == data->processId) {
		if (data->title != NULL && wcslen(data->title) > 0) {
			wchar_t buffer[256];
			GetWindowTextW(hWnd, buffer, 256);
			if (wcsstr(buffer, data->title) != NULL) {
				data->hWndFound = hWnd;
				return FALSE;
			}
		}
		else if (IsWindowVisible(hWnd)) {
			data->hWndFound = hWnd;
			return FALSE;
		}
	}
	return TRUE;
}

// Установка текста в буфер обмена
void SetToClipboard(const wchar_t* text)
{
	if (text == NULL) return;

	if (OpenClipboard(NULL))
	{
		EmptyClipboard();
		size_t len = (wcslen(text) + 1) * sizeof(wchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		if (hMem != NULL)
		{
			void* pData = GlobalLock(hMem);
			if (pData != NULL)
			{
				memcpy(pData, text, len);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
			if (pData == NULL) GlobalFree(hMem);
		}
		CloseClipboard();
	}
}

// Получение текста из буфера обмена (реализация)
// Возвращает true, если успешно прочитано. Результат записывается в память 1С (memMgr)
bool GetFromClipboard(tVariant* pvarRetValue, IMemoryManager* memMgr)
{
	bool success = false;

	// Инициализируем пустой строкой
	TV_VT(pvarRetValue) = VTYPE_PWSTR;
	pvarRetValue->pwstrVal = NULL;
	pvarRetValue->wstrLen = 0;

	if (OpenClipboard(NULL))
	{
		if (IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			HGLOBAL hGlobal = GetClipboardData(CF_UNICODETEXT);
			if (hGlobal != NULL)
			{
				wchar_t* pText = (wchar_t*)GlobalLock(hGlobal);
				if (pText != NULL)
				{
					// Выделяем память через менеджер памяти 1С
					size_t len = wcslen(pText) + 1;
					if (memMgr->AllocMemory((void**)&pvarRetValue->pwstrVal, len * sizeof(WCHAR_T)))
					{
						convToShortWchar(&pvarRetValue->pwstrVal, pText, len);
						pvarRetValue->wstrLen = len - 1;
						success = true;
					}
					GlobalUnlock(hGlobal);
				}
			}
		}
		CloseClipboard();
	}

	// Если не удалось прочитать или буфер пуст/не текст, возвращаем пустую строку
	if (!success && pvarRetValue->pwstrVal == NULL) {
		if (memMgr->AllocMemory((void**)&pvarRetValue->pwstrVal, sizeof(WCHAR_T))) {
			memset(pvarRetValue->pwstrVal, 0, sizeof(WCHAR_T));
			pvarRetValue->wstrLen = 0;
		}
	}

	return true;
}

// --------------------------------------------------------------------------

static WcharWrapper s_names(g_kClassNames);

//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
	if (!*pInterface)
	{
		*pInterface = new CAddInNative;
		return (long)*pInterface;
	}
	return 0;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
	if (!*pIntf)
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
	return 2000;
}
//---------------------------------------------------------------------------//
void CAddInNative::Done()
{
	// Очистка кэша
	if (g_pCachedTextPattern != NULL) {
		g_pCachedTextPattern->Release();
		g_pCachedTextPattern = NULL;
	}
	if (g_pCachedElement != NULL) {
		g_pCachedElement->Release();
		g_pCachedElement = NULL;
	}

	// Освобождение IUIAutomation
	if (pAutomation != NULL) {
		pAutomation->Release();
		pAutomation = NULL;
	}
	// Освобождение COM
	CoUninitialize();
}/////////////////////////////////////////////////////////////////////////////
 // ILanguageExtenderBase
 //---------------------------------------------------------------------------//
bool CAddInNative::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
	wchar_t *wsExtension = L"AddIn";
	int iActualSize = ::wcslen(wsExtension) + 1;
	WCHAR_T* dest = 0;

	if (m_iMemory)
	{
		if (m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNProps()
{
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

	switch (lMethodAlias)
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

	iActualSize = wcslen(wsCurrentName) + 1;

	if (m_iMemory && wsCurrentName)
	{
		if (m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
	}

	return wsMethodName;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNParams(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethSleep:
		return 1;
	case eMethPID:
		return 0;
	case eMethIsAdmin:
		return 0;
	case eMethGetCaretPos:
		return 3;
	case eMethMoveWindowToCaretPos:
		return 3;
	case eMethRun:
		return 5;
	case eMethSetClipboard:
		return 1;
	case eMethGetClipboard: // Параметр 1: Формат (по умолчанию "text")
		return 1;
	default:
		return 0;
	}

	return 0;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum,
	tVariant *pvarParamDefValue)
{
	TV_VT(pvarParamDefValue) = VTYPE_EMPTY;

	switch (lMethodNum)
	{
	case eMethSleep:
		break;
	case eMethPID:
		break;
	case eMethIsAdmin:
		break;
	case eMethGetCaretPos:
		if (lParamNum == 2)
		{
			TV_VT(pvarParamDefValue) = VTYPE_BOOL;
			pvarParamDefValue->bVal = false;
			return true;
		}
		break;
	case eMethMoveWindowToCaretPos:
		if (lParamNum == 2)
		{
			TV_VT(pvarParamDefValue) = VTYPE_PWSTR;
			pvarParamDefValue->pwstrVal = NULL;
			pvarParamDefValue->wstrLen = 0;
			return true;
		}
		break;
	case eMethRun:
		break;
	case eMethSetClipboard:
		break;
	case eMethGetClipboard:
		if (lParamNum == 0) // Формат = "text"
		{
			// По умолчанию можно не задавать, 1С передаст Empty, но для порядка можно вернуть строку
			// В коде CallAsFunc мы просто проверим, что если передан параметр, то это формат.
			// Здесь вернем пустую структуру, т.к. значение по умолчанию не критично для логики C++.
			// Или можно явно вернуть "text".
			static const wchar_t* defFormat = L"text";
			TV_VT(pvarParamDefValue) = VTYPE_PWSTR;
			// Внимание: мы не можем выделить память здесь так же просто, 
			// 1С ожидает, что мы просто заполним структуру, но лучше оставить VTYPE_EMPTY
			// если параметр необязательный в манифесте. 
			// В данном коде оставим Empty, обработаем отсутствие параметра в CallAsFunc.
		}
		break;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::HasRetVal(const long lMethodNum)
{
	switch (lMethodNum)
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
	case eMethSetClipboard:
		return false;
	case eMethGetClipboard: // Функция возвращает значение
		return true;
	default:
		return false;
	}
}

// =========================================================================
void StoreCaretPos(int xOffset, int yOffset)
{
	HWND hWindow = NULL;
	DWORD remoteThreadId = 0;
	hWindow = GetFocus();
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

void StoreCaretPosUIA(int xOffset, int yOffset)
{
	CaretLeft = 0;
	CaretTop = 0;
	if (pAutomation == NULL) {
		return;
	}

	HRESULT hr;
	RECT docRect = { 0 };
	IUIAutomationTextRangeArray *pTextRangeArray = NULL;
	IUIAutomationTextRange *pTextRange = NULL;
	IUIAutomationTextRange *pExpandedRange = NULL;
	bool isCacheValid = false;

	if (g_pCachedElement != NULL && g_pCachedTextPattern != NULL)
	{
		hr = g_pCachedElement->get_CurrentBoundingRectangle(&docRect);
		if (SUCCEEDED(hr))
		{
			hr = g_pCachedTextPattern->GetSelection(&pTextRangeArray);
			if (SUCCEEDED(hr)) {
				isCacheValid = true;
			}
		}
	}

	if (!isCacheValid)
	{
		if (g_pCachedTextPattern) { g_pCachedTextPattern->Release(); g_pCachedTextPattern = NULL; }
		if (g_pCachedElement) { g_pCachedElement->Release(); g_pCachedElement = NULL; }
		hr = pAutomation->GetFocusedElement(&g_pCachedElement);
		if (FAILED(hr) || g_pCachedElement == NULL) return;
		g_pCachedElement->get_CurrentBoundingRectangle(&docRect);
		hr = g_pCachedElement->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern), (void**)&g_pCachedTextPattern);
		if (FAILED(hr) || g_pCachedTextPattern == NULL) {
			g_pCachedElement->Release(); g_pCachedElement = NULL;
			return;
		}
		hr = g_pCachedTextPattern->GetSelection(&pTextRangeArray);
		if (FAILED(hr)) {
			g_pCachedTextPattern->Release(); g_pCachedTextPattern = NULL;
			g_pCachedElement->Release(); g_pCachedElement = NULL;
			return;
		}
	}
	if (pTextRangeArray != NULL)
	{
		int rangeCount = 0;
		pTextRangeArray->get_Length(&rangeCount);
		if (rangeCount > 0)
		{
			hr = pTextRangeArray->GetElement(0, &pTextRange);
			if (SUCCEEDED(hr) && pTextRange != NULL)
			{
				pTextRange->Clone(&pExpandedRange);
				if (pExpandedRange != NULL)
				{
					pExpandedRange->ExpandToEnclosingUnit(TextUnit_Character);
					SAFEARRAY *rectArray = NULL;
					hr = pExpandedRange->GetBoundingRectangles(&rectArray);
					bool success = SUCCEEDED(hr) && rectArray != NULL && rectArray->rgsabound[0].cElements > 0;
					if (!success)
					{
						IUIAutomationTextRange *pFallbackRange = NULL;
						pTextRange->Clone(&pFallbackRange);
						if (pFallbackRange != NULL)
						{
							int actualMoved = 0;
							hr = pFallbackRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -1, &actualMoved);
							if (SUCCEEDED(hr))
							{
								hr = pFallbackRange->GetBoundingRectangles(&rectArray);
								success = SUCCEEDED(hr) && rectArray != NULL && rectArray->rgsabound[0].cElements > 0;
							}
							pFallbackRange->Release();
						}
					}
					if (success)
					{
						double* pRect = NULL;
						hr = SafeArrayAccessData(rectArray, (void**)&pRect);
						if (SUCCEEDED(hr) && pRect != NULL)
						{
							double left = pRect[0];
							double top = pRect[1];
							double width = pRect[2];
							double height = pRect[3];
							CaretLeft = (int)left;
							CaretTop = (int)top + height;
							SafeArrayUnaccessData(rectArray);
						}
						SafeArrayDestroy(rectArray);
					}
					else
					{
						pExpandedRange->ExpandToEnclosingUnit(TextUnit_Line);
						SAFEARRAY *rectArrayY = NULL;
						hr = pExpandedRange->GetBoundingRectangles(&rectArrayY);
						if (SUCCEEDED(hr) && rectArrayY != NULL && rectArrayY->rgsabound[0].cElements > 0)
						{
							double* pRect = NULL;
							if (SUCCEEDED(SafeArrayAccessData(rectArrayY, (void**)&pRect)) && pRect != NULL)
							{
								double top = pRect[1];
								double height = pRect[3];
								CaretTop = (int)top + height;
								SafeArrayUnaccessData(rectArrayY);
							}
							SafeArrayDestroy(rectArrayY);
						}
					}
					pExpandedRange->Release();
				}
				pTextRange->Release();
			}
		}
		pTextRangeArray->Release();
	}
	CaretLeft += xOffset;
	CaretTop += yOffset;
}

void MoveWindowToCaret(bool AllowOutScreen, bool MakeAlwaysOnTop, const wchar_t* Title)
{
	HWND hWindow = NULL;

	// Подготавливаем данные для поиска в текущем процессе
	FindWindowData data = { GetCurrentProcessId(), Title, NULL };

	// Перебираем окна системы и фильтруем по нашему PID
	EnumWindows(EnumWindowsProc, (LPARAM)&data);
	hWindow = data.hWndFound;

	if (hWindow == NULL) return;

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
		UseUIAutomation = false;

		if (lSizeArray > 0) xOffset = TV_INT(paParams);
		if (lSizeArray > 1) yOffset = TV_INT(paParams + 1);
		if (lSizeArray > 2) UseUIAutomation = TV_BOOL(paParams + 2);

		if (UseUIAutomation)
		{
			StoreCaretPosUIA(xOffset, yOffset);
		}
		else
		{
			StoreCaretPos(xOffset, yOffset);
		}
		return true;
	case eMethMoveWindowToCaretPos:
	{	bool AllowOutScreen;
	AllowOutScreen = false;
	bool MakeAlwaysOnTop;
	MakeAlwaysOnTop = false;
	wchar_t* Title = NULL;

	if (lSizeArray > 0) AllowOutScreen = TV_BOOL(paParams);
	if (lSizeArray > 1) MakeAlwaysOnTop = TV_BOOL(paParams + 1);

	if (lSizeArray > 2) {
		if (paParams[2].vt == VTYPE_PWSTR) {
			Title = paParams[2].pwstrVal;
		}
	}
	MoveWindowToCaret(AllowOutScreen, MakeAlwaysOnTop, Title);
	return true;
	}
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
	case eMethSetClipboard:
		if (lSizeArray > 0 && paParams[0].vt == VTYPE_PWSTR)
		{
			SetToClipboard(paParams[0].pwstrVal);
			return true;
		}
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
	switch (lMethodNum)
	{
	case eMethPID:
		pvarRetValue->intVal = GetCurrentProcessId();
		pvarRetValue->vt = VTYPE_I4;
		return true;
	case eMethIsAdmin:
		pvarRetValue->bVal = IsUserAnAdmin();
		pvarRetValue->vt = VTYPE_BOOL;
		return true;
	case eMethGetClipboard:
		// Сюда можно добавить проверку параметра Format (paParams[0]), 
		// но сейчас мы всегда возвращаем текст, так как реализация только для CF_UNICODETEXT
		return GetFromClipboard(pvarRetValue, m_iMemory);
	default:
		return false;
	}
	return ret;
}

//---------------------------------------------------------------------------//
void CAddInNative::SetLocale(const WCHAR_T* loc)
{
#ifndef __linux__
	_wsetlocale(LC_ALL, loc);
#else
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
// РЕАЛИЗАЦИИ ФУНКЦИЙ convToShortWchar И ПР. УДАЛЕНЫ ОТСЮДА
// (они перенесены в начало файла)
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
