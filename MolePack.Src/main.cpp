
/*

  Copyright (C) 2014, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#include <stdio.h>
#include <foobar/Win32Cmdline.hxx>
#include <foobar/DateTime.hxx>
#include <foobar/Console.hxx>
#include <foobar/StringTool.hxx>
#include <foobar/Path.hxx>
#include <libsynfo.h>
#include <molepkgr.hxx>

time_t _BUILD_TIME = POSIXBUILDTIME;

bool has_oneof_opts(foobar::Cmdline cmdl, const char* opts)
{
	return foobar::meta_split(opts, opts + strlen(opts), ',', [cmdl](std::string & optname) -> bool
	{
		return cmdl->opt[optname].Specified();
	});
}

using foobar::Console;
using foobar::format;

void usage()
{
	Console | "molepack.exe [options] <configfile>";
	Console | "possible options:";
	Console | "  -o <target file> ~ where to write packed executable or package";
	Console | "  -e <executable>  ~ executable will put in package for run on start";
	Console | "  -L               ~ enable logging";
	Console | "  -S <stub file>   ~ specified executable stub for use as launcher";
}

std::string tabs(int count)
{
	return std::string(count * 2, ' ');
}

int wmain(int argc, const wchar_t* argv[])
{
	try
	{
		const char* opts =  "U,help|?|h,o:|target:,e:|source:,g:,nologo";

		auto cmdl = foobar::Win32Cmdline::Parse(opts, argv, argc);

		if (!has_oneof_opts(cmdl, "?,U,nologo"))
		{
			auto dt = foobar::DateTime::FromPOSIXtime(_BUILD_TIME);
			Console | format("  Molebox Virtualization Wrapper (built at %d %s %d)",
			                 dt.Day(), dt.Smon(), dt.Year());
			Console | format("  follow me on github goo.gl/Ur4E1O");
			Console | "-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - ";
		}

		if (!cmdl->Good())
		{
			std::for_each(cmdl->errors.begin(), cmdl->errors.end(),
			[](foobar::strarg_t a) {Console | format("error: %s", a);});
			Console | "-";
			usage();
			return -1;
		}

		if (cmdl->opt["?"].Specified())
		{
			usage();
			return 0;
		}

		if (cmdl->opt["U"].Specified())
		{
			char os_info[SYNFO_OS_STRING_LENGTH];
			Synfo_Get_Os_String(os_info);
			char cpu_info[SYNFO_CPU_STRING_LENGTH];
			Synfo_Get_Cpu_String(cpu_info);

			auto dt = foobar::DateTime::FromPOSIXtime(_BUILD_TIME);
			Console | format("mxbpack buildtime %d %s %d", dt.Day(), dt.Smon(), dt.Year());
			Console | format("cmdline: %s", GetCommandLineW());
			Console | format("system: %s", os_info);
			Console | format("cpu: %s", cpu_info);
			Console | "-";
		}

		if (cmdl->argv.size() != 1)
		{
			Console | "error: this utility requires one mandatory parameter - config file name";
			Console | "-";
			usage();
			return 0;
		}

		auto config_file = foobar::Path<wchar_t>(cmdl->argv[0]).Fullpath();

		if (cmdl->opt["g"].Specified())
		{
			foobar::Path<wchar_t> search_in_dir = cmdl->opt["g"].Str();
			search_in_dir.Chdir();
			Console | format("work directory changed to '%s'",
			                 foobar::w_right(search_in_dir.Fullpath(),40));
		}

		MoleboxConfig conf;

		struct Tracker : MoleboxNotify
		{
			void Error(const wchar_t* details, MXBPAK_ERROR error) OVERRIDE
			{
				Console | format("%serror: %s", tabs(Stage()), details);
			}
			void Info(const wchar_t* details) OVERRIDE
			{
				Console | format("%s%s", tabs(Stage()), details);
			}
			void Begin(const wchar_t* details) OVERRIDE
			{
				if (foobar::length_of(details))
					Console | format("%s%s", tabs(Stage() - 1), details);
			}
			void End(const wchar_t* details, bool succeeded) OVERRIDE
			{
				if (foobar::length_of(details))
					Console | format("%s%s", tabs(Stage()), details);
			}
		} tracker;

		if (conf.Read(config_file, tracker))
		{
			if ( cmdl->opt["e"].Specified() )
				conf["source"] = cmdl->opt["e"].Cstr();
			if ( cmdl->opt["o"].Specified() )
				conf["target"] = cmdl->opt["o"].Cstr();
			if ( cmdl->opt["S"].Specified() )
				conf["stub"] = cmdl->opt["o"].Cstr();
			conf.Pack(tracker);
		}

		return 0;
	}
	catch (std::exception& e)
	{
		fputs("error: ", stderr);
		fputs(e.what(), stderr);
		fputs("\n", stderr);
		return -1;
	}
}
