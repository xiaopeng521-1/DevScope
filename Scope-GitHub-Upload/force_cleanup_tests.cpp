#include "main.cpp"
#include <cassert>
#include <iostream>
int main(int argc,char**){
 if(argc>1){Sleep(30000);return 0;}
 const auto root=fs::current_path()/L"force_cleanup_fixture";
 assert(!fs::exists(root));fs::create_directory(root);
 auto locked=root/L"locked.txt";std::ofstream(locked)<<"fixture";
 HANDLE lock=CreateFileW(locked.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
 ForceCleanupResult first;forceRemoveTree(root,first);assert(first.removed==0&&first.failures.size()==2);
 assert(first.failures[0].path==locked&&first.failures[0].error==ERROR_SHARING_VIOLATION);
 assert(first.failures[1].path==root);
 std::vector<fs::path> queued;
 assert(scheduleCleanup(first.failures,[&](const fs::path& p){queued.push_back(p);return true;})==2);
 assert(queued[0]==locked&&queued[1]==root);
 assert(scheduleCleanup({{locked,ERROR_ACCESS_DENIED}},[](const fs::path&){assert(false);return true;})==0);
 assert(scheduleCleanup(first.failures,[](const fs::path&){return false;})==0);
 CloseHandle(lock);SetFileAttributesW(locked.c_str(),FILE_ATTRIBUTE_READONLY);
 ForceCleanupResult second;forceRemoveTree(root,second);assert(second.failures.empty()&&second.removed==2&&!fs::exists(root));
 fs::create_directory(root);wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);
 auto child=root/L"fixture.exe";fs::copy_file(self,child);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
 std::wstring command=L"\""+child.wstring()+L"\" --hold";
 assert(CreateProcessW(child.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));CloseHandle(process.hThread);
 assert(!confirmStopApplication(root,[](const std::wstring&){return IDNO;}));
 assert(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT);
 assert(confirmStopApplication(root,[](const std::wstring& prompt){assert(prompt.find(L"fixture.exe")!=std::wstring::npos);return IDYES;}));
 assert(WaitForSingleObject(process.hProcess,1000)==WAIT_OBJECT_0);CloseHandle(process.hProcess);
 ForceCleanupResult final;forceRemoveTree(root,final);assert(final.failures.empty()&&!fs::exists(root));
 ForceCleanupResult protectedResult;forceRemoveTree(fs::current_path(),protectedResult);assert(protectedResult.removed==0&&!protectedResult.failures.empty());
 std::cout<<"PASS: lock reporting, read-only removal, reboot child-first order and failures (mock queue), confirmed own-process exit, cancel, protected root\n";
}
