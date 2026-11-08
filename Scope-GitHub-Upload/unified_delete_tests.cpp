#include "main.cpp"
#include <cassert>
#include <iostream>
int main(){
 singleDeleteFlow=true;
 assert(deleteDialog(nullptr,L"authorized action",L"test",MB_YESNO)==IDYES);
 assert(deleteDialog(nullptr,L"result",L"test",MB_OK)==IDOK);
 auto root=fs::current_path()/L"unified_delete_fixture";assert(!fs::exists(root));fs::create_directory(root);std::ofstream(root/L"child.txt")<<"fixture";
 assert(runForceCleanup({root},true)==0);assert(!fs::exists(root));
 assert(deleteSessionLog.find(L"已删除")!=std::wstring::npos);
 auto expected=deleteSessionLog;
 {DeleteReportWriter writer;}
 auto report=readDeleteReport(GetCurrentProcess());assert(report==expected);
 assert(readDeleteReport(GetCurrentProcess()).empty());
 // Scheduling is tested with an injected operation; do not touch Windows reboot state.
 fs::create_directory(root);auto file=root/L"held.txt";std::ofstream(file)<<"fixture";
 std::vector<CleanupFailure> plan;size_t unreadable=0;collectRebootPlan(root,plan,unreadable);assert(unreadable==0&&plan.size()==2&&plan[0].path==file&&plan[1].path==root);
 std::vector<fs::path> order;
 assert(scheduleCleanup({{file,ERROR_SHARING_VIOLATION},{root,ERROR_DIR_NOT_EMPTY}},[&](const fs::path& p){order.push_back(p);return true;})==2);
 assert(order[0]==file&&order[1]==root);fs::remove(file);fs::remove(root);
 singleDeleteFlow=false;
 std::cout<<"PASS: unified cleanup without internal dialogs, report roundtrip, missing report fallback, child-before-parent reboot plan (mock only)\n";
}
