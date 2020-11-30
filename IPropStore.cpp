﻿
/***
 *     ▄████▄  ▒██   ██▒ ██▒   █▓ █    ██   ██████ ▓█████  ██▀███
 *    ▒██▀ ▀█  ▒▒ █ █ ▒░▓██░   █▒ ██  ▓██▒▒██    ▒ ▓█   ▀ ▓██ ▒ ██▒
 *    ▒▓█    ▄ ░░  █   ░ ▓██  █▒░▓██  ▒██░░ ▓██▄   ▒███   ▓██ ░▄█ ▒
 *    ▒▓▓▄ ▄██▒ ░ █ █ ▒   ▒██ █░░▓▓█  ░██░  ▒   ██▒▒▓█  ▄ ▒██▀▀█▄
 *    ▒ ▓███▀ ░▒██▒ ▒██▒   ▒▀█░  ▒▒█████▓ ▒██████▒▒░▒████▒░██▓ ▒██▒
 *    ░ ░▒ ▒  ░▒▒ ░ ░▓ ░   ░ ▐░  ░▒▓▒ ▒ ▒ ▒ ▒▓▒ ▒ ░░░ ▒░ ░░ ▒▓ ░▒▓░
 *      ░  ▒   ░░   ░▒ ░   ░ ░░  ░░▒░ ░ ░ ░ ░▒  ░ ░ ░ ░  ░  ░▒ ░ ▒░
 *    ░         ░    ░       ░░   ░░░ ░ ░ ░  ░  ░     ░     ░░   ░
 *    ░ ░       ░    ░        ░     ░           ░     ░  ░   ░
 *    ░                      ░
 *
 *
 *	 IPropertyStore
 *	 24 April 2019
 *	 Class for gathering APO properties from registry (APOFilter.h)
 */

#include "stdafx.h"
#include "IPropStore.h"

IPropertyStoreFX::IPropertyStoreFX(std::wstring& Device, REGSAM dwAccess)
	:_guid(Device), _dwAcc(dwAccess)
{
	InitializeCriticalSection(&cr);
};

IPropertyStoreFX::~IPropertyStoreFX() {
	DeleteCriticalSection(&cr);
	RegCloseKey(reg);
	//RegCloseKey(regProp);
}

HRESULT IPropertyStoreFX::QueryInterface(const IID& riid, void** ppvObject) {
	if (riid == GUID_NULL)
		return S_OK;
	return E_NOINTERFACE;
};

ULONG IPropertyStoreFX::AddRef() {
	return InterlockedIncrement(&ref);
};

ULONG IPropertyStoreFX::Release() {
	if (InterlockedDecrement(&ref) == 1) {
		this->~IPropertyStoreFX();
		return 0;
	}
	return ref;
};

