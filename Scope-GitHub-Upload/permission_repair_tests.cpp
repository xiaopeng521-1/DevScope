#include "main.cpp"
#include <cassert>
#include <iostream>
bool canWrite(const fs::path& path){HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;CloseHandle(h);return true;}
int main(){
 auto root=fs::current_path()/L"permission_repair_fixture";assert(!fs::exists(root));fs::create_directory(root);
 auto file=root/L"test.txt";std::ofstream(file)<<"fixture";
 HANDLE saved=CreateFileW(file.c_str(),READ_CONTROL|WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);assert(saved!=INVALID_HANDLE_VALUE);
 PSECURITY_DESCRIPTOR original=nullptr;PACL oldAcl=nullptr;PSID owner=nullptr;
 assert(GetSecurityInfo(saved,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&oldAcl,nullptr,&original)==ERROR_SUCCESS);
 EXPLICIT_ACCESSW access{};access.grfAccessPermissions=FILE_GENERIC_READ;access.grfAccessMode=SET_ACCESS;access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.ptstrName=(LPWSTR)owner;
 PACL restricted=nullptr;assert(SetEntriesInAclW(1,&access,nullptr,&restricted)==ERROR_SUCCESS);
 assert(SetSecurityInfo(saved,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,restricted,nullptr)==ERROR_SUCCESS);LocalFree(restricted);
 assert(!canWrite(file));
 {PermissionRepair repair(file,[](DWORD){assert(false);});DWORD result=repair.apply();std::cout<<"repair code="<<result<<"\n";assert(result==ERROR_SUCCESS);assert(canWrite(file));}
 assert(!canWrite(file)); // Failed deletion restores the original restricted ACL.
 {PermissionRepair repair(file,[](DWORD){assert(false);});assert(repair.apply()==ERROR_SUCCESS);assert(DeleteFileW(file.c_str()));repair.deleted=true;}
 CloseHandle(saved);LocalFree(original);assert(!fs::exists(file));
 auto a=root/L"a.txt",b=root/L"b.txt";std::ofstream(a)<<"shared";assert(CreateHardLinkW(b.c_str(),a.c_str(),nullptr));
 {PermissionRepair repair(a,[](DWORD){assert(false);});assert(repair.apply()==ERROR_NOT_SUPPORTED);}
 fs::remove(a);fs::remove(b);
 auto blocked=root/L"blocked";fs::create_directory(blocked);std::ofstream(blocked/L"child.txt")<<"fixture";
 HANDLE directory=CreateFileW(blocked.c_str(),MAXIMUM_ALLOWED,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);assert(directory!=INVALID_HANDLE_VALUE);
 PSECURITY_DESCRIPTOR directorySD=nullptr;PSID directoryOwner=nullptr;assert(GetSecurityInfo(directory,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&directoryOwner,nullptr,nullptr,nullptr,&directorySD)==ERROR_SUCCESS);
 access.grfAccessPermissions=READ_CONTROL|FILE_READ_ATTRIBUTES|SYNCHRONIZE;access.Trustee.ptstrName=(LPWSTR)directoryOwner;
 assert(SetEntriesInAclW(1,&access,nullptr,&restricted)==ERROR_SUCCESS);
 assert(SetSecurityInfo(directory,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,restricted,nullptr)==ERROR_SUCCESS);LocalFree(restricted);LocalFree(directorySD);
 ForceCleanupResult normal;forceRemoveTree(blocked,normal);assert(!normal.failures.empty());
 ForceCleanupResult retry;forceRemoveTree(blocked,retry,true);for(auto& fail:retry.failures)std::cerr<<"retry failure="<<fail.error<<"\n";std::cerr<<"removed="<<retry.removed<<"\n";assert(retry.failures.empty()&&retry.removed==2);
 CloseHandle(directory);assert(!fs::exists(blocked));fs::remove(root);
 {PermissionRepair repair(fs::current_path(),[](DWORD){assert(false);});assert(repair.apply()==ERROR_ACCESS_DENIED);}
 std::cout<<"PASS: restrictive ACL repaired, failure restores access, deletion succeeds, hardlinks and protected root rejected\n";
}
