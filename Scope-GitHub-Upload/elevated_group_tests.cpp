#include "main.cpp"
#include <cassert>
#include <iostream>
int main(){
 for(auto value:std::vector<std::wstring>{LR"(C:\Some Folder\应用.exe)",LR"(D:\trailing\)",L"quoted \"value\"",LR"(D:\a&b$(test).exe)"}){auto command=L"DevScope.exe "+quoteArgument(value);int argc=0;auto argv=CommandLineToArgvW(command.c_str(),&argc);assert(argv&&argc==2&&value==argv[1]);LocalFree(argv);}
 assert(!validDeleteTarget(L"C:\\"));assert(!validDeleteTarget(L"relative.exe"));wchar_t windows[MAX_PATH];GetWindowsDirectoryW(windows,MAX_PATH);assert(!validDeleteTarget(windows));assert(!validDeleteTarget(fs::current_path()));
 Entry java;java.name=L"JDK 17";java.path=L"F:\\test\\jdk17";java.kind=L"Java 环境";
 Entry java8=java;java8.name=L"JDK 8";
 Entry python;python.name=L"Python 3.12";python.path=L"F:\\test\\python";python.kind=L"已安装应用";
 Entry node;node.name=L"node.exe";node.path=L"F:\\test\\node.exe";node.kind=L"编程环境";
 assert(family(java)==family(java8));assert(family(python)==L"Python");assert(family(node)==L"Node.js");
 INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);win=CreateWindowW(L"STATIC",L"test",0,0,0,600,400,nullptr,nullptr,nullptr,nullptr);searchBox=CreateWindowW(L"EDIT",L"",WS_CHILD,0,0,100,20,win,nullptr,nullptr,nullptr);statusLabel=CreateWindowW(L"STATIC",L"",WS_CHILD,0,0,100,20,win,nullptr,nullptr,nullptr);list=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|LVS_ICON,0,0,500,300,win,nullptr,nullptr,nullptr);icons=ImageList_Create(32,32,ILC_COLOR32|ILC_MASK,8,8);ListView_SetImageList(list,icons,LVSIL_NORMAL);
 developerView=true;rows={java,python,node};refresh();assert(ListView_IsGroupViewEnabled(list));LVITEMW item{};item.mask=LVIF_GROUPID;item.iItem=1;ListView_GetItem(list,&item);int javaGroup=item.iGroupId;item.iItem=2;ListView_GetItem(list,&item);assert(item.iGroupId!=javaGroup);
 rows.push_back(java8);refresh(true);item.iItem=4;ListView_GetItem(list,&item);assert(item.iGroupId==javaGroup&&visible.size()==5);
 developerView=false;refresh();assert(!ListView_IsGroupViewEnabled(list));DestroyWindow(win);ImageList_Destroy(icons);
 std::cout<<"PASS: quoting, protected targets, environment categories, incremental native grouping\n";
}
