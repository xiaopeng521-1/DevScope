#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cassert>
#include <iostream>
#include "uninstaller_wait.h"
int main(int argc,char** argv){
 wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);
 if(argc>1){
  if(std::string(argv[1])=="leaf"){Sleep(1200);return 0;}
  if(std::string(argv[1])=="failure")return 7;
  std::wstring command=L"\""+std::wstring(self)+L"\" "+(std::string(argv[1])=="root"?L"child":L"leaf");
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  if(!CreateProcessW(self,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return 9;
  CloseHandle(process.hThread);CloseHandle(process.hProcess);return 0;
 }
 DWORD code=1;auto started=GetTickCount64();
 assert(waitForUninstaller(self,L"root",code));assert(code==0);
 assert(GetTickCount64()-started>=1200);
 assert(waitForUninstaller(self,L"failure",code));assert(code==7);
 std::cout<<"PASS: waits for child and grandchild after bootstrapper exits; preserves failure code\n";
}
