#pragma once
// Dedicated elevated child: command line paths, no shell commands or mutable request file.
std::wstring quoteArgument(const std::wstring& value){std::wstring result(1,wchar_t(34));size_t slashes=0;for(auto c:value){if(c==wchar_t(92)){slashes++;continue;}if(c==wchar_t(34)){result.append(slashes*2+1,wchar_t(92));result+=c;}else{result.append(slashes,wchar_t(92));result+=c;}slashes=0;}result.append(slashes*2,wchar_t(92));result+=wchar_t(34);return result;}
bool elevatedToken(){HANDLE token;TOKEN_ELEVATION elevation{};DWORD size=0;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;bool ok=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)&&elevation.TokenIsElevated;CloseHandle(token);return ok;}
bool validDeleteTarget(const fs::path& raw){auto text=raw.wstring();if(text.size()<4||text[1]!=L':'||!raw.is_absolute())return false;for(auto&part:raw)if(part==L"..")return false;if(!deletionPathAllowed(raw))return false;HANDLE handle=CreateFileW(raw.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(handle==INVALID_HANDLE_VALUE)return false;wchar_t resolved[32768];DWORD length=GetFinalPathNameByHandleW(handle,resolved,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);CloseHandle(handle);if(!length||length>=32768)return false;std::wstring actual(resolved,length);if(actual.size()>4&&actual[0]==wchar_t(92)&&actual[1]==wchar_t(92)&&actual[2]==L'?'&&actual[3]==wchar_t(92))actual.erase(0,4);if(!deletionPathAllowed(actual))return false;return true;}
#include "force_cleanup.h"
#include "uninstaller_resolution.h"
int handleElevatedDeleteCommand(){int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return -1;std::wstring mode=argc>=2?argv[1]:L"";bool unified=mode==L"--delete-unified"||mode==L"--delete-unified-components";bool components=mode==L"--delete-elevated-components"||mode==L"--delete-current-components"||mode==L"--delete-unified-components";bool needsElevation=unified||mode==L"--delete-elevated"||mode==L"--delete-elevated-components";if(!needsElevation&&mode!=L"--delete-current"&&mode!=L"--delete-current-components"){LocalFree(argv);return -1;}singleDeleteFlow=unified;DeleteReportWriter reportWriter;int start=2;std::wstring uninstall;if(argc>=4&&std::wstring(argv[2])==L"--uninstaller"){uninstall=argv[3];start=4;}std::vector<fs::path> targets;for(int i=start;i<argc;i++)targets.push_back(argv[i]);LocalFree(argv);
 if(targets.empty()||targets.size()>64){deleteDialog(nullptr,L"删除请求为空或超出限制，未执行删除。",L"请求无效",MB_OK);return 2;}
 for(auto&p:targets)if(!validDeleteTarget(p)){deleteDialog(nullptr,L"目标属于受保护位置或路径格式无效，未执行删除。",L"删除保护",MB_OK|MB_ICONWARNING);return 2;}
 if(needsElevation&&!elevatedToken()){deleteDialog(nullptr,L"未获得管理员权限，未执行删除。",L"未授权",MB_OK|MB_ICONWARNING);return 2;}
 std::vector<Entry> registered;installed(registered,HKEY_LOCAL_MACHINE,KEY_WOW64_64KEY);installed(registered,HKEY_LOCAL_MACHINE,KEY_WOW64_32KEY);installed(registered,HKEY_CURRENT_USER,KEY_WOW64_64KEY);installed(registered,HKEY_CURRENT_USER,KEY_WOW64_32KEY);
 for(auto&p:targets)for(auto&e:registered)if(!components&&e.system&&(inside(p.wstring(),e.path)||inside(e.path,p.wstring()))){deleteDialog(nullptr,L"目标与登记的系统组件重叠，未执行删除。",L"删除保护",MB_OK|MB_ICONWARNING);return 2;}
 if(unified){
  std::vector<std::wstring> commands;std::set<std::wstring> seen;
  auto addCommand=[&](const std::wstring& command){if(!command.empty()&&seen.insert(lower(command)).second)commands.push_back(command);};
  addCommand(uninstall);
  if(commands.empty())for(const auto& entry:registered)if(fs::path(entry.path).is_absolute()&&inside(entry.path,targets.front().wstring()))addCommand(entry.quietUninstallCommand.empty()?entry.uninstallCommand:entry.quietUninstallCommand);
  if(commands.empty()){
   std::error_code ec;if(fs::is_directory(targets.front(),ec)){auto found=resolveUninstaller(L"",targets.front());if(!found.command.empty())addCommand(found.command);}
  }
  for(const auto& command:commands){
   auto parsed=parseUninstaller(command);DWORD code=1;
   deleteSessionLog+=L"运行软件自带卸载程序：\r\n"+command+L"\r\n";
   if(parsed.executable.empty()||!waitForUninstaller(parsed.executable,parsed.arguments,code)){deleteSessionLog+=L"无法启动或监控卸载程序，已停止，未继续清理。\r\n";return 2;}
   if(code==3010||code==1641){deleteSessionLog+=L"卸载程序要求重启（代码 "+std::to_wstring(code)+L"）。正在提交已确认范围内的残留删除队列。\r\n";return deferRemainingCleanup(targets);}
   if(code!=0){deleteSessionLog+=L"卸载返回代码 "+std::to_wstring(code)+L"，可能取消或失败，未继续清理。\r\n";return 2;}
  }
  return runForceCleanup(targets,components);
 }
 if(needsElevation&&uninstall.empty()){
  for(const auto& entry:registered)if(lower(entry.name)==L"悟空"&&lower(fs::path(entry.path).lexically_normal().wstring())==lower(targets.front().lexically_normal().wstring())){
   auto command=entry.quietUninstallCommand.empty()?entry.uninstallCommand:entry.quietUninstallCommand;
   auto resolved=resolveUninstaller(command,targets.front());
   if(resolved.command.empty()){
    auto message=L"没有找到可运行的卸载入口。检查结果：\n\n"+resolved.diagnostic+L"缺失文件不能由本工具凭空恢复。若提示文件不存在，请使用该软件可信的原安装包修复/覆盖安装后再卸载；若提示拒绝访问，请保留错误码以便诊断。\n\n是否改为直接清理？会尝试处理服务、自启项及文件权限，但不保证解除驱动保护。";
    if(deleteDialog(nullptr,message.c_str(),L"入口不可用：是否直接清理",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)==IDYES)return runForceCleanup(targets,components);return 2;
   }
   auto prompt=L"悟空带有主动防御服务和内核驱动。\n\n"+resolved.diagnostic+(resolved.fallback?L"登记入口不可用，以上是安装目录中的候选卸载程序（未验证发布者身份）。请核对是否启动：\n":L"是否启动已登记的卸载程序：\n")+resolved.command+L"\n\n官方窗口可能需要确认；完成后请重启再扫描。";
   prompt+=L"\n\n是：运行上述卸载程序（优先登记的静默命令；没有静默命令时可能有官方弹窗）。\n否：直接清理，跳过卸载程序，必要时提示修复文件权限。\n取消：返回。";
   int choice=deleteDialog(nullptr,prompt.c_str(),L"悟空：选择清理方式",MB_YESNOCANCEL|MB_DEFBUTTON3|MB_ICONQUESTION);if(choice==IDCANCEL)return 2;if(choice==IDNO)return runForceCleanup(targets,components);
   if(!runUninstaller(resolved.command))return 2;
   deleteDialog(nullptr,L"官方卸载已返回并经确认。请保存工作后重启虚拟机，再重新扫描核实服务、驱动和残留。尚未确认完整移除，因此保留应用条目。",L"需要重启后核实",MB_OK|MB_ICONINFORMATION);return 4;
  }
  return runForceCleanup(targets,components);
 }
 if(!runUninstaller(uninstall))return 2;
 unsigned long long removed=0,failed=0;for(auto&p:targets){std::error_code ec;if(!fs::exists(p,ec)&&!ec)continue;if(validDeleteTarget(p))deleteConfirmedTarget(p,removed,failed);else failed++;}
 askRemoveEmptyParents(targets,removed,failed);
 auto result=L"删除 / 残留清理结束。\n已删除 "+std::to_wstring(removed)+L" 项；失败或跳过 "+std::to_wstring(failed)+L" 项。\n\n未列出的内容没有处理。被占用或受系统保护的文件可能仍无法删除。";bool complete=failed==0&&targetGone(targets.front().wstring());deleteDialog(nullptr,result.c_str(),complete?(needsElevation?L"已被强制删除，请确认":L"已删除，请确认"):L"部分内容未删除",MB_OK|(complete?MB_ICONINFORMATION:MB_ICONWARNING));return complete?0:1;
}
void launchElevatedDelete(const std::vector<fs::path>& targets,bool components=false,const std::wstring& uninstall=L"",bool elevate=true,bool unified=false){std::wstring args=unified?L"--delete-unified":elevate?L"--delete-elevated":L"--delete-current";if(components)args+=L"-components";if(!uninstall.empty())args+=L" --uninstaller "+quoteArgument(uninstall);for(auto&p:targets){if(!validDeleteTarget(p)){MessageBoxW(win,L"目标校验失败，未执行删除。",L"删除保护",MB_OK);return;}args+=L" "+quoteArgument(p.lexically_normal().wstring());}if(args.size()>24000){MessageBoxW(win,L"目标路径过多，请分批处理。",L"未执行删除",MB_OK);return;}
 wchar_t exe[32768];if(!GetModuleFileNameW(nullptr,exe,32768))return;SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.hwnd=win;info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;info.lpVerb=elevate?L"runas":L"open";info.lpFile=exe;info.lpParameters=args.c_str();info.nShow=SW_SHOWNORMAL;
 if(!ShellExecuteExW(&info)){DWORD error=GetLastError();auto message=error==ERROR_CANCELLED?L"已取消管理员授权，未执行删除。":L"无法启动管理员删除，系统错误："+std::to_wstring(error);MessageBoxW(win,message.c_str(),L"未执行删除",MB_OK);return;}
 if(!info.hProcess){MessageBoxW(win,L"管理员请求已发出，但无法监控进程。请查看管理员结果窗口。",L"管理员删除",MB_OK);return;}latestDeleteReport.clear();elevatedTargetPath=targets.front().wstring();elevatedDeleteProcess=info.hProcess;busy=true;for(int id:{13,14,21,22})EnableWindow(GetDlgItem(win,id),FALSE);SetWindowTextW(statusLabel,L"管理员删除处理中，请查看授权或结果窗口…");SetTimer(win,3,250,nullptr);
}
void pollElevatedDelete(){if(!elevatedDeleteProcess)return;if(WaitForSingleObject(elevatedDeleteProcess,0)!=WAIT_OBJECT_0)return;DWORD code=2;GetExitCodeProcess(elevatedDeleteProcess,&code);latestDeleteReport=readDeleteReport(elevatedDeleteProcess);CloseHandle(elevatedDeleteProcess);elevatedDeleteProcess=nullptr;KillTimer(win,3);busy=false;for(int id:{13,14,21,22})EnableWindow(GetDlgItem(win,id),TRUE);SetWindowTextW(statusLabel,code==4?L"官方卸载已返回，服务与驱动需重启后核实；请重新扫描。":code==3?L"已安排重启删除，尚未完成；重启后请重新扫描核实。":code==0?L"管理员删除已结束；重新扫描可更新列表。":L"管理员删除有失败项或已拒绝；请查看结果并重新扫描。");if(!latestDeleteReport.empty())SetWindowTextW(details,latestDeleteReport.c_str());if(code==0)beginRemovalAnimation(elevatedTargetPath);}
