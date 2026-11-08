#pragma once
struct ScopedFilePrivileges{
 HANDLE token=nullptr;std::vector<TOKEN_PRIVILEGES> previous;
 ScopedFilePrivileges(){
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return;
  for(auto name:{SE_BACKUP_NAME,SE_RESTORE_NAME,SE_TAKE_OWNERSHIP_NAME}){
   TOKEN_PRIVILEGES requested{},old{};requested.PrivilegeCount=1;
   if(!LookupPrivilegeValueW(nullptr,name,&requested.Privileges[0].Luid))continue;
   requested.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;DWORD length=sizeof(old);
   SetLastError(ERROR_SUCCESS);
   if(AdjustTokenPrivileges(token,FALSE,&requested,sizeof(old),&old,&length)&&GetLastError()==ERROR_SUCCESS)previous.push_back(old);
  }
 }
 ~ScopedFilePrivileges(){if(token){for(auto it=previous.rbegin();it!=previous.rend();++it)AdjustTokenPrivileges(token,FALSE,&*it,0,nullptr,nullptr);CloseHandle(token);}}
};
struct PermissionRepair{
 fs::path path;ScopedFilePrivileges privileges;HANDLE handle=INVALID_HANDLE_VALUE;
 PSECURITY_DESCRIPTOR original=nullptr;PSID owner=nullptr;PACL dacl=nullptr;
 SECURITY_DESCRIPTOR_CONTROL control=0;bool changed=false,deleted=false,attempted=false,ownershipChanged=false;
 std::function<void(DWORD)> restoreFailure;
 PermissionRepair(const fs::path& p,std::function<void(DWORD)> failed):path(p),restoreFailure(failed){}
 DWORD apply(){
  if(attempted)return ERROR_ACCESS_DENIED;attempted=true;
  if(!deletionPathAllowed(path))return ERROR_ACCESS_DENIED;
  // MAXIMUM_ALLOWED prevents SetSecurityInfo from propagating ACLs to children.
  // Each child is validated and repaired independently, never through a junction.
  handle=CreateFileW(path.c_str(),MAXIMUM_ALLOWED,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return GetLastError();
  BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(handle,&info))return GetLastError();
  if((info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||(!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&info.nNumberOfLinks>1))return ERROR_NOT_SUPPORTED;
  wchar_t finalName[32768];DWORD length=GetFinalPathNameByHandleW(handle,finalName,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
  if(!length||length>=32768)return ERROR_INVALID_NAME;
  std::wstring finalPath(finalName,length);if(finalPath.rfind(L"\\\\?\\",0)==0)finalPath.erase(0,4);
  if(lower(fs::path(finalPath).lexically_normal().wstring())!=lower(path.lexically_normal().wstring())||!deletionPathAllowed(finalPath))return ERROR_ACCESS_DENIED;
  DWORD error=GetSecurityInfo(handle,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&dacl,nullptr,&original);
  if(error)return error;DWORD revision=0;if(!GetSecurityDescriptorControl(original,&control,&revision))return GetLastError();
  if(!privileges.token)return ERROR_PRIVILEGE_NOT_HELD;
  DWORD size=0;GetTokenInformation(privileges.token,TokenUser,nullptr,0,&size);std::vector<BYTE> user(size);
  if(!size||!GetTokenInformation(privileges.token,TokenUser,user.data(),size,&size))return GetLastError();
  PSID sid=((TOKEN_USER*)user.data())->User.Sid;
  EXPLICIT_ACCESSW access{};access.grfAccessPermissions=FILE_ALL_ACCESS;access.grfAccessMode=SET_ACCESS;access.grfInheritance=NO_INHERITANCE;
  access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.TrusteeType=TRUSTEE_IS_USER;access.Trustee.ptstrName=(LPWSTR)sid;
  PACL replacement=nullptr;error=SetEntriesInAclW(1,&access,nullptr,&replacement);if(error)return error;
  ownershipChanged=!EqualSid(owner,sid);
  error=SetSecurityInfo(handle,SE_FILE_OBJECT,(ownershipChanged?OWNER_SECURITY_INFORMATION:0)|DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,ownershipChanged?sid:nullptr,nullptr,replacement,nullptr);
  LocalFree(replacement);
  // Conservatively restore even when Windows reports a partial failure.
  changed=true;return error;
 }
 ~PermissionRepair(){
  if(changed&&!deleted){DWORD error=SetSecurityInfo(handle,SE_FILE_OBJECT,(ownershipChanged?OWNER_SECURITY_INFORMATION:0)|DACL_SECURITY_INFORMATION|((control&SE_DACL_PROTECTED)?PROTECTED_DACL_SECURITY_INFORMATION:UNPROTECTED_DACL_SECURITY_INFORMATION),owner,nullptr,dacl,nullptr);if(error&&restoreFailure)restoreFailure(error);}
  if(original)LocalFree(original);if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
 }
};
