#pragma once
bool serviceBelongsTo(const std::wstring& command,const fs::path& root){
 auto parsed=parseUninstaller(command);
 if(parsed.executable.empty()||!fs::path(parsed.executable).is_absolute())return false;
 for(const auto& part:fs::path(parsed.executable))if(part==L"..")return false;
 return inside(fs::path(parsed.executable).lexically_normal().wstring(),root.lexically_normal().wstring());
}
struct OwnedService{SC_HANDLE handle=nullptr;std::wstring name;DWORD start=0;bool changed=false;};
struct ServiceCleanup {
 SC_HANDLE manager=nullptr;std::vector<OwnedService> items;bool committed=false;
 ~ServiceCleanup(){for(auto& item:items){if(item.changed&&!committed&&!ChangeServiceConfigW(item.handle,SERVICE_NO_CHANGE,item.start,SERVICE_NO_CHANGE,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr))deleteDialog(nullptr,(L"无法恢复服务启动方式："+item.name+L"。请在服务管理器中检查。错误："+std::to_wstring(GetLastError())).c_str(),L"服务恢复失败",MB_OK|MB_ICONWARNING);if(item.handle)CloseServiceHandle(item.handle);}if(manager)CloseServiceHandle(manager);}
 bool error(const std::wstring& step,DWORD code){auto text=step+L"\nWindows 错误："+std::to_wstring(code)+L"\n未继续删除文件。若软件有自保护，请使用程序内的官方卸载入口，完成后重启复查。";deleteDialog(nullptr,text.c_str(),L"服务处理未完成",MB_OK|MB_ICONWARNING);return false;}
 bool prepare(const fs::path& root){
  manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ENUMERATE_SERVICE|SC_MANAGER_CONNECT);if(!manager)return error(L"无法检查服务",GetLastError());
  DWORD needed=0,count=0,resume=0;
  EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_STATE_ALL,nullptr,0,&needed,&count,&resume,nullptr);
  if(!needed)return error(L"未能取得服务清单",GetLastError());
  std::vector<BYTE> buffer(needed);resume=0;
  if(!EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_STATE_ALL,buffer.data(),(DWORD)buffer.size(),&needed,&count,&resume,nullptr))return error(L"读取服务清单失败",GetLastError());
  auto entries=(ENUM_SERVICE_STATUS_PROCESSW*)buffer.data();std::wstring summary;
  for(DWORD i=0;i<count;i++){
   SC_HANDLE query=OpenServiceW(manager,entries[i].lpServiceName,SERVICE_QUERY_CONFIG);if(!query)continue;
   DWORD size=0;QueryServiceConfigW(query,nullptr,0,&size);std::vector<BYTE> config(size);
   bool ok=size&&QueryServiceConfigW(query,(QUERY_SERVICE_CONFIGW*)config.data(),size,&size);CloseServiceHandle(query);if(!ok)continue;
   auto c=(QUERY_SERVICE_CONFIGW*)config.data();if(!serviceBelongsTo(c->lpBinaryPathName,root))continue;
   SC_HANDLE service=OpenServiceW(manager,entries[i].lpServiceName,SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_STOP|SERVICE_CHANGE_CONFIG|DELETE);
   if(!service)return error(L"无权处理服务："+std::wstring(entries[i].lpServiceName),GetLastError());
   items.push_back({service,entries[i].lpServiceName,c->dwStartType,false});summary+=std::wstring(entries[i].lpServiceName)+L"\n"+c->lpBinaryPathName+L"\n\n";
  }
  if(items.empty())return true;
  auto prompt=L"以下服务的程序路径位于选中安装目录内：\n\n"+summary+L"确认后禁用其自启并停止服务；文件全部清理成功后移除服务登记。若未完成，会尝试恢复原启动方式（不会自动重新启动服务）。是否继续？";
  if(deleteDialog(nullptr,prompt.c_str(),L"处理软件自启服务",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return false;
  for(auto& item:items){
   // Recheck configuration after the confirmation dialog.
   DWORD size=0;QueryServiceConfigW(item.handle,nullptr,0,&size);std::vector<BYTE> config(size);
   if(!size||!QueryServiceConfigW(item.handle,(QUERY_SERVICE_CONFIGW*)config.data(),size,&size))return error(L"服务配置复核失败",GetLastError());
   auto c=(QUERY_SERVICE_CONFIGW*)config.data();if(!serviceBelongsTo(c->lpBinaryPathName,root))return error(L"服务路径已改变",ERROR_INVALID_DATA);
   item.start=c->dwStartType;
   if(!ChangeServiceConfigW(item.handle,SERVICE_NO_CHANGE,SERVICE_DISABLED,SERVICE_NO_CHANGE,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr))return error(L"无法禁用自启："+item.name,GetLastError());item.changed=true;
   SERVICE_STATUS_PROCESS status{};DWORD bytes=0;
   if(!QueryServiceStatusEx(item.handle,SC_STATUS_PROCESS_INFO,(BYTE*)&status,sizeof(status),&bytes))return error(L"无法读取服务状态",GetLastError());
   if(status.dwCurrentState==SERVICE_STOPPED)continue;
   SERVICE_STATUS ignored{};if(status.dwCurrentState!=SERVICE_STOP_PENDING&&!ControlService(item.handle,SERVICE_CONTROL_STOP,&ignored))return error(L"无法停止服务："+item.name,GetLastError());
   auto deadline=GetTickCount64()+15000;
   do{Sleep(200);if(!QueryServiceStatusEx(item.handle,SC_STATUS_PROCESS_INFO,(BYTE*)&status,sizeof(status),&bytes))return error(L"无法核实停止结果",GetLastError());}while(status.dwCurrentState!=SERVICE_STOPPED&&GetTickCount64()<deadline);
   if(status.dwCurrentState!=SERVICE_STOPPED)return error(L"等待服务停止超时："+item.name,ERROR_TIMEOUT);
  }return true;
 }
 bool finish(){
  bool ok=true;
  for(auto& item:items){
   if(!DeleteService(item.handle)){error(L"文件已清理，但服务登记移除失败："+item.name,GetLastError());ok=false;continue;}
   item.changed=false;CloseServiceHandle(item.handle);item.handle=nullptr;
   SC_HANDLE check=OpenServiceW(manager,item.name.c_str(),SERVICE_QUERY_STATUS);
   DWORD status=check?ERROR_SERVICE_MARKED_FOR_DELETE:GetLastError();if(check)CloseServiceHandle(check);
   if(status!=ERROR_SERVICE_DOES_NOT_EXIST){deleteDialog(nullptr,(L"服务已标记删除，但仍待系统释放或重启："+item.name).c_str(),L"服务移除尚未完成",MB_OK|MB_ICONWARNING);ok=false;}
  }
  committed=ok;return ok;
 }
};
