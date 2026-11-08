#pragma once
// Candidate paths are exact product directories, never publisher-wide folders.
std::vector<fs::path> relatedDataLocations(const Entry& e){
 std::vector<fs::path> result;
 auto add=[&](const wchar_t* variable,const fs::path& tail){wchar_t base[32768];if(GetEnvironmentVariableW(variable,base,32768))result.push_back(fs::path(base)/tail);};
 auto safeName=[](const std::wstring& name){
  static const std::set<std::wstring> generic={L"adobe",L"microsoft",L"google",L"oracle",L"mozilla",L"common files",L"programs",L"bin",L"application",L"applications",L"temp",L"cache",L"packages",L"data",L"users"};
  return name.size()>2&&name.find_first_of(L"\\/:*?\"<>|")==std::wstring::npos&&name.back()!=L'.'&&name.back()!=L' '&&!generic.count(lower(name));
 };
 std::error_code ec;
 std::set<std::wstring> names{e.name};
 if(fs::is_directory(e.path,ec))names.insert(fs::path(e.path).filename().wstring());
 for(const auto& name:names)if(safeName(name)){
  for(auto variable:{L"LOCALAPPDATA",L"APPDATA",L"ProgramData"})add(variable,name);
  add(L"USERPROFILE",fs::path(L"AppData")/L"LocalLow"/name);
  if(lower(name).find(L"adobe ")==0)for(auto variable:{L"LOCALAPPDATA",L"APPDATA",L"ProgramData"})add(variable,fs::path(L"Adobe")/name);
 }
 auto name=lower(e.name);
 if(name==L"google chrome"){
  add(L"LOCALAPPDATA",L"Google/Chrome/User Data");
 }else if(name==L"microsoft edge"){
  add(L"LOCALAPPDATA",L"Microsoft/Edge/User Data");
 }else if(name.find(L"mozilla firefox")==0){
  add(L"LOCALAPPDATA",L"Mozilla/Firefox");add(L"APPDATA",L"Mozilla/Firefox");
 }else if(name.find(L"microsoft visual studio code")==0){
  add(L"APPDATA",L"Code");
 }
 return result;
}
