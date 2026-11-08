#pragma once
struct ResolvedUninstaller{std::wstring command,diagnostic;bool fallback=false;};
std::wstring checkUninstallerFile(const fs::path& executable,const fs::path& root){
 if(!executable.is_absolute()||!inside(executable.lexically_normal().wstring(),root.lexically_normal().wstring()))return L"路径不在所选安装目录内";
 for(const auto& part:executable)if(part==L"..")return L"路径包含上级目录跳转";
 DWORD attributes=GetFileAttributesW(executable.c_str());
 if(attributes==INVALID_FILE_ATTRIBUTES){DWORD error=GetLastError();return (error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?L"文件不存在，可能已被移除":error==ERROR_ACCESS_DENIED?L"读取被拒绝，无法判断文件是否存在":L"无法读取文件属性")+std::wstring(L"（Windows 错误 ")+std::to_wstring(error)+L"）";}
 if(attributes&FILE_ATTRIBUTE_DIRECTORY)return L"这是目录，不是卸载程序";
 if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)return L"这是链接，未作为卸载程序运行";
 HANDLE handle=CreateFileW(executable.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(handle==INVALID_HANDLE_VALUE)return L"无法打开文件核实路径（Windows 错误 "+std::to_wstring(GetLastError())+L"）";
 wchar_t finalPath[32768];DWORD length=GetFinalPathNameByHandleW(handle,finalPath,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);DWORD error=GetLastError();CloseHandle(handle);
 if(!length||length>=32768)return L"无法解析实际路径（Windows 错误 "+std::to_wstring(error)+L"）";
 std::wstring actual(finalPath,length);if(actual.rfind(L"\\\\?\\",0)==0)actual.erase(0,4);
 if(!inside(actual,root.lexically_normal().wstring()))return L"实际文件位置在安装目录之外";
 return L"";
}
ResolvedUninstaller resolveUninstaller(const std::wstring& registered,const fs::path& root,
 const std::function<std::wstring(const fs::path&,const fs::path&)>& check=checkUninstallerFile){
 ResolvedUninstaller result;std::set<std::wstring> tried;
 wchar_t expanded[32768];DWORD size=ExpandEnvironmentStringsW(registered.c_str(),expanded,32768);
 auto parsed=parseUninstaller(size&&size<=32768?expanded:registered);
 auto attempt=[&](const std::wstring& exe,const std::wstring& args,bool fallback){
  if(!tried.insert(lower(exe)).second)return false;
  auto reason=check(exe,root);
  result.diagnostic+=exe+L"\n"+(reason.empty()?L"文件存在，路径核实通过":reason)+L"\n\n";
  if(!reason.empty())return false;
  result.command=quoteArgument(exe)+args;result.fallback=fallback;return true;
 };
 if(!parsed.executable.empty()){if(attempt(parsed.executable,parsed.arguments,false))return result;}
 else result.diagnostic=L"注册表卸载命令为空或无法解析：\n"+registered+L"\n\n";
 for(auto name:{L"uninst.exe",L"uninstall.exe"})if(attempt((root/name).wstring(),L"",true))break;
 return result;
}
