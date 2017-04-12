
#include <iostream>
#include <string>
#include <Windows.h>

static void Replace(std::string& buff, const std::string& what, const std::string& with)
{
	std::string tmp(buff);
	size_t pos = tmp.find(what);

	while( pos != std::string::npos )
	{
		tmp.replace(pos, what.length(), with);
		pos = tmp.find(what, pos + with.length());
	}

	buff = tmp;
}

static bool RenameFile(const std::string& oldname, const std::string& newname, const std::string& path)
{
	std::string	newpath(path);
	std::string	name;
	size_t		pos, end;
	size_t		start = path.find_last_of("\\/");

	if( start != std::string::npos )
		++start;
	else
		start = 0;

	name = path.substr(start);

	end = name.find_last_of(".");
	name = name.substr(0, end);

	pos = name.find(oldname);

	if( pos != std::string::npos )
	{
		name.replace(pos, oldname.length(), newname);
		newpath.replace(start, end, name);

		MoveFileA(path.c_str(), newpath.c_str());
	}

	return (pos != std::string::npos);
}

static bool ReplaceInFile(const std::string& oldname, const std::string& newname, const std::string& file)
{
	std::string			buff;
	size_t				pos = 0;
	FILE*				fd = 0;
	bool				found = false;

	fopen_s(&fd, file.c_str(), "rb");

	if( !fd )
		return false;

	fseek(fd, 0, SEEK_END);
	size_t length = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	buff.resize(length);

	fread(&buff[0], 1, length, fd);
	fclose(fd);

	pos = buff.find(oldname);

	while( pos != std::string::npos )
	{
		found = true;

		buff.replace(pos, oldname.length(), newname);
		pos = buff.find(oldname, pos + newname.length());
	}
	
	if( found )
	{
		fopen_s(&fd, file.c_str(), "wb");

		if( !fd )
			return false;

		fwrite(buff.data(), 1, buff.length(), fd);
		fclose(fd);
	}

	return found;
}

void ProcessRecursive(const std::string& oldname, const std::string& newname, const std::string& path)
{
	WIN32_FIND_DATAA data;
	memset(&data, 0, sizeof(data));

	HANDLE h = FindFirstFileA((path + "*.*").c_str(), &data);

	if( h != INVALID_HANDLE_VALUE )
	{
		BOOL success = 1;

		do
		{
			std::string str(data.cFileName);

			if( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if( str != "." && str != ".." )
				{
					printf("Entering directory '%s'\n", data.cFileName);

					ProcessRecursive(oldname, newname, path + str + "\\");

					if( RenameFile(oldname, newname, path + data.cFileName) )
						printf("Renamed '%s'\n", data.cFileName);
				}
			}
			else
			{
				size_t p1 = str.rfind(".vcxproj");
				size_t p2 = str.rfind(".filters");
				size_t p3 = str.rfind(".sln");

				if( p1 != std::string::npos || p2 != std::string::npos || p3 != std::string::npos )
				{
					if( ReplaceInFile(oldname, newname, path + data.cFileName) )
						printf("Updated '%s'\n", data.cFileName);

					if( RenameFile(oldname, newname, path + data.cFileName) )
						printf("Renamed '%s'\n", data.cFileName);
				}
			}

			success = FindNextFileA(h, &data);
		}
		while( success != 0 );

		FindClose(h);
	}
	else
	{
		DWORD err = GetLastError();
		LPVOID lpMsgBuf = 0;

		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, NULL);

		std::cout << "File error (" << err << "):\n" << (const char*)lpMsgBuf << "\n";
		std::cout << path << "\n";

		LocalFree(lpMsgBuf);
	}
}

int main(int argc, char* argv[])
{
	if( argc != 4 )
	{
		std::cout << "Usage: renumber.exe oldname newname directory\n\n";
	}
	else
	{
		std::string oldname(argv[1]);
		std::string newname(argv[2]);
		std::string folder(argv[3]);

		Replace(folder, "/", "\\");

		if( folder[folder.length() - 1] != '\\' )
			folder += "\\";

		ProcessRecursive(oldname, newname, folder);
	}

	//system("pause");
	return 0;
}
