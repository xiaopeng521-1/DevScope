#include "main.cpp"
#include <cassert>
int main(){INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);
 win=CreateWindowW(L"STATIC",L"test",0,0,0,600,400,nullptr,nullptr,nullptr,nullptr);
 searchBox=CreateWindowW(L"EDIT",L"",WS_CHILD,0,0,100,20,win,nullptr,nullptr,nullptr);
 statusLabel=CreateWindowW(L"STATIC",L"",WS_CHILD,0,0,100,20,win,nullptr,nullptr,nullptr);
 list=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|LVS_ICON,0,0,500,300,win,nullptr,nullptr,nullptr);
 icons=ImageList_Create(32,32,ILC_COLOR32|ILC_MASK,8,8);ListView_SetImageList(list,icons,LVSIL_NORMAL);
 Entry app;app.name=L"App";app.path=L"F:\\Sample\\app.exe";Entry sys;sys.name=L"System";sys.system=true;sys.path=L"C:\\Windows\\sample.exe";rows={app,sys};refresh();assert(visible.size()==2&&visible[0]==-1&&visible[1]==0);
 rows.push_back(sys);refresh(true);assert(visible.size()==2);
 systemView=true;refresh();assert(visible.size()==3&&visible[0]==-2&&visible[1]==1&&visible[2]==2);
 systemView=false;refresh();assert(visible.size()==2);
 DestroyWindow(win);ImageList_Destroy(icons);return 0;}