HRESULT IPropertyStoreFX::Getvalue(REFPROPERTYKEY key,
	PROPVARIANT* pv) {

	auto RET_DEVICE_STRING = [pv](wchar_t* p) {
		if (p == 0)
			return E_FAIL;
		wchar_t* name = (wchar_t*) CoTaskMemAlloc(240);
		if (name == 0)
			return E_OUTOFMEMORY;
		memset(name, 0, 240);
		wcscpy(name, p);
		pv->vt = VT_LPWSTR;
		pv->pwszVal = name;
		return S_OK;
	};

	EnterCriticalSection(&cr);

	HRESULT hr = E_FAIL;

	if (pv == 0) { 
		hr = E_POINTER;
		goto LEAVE_;
	}

	wchar_t keystr[128] = { 0 };

	memset(pv, 0, sizeof(PROPVARIANT));

	if (key == PKEY_AudioEndpoint_GUID)
	{
		CLSID cl = GUID_NULL;
		LPOLESTR gd = 0;

		if (FAILED(CLSIDFromString(_guid.c_str(), &cl))) 
		{
			hr = E_FAIL;
			goto LEAVE_;
		}
		if (SUCCEEDED(StringFromCLSID(cl, &gd)))
		{
			pv->vt = VT_LPWSTR;
			pv->pwszVal = gd;
			hr = S_OK;
			goto LEAVE_;
		}
		else {
			CoTaskMemFree(gd);
			hr = E_FAIL;
			goto LEAVE_;
		}
	}

	if (key == PKEY_DeviceInterface_FriendlyName) //soundcard name
	{
		RET_DEVICE_STRING(L"ESI Juli@");
			goto LEAVE_;
	} //...

	if (key == PKEY_Device_FriendlyName) //interface name
	{
		RET_DEVICE_STRING(L"Speakers (ESI Juli@)");
			goto LEAVE_;
	} //...

	//Convert CLSID to GUID string
	hr = StringCchPrintf(keystr, 128, L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x},%d", key.fmtid.Data1,
		key.fmtid.Data2,
		key.fmtid.Data3,
		key.fmtid.Data4[0],
		key.fmtid.Data4[1],
		key.fmtid.Data4[2],
		key.fmtid.Data4[3],
		key.fmtid.Data4[4],
		key.fmtid.Data4[5],
		key.fmtid.Data4[6],
		key.fmtid.Data4[7],
		key.pid);

	if (FAILED(hr)) {
		hr = E_FAIL;
		goto LEAVE_;
	}

	DWORD type = 0;
	DWORD size = 0;

	LSTATUS status = RegQueryValueExW(reg, keystr, 0, &type, 0, &size);

	if (status) {
		hr = E_FAIL;
		goto LEAVE_;
	}

	if (type == REG_NONE) {
		hr = E_FAIL;
		goto LEAVE_;
	}

	if (type == REG_SZ)
	{
		DWORD sdata = size;

		wchar_t* x = (wchar_t*) CoTaskMemAlloc(size);

		LSTATUS status = RegGetValueW(reg, 0, keystr, RRF_RT_REG_SZ, 0, x, &sdata);

		if (!status) {
			pv->vt = VT_LPWSTR;
			pv->pwszVal = x;
			hr = S_OK;
		}
		goto LEAVE_;
	}

	if (type == REG_DWORD)
	{
		DWORD sdata = size;

		if (size == 4)
		{
			LSTATUS status = RegGetValueW(reg, 0, keystr, RRF_RT_REG_DWORD, 0, &pv->ulVal, &sdata);

			if (!status) {
				pv->vt = VT_UI4;
				hr = S_OK;
			}
		}
		goto LEAVE_;
	}

	if (type == REG_BINARY)
	{
		DWORD sdata = size;
		PROPVARIANT pvdata = { 0 };

		LSTATUS status = RegGetValueW(reg, 0, keystr, RRF_RT_REG_BINARY, 0, &pvdata, &sdata);

		if (!status)
		{
			hr = DeserializePropVarinat(type, &pvdata, size, pv);
		}
		goto LEAVE_;
	}

	if (type == REG_MULTI_SZ)
	{
		DWORD sdata = size;
		LPVOID str = CoTaskMemAlloc(size);
		LSTATUS status = RegGetValueW(reg, 0, keystr, RRF_RT_REG_MULTI_SZ, 0, str, &sdata);

		if (!status) {
			hr = InitPropVariantFromStringAsVector((PCWSTR) str, pv);

			if (FAILED(hr))
				hr = E_FAIL;

			CoTaskMemFree(str);
			hr = S_OK;
		}
		goto LEAVE_;
	}

	if (type == REG_EXPAND_SZ) {
		size_t s = size + 2;
		DWORD sdata;

		void* vm = CoTaskMemAlloc(s);

		if (vm)
		{
			hr = E_FAIL;
			goto LEAVE_;
		}

			IMalloc* ml;
			size_t s2 = 0;

			if (SUCCEEDED(CoGetMalloc(true, &ml)))
			{
				s2 = ml->GetSize(vm);
				ml->Release();
			}

			memset(vm, 0, s2);

			LSTATUS status = RegGetValueW(reg, 0, keystr, 0x6, 0, vm, &sdata);

			if (status != ERROR_FILE_NOT_FOUND) {
				if (status == ERROR_SUCCESS)
				{
					wchar_t buf[520];
					memset(buf, 0, 208);

					if (SHLoadIndirectString((PCWSTR) vm, buf, 260, 0) != E_FAIL)
					{
						pv->vt = VT_LPWSTR;
						pv->pwszVal = (LPWSTR)buf;
						hr = S_OK;
						goto LEAVE_;
					}
					else
					{
						if (GetLastError() == ERROR_ACCESS_DENIED) {
							hr = E_FAIL;
							goto LEAVE_;
						}
					}
				}
				else
				{
					hr = E_FAIL;
					goto LEAVE_;
				}
			}
			goto LEAVE_;
	}

	LEAVE_:
	LeaveCriticalSection(&this->cr);
	return hr;
};

