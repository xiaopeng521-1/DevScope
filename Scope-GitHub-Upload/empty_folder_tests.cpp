#include "main.cpp"
#include <cassert>
#include <iostream>
void answerDialog(int answer,const fs::path& folder,bool addFile){for(int i=0;i<500;i++){HWND d=FindWindowW(L"#32770",L"是否删除空文件夹");DWORD pid=0;if(d)GetWindowThreadProcessId(d,&pid);if(d&&pid==GetCurrentProcessId()){if(addFile)std::ofstream(folder/L"new-file.txt")<<"keep";SendMessageW(GetDlgItem(d,answer),BM_CLICK,0,0);return;}Sleep(10);}assert(false);}
int main(){auto folder=fs::current_path()/L"empty_folder_test_fixture";assert(!fs::exists(folder));fs::create_directory(folder);std::vector<fs::path> targets{folder/L"deleted-app.exe"};unsigned long long removed=0,failed=0;
 std::thread no(answerDialog,IDNO,folder,false);askRemoveEmptyParents(targets,removed,failed);no.join();assert(fs::exists(folder)&&removed==0);
 std::thread changed(answerDialog,IDYES,folder,true);askRemoveEmptyParents(targets,removed,failed);changed.join();assert(fs::exists(folder/L"new-file.txt")&&removed==0&&failed==1);fs::remove(folder/L"new-file.txt");
 std::thread yes(answerDialog,IDYES,folder,false);askRemoveEmptyParents(targets,removed,failed);yes.join();assert(!fs::exists(folder)&&removed==1);std::cout<<"PASS: empty-folder cancel, confirm, concurrent new-file protection\n";}
