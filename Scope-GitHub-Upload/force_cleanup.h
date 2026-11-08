#pragma once
#include "service_cleanup.h"
#include "startup_cleanup.h"
#include "permission_repair.h"
#include <memory>
struct CleanupFailure{fs::path path;DWORD error;};
struct ForceCleanupResult{unsigned long long removed=0;std::vector<CleanupFailure> failures;};
bool rebootEligible(DWORD error){return error==ERROR_SHARING_VIOLATION||error==ERROR_LOCK_VIOLATION||error==ERROR_DIR_NOT_EMPTY;}
void forceRemoveTree(const fs::path& path,ForceCleanupResult& result,bool repairPermissions=false){
 std::unique_ptr<PermissionRepair> repair;
 auto repairAccess=[&](){if(!repairPermissions||repair)return false;repair=std::make_unique<PermissionRepair>(path,[&](DWORD error){result.failures.push_back({path,error});});return repair->apply()==ERROR_SUCCESS;};
 if(targetGone(path.wstring()))return;
 if(!validDeleteTarget(path)&&(!repairAccess()||!validDeleteTarget(path))){result.failures.push_back({path,ERROR_ACCESS_DENIED});return;}
 DWORD attributes=GetFileAttributesW(path.c_str());
 if(attributes==INVALID_FILE_ATTRIBUTES){result.failures.push_back({path,GetLastError()});return;}
 if((attributes&FILE_ATTRIBUTE_DIRECTORY)&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT)){
  WIN32_FIND_DATAW data{};auto pattern=path/L"*";
  HANDLE search=FindFirstFileW(pattern.c_str(),&data);DWORD enumerationError=search==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;
  if(search==INVALID_HANDLE_VALUE&&enumerationError==ERROR_ACCESS_DENIED&&repairAccess()){search=FindFirstFileW(pattern.c_str(),&data);enumerationError=search==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;}
  if(search==INVALID_HANDLE_VALUE){if(enumerationError!=ERROR_FILE_NOT_FOUND){result.failures.push_back({path,enumerationError});return;}}
  else{
   do{std::wstring name=data.cFileName;if(name!=L"."&&name!=L"..")forceRemoveTree(path/name,result,repairPermissions);}while(FindNextFileW(search,&data));
   enumerationError=GetLastError();FindClose(search);if(enumerationError!=ERROR_NO_MORE_FILES){result.failures.push_back({path,enumerationError});return;}
  }
 }
 if(attributes&FILE_ATTRIBUTE_READONLY)SetFileAttributesW(path.c_str(),attributes&~FILE_ATTRIBUTE_READONLY);
 DWORD error=0;
 for(int attempt=0;attempt<3;attempt++){
  BOOL ok=(attributes&FILE_ATTRIBUTE_DIRECTORY)?RemoveDirectoryW(path.c_str()):DeleteFileW(path.c_str());
  if(ok){if(repair)repair->deleted=true;result.removed++;return;}
  error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND){if(repair)repair->deleted=true;return;}
  if(error==ERROR_ACCESS_DENIED&&repairAccess())continue;
  if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION)break;
  if(attempt<2)Sleep(100);
 }
 result.failures.push_back({path,error});
}
std::wstring failureDescription(const CleanupFailure& failure){
 wchar_t message[512]{};FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,failure.error,0,message,512,nullptr);
 return failure.path.wstring()+L"\n  错误 "+std::to_wstring(failure.error)+L"："+message+L"\n";
}
// Only executable images inside the selected installation path are candidates.
// Keep handles open across confirmation so PID reuse cannot target another process.
bool confirmStopApplication(const fs::path& installation,const std::function<int(const std::wstring&)>& confirm={}){
 DWORD attr=GetFileAttributesW(installation.c_str());
 if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return true;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;
 std::vector<HANDLE> handles;std::wstring names;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
 using CriticalCheck=BOOL(WINAPI*)(HANDLE,PBOOL);
 auto criticalCheck=(CriticalCheck)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"IsProcessCritical");
 if(Process32FirstW(snapshot,&entry))do{
  if(entry.th32ProcessID==GetCurrentProcessId()||entry.th32ProcessID<=4)continue;
  HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,entry.th32ProcessID);if(!process)continue;
  wchar_t image[32768];DWORD length=32768;BOOL critical=TRUE;
  if(QueryFullProcessImageNameW(process,0,image,&length)&&inside(image,installation.wstring())&&validDeleteTarget(image)&&criticalCheck&&criticalCheck(process,&critical)&&!critical){
   handles.push_back(process);names+=std::wstring(image)+L" (PID "+std::to_wstring(entry.th32ProcessID)+L")\n";
  }else CloseHandle(process);
 }while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);
 if(handles.empty())return true;
 auto prompt=L"以下进程属于待删除的安装位置：\n\n"+names+L"\n是否结束这些进程并继续强制删除？未保存的内容会丢失。选择“否”取消本次删除。";
 bool proceed=(confirm?confirm(prompt):deleteDialog(nullptr,prompt.c_str(),L"解除软件自身进程占用",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING))==IDYES;
 if(proceed){for(auto process:handles)if(WaitForSingleObject(process,0)==WAIT_TIMEOUT)TerminateProcess(process,1);auto deadline=GetTickCount64()+5000;for(auto process:handles){auto now=GetTickCount64();WaitForSingleObject(process,now<deadline?(DWORD)(deadline-now):0);}}
 if(proceed)for(auto process:handles)if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0){proceed=false;deleteDialog(nullptr,L"目标进程未能结束（可能仍被保护或正在退出），未继续删除文件。",L"结束进程失败",MB_OK|MB_ICONWARNING);break;}
 for(auto process:handles)CloseHandle(process);return proceed;
}
// Injectable operation keeps tests from changing the actual Windows reboot queue.
size_t scheduleCleanup(const std::vector<CleanupFailure>& failures,const std::function<bool(const fs::path&)>& schedule){
 size_t pending=0;std::set<std::wstring> seen;
 // Failure traversal is postorder: children must be queued before their directories.
 for(const auto& failure:failures)if(rebootEligible(failure.error)&&seen.insert(lower(failure.path.wstring())).second&&validDeleteTarget(failure.path)&&schedule(failure.path))pending++;
 return pending;
}
void collectRebootPlan(const fs::path& path,std::vector<CleanupFailure>& plan,size_t& failed){
 if(targetGone(path.wstring()))return;
 if(plan.size()>=100000||!validDeleteTarget(path)){failed++;return;}
 DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT)){failed++;return;}
 if(attr&FILE_ATTRIBUTE_DIRECTORY){
  WIN32_FIND_DATAW data{};auto pattern=path/L"*";HANDLE search=FindFirstFileW(pattern.c_str(),&data);
  if(search==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_NOT_FOUND){failed++;return;}}
  else{do{std::wstring name=data.cFileName;if(name!=L"."&&name!=L"..")collectRebootPlan(path/name,plan,failed);}while(FindNextFileW(search,&data));auto error=GetLastError();FindClose(search);if(error!=ERROR_NO_MORE_FILES){failed++;return;}}
 }
 plan.push_back({path,(DWORD)((attr&FILE_ATTRIBUTE_DIRECTORY)?ERROR_DIR_NOT_EMPTY:ERROR_SHARING_VIOLATION)});
}
int deferRemainingCleanup(const std::vector<fs::path>& targets){
 std::vector<CleanupFailure> plan;size_t unreadable=0;for(const auto& path:targets)collectRebootPlan(path,plan,unreadable);
 size_t pending=scheduleCleanup(plan,[](const fs::path& path){BOOL ok=MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);if(!ok)deleteSessionLog+=L"重启删除提交失败："+path.wstring()+L"，错误 "+std::to_wstring(GetLastError())+L"\r\n";return ok!=FALSE;});
 deleteSessionLog+=L"卸载器要求重启。剩余路径已按先文件、后目录提交重启删除："+std::to_wstring(pending)+L" 项；无法枚举："+std::to_wstring(unreadable)+L" 项。未提交项仍可能残留，重启后请复查。\r\n";
 return pending?3:4;
}
int runForceCleanup(const std::vector<fs::path>& targets,bool sharedComponent){
 ServiceCleanup services;
 if(!services.prepare(targets.front()))return 2;
 StartupCleanup startup;if(!startup.prepare(targets.front()))return 2;
 if(!confirmStopApplication(targets.front()))return 2;
 ForceCleanupResult result;for(const auto& path:targets)forceRemoveTree(path,result);
 if(std::any_of(result.failures.begin(),result.failures.end(),[](const CleanupFailure& failure){return failure.error==ERROR_ACCESS_DENIED;})){
  std::wstring denied;for(const auto& failure:result.failures)if(failure.error==ERROR_ACCESS_DENIED&&denied.size()<6000)denied+=failure.path.wstring()+L"\n";
  auto prompt=L"以下目标因访问权限不足未删除：\n\n"+denied+L"\n是否修复这些目标及删除范围内剩余项的权限并重试？必要时接管所有权、禁用继承，临时只给当前管理员完整控制。不会修改父目录权限，不处理链接目标或多硬链接文件；失败时尝试恢复原权限。此操作不能绕过驱动自保护。";
  if(deleteDialog(nullptr,prompt.c_str(),L"修复高级权限并强制删除",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)==IDYES){ForceCleanupResult retry;retry.removed=result.removed;for(const auto& path:targets)forceRemoveTree(path,retry,true);result=std::move(retry);}
 }
 size_t pending=0;std::wstring report;
 for(size_t i=0;i<result.failures.size()&&i<30;i++)report+=failureDescription(result.failures[i]);
 if(result.failures.size()>30)report+=L"另有 "+std::to_wstring(result.failures.size()-30)+L" 项未删除。\n";
 if(std::any_of(result.failures.begin(),result.failures.end(),[](const CleanupFailure& f){return rebootEligible(f.error);})){ 
  auto prompt=L"以下内容尚未删除：\n\n"+report+L"\n是否将其中被占用文件及其非空目录安排在下次重启时删除？不会立即重启。权限不足项不会自动解除权限。重启前请勿在这些路径重新安装或保存新文件。";
  if(deleteDialog(nullptr,prompt.c_str(),L"是否安排重启删除",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES)
   pending=scheduleCleanup(result.failures,[](const fs::path& path){return MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT)!=FALSE;});
 }
 unsigned long long failed=result.failures.size();askRemoveEmptyParents(targets,result.removed,failed);
 bool gone=std::all_of(targets.begin(),targets.end(),[](const fs::path& p){return targetGone(p.wstring());});
 bool complete=failed==0&&pending==0&&gone;
 if(complete&&!services.finish()){complete=false;failed++;}
 startup.committed=complete;
 auto text=L"已删除："+std::to_wstring(result.removed)+L" 项\n尚未删除/跳过："+std::to_wstring(failed)+L" 项\n其中已安排重启删除："+std::to_wstring(pending)+L" 项\n\n"+report;
 if(pending)text+=L"重启删除只是已提交，尚未完成；请重启后重新扫描核实。应用暂留在列表，不播放成功特效。\n";
 text+=L"仅处理确认过的路径；未识别的注册项、服务、驱动和关联数据没有处理。";
 deleteDialog(nullptr,text.c_str(),complete?L"已被强制删除，请确认":pending?L"待重启核实删除":L"部分内容未删除",MB_OK|(complete?MB_ICONINFORMATION:MB_ICONWARNING));
 return complete?0:pending?3:1;
}
