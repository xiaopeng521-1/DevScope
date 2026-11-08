#pragma once
// Start suspended so descendants cannot escape tracking before job assignment.
// No kill-on-close or breakaway flags: monitoring must never terminate uninstallers.
bool waitForUninstaller(const std::wstring& executable,const std::wstring& arguments,DWORD& code){
 HANDLE job=CreateJobObjectW(nullptr,nullptr);
 if(!job)return false;
 std::wstring command=L"\""+executable+L"\" "+arguments;
 STARTUPINFOW startup{};startup.cb=sizeof(startup);
 PROCESS_INFORMATION process{};
 if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,nullptr,&startup,&process)){
  CloseHandle(job);
  // Shell activation may be required (for example, an elevation manifest).
  // The caller explicitly confirms completion to cover delegated service work.
  SHELLEXECUTEINFOW shell{};shell.cbSize=sizeof(shell);
  shell.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;
  shell.lpFile=executable.c_str();shell.lpParameters=arguments.c_str();shell.nShow=SW_SHOWNORMAL;
  if(!ShellExecuteExW(&shell)||!shell.hProcess)return false;
  bool ok=WaitForSingleObject(shell.hProcess,INFINITE)==WAIT_OBJECT_0&&GetExitCodeProcess(shell.hProcess,&code);
  CloseHandle(shell.hProcess);return ok;
 }
 bool tracked=AssignProcessToJobObject(job,process.hProcess)!=FALSE;
 if(ResumeThread(process.hThread)==DWORD(-1)){
  // Only this newly created, still suspended process is terminated on launch failure.
  TerminateProcess(process.hProcess,1);CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(job);return false;
 }
 CloseHandle(process.hThread);
 bool ok=true;
 if(tracked){
  for(;;){
   JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
   if(!QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr)){ok=false;break;}
   if(accounting.ActiveProcesses==0)break;
   Sleep(100);
  }
 }
 if(ok)ok=WaitForSingleObject(process.hProcess,INFINITE)==WAIT_OBJECT_0&&GetExitCodeProcess(process.hProcess,&code);
 CloseHandle(process.hProcess);CloseHandle(job);return ok;
}
