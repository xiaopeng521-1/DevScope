#include "main.cpp"
#include <cassert>
int main(){
 assert(sizeValue(L"1.0 GB")>sizeValue(L"900.0 MB"));
 assert(sizeValue(L"未计算")<0);
 assert(!removable(L"C:\\"));
 wchar_t windows[MAX_PATH];GetWindowsDirectoryW(windows,MAX_PATH);
 assert(!removable(windows));
 Entry e;e.path=std::wstring(windows)+L"\\System32\\cmd.exe";assert(systemEntry(e));
 e.path=L"F:\\sample\\jdk-17";e.kind=L"Java 环境";assert(family(e)==L"Java / JDK");
 auto fixture=fs::current_path()/L"delete_test_fixture";
 assert(!fs::exists(fixture));fs::create_directory(fixture);
 std::ofstream(fixture/L"sample.txt")<<"test only";
 assert(removable(fixture));unsigned long long deleted=0,failed=0;
 removeTree(fixture,deleted,failed);assert(deleted==2&&failed==0&&!fs::exists(fixture));
 return 0;
}
