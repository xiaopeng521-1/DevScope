#pragma once
std::vector<Entry> registeredInside(const fs::path& root){
 std::vector<Entry> all;installed(all,HKEY_LOCAL_MACHINE,KEY_WOW64_64KEY);installed(all,HKEY_LOCAL_MACHINE,KEY_WOW64_32KEY);installed(all,HKEY_CURRENT_USER,KEY_WOW64_64KEY);installed(all,HKEY_CURRENT_USER,KEY_WOW64_32KEY);
 std::set<std::wstring> seen;all.erase(std::remove_if(all.begin(),all.end(),[&](const Entry& e){return !fs::path(e.path).is_absolute()||!inside(e.path,root.wstring())||!seen.insert(lower(e.name+L"|"+e.path)).second;}),all.end());return all;
}
void deleteWholeDirectory(const fs::path& path){
 if(busy){MessageBoxW(win,L"请先停止扫描或等待当前操作完成。",L"强制删除",MB_OK);return;}
 if(!validDeleteTarget(path)){MessageBoxW(win,L"不能删除此系统/共享根目录，或路径无法验证。请选择具体文件或软件目录。",L"目标不可删除",MB_OK|MB_ICONWARNING);return;}
 auto apps=registeredInside(path);std::vector<fs::path> targets{path};std::set<std::wstring> seen{lower(path.wstring())};
 std::wstring prompt=L"将永久删除（不进入回收站）：\n"+path.wstring()+L"\n\n目录内所有文件、子目录及未识别内容都会删除。\n已登记在此范围内的应用：\n";
 for(auto& app:apps){prompt+=app.name+L" — "+app.path+L"\n";for(auto& p:relatedFiles(app))if(!inside(p.wstring(),path.wstring())&&seen.insert(lower(p.wstring())).second)targets.push_back(p);}
 if(apps.empty())prompt+=L"未找到登记项，这不代表目录内没有应用。\n";
 prompt+=L"\n额外识别的关联目录/快捷方式：\n";for(size_t i=1;i<targets.size();i++)prompt+=targets[i].wstring()+L"\n";
 prompt+=unifiedDeleteNotice();
 if(MessageBoxW(win,prompt.c_str(),L"整目录 / 文件强制删除：核对全部范围",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;
 launchElevatedDelete(targets,true,L"",true,true);
}
void chooseForceDelete(bool folder=false){
 if(busy){MessageBoxW(win,L"请先停止扫描或等待当前操作完成。",L"强制删除",MB_OK);return;}
 int mode=folder?IDNO:IDYES;
 IFileOpenDialog* dialog=nullptr;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return;
 DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_FILEMUSTEXIST|FOS_NOCHANGEDIR|(mode==IDNO?FOS_PICKFOLDERS:0));
 dialog->SetTitle(L"选择要永久删除的文件或文件夹");fs::path selected;
 if(SUCCEEDED(dialog->Show(win))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR value=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))){selected=value;CoTaskMemFree(value);}item->Release();}}
 dialog->Release();if(!selected.empty())deleteWholeDirectory(selected);
}
