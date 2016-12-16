
/*

  Copyright (C) 2011, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#include <windows.h>
#include <molebox.h>

static MOLEBOX_API* Molebox_API = 0;
static HMODULE Molebox_Dll_Instance = 0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID _)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		Molebox_Dll_Instance = hinstDLL;
	return TRUE;
}

EXPORTABLE
const void* Molebox_Get_Core(uint32_t* out_core_size)
{
	static const void* overlay = 0;
	static uint32_t overlay_size = 0;

	if (overlay)
	{
	succeeded:
		if (out_core_size) *out_core_size = overlay_size;
		return overlay;
	}
	else
	{
		HRSRC hr = FindResourceW(Molebox_Dll_Instance, MAKEINTRESOURCE(1), RT_RCDATA);
		if (!hr)
			abort(); /* impossible if build right */
		overlay = LockResource(LoadResource(Molebox_Dll_Instance, hr));
		if (!overlay)
			abort(); /* impossible if build right */
		overlay_size = SizeofResource(Molebox_Dll_Instance, hr);
		goto succeeded;
	}
}

#if !defined _M_AMD64

static MOLEBOX_API* Molebox_Load_Core(int opts, void* key, int64_t offs)
{
	enum { LOADER_RESERVE = 4096 * 16 };
	MOLEBOX_API* api = 0;
	void *(*starter)() = VirtualAlloc(0, LOADER_RESERVE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	const void* overlay = Molebox_Get_Core(0);
	memcpy(starter, overlay, LOADER_RESERVE);
	api = starter(overlay, opts, key, &offs);
	VirtualFree(starter, 0, MEM_RELEASE);
	return api;
}

EXPORTABLE
int Molebox_Init(int opts, void* key, int64_t offs)
{
	if (!Molebox_Query_API())
	{
		if (!Molebox_API)
			Molebox_API = Molebox_Load_Core(opts, key, offs);
		if (!Molebox_API)
			return MOLEBOX_CORE_ERROR_FAILED_TO_LOAD;
	}

	return Molebox_API->buildno;
}

EXPORTABLE
MOLEBOX_API* Molebox_Query_API()
{
	if (!Molebox_API)
		GetEnvironmentVariableW(L"molebox;api", (LPWSTR)&Molebox_API, sizeof(Molebox_API));
	return Molebox_API;
}

EXPORTABLE
int Molebox_BuildNo()
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	return Molebox_API->buildno;
}

EXPORTABLE
int Molebox_PcId()
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	return Molebox_API->PcId();
}

EXPORTABLE
int Molebox_CmdLine_Set(const wchar_t* cmdl)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	Molebox_API->CmdLine_Set(cmdl);
	return 0;
}

EXPORTABLE
int Molebox_CmdLine_Parse(int* argc, wchar_t** *argv)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	Molebox_API->CmdLine_Parse(argc, argv);
	return 0;
}

EXPORTABLE
int Molebox_CmdLine_SetArgs(const wchar_t** args, size_t count)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	Molebox_API->CmdLine_SetArgs(args, count);
	return 0;
}

EXPORTABLE
int Molebox_CmdLine_InsertArgs(int after, const wchar_t** args, size_t count)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	Molebox_API->CmdLine_InsertArgs(after, args, count);
	return 0;
}

EXPORTABLE
int Molebox_Mount(const wchar_t* mask, char* passwd, int opts)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	return Molebox_API->Mount(mask, passwd, opts);
}

EXPORTABLE
int Molebox_Mount_At(const wchar_t* path, char* passwd, int opts, int64_t offs)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	return Molebox_API->Mount_At(path, passwd, opts, offs);
}

EXPORTABLE
int Molebox_Exec(const wchar_t* progname, int opts)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	return Molebox_API->Exec(progname, opts);
}

EXPORTABLE int Molebox_Mount_RsRc(HANDLE hMod, int rcid, char* passwd, int opts)
{
	if (!Molebox_API)
		return MOLEBOX_CORE_ERROR_IS_NOT_LOADED;
	else
	{
		HRSRC hr;
		void* rc;
		int64_t offs = 0;
		int path_L = 0;
		wchar_t path[256] = {0};

		wchar_t* rcT = rcid >= 0 ? (wchar_t*)10 : (wchar_t*)2;
		rcid = rcid < 0 ? -rcid : rcid;

		if (!hMod) hMod = GetModuleHandleW(0);

		path_L = GetModuleFileNameW(hMod, path, sizeof(path) / sizeof(path[0]));
		if (!path_L)
			return MOLEBOX_CORE_ERROR_FILE_NOT_FOUND;

		hr = FindResourceW(hMod, (wchar_t*)rcid, rcT);
		if (!hr)
			return MOLEBOX_CORE_ERROR_RSRC_NOT_FOUND;

		rc = LockResource(LoadResource(hMod, hr));
		if (rc && rc > (void*)hMod)
		{
			int i;
			unsigned long m = (unsigned long)hMod & ~(unsigned long)0x0ffff;
			unsigned long rva = (unsigned long)rc - m;
			IMAGE_NT_HEADERS* nth = ((IMAGE_NT_HEADERS*)((unsigned char*)m + ((IMAGE_DOS_HEADER*)m)->e_lfanew));
			IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nth);
			for (i = 0; i < nth->FileHeader.NumberOfSections; ++i)
				if ((rva - sec[i].VirtualAddress) < sec[i].SizeOfRawData)
				{
					offs = (sec[i].PointerToRawData + (rva - sec[i].VirtualAddress));
					break;
				}
			if (!offs)
				return MOLEBOX_CORE_ERROR_RSRC_NOT_FOUND;
		}
		else
			return MOLEBOX_CORE_ERROR_RSRC_NOT_FOUND;

		return Molebox_Mount_At(path, passwd, opts, offs);
	}
}

#endif
