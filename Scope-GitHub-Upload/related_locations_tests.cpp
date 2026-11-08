#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cwctype>
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
struct Entry{std::wstring name,path;};
std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),towlower);return s;}
#include "related_locations.h"
int main(){
 SetEnvironmentVariableW(L"LOCALAPPDATA",L"C:\\Users\\Fixture\\AppData\\Local");
 SetEnvironmentVariableW(L"APPDATA",L"C:\\Users\\Fixture\\AppData\\Roaming");
 SetEnvironmentVariableW(L"USERPROFILE",L"C:\\Users\\Fixture");
 SetEnvironmentVariableW(L"ProgramData",L"C:\\ProgramData");
 auto paths=relatedDataLocations({L"Fixture App",L"F:\\NotPresent\\app.exe"});
 auto has=[&](const wchar_t* p){return std::find(paths.begin(),paths.end(),fs::path(p))!=paths.end();};
 assert(has(L"C:\\ProgramData\\Fixture App"));
 assert(has(L"C:\\Users\\Fixture\\AppData\\LocalLow\\Fixture App"));
 assert(has(L"C:\\Users\\Fixture\\AppData\\Roaming\\Fixture App"));
 assert(relatedDataLocations({L"Adobe",L"F:\\NotPresent\\app.exe"}).empty());
 assert(relatedDataLocations({L"..\\Other",L"F:\\NotPresent\\app.exe"}).empty());
 paths=relatedDataLocations({L"Mozilla Firefox",L"F:\\NotPresent\\firefox.exe"});
 assert(has(L"C:\\Users\\Fixture\\AppData\\Roaming\\Mozilla\\Firefox"));
 std::cout<<"PASS: C-drive candidates for other-drive apps, Firefox profiles, shared vendor and traversal exclusions; no files deleted\n";
}
