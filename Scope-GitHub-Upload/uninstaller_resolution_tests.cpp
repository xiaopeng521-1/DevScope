#include "main.cpp"
#include <cassert>
#include <iostream>
int main(){
 fs::path root=LR"(C:\Program Files (x86)\Wukong)";
 auto missing=[](const fs::path&,const fs::path&){return std::wstring(L"文件不存在（2）");};
 auto absent=resolveUninstaller((root/L"uninst.exe").wstring(),root,missing);
 assert(absent.command.empty()&&absent.diagnostic.find(L"文件不存在")!=std::wstring::npos);
 auto fallback=resolveUninstaller((root/L"old.exe").wstring(),root,[](const fs::path& p,const fs::path&){return p.filename()==L"uninstall.exe"?L"":std::wstring(L"拒绝访问（5）");});
 assert(fallback.fallback&&parseUninstaller(fallback.command).executable==(root/L"uninstall.exe").wstring());
 assert(fallback.diagnostic.find(L"拒绝访问")!=std::wstring::npos);
 auto original=resolveUninstaller(quoteArgument((root/L"uninst.exe").wstring())+L" /remove",root,[](const fs::path&,const fs::path&){return L"";});
 assert(!original.fallback&&parseUninstaller(original.command).arguments==L" /remove");
 assert(!checkUninstallerFile(LR"(C:\Elsewhere\uninst.exe)",root).empty());
 assert(!checkUninstallerFile(root/L".."/L"Other"/L"uninst.exe",root).empty());
 auto fixture=fs::current_path()/L"not_present_uninstaller_test.exe";assert(!fs::exists(fixture));
 assert(checkUninstallerFile(fixture,fs::current_path()).find(L"文件不存在")!=std::wstring::npos);
 std::cout<<"PASS: missing file, denied candidate, fallback, argument preservation and outside-root rejection; no uninstall executed\n";
}
