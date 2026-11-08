#pragma once
// A unified flow is enabled only in the elevated helper after the GUI's scope confirmation.
bool singleDeleteFlow=false;
std::wstring deleteSessionLog,latestDeleteReport;
int deleteDialog(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){
 if(!singleDeleteFlow)return MessageBoxW(owner,text,title,flags);
 if(deleteSessionLog.size()<400000)deleteSessionLog+=std::wstring(title?title:L"")+L"\r\n"+(text?text:L"")+L"\r\n\r\n";
 // All these operations are described in the one scope confirmation in the GUI.
 return (flags&MB_TYPEMASK)==MB_YESNO||(flags&MB_TYPEMASK)==MB_YESNOCANCEL?IDYES:IDOK;
}
std::wstring deleteReportPath(HANDLE process){
 wchar_t directory[32768];DWORD count=GetTempPathW(32768,directory);if(!count||count>=32768)return L"";
 FILETIME created{},exited{},kernel{},user{};if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return L"";
 return std::wstring(directory)+L"DevScope-delete-"+std::to_wstring(GetProcessId(process))+L"-"+std::to_wstring(created.dwHighDateTime)+L"-"+std::to_wstring(created.dwLowDateTime)+L".report";
}
struct DeleteReportWriter{
 ~DeleteReportWriter(){if(!singleDeleteFlow)return;auto path=deleteReportPath(GetCurrentProcess());if(path.empty())return;
  HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return;
  DWORD written=0;WriteFile(file,deleteSessionLog.data(),(DWORD)(deleteSessionLog.size()*sizeof(wchar_t)),&written,nullptr);CloseHandle(file);
 }
};
std::wstring readDeleteReport(HANDLE process){
 auto path=deleteReportPath(process);if(path.empty())return L"";
 HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(file==INVALID_HANDLE_VALUE)return L"";
 BY_HANDLE_FILE_INFORMATION info{};std::wstring result;
 if(GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&info.nFileSizeHigh==0&&info.nFileSizeLow<=1024*1024&&info.nFileSizeLow%sizeof(wchar_t)==0){
  result.resize(info.nFileSizeLow/sizeof(wchar_t));DWORD read=0;if(!ReadFile(file,result.data(),info.nFileSizeLow,&read,nullptr)||read!=info.nFileSizeLow)result.clear();
 }
 CloseHandle(file);DeleteFileW(path.c_str());return result;
}
std::wstring unifiedDeleteNotice(){return L"\n将优先运行自带卸载器，并清理上述文件、关联自启及空父目录；必要时结束进程、修复权限。请先保存工作，删除不进入回收站。\n\n部分残留需重启后才会消失；未能安排删除的项目会提示。不会自动重启。重启前请勿向这些路径保存新文件。\n\n是否开始清理？";}
