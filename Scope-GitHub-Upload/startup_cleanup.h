#pragma once
struct StartupValue{HKEY key;std::wstring name,location;DWORD type;std::vector<BYTE> data;bool removed=false;};
struct StartupCleanup{
 std::vector<StartupValue> items;bool committed=false;
 ~StartupCleanup(){for(auto& item:items){if(item.removed&&!committed){DWORD type=0,size=0;auto exists=RegQueryValueExW(item.key,item.name.c_str(),nullptr,&type,nullptr,&size);if(exists!=ERROR_FILE_NOT_FOUND||RegSetValueExW(item.key,item.name.c_str(),0,item.type,item.data.data(),(DWORD)item.data.size())!=ERROR_SUCCESS)deleteDialog(nullptr,(L"启动项恢复未完成，请检查：\n"+item.location+L"\\"+item.name).c_str(),L"请检查启动项",MB_OK|MB_ICONWARNING);}RegCloseKey(item.key);}}
 bool prepare(const fs::path& root){
  std::wstring summary;
  for(auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE})for(REGSAM view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY})for(auto leaf:{L"Run",L"RunOnce"}){
   auto keyPath=std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\")+leaf;
   HKEY key=nullptr;auto opened=RegOpenKeyExW(hive,keyPath.c_str(),0,KEY_QUERY_VALUE|view,&key);if(opened==ERROR_FILE_NOT_FOUND)continue;
   if(opened!=ERROR_SUCCESS){deleteDialog(nullptr,L"无法读取启动项清单，未继续删除。",L"启动项检查失败",MB_OK);return false;}
   for(DWORD index=0;;index++){
    wchar_t name[16384];DWORD nameSize=16384,type=0,size=0;
    auto status=RegEnumValueW(key,index,name,&nameSize,nullptr,&type,nullptr,&size);if(status==ERROR_NO_MORE_ITEMS)break;
    if(status!=ERROR_SUCCESS){RegCloseKey(key);return false;}if(type!=REG_SZ&&type!=REG_EXPAND_SZ)continue;
    if(size>65536||size%sizeof(wchar_t))continue;
    std::vector<BYTE> data(size+sizeof(wchar_t),0);DWORD readSize=size;
    if(RegQueryValueExW(key,name,nullptr,&type,data.data(),&readSize)!=ERROR_SUCCESS)continue;
    std::wstring command((wchar_t*)data.data());wchar_t expanded[32768];
    if(type==REG_EXPAND_SZ){DWORD n=ExpandEnvironmentStringsW(command.c_str(),expanded,32768);if(!n||n>32768)continue;command=expanded;}
    if(!serviceBelongsTo(command,root))continue;
    HKEY writable=nullptr;if(RegOpenKeyExW(hive,keyPath.c_str(),0,KEY_QUERY_VALUE|KEY_SET_VALUE|view,&writable)!=ERROR_SUCCESS){RegCloseKey(key);deleteDialog(nullptr,L"找到关联启动项，但无权修改，未继续删除。",L"启动项检查失败",MB_OK);return false;}
    data.resize(readSize);auto location=std::wstring(hive==HKEY_CURRENT_USER?L"当前用户\\":L"计算机\\")+leaf+(view==KEY_WOW64_64KEY?L" (64)":L" (32)");
    items.push_back({writable,name,location,type,data,false});summary+=location+L"\\"+name+L"\n"+command+L"\n\n";
   }RegCloseKey(key);
  }
  if(items.empty())return true;
  if(deleteDialog(nullptr,(L"发现以下启动项指向删除范围：\n\n"+summary+L"是否先移除这些自启入口？若清理失败会尝试恢复。这里只处理精确路径匹配的 Run/RunOnce，不撤销未知系统设置。若有管理员身份切换，“当前用户”指管理员助手账户。").c_str(),L"卸载前关闭自启",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return false;
  for(auto& item:items){
   DWORD type=0,size=0;auto state=RegQueryValueExW(item.key,item.name.c_str(),nullptr,&type,nullptr,&size);
   // WOW64 can expose the same user key through both views.
   if(state==ERROR_FILE_NOT_FOUND)continue;
   std::vector<BYTE> current(size);
   if(state!=ERROR_SUCCESS||type!=item.type||size!=item.data.size()||RegQueryValueExW(item.key,item.name.c_str(),nullptr,&type,current.data(),&size)!=ERROR_SUCCESS||current!=item.data){deleteDialog(nullptr,L"启动项在确认期间发生变化，已停止清理。",L"需要重新核实",MB_OK);return false;}
   auto status=RegDeleteValueW(item.key,item.name.c_str());if(status!=ERROR_SUCCESS){deleteDialog(nullptr,(L"无法移除启动项："+item.name+L"，错误 "+std::to_wstring(status)).c_str(),L"启动项处理失败",MB_OK);return false;}item.removed=true;
  }return true;
 }
};
