#include "main.cpp"
#include <cassert>
#include <iostream>
int main(){
 // A temporary volatile test key, never a Windows Run/RunOnce key.
 std::wstring path=L"Software\\DevScopeTest_"+std::to_wstring(GetCurrentProcessId());
 HKEY key=nullptr;DWORD disposition=0;
 assert(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&key,&disposition)==ERROR_SUCCESS);
 assert(disposition==REG_CREATED_NEW_KEY);
 std::wstring command=LR"("F:\Fixture\test.exe" --start)";
 std::vector<BYTE> data((command.size()+1)*sizeof(wchar_t));memcpy(data.data(),command.c_str(),data.size());
 assert(RegSetValueExW(key,L"Fixture",0,REG_SZ,data.data(),(DWORD)data.size())==ERROR_SUCCESS);
 {
  StartupCleanup cleanup;HKEY handle=nullptr;assert(RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_ALL_ACCESS,&handle)==ERROR_SUCCESS);
  assert(RegDeleteValueW(handle,L"Fixture")==ERROR_SUCCESS);
  cleanup.items.push_back({handle,L"Fixture",path,REG_SZ,data,true});
 }
 DWORD size=0;assert(RegQueryValueExW(key,L"Fixture",nullptr,nullptr,nullptr,&size)==ERROR_SUCCESS&&size==data.size());
 std::vector<BYTE> restored(size);assert(RegQueryValueExW(key,L"Fixture",nullptr,nullptr,restored.data(),&size)==ERROR_SUCCESS&&restored==data);
 {
  StartupCleanup cleanup;HKEY handle=nullptr;assert(RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_ALL_ACCESS,&handle)==ERROR_SUCCESS);
  assert(RegDeleteValueW(handle,L"Fixture")==ERROR_SUCCESS);
  cleanup.items.push_back({handle,L"Fixture",path,REG_SZ,data,true});cleanup.committed=true;
 }
 assert(RegQueryValueExW(key,L"Fixture",nullptr,nullptr,nullptr,&size)==ERROR_FILE_NOT_FOUND);
 RegCloseKey(key);assert(RegDeleteKeyW(HKEY_CURRENT_USER,path.c_str())==ERROR_SUCCESS);
 assert(serviceBelongsTo(command,LR"(F:\Fixture)"));assert(!serviceBelongsTo(command,LR"(F:\FixtureOther)"));
 std::cout<<"PASS: temporary startup value rollback, commit and exact path boundary; no real startup entries modified\n";
}
