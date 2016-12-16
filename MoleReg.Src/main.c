
/*

  Copyright (C) 2014, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#define _HEAPINSPECTOR 255

#include <c+/oj.hc>
#include <c+/program.hc>
#include <c+/array.hc>
#include <c+/buffer.hc>

#include <molebox.h>

#include "regimport.hc"

int silent = 0;

void Do_Component_Register()
{
	int i;
	CoInitialize(0);

	for (i = 0 ; i < Prog_Arguments_Count(); ++i)
	{
		void* ffh;
		WIN32_FIND_DATAW wfd;
		char* cot_para_name = Prog_Argument(i);
		char* cot_full_name = Path_Fullname(cot_para_name);
		wchar_t* file_patt = Str_Utf8_To_Unicode(Path_Basename(cot_full_name));
		wchar_t* file_path = Str_Utf8_To_Unicode(Path_Dirname(cot_full_name));
		wchar_t* file_full = Str_Utf8_To_Unicode(cot_full_name);

		ffh = FindFirstFileW(file_full, &wfd);

		if (INVALID_HANDLE_VALUE != ffh) do
			{
				long(__stdcall * DLLregisterServer)(void);
				void* hdll;
				wchar_t* S;

				S = Str_Unicode_Join_2('\\', file_path, wfd.cFileName);
				hdll = LoadLibraryW(S);
				//printf("%S=>%08x\n",S,hdll);

				if (hdll)
				{
					*(void**)&DLLregisterServer = GetProcAddress(hdll, "DllRegisterServer");
					if (DLLregisterServer)
					{
						long err = DLLregisterServer();
						if (!SUCCEEDED(err))
							silent || fprintf(stderr, "! %S failed to register\n", wfd.cFileName);
						else
							silent || fprintf(stderr, "+ %S registered successful\n", wfd.cFileName);
					}
					else
						silent || fprintf(stderr, "- %S doesn't have DllRegisterServer entery\n", wfd.cFileName);
				}
				else
				{
					silent || fprintf(stderr, "! %S failed to load\n", wfd.cFileName);
				}
			}
			while (FindNextFileW(ffh, &wfd));
	}
}

wchar_t* Normalize_G_Path(wchar_t* value_data, DWORD* data_len)
{
	int i;
	static C_ARRAY* G_Path_List = 0;
	static wchar_t* WinSys_Folder = 0;
	static wchar_t* System32_Folder = 0;
	static wchar_t* User_Home = 0;

	if (!G_Path_List)
	{
		enum {bf_size = 4096};
		wchar_t* S = 0;
		wchar_t* p;

		p = __Malloc(bf_size * sizeof(wchar_t));
		G_Path_List = __Refe(Array_Ptrs());

		if (Prog_Has_Opt("G"))
		{
			S = Str_Utf8_To_Unicode(Current_Directory());
			Str_Unicode_Cat(&S, L"\\", 1);
		}

		if (Prog_Has_Opt("g"))
		{
			S = Str_Utf8_To_Unicode(Prog_First_Opt("g", 0));
		}

		if (S)
		{

			GetShortPathNameW(S, p, bf_size);
			Oj_Push(G_Path_List, Str_Unicode_Copy_Npl(p, -1));

			GetLongPathNameW(S, p, bf_size);
			Oj_Push(G_Path_List, Str_Unicode_Copy_Npl(p, -1));

			if (p[1] == ':')
			{
				wchar_t Q[3] = { S[0], ':', 0 };
				wchar_t N[MAX_PATH + 1] = {0};
				DWORD foo = MAX_PATH;
				int err;
				p[0] = 0;
				if (0 == WNetGetConnectionW(Q, N, &foo))
				{
					wchar_t* q = Str_Unicode_Concat(N, p + 2);
					Oj_Push(G_Path_List, __Retain(q));
					GetShortPathNameW(q, p, bf_size);
					Oj_Push(G_Path_List, Str_Unicode_Copy_Npl(p, -1));
				}
			}
		}

		GetWindowsDirectoryW(p, bf_size);
		WinSys_Folder = Str_Unicode_Copy_Npl(p, -1);
		System32_Folder = Str_Unicode_Join_2('\\', WinSys_Folder, L"system32");

		if (Prog_Has_Opt("t"))
		{
			for (i = 0; i < G_Path_List->count; ++i)
			{
				silent || printf("%d: %S\n", i, (wchar_t*)G_Path_List->at[i]);
			}
		}
	}

	for (i = 0; i < G_Path_List->count; ++i)
	{
		if (Str_Unicode_Starts_With_Nocase(value_data, G_Path_List->at[i]))
		{
			int L = wcslen(G_Path_List->at[i]);
			wchar_t* S = Str_Unicode_Join_2('\\', L"{%APPFOLDER%}", value_data + L);
			*data_len = (wcslen(S) + 1) * sizeof(wchar_t);
			return S;
		}
	}

	if (Str_Unicode_Starts_With_Nocase(value_data, User_Home))
	{
		int L = wcslen(User_Home);
		wchar_t* S = Str_Unicode_Join_2('\\', L"{%USERHOME%}", value_data + L);
		*data_len = (wcslen(S) + 1) * sizeof(wchar_t);
		return S;
	}

	if (Str_Unicode_Starts_With_Nocase(value_data, System32_Folder))
	{
		int L = wcslen(WinSys_Folder);
		wchar_t* S = Str_Unicode_Join_2('\\', L"{%SYSTEM32%}", value_data + L);
		*data_len = (wcslen(S) + 1) * sizeof(wchar_t);
		return S;
	}

	if (Str_Unicode_Starts_With_Nocase(value_data, WinSys_Folder))
	{
		int L = wcslen(WinSys_Folder);
		wchar_t* S = Str_Unicode_Join_2('\\', L"{%WINSYS%}", value_data + L);
		*data_len = (wcslen(S) + 1) * sizeof(wchar_t);
		return S;
	}

	return __Memcopy(value_data, *data_len);
}

static void* regfile;
static wchar_t keyname[MAX_PATH * 2 + 1] = {0};
void Dump_Registry_SubKeys(HKEY key, int off)
{
	static int err;
	int i;
	keyname[off++] = '\\';

	for (i = 0; ; ++i)
	{
		err = RegEnumKeyW(key, i, keyname + off, MAX_PATH);
		if (err == ERROR_NO_MORE_ITEMS)
		{
			// ok
			break;
		}
		else if (err)
		{
			silent || fprintf(stderr, "! failed to enumerate registry key: %d\n", err);
			break;
		}
		else
		{
			HKEY hkey;

			if (Str_Unicode_Starts_With_Nocase(keyname, L"HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES"))
				continue;

			if (0 == (err = RegOpenKeyW(key, keyname + off, &hkey)))
			{
				__Auto_Release
				{
					int j;
					DWORD tp = 0, data_len = 4096, name_len = MAX_PATH;
					static wchar_t value_name[MAX_PATH + 1];
					static byte_t  value_data[4096 + 2];
					REGISTRY_RECORD* rr = REGISTRY_RECORD_Init(keyname, 0);
					char* S;
					for (j = 0; 0 == RegEnumValueW(hkey, j, value_name, &name_len, 0, &tp, value_data, &data_len); ++j)
					{
						void* rrval = 0;
						if (tp = REG_SZ)
						{
							wchar_t* S;

							if (data_len >= 2 && (value_data[data_len - 1] == 0 && value_data[data_len - 2] == 0))
								data_len -= 2;

							value_data[data_len] = 0;
							value_data[data_len + 1] = 0;
							data_len += 2;

							S = Normalize_G_Path((wchar_t*)value_data, &data_len);

							if (wcslen((wchar_t*)value_name))
								rrval = REGISTRY_VALUE_Init(Str_Unicode_Copy_Npl((wchar_t*)value_name, -1),
								__Retain(S),
								data_len, tp);
							else
								rr->dfltval = __Retain(S);
						}
						else
						{
							rrval = REGISTRY_VALUE_Init(Str_Unicode_Copy_Npl((wchar_t*)value_name, -1),
							                            __Memcopy_Npl(value_data, data_len),
							                            data_len, tp);
							//printf("+ key: %S\n",keyname);
							//printf("&& %d: '%S'(%d)\n",j,value_name,tp);
						}
						if (rrval)
							Oj_Push(rr->valarr, __Retain(rrval));
					}

					if (rr->dfltval || rr->valarr->count)
					{
						void* S = Format_Registry_Record(rr, (regfile ? RR_CHARSET_UNICODE : RR_CHARSET_UTF8));
						if (regfile)
						{
							int i, L;
							Str_Unicode_Cr_To_CfLr_Inplace((void*)&S);
							L = wcslen(S);
							__Elm_Append(&S, L, L"\r\n", 2, sizeof(wchar_t), 0);
							L += 2;

							L *= sizeof(wchar_t);
							Oj_Write(regfile, S, L, L);
						}
						else
							puts(S);
					}
				}

				Dump_Registry_SubKeys(hkey, wcslen(keyname));
				RegCloseKey(hkey);
			}
			else if (err != 2)
				silent || fprintf(stderr, "! failed to open subkey '%S', error %d\n", keyname, err);
		}
	}
}

void Do_Registry_Dump()
{

	if (Prog_Has_Opt("o"))
		regfile = Cfile_Open(Prog_First_Opt("o", 0), "w+");
	else
		regfile = 0;

	if (regfile)
		Oj_Write_BOM(regfile, C_BOM_UTF16_LE);

	wcscpy(keyname, L"HKEY_CLASSES_ROOT");
	Dump_Registry_SubKeys(HKEY_CLASSES_ROOT, wcslen(keyname));
	wcscpy(keyname, L"HKEY_CURRENT_USER");
	Dump_Registry_SubKeys(HKEY_CURRENT_USER, wcslen(keyname));
	wcscpy(keyname, L"HKEY_LOCAL_MACHINE");
	Dump_Registry_SubKeys(HKEY_LOCAL_MACHINE, wcslen(keyname));

	if (regfile)
		Oj_Close(regfile);
}

void copyright()
{
	silent || fprintf(stderr, "Molebox ActiveX Registry Maker (OSS)\n");
	silent || fprintf(stderr, "follow me on github goo.gl/Ur4E1O\n~\n");
}

void usage()
{
	silent || fprintf(stderr, "usage: mkreg -G -o regfile.reg *.ocx\n");
}

int main(int argc, char** argv)
{

	Prog_Init(argc, argv, "?|h,o:,g:,G,j,t,s", PROG_EXIT_ON_ERROR);

	if (Prog_Has_Opt("?") || !Prog_Arguments_Count())
	{
		copyright();
		usage();
		exit(-1);
	}

	if (Prog_Has_Opt("s"))
		silent = 1;
	copyright();

	__Try_Exit
	{
		MOLEBOX_API* api;
		int opts = MOLEBOX_CORE_DEFAULT;

#ifdef _DEBUG
		opts |= MOLEBOX_CORE_DEBUG_LOG;
#endif

		if ( Molebox_Init(opts, 0, 0) < 0 )
		{
			silent || fprintf(stderr,"could not initialize molebox runtime\n");
			exit(1);
		}

		api = Molebox_Query_API();

		if (Prog_Has_Opt("j"))
			api->Set_Registry_Virtualization(VIRTUAL_VIRTUALIZE_REGISTRY);
		else
			api->Set_Registry_Virtualization(FULLY_VIRTUALIZE_REGISTRY);
		Do_Component_Register();

		api->Set_Registry_Virtualization(VIRTUAL_VIRTUALIZE_REGISTRY);
		Do_Registry_Dump();
	}

	return 0;
}