bool IPropertyStoreFX::TryOpenPropertyStoreRegKey()
{
	HRESULT hr = S_OK;
	bool result = false;
	EDataFlow devicetype = eRender;
	IMMDeviceEnumerator* enumemator = 0;

	EnterCriticalSection(&cr);

	std::wstring regfx = MM_DEV_AUD_REG_PATH;
	//std::wstring regprop = MM_DEV_AUD_REG_PATH;

	if ((SUCCEEDED(CoCreateInstance
	(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**) &enumemator
	))) | enumemator != 0)
	{
		IMMDevice* imd = 0;
		IMMEndpoint* ime = 0;
		std::wstring fulldevice = L"{0.0.0.00000000}.";
		fulldevice += _guid;

		hr = enumemator->GetDevice(fulldevice.c_str(), &imd);
		if ((SUCCEEDED(hr)) | imd != 0)
		{
			if (SUCCEEDED(imd->QueryInterface(__uuidof(IMMEndpoint), (void**) &ime)))
			{
				if (ime == 0)
					return false;
				if (FAILED(ime->GetDataFlow(&devicetype)))
					return false;

				switch (devicetype)
				{
				case eAll:
					return false;
				case eRender:
					regfx += L"Render\\" + _guid + L"\\FxProperties";
					break;
				case eCapture:
					regfx += L"Capture\\" + _guid + L"\\FxProperties";
					break;
				default:
					break;
				}

				/*
				regprop += L"Render\\" + _guid + L"\\Properties";
				regprop += L"Capture\\" + _guid + L"\\Properties";
				*/
			}
			ime->Release();
			imd->Release();

			if (reg = RegistryHelper::openKey(regfx, _dwAcc))
				result = true;
			/*
			if (!(regProp = RegistryHelper::openKey(regProp, _dwAcc)))
				return false;
			*/
		}
	}

	LeaveCriticalSection(&cr);
	return result;
}

HRESULT IPropertyStoreFX::DeserializePropVarinat(int type, void* src, size_t cb, PROPVARIANT* dest)
{
	if (!src)
		return E_FAIL;
	if	(!cb)
		return E_FAIL;

	*dest = { 0 };

	if (type == REG_DWORD)
	{
		dest->vt = VT_UI4;
		dest->lVal = *((LONG*)src);
		return S_OK;
	}
	else if (type == REG_SZ) {
		size_t s = wcslen((wchar_t*)src) * 2;
		LPWSTR mem = (LPWSTR)CoTaskMemAlloc(s);
		if (!mem)
			return E_FAIL;

		memcpy(mem, src, s);

		dest->vt = VT_LPWSTR;
		dest->pwszVal = mem;
		return S_OK;
	}

	PROPVARIANT* p = (PROPVARIANT*)src;
	VARTYPE t = p->vt;

	if (t == VT_EMPTY) {
		size_t s = cb - 8;

		void* mem = CoTaskMemAlloc(s);
		if (!mem)
			return E_FAIL;

		dest->blob.cbSize = p->wReserved2;
		dest->blob.pBlobData = (BYTE*) mem;

		memcpy(mem, &p->pbstrVal, s);
		dest->vt = p->vt;
		return S_OK;
	}
	else if (t == VT_BLOB) {
		size_t s = cb - 8;
		void* mem = CoTaskMemAlloc(s);
		if (!mem)
			return E_FAIL;

		dest->blob.pBlobData =(BYTE*) mem;
		dest->blob.cbSize = cb;

		memcpy(mem, &p->blob, s);
		dest->vt = VT_EMPTY;
		return S_OK;
	}
	else if (t == VT_UI8 || t == VT_FILETIME) {
		size_t s = cb - 8;
		memcpy(&dest->pbVal, &p->pbVal, s);
		return S_OK;
	}

	switch (p->vt)
	{
	case VT_I2:
	case VT_I4:
	case VT_R4:
	case VT_R8:
	case VT_CY:
	case VT_DATE:
	case VT_ERROR:
	case VT_BOOL:
	case VT_UI1:
	case VT_UI2:
	case VT_I8:
	{
		memcpy(&dest->pbVal, &p->pbVal, cb - 8);
	}
	break;
	case VT_LPSTR:
	{
		char* str = (char*) &p->pbVal;
		size_t s = strlen(str);
		char* val = (char*) CoTaskMemAlloc(s);
		if (!val)
			return E_FAIL;

		memcpy(val, str, s);
		dest->pszVal = val;
		dest->vt = VT_EMPTY;
	}
	break;
	case VT_BSTR:
	{
		size_t s = wcslen(p->bstrVal + 1);
		void* mem = CoTaskMemAlloc(s * 2);
		if (!mem)
			return E_FAIL;
		memcpy(mem, &p->bstrVal, s * 2);

		dest->pbstrVal = (BSTR*)mem;
		dest->vt = VT_EMPTY;
	}
	break;
	case VT_CLSID:
	{
		GUID* g = (GUID*)CoTaskMemAlloc(sizeof(GUID));
		if (!g)
			return E_FAIL;
		memcpy(g, &p->pbVal, sizeof(GUID));
		dest->puuid = g;
		dest->vt = VT_EMPTY;
	}
	break;
	};
	return S_OK;
}

