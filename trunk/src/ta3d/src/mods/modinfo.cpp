#include <logs/logs.h>
#include <misc/paths.h>
#include <misc/files.h>
#include <misc/tdf.h>
#include "modinfo.h"
#include "mods.h"

namespace TA3D
{
    ModInfo::ModInfo(const int ID, const String &name, const String &version, const String &author, const String &comment, const String &url)
		: ID(ID), name(name), version(version), author(author), comment(comment), url(url), installed(false), availableUpdate(false)
    {
    }

    ModInfo::ModInfo(const String &info)
    {
        parse(info);
    }

    void ModInfo::parse(const String &info)
    {
        String::Vector params;

        String::Size pos = 0;
        while((pos = info.find_first_of("\"", pos)) != String::npos)
        {
            ++pos;
            String p;
            while(pos < info.size() && info[pos] != '"')
            {
                if (info[pos] == '\\' && pos + 1 < info.size())
                {
                    ++pos;
					if (info[pos] == 'n')
						p << '\n';
					else
						p << info[pos];
                }
				else if (info[pos] != '\r')
                    p << info[pos];
                ++pos;
            }
            ++pos;
            params.push_back(p);
        }

        if (params.size() != 6)     // If there is not the expected number of parameters, then we can't trust this information
        {
            *this = ModInfo();
            return;
        }

        ID = params[0].to<sint32>();
        version = params[1];
        name = params[2];
        url = params[3];
        author = params[4];
        comment = params[5];
		installed = false;
		availableUpdate = false;
    }

    void ModInfo::read(const String &modName)
    {
		name = modName;
		String filename = getPathToMod() << Paths::Separator << "info.mod";

		availableUpdate = false;

		if (!Paths::Exists(filename))
		{
			ID = -1;
			name = modName;
			version = String();
			author = String();
			comment = String();
			url = String();
			installed = false;
			return;
		}

		TDFParser file(filename, false, false, false, true);
        ID = file.pullAsInt("mod.ID", -1);
        name = file.pullAsString("mod.name");
        version = file.pullAsString("mod.version");
        author = file.pullAsString("mod.author");
		comment = file.pullAsString("mod.comment").replace("\\n", "\n");
        url = file.pullAsString("mod.url");
		installed = file.pullAsBool("mod.installed", false);
    }

	String ModInfo::getPathToMod() const
	{
		String path = Paths::Resources;
		path << "mods" << Paths::Separator << cleanStringForPortablePathName(name);
		return path;
	}

    void ModInfo::write()
    {
		if (name.empty() || ID == -1)       // Don't save empty data
            return;

		String filename = getPathToMod() << Paths::Separator << "info.mod";

        String file;
        file << "// TA3D Mod info file\n"
        << "// autogenerated by ta3d\n"
        << "[mod]\n"
        << "{\n"
        << "    ID = " << ID << ";\n"
        << "    name = " << name << ";\n"
        << "    version = " << version << ";\n"
        << "    author = " << author << ";\n"
		<< "    comment = " << String(comment).replace("\n", "\\n") << ";\n"
        << "    url = " << url << ";\n"
		<< "    installed = " << installed << ";\n"
        << "}";
		Paths::MakeDir(Paths::ExtractFilePath(filename, false));
        Paths::Files::SaveToFile(filename, file);
    }

	void ModInfo::setInstalled(bool b)
	{
		installed = b;
		write();

		Mods::instance()->update();
	}

	void ModInfo::uninstall()
	{
		if (ID < 0 || !installed)	return;

		LOG_INFO(LOG_PREFIX_RESOURCES << "uninstalling mod '" << name << "'");

		String modpath = getPathToMod();
		Paths::RemoveDir(modpath);
		installed = false;
		write();

		LOG_INFO(LOG_PREFIX_RESOURCES << "mod uninstalled");
	}

	String ModInfo::cleanStringForPortablePathName(const String &s)
	{
		String result;
		result.reserve(s.size());
		for(uint32 i = 0 ; i < s.size() ; ++i)
		{
			switch(s[i])
			{
			case ':':
			case '/':
			case '\\':
			case '?':
			case '*':
			case '<':
			case '>':
			case '|':
				break;
			default:
				result << s[i];
			};
		}

		return result;
	}
}
