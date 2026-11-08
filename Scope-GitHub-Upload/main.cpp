#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <aclapi.h>
#include <shlobj.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <optional>
#include <deque>
#include <functional>
#include "game2048.h"
#include "mini_games.h"
namespace fs=std::filesystem;
#include "delete_session.h"
struct Entry {std::wstring name,version,kind,path,size=L"未计算",memory=L"未运行 / 无权限",permission,cache,iconPath; bool system=false; mutable std::wstring familyCache,searchCache; std::wstring uninstallCommand,quietUninstallCommand,locationSource;};
HWND progressBar=nullptr,gameWindow=nullptr;bool scanning=false,gameOffered=false;ULONGLONG scanStarted=0;std::unordered_map<int,int> loadedIcons;HANDLE elevatedDeleteProcess=nullptr;
HWND win,list,searchBox,statusLabel,scanBtn,sizeBtn,details; HIMAGELIST icons; HFONT font,titleFont; std::vector<Entry> rows; std::vector<int> visible;
void clearSummaries();
void invalidateOwnership();
std::wstring componentSummary(const Entry&,size_t limit=500);
void clearDetailCache();
std::vector<Entry> inspectedDirectories;
std::vector<Entry> pendingRows, finalRows; size_t published=0, rendered=0; bool systemView=false,developerView=false,unknownView=false;
std::mutex lockRows; std::atomic<bool> busy{false},closing{false},cancelScan{false}; std::thread worker;
namespace Theme { constexpr COLORREF background=RGB(245,247,250),panel=RGB(255,255,255),text=RGB(27,43,61),muted=RGB(82,103,124),accent=RGB(20,106,128),danger=RGB(170,43,61); }
constexpr UINT DONE=WM_APP+1; HBRUSH bg=CreateSolidBrush(RGB(245,247,250));
std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),::towlower);return s;}
std::wstring bytes(unsigned long long n){wchar_t b[80];double d=(double)n;const wchar_t* u[]={L"B",L"KB",L"MB",L"GB",L"TB"};int i=0;while(d>=1024&&i<4){d/=1024;i++;}swprintf(b,80,L"%.1f %ls",d,u[i]);return b;}
std::wstring regstr(HKEY k,const wchar_t* name){DWORD type=0,n=0;if(RegQueryValueExW(k,name,0,&type,0,&n)!=ERROR_SUCCESS||(type!=REG_SZ&&type!=REG_EXPAND_SZ)||n>65536)return L"";std::vector<wchar_t>b(n/2+2);if(RegQueryValueExW(k,name,0,0,(BYTE*)b.data(),&n)!=ERROR_SUCCESS)return L"";std::wstring s=b.data();if(type==REG_EXPAND_SZ){wchar_t out[32768];if(ExpandEnvironmentStringsW(s.c_str(),out,32768)<32768)s=out;}return s;}
std::wstring clean(std::wstring p){if(p.size()>1&&p.front()==L'"'){auto e=p.find(L'"',1);p=p.substr(1,e-1);}return p;}
std::wstring mainExecutableName(const std::wstring& name){auto n=lower(name);if(n.find(L"photoshop")!=std::wstring::npos)return L"Photoshop.exe";if(n.find(L"premiere pro")!=std::wstring::npos)return L"Adobe Premiere Pro.exe";if(n.find(L"illustrator")!=std::wstring::npos)return L"Illustrator.exe";if(n.find(L"after effects")!=std::wstring::npos)return L"AfterFX.exe";return L"";}
#include "installation_location.h"
void installed(std::vector<Entry>& out,HKEY root,REGSAM view){HKEY key; if(RegOpenKeyExW(root,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,KEY_READ|view,&key)!=ERROR_SUCCESS)return; wchar_t name[512];for(DWORD i=0;!closing;i++){DWORD len=512;auto r=RegEnumKeyExW(key,i,name,&len,0,0,0,0);if(r==ERROR_NO_MORE_ITEMS)break;if(r!=ERROR_SUCCESS)continue;HKEY sub;if(RegOpenKeyExW(key,name,0,KEY_READ|view,&sub)!=ERROR_SUCCESS)continue;Entry e;e.name=regstr(sub,L"DisplayName");e.version=regstr(sub,L"DisplayVersion");e.path=clean(regstr(sub,L"InstallLocation"));e.kind=L"已安装应用";e.iconPath=regstr(sub,L"DisplayIcon");e.uninstallCommand=regstr(sub,L"UninstallString");e.quietUninstallCommand=regstr(sub,L"QuietUninstallString");DWORD systemFlag=0,flagSize=sizeof(systemFlag);RegQueryValueExW(sub,L"SystemComponent",0,nullptr,(BYTE*)&systemFlag,&flagSize);e.system=systemFlag==1;DWORD sz=0,n=4,t=0;if(RegQueryValueExW(sub,L"EstimatedSize",0,&t,(BYTE*)&sz,&n)==ERROR_SUCCESS&&t==REG_DWORD)e.size=bytes((unsigned long long)sz*1024)+L"（注册表估算）";auto primary=mainExecutableName(e.name);if(!primary.empty()&&!e.path.empty()){auto candidate=fs::path(e.path)/primary;std::error_code ec;if(fs::is_regular_file(candidate,ec))e.iconPath=candidate.wstring();}resolveRegisteredPath(e);RegCloseKey(sub);if(!e.name.empty()){out.push_back(e);}}RegCloseKey(key);}
bool inside(const std::wstring& file,const std::wstring& dir){auto a=lower(file),b=lower(dir);while(!b.empty()&&(b.back()==L'\\'||b.back()==L'/'))b.pop_back();return !b.empty()&&(a==b||(a.size()>b.size()&&a.compare(0,b.size(),b)==0&&(a[b.size()]==L'\\'||a[b.size()]==L'/')));}

bool systemEntry(const Entry& e){static const std::wstring windows=[](){wchar_t p[MAX_PATH]{};GetWindowsDirectoryW(p,MAX_PATH);return std::wstring(p);}();auto p=lower(e.path);return e.system||inside(e.path,windows)||p.find(LR"(\windowsapps\)")!=std::wstring::npos||p.find(LR"(\windows defender)")!=std::wstring::npos;}

const std::vector<std::wstring> environmentFamilies={L"快捷入口",L"Java / JDK",L"Python",L"Node.js",L"C / C++",L".NET",L"Go",L"Rust",L"PHP",L"Ruby",L"SDK / CDK",L"构建工具",L"Git",L"容器工具",L"其他开发环境"};
std::wstring computeFamily(const Entry& e){if(systemEntry(e))return L"系统组件";auto n=lower(e.name);auto filename=lower(fs::path(e.path).filename().wstring());auto path=lower(e.path);
 if(e.kind==L"Java 环境"||n.find(L"jdk")!=std::wstring::npos||n.find(L"jre")!=std::wstring::npos||n.find(L"java ")==0||n==L"java.exe"||n==L"javac.exe")return L"Java / JDK";
 if(n.find(L"python")!=std::wstring::npos||n.find(L"anaconda")!=std::wstring::npos||n.find(L"miniconda")!=std::wstring::npos)return L"Python";
 if(n.find(L"node.js")!=std::wstring::npos||n==L"node.exe"||n==L"npm.cmd"||n==L"npx.cmd")return L"Node.js";
 if(n==L"gcc.exe"||n==L"g++.exe"||n==L"clang.exe"||n==L"clang++.exe"||n==L"cl.exe"||n.find(L"mingw")!=std::wstring::npos||n.find(L"llvm")!=std::wstring::npos)return L"C / C++";
 if(n==L"dotnet.exe"||n.find(L".net")!=std::wstring::npos)return L".NET";
 if(n==L"go.exe"||n.find(L"go programming")==0)return L"Go";
 if(n==L"rustc.exe"||n==L"cargo.exe"||n.find(L"rustup")!=std::wstring::npos)return L"Rust";
 if(n==L"php.exe")return L"PHP";if(n==L"ruby.exe")return L"Ruby";
 if(n.find(L"sdk")!=std::wstring::npos||n.find(L"cdk")!=std::wstring::npos||path.find(L"\\sdk\\")!=std::wstring::npos)return L"SDK / CDK";
 if(n==L"cmake.exe"||n==L"ninja.exe"||n==L"mvn.cmd"||n==L"gradle.bat")return L"构建工具";
 if(n==L"git.exe"||n.find(L"git version")==0)return L"Git";
 if(n==L"docker.exe"||n.find(L"docker desktop")==0)return L"容器工具";
 if(e.kind==L"编程环境")return L"其他开发环境";return L"应用与可执行文件";
}
std::wstring family(const Entry& e){if(e.familyCache.empty())e.familyCache=computeFamily(e);return e.familyCache;}
const std::wstring& searchable(const Entry&e){if(e.searchCache.empty())e.searchCache=lower(e.name+L" "+family(e)+L" "+e.kind+L" "+e.version+L" "+e.path+L" "+e.cache);return e.searchCache;}
int environmentGroup(const Entry& e){auto f=family(e);auto it=std::find(environmentFamilies.begin(),environmentFamilies.end(),f);return it==environmentFamilies.end()?14:(int)(it-environmentFamilies.begin());}
HICON appIcon(const Entry&e){
 HICON large=nullptr;std::wstring source=e.iconPath;int index=0;
 auto comma=source.rfind(L',');if(comma!=std::wstring::npos){try{index=std::stoi(source.substr(comma+1));source.resize(comma);}catch(...){}}
 source=clean(source);if(lower(fs::path(source).extension().wstring())==L".ico")large=(HICON)LoadImageW(nullptr,source.c_str(),IMAGE_ICON,32,32,LR_LOADFROMFILE);else if(!source.empty())ExtractIconExW(source.c_str(),index,&large,nullptr,1);if(large)return large;
 std::error_code ec;fs::path p(e.path);if(fs::is_directory(p,ec)){if(fs::exists(p/L"bin"/L"java.exe",ec))p/=L"bin/java.exe";else{fs::directory_iterator it(p,ec),end;for(;it!=end;it.increment(ec)){if(ec)break;if(lower(it->path().extension().wstring())==L".exe"){p=it->path();break;}}}}
 if(lower(p.extension().wstring())==L".exe")ExtractIconExW(p.c_str(),0,&large,nullptr,1);
 return large;
}
double sizeValue(const std::wstring& text){wchar_t* end=nullptr;double n=wcstod(text.c_str(),&end);if(end==text.c_str())return -1;std::wstring unit=end;for(auto pair:std::vector<std::pair<std::wstring,double>>{{L"TB",1099511627776.0},{L"GB",1073741824.0},{L"MB",1048576.0},{L"KB",1024.0}})if(unit.find(pair.first)!=std::wstring::npos)return n*pair.second;return n;}
void memory(std::vector<Entry>& out){
 std::unordered_map<std::wstring,std::vector<size_t>> index;for(size_t i=0;i<out.size();i++){auto key=lower(fs::path(out[i].path).lexically_normal().wstring());while(key.size()>3&&key.back()==wchar_t(92))key.pop_back();index[key].push_back(i);}
 std::vector<unsigned long long> sums(out.size());std::vector<bool> found(out.size());HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return;PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);
 if(Process32FirstW(snapshot,&pe))do{HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pe.th32ProcessID);if(!p)continue;wchar_t raw[32768];DWORD n=32768;PROCESS_MEMORY_COUNTERS pm{};if(QueryFullProcessImageNameW(p,0,raw,&n)&&GetProcessMemoryInfo(p,&pm,sizeof(pm))){fs::path path=fs::path(raw).lexically_normal();while(!path.empty()){auto hit=index.find(lower(path.wstring()));if(hit!=index.end())for(auto i:hit->second){sums[i]+=pm.WorkingSetSize;found[i]=true;}auto parent=path.parent_path();if(parent==path)break;path=parent;}}CloseHandle(p);}while(!closing&&!cancelScan&&Process32NextW(snapshot,&pe));CloseHandle(snapshot);for(size_t i=0;i<out.size();i++)if(found[i])out[i].memory=bytes(sums[i]);
}

std::atomic<unsigned long long> visited{0},skipped{0};
std::wstring accessHint(const std::wstring& path){
 DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)return L"未知 / 路径不可访问";
 PSECURITY_DESCRIPTOR sd=nullptr;auto r=GetNamedSecurityInfoW((LPWSTR)path.c_str(),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,nullptr,&sd);
 if(r!=ERROR_SUCCESS)return L"权限未知";
 HANDLE token=nullptr,imp=nullptr;BOOL allowed=FALSE;DWORD granted=0;PRIVILEGE_SET privileges{};DWORD length=sizeof(privileges);
 GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};DWORD desired=(attr&FILE_ATTRIBUTE_DIRECTORY)?FILE_ADD_FILE:FILE_WRITE_DATA;
 BOOL checked=FALSE;if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&token)&&DuplicateToken(token,SecurityImpersonation,&imp))checked=AccessCheck(sd,imp,desired,&mapping,&privileges,&length,&granted,&allowed);
 if(imp)CloseHandle(imp);if(token)CloseHandle(token);LocalFree(sd);
 return !checked?L"权限未知":allowed?L"允许写入；修改前备份":L"当前权限不可写";
}
// Worker publishes batches; only the UI thread owns displayed rows.
void publishRows(const std::vector<Entry>& source){
 std::lock_guard<std::mutex> guard(lockRows);
 for(;published<source.size();++published){auto e=source[published];e.permission=L"待检查";e.cache=L"待检查";pendingRows.push_back(std::move(e));}
}
void drainRows(){invalidateOwnership();clearSummaries();std::lock_guard<std::mutex> guard(lockRows);for(auto& e:pendingRows)rows.push_back(std::move(e));pendingRows.clear();}
void deepScan(std::vector<Entry>& out,std::vector<fs::path> roots={}){
 std::set<std::wstring> javas;std::unordered_map<std::wstring,std::vector<size_t>> unresolved;for(size_t i=0;i<out.size();i++)if(out[i].kind==L"已安装应用"&&!fs::path(out[i].path).is_absolute())unresolved[normalizedApplicationName(out[i].name)].push_back(i);if(roots.empty()){DWORD drives=GetLogicalDrives();for(int d=0;d<26;d++){if(!(drives&(1u<<d)))continue;std::wstring root=std::wstring(1,wchar_t(L'A'+d))+L":"+wchar_t(92);UINT type=GetDriveTypeW(root.c_str());if(type==DRIVE_FIXED||type==DRIVE_REMOVABLE)roots.push_back(root);}}
 for(const auto& root:roots){if(closing||cancelScan)break;
 std::vector<fs::path> pending{fs::path(root)};
 while(!pending.empty()&&!closing&&!cancelScan){auto dir=pending.back();pending.pop_back();std::error_code ec;WIN32_FIND_DATAW data{};std::wstring pattern=(dir/L"*").wstring();std::wstring prefix(2,wchar_t(92));prefix+=L"?";prefix+=wchar_t(92);if(pattern.rfind(prefix,0)!=0)pattern=prefix+pattern;
 HANDLE handle=FindFirstFileExW(pattern.c_str(),FindExInfoBasic,&data,FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);if(handle==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_INVALID_PARAMETER)handle=FindFirstFileExW(pattern.c_str(),FindExInfoBasic,&data,FindExSearchNameMatch,nullptr,0);if(handle==INVALID_HANDLE_VALUE){skipped++;continue;}
 do {if(closing||cancelScan)break;if(wcscmp(data.cFileName,L".")==0||wcscmp(data.cFileName,L"..")==0)continue;auto path=dir/data.cFileName;DWORD attr=data.dwFileAttributes;visited++;if(visited.load()%512==0)publishRows(out);
 if(attr==INVALID_FILE_ATTRIBUTES){skipped++;}else if(attr&FILE_ATTRIBUTE_REPARSE_POINT){skipped++;}else if(attr&FILE_ATTRIBUTE_DIRECTORY){pending.push_back(path);}else{
 auto name=lower(path.filename().wstring());
 if(name==L"release"&&lower(dir.filename().wstring())!=L"node_modules"){
 std::ifstream in(path);std::string line,ver;while(std::getline(in,line))if(line.rfind("JAVA_VERSION=",0)==0){ver=line.substr(13);if(!ver.empty()&&ver.front()=='\"')ver.erase(0,1);if(!ver.empty()&&ver.back()=='\"')ver.pop_back();break;}
 if(!ver.empty()&&fs::exists(dir/L"bin"/L"java.exe",ec)&&javas.insert(lower(dir.wstring())).second){std::wstring v(ver.begin(),ver.end()),major=v.substr(0,v.find(L'.'));if(major==L"1"&&v.size()>2)major=v.substr(2,v.find(L'.',2)-2);bool jdk=fs::exists(dir/L"bin"/L"javac.exe",ec);out.push_back({(jdk?L"JDK ":L"JRE ")+major,v,L"Java 环境",dir.wstring()});}
 }
 if(name==L"photoshop.exe"||name==L"adobe premiere pro.exe"||name==L"illustrator.exe"||name==L"afterfx.exe"){Entry app;app.name=path.stem().wstring();app.kind=L"应用主程序";app.path=dir.wstring();app.iconPath=path.wstring();out.push_back(app);}
 if(name.size()>4&&name.substr(name.size()-4)==L".exe"){
 auto missing=unresolved.find(normalizedApplicationName(path.stem().wstring()));if(missing!=unresolved.end()&&missing->second.size()==1&&plausibleApplicationRoot(dir)){size_t index=missing->second.front();if(!fs::path(out[index].path).is_absolute()&&diskProgramMatchesRegistration(out[index],path)){out[index].path=dir.wstring();out[index].iconPath=path.wstring();out[index].locationSource=L"深度扫描：同名主程序定位";out[index].familyCache.clear();out[index].searchCache.clear();Entry located=out[index];out.push_back(std::move(located));}unresolved.erase(missing);}

 static const std::set<std::wstring> tools={L"python.exe",L"node.exe",L"java.exe",L"javac.exe",L"go.exe",L"rustc.exe",L"cargo.exe",L"gcc.exe",L"g++.exe",L"clang.exe",L"cl.exe",L"cmake.exe",L"dotnet.exe",L"git.exe"};
 out.push_back({path.filename().wstring(),L"未读取",tools.count(name)?L"编程环境":L"可执行文件（待识别）",path.wstring()});
 }
 } }while(FindNextFileW(handle,&data));if(GetLastError()!=ERROR_NO_MORE_FILES&&!cancelScan&&!closing)skipped++;FindClose(handle);
 }
 }
}

std::wstring cachePaths(const Entry& e,const std::function<bool()>& cancelled={}){
 std::vector<std::pair<fs::path,bool>> roots;
 wchar_t local[32768]={},roam[32768]={};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);GetEnvironmentVariableW(L"APPDATA",roam,32768);
 std::wstring name=lower(e.name),exe=lower(fs::path(e.path).filename().wstring());
 auto add=[&](const wchar_t* base,const wchar_t* tail,bool known){if(*base)roots.push_back({fs::path(base)/tail,known});};
 if(exe==L"chrome.exe"||name==L"google chrome")add(local,L"Google/Chrome/User Data",true);
 if(exe==L"msedge.exe"||name==L"microsoft edge")add(local,L"Microsoft/Edge/User Data",true);
 if(exe==L"firefox.exe"||name.find(L"mozilla firefox")==0)add(local,L"Mozilla/Firefox/Profiles",true);
 if(exe==L"code.exe"||name.find(L"microsoft visual studio code")==0)add(roam,L"Code",true);
 std::wstring stem=fs::path(e.name).stem().wstring();
 if(stem.size()>2&&stem.find_first_of(L"\\/:*")==std::wstring::npos){add(local,stem.c_str(),false);add(roam,stem.c_str(),false);}
 std::error_code ec;fs::path app(e.path);if(fs::is_regular_file(app,ec))app=app.parent_path();if(fs::is_directory(app,ec))roots.push_back({app,false});
 std::set<std::wstring> found;
 const std::set<std::wstring> cacheNames={L"cache",L"caches",L"code cache",L"gpucache",L"cache2",L"cacheddata",L"shadercache",L"dawncache"};
 std::wstring result;
 for(auto& root:roots){std::vector<std::pair<fs::path,int>> dirs{{root.first,0}};int checked=0;while(!dirs.empty()&&checked++<(root.second?300:30)&&!closing&&(!cancelled||!cancelled())){auto current=dirs.back();dirs.pop_back();fs::directory_iterator it(current.first,ec),end;ec.clear();while(it!=end&&(!cancelled||!cancelled())){auto p=it->path();DWORD a=GetFileAttributesW(p.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT)){auto n=lower(p.filename().wstring());if(cacheNames.count(n)){if(found.insert(lower(p.wstring())).second){if(!result.empty())result+=L" | ";result+=(root.second?L"[已知规则] ":L"[候选需核实] ")+p.wstring();}}else if(root.second&&current.second<2)dirs.push_back({p,current.second+1});}it.increment(ec);if(ec){ec.clear();break;}}}}
 return result.empty()?L"未定位（不代表无缓存）":result;
}
void scan(){std::vector<Entry> result;const wchar_t* cmds[]={L"python.exe",L"python3.exe",L"node.exe",L"npm.cmd",L"git.exe",L"java.exe",L"javac.exe",L"dotnet.exe",L"go.exe",L"rustc.exe",L"cargo.exe",L"gcc.exe",L"g++.exe",L"clang.exe",L"cl.exe",L"cmake.exe",L"ninja.exe",L"php.exe",L"ruby.exe",L"docker.exe",L"code.cmd"};for(auto cmd:cmds){wchar_t p[32768];DWORD n=SearchPathW(0,cmd,0,32768,p,0);if(n&&n<32768)result.push_back({cmd,L"未读取",L"编程环境",p});}installed(result,HKEY_LOCAL_MACHINE,KEY_WOW64_64KEY);installed(result,HKEY_LOCAL_MACHINE,KEY_WOW64_32KEY);installed(result,HKEY_CURRENT_USER,KEY_WOW64_64KEY);installed(result,HKEY_CURRENT_USER,KEY_WOW64_32KEY);publishRows(result);deepScan(result);publishRows(result);for(auto& e:result){e.permission=L"点选后检查";e.cache=L"点选后检查";}std::stable_sort(result.begin(),result.end(),[](const Entry&a,const Entry&b){if(a.kind==L"Java 环境"&&b.kind!=a.kind)return true;if(b.kind==L"Java 环境"&&a.kind!=b.kind)return false;return lower(a.path)<lower(b.path);});std::set<std::wstring> seen;result.erase(std::remove_if(result.begin(),result.end(),[&](const Entry&e){return !seen.insert(lower(e.name+L"|"+e.version+L"|"+e.path)).second;}),result.end());if(!closing&&!cancelScan)memory(result);{std::lock_guard<std::mutex> g(lockRows);finalRows=std::move(result);}PostMessageW(win,DONE,0,0);}
#include "application_ownership.h"
bool rebuildingList=false,suppressSearch=false;
void invalidateDetailSelection();

std::vector<int> renderQueue;size_t renderCursor=0;int preferredRow=-1;
void renderBatch(){if(renderCursor>=renderQueue.size()){KillTimer(win,5);return;}rebuildingList=true;SendMessageW(list,WM_SETREDRAW,FALSE,0);auto started=GetTickCount64();int count=0;while(renderCursor<renderQueue.size()&&count++<80){int row=renderQueue[renderCursor++];if(row<0||size_t(row)>=rows.size())continue;auto&e=rows[row];int index=(int)visible.size();visible.push_back(row);LVITEMW item{};item.mask=LVIF_TEXT|LVIF_IMAGE|LVIF_GROUPID;item.iGroupId=developerView?environmentGroup(e):I_GROUPIDNONE;item.iItem=index;item.iImage=0;item.pszText=(LPWSTR)e.name.c_str();ListView_InsertItem(list,&item);if(GetTickCount64()-started>=8)break;}SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,FALSE);rebuildingList=false;if(renderCursor<renderQueue.size())SetTimer(win,5,30,nullptr);else KillTimer(win,5);}
void refresh(bool append=false){if(ownershipDirty){rebuildOwnership();if(ownershipVisibilityChanged)append=false;}rebuildingList=true;wchar_t q[1024];GetWindowTextW(searchBox,q,1024);std::wstring query=lower(q);SendMessageW(list,WM_SETREDRAW,FALSE,0);
 if(!append){invalidateDetailSelection();KillTimer(win,5);renderQueue.clear();renderCursor=0;ListView_DeleteAllItems(list);ListView_RemoveAllGroups(list);ListView_EnableGroupView(list,developerView?TRUE:FALSE);if(developerView)for(size_t id=0;id<environmentFamilies.size();id++){LVGROUP group{};group.cbSize=sizeof(group);group.mask=LVGF_HEADER|LVGF_GROUPID;group.iGroupId=(int)id;group.pszHeader=(LPWSTR)environmentFamilies[id].c_str();ListView_InsertGroup(list,-1,&group);}loadedIcons.clear();visible.clear();rendered=0;visible.push_back(systemView?-2:-1);if(!ImageList_GetImageCount(icons))ImageList_AddIcon(icons,LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101)));LVITEMW item{};item.mask=LVIF_TEXT|LVIF_IMAGE|LVIF_GROUPID;item.iGroupId=developerView?0:I_GROUPIDNONE;item.iImage=0;item.pszText=(LPWSTR)(systemView?L"← 返回全部应用":L"系统组件");ListView_InsertItem(list,&item);}
 for(size_t i=rendered;i<rows.size();i++){auto&e=rows[i];if(systemEntry(e)!=systemView)continue;if(i<applicationOwner.size()&&applicationOwner[i]!=(int)i)continue;if(!systemView&&unknownView!=unidentifiedApplication(e))continue;if(developerView&&family(e)==L"应用与可执行文件")continue;if(searchable(e).find(query)==std::wstring::npos)continue;renderQueue.push_back((int)i);}rendered=rows.size();if(preferredRow>=0){auto found=std::find(renderQueue.begin()+renderCursor,renderQueue.end(),preferredRow);if(found!=renderQueue.end())std::rotate(renderQueue.begin()+renderCursor,found,found+1);preferredRow=-1;}
 SendMessageW(list,WM_SETREDRAW,TRUE,0);rebuildingList=false;renderBatch();auto text=L"符合当前分类 / 搜索："+std::to_wstring(renderQueue.size())+L" 项"+(scanning?L" · 扫描中":cancelScan?L" · 已停止（部分结果）":L" · 扫描完成");SetWindowTextW(statusLabel,text.c_str());}
void startScan(){if(busy.exchange(true)){cancelScan=true;return;}cancelScan=false;invalidateOwnership();clearSummaries();clearDetailCache();rows.clear();pendingRows.clear();finalRows.clear();published=0;refresh();visited=0;skipped=0;SetWindowTextW(scanBtn,L"停止扫描");scanning=true;gameOffered=false;scanStarted=GetTickCount64();ShowWindow(progressBar,SW_SHOW);SendMessageW(progressBar,PBM_SETMARQUEE,TRUE,40);SetTimer(win,1,1000,nullptr);if(worker.joinable())worker.join();EnableWindow(sizeBtn,FALSE);SetWindowTextW(statusLabel,L"正在扫描所有本地磁盘…");worker=std::thread(scan);}
void calc(){if(busy)return;int sel=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(sel<0){MessageBoxW(win,L"请先选择一个应用或编程环境。",L"计算磁盘占用",MB_OK);return;}int index=visible[sel];if(index<0)return;auto path=rows[index].path;std::error_code ec;if(!fs::exists(path,ec)){MessageBoxW(win,L"该条目未提供可访问的安装位置。",L"无法计算",MB_OK);return;}if(worker.joinable())worker.join();busy=true;EnableWindow(scanBtn,FALSE);EnableWindow(sizeBtn,FALSE);SetWindowTextW(statusLabel,L"正在统计所选安装位置的文件大小，大目录可能需要一些时间…");worker=std::thread([index,path]{unsigned long long total=0;bool partial=false;std::error_code ec;fs::path p(path);if(fs::is_regular_file(p,ec)){auto n=fs::file_size(p,ec);if(!ec)total=n;else partial=true;}else{fs::recursive_directory_iterator it(p,fs::directory_options::none,ec),end;if(ec)partial=true;while(it!=end&&!closing){DWORD attrs=GetFileAttributesW(it->path().c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){it.disable_recursion_pending();partial=true;}auto st=it->symlink_status(ec);if(ec){partial=true;ec.clear();}else if(fs::is_regular_file(st)){auto n=it->file_size(ec);if(!ec)total+=n;else{partial=true;ec.clear();}}it.increment(ec);if(ec){partial=true;ec.clear();}}} {std::lock_guard<std::mutex>g(lockRows);rows[index].size=bytes(total)+(partial?L"（不完整）":L"（文件合计）");}PostMessageW(win,DONE,0,0);});}


std::unordered_map<std::wstring,std::wstring> summaries;void clearSummaries(){summaries.clear();}
std::wstring groupSummary(const Entry& selected){auto f=family(selected);auto cached=summaries.find(f);if(cached!=summaries.end())return cached->second;size_t count=0;double total=0;std::unordered_map<std::wstring,double> paths;for(const auto&e:rows)if(family(e)==f){count++;double n=sizeValue(e.memory);if(n>=0)paths[lower(fs::path(e.path).lexically_normal().wstring())]=n;}
 for(const auto&entry:paths){bool nested=false;auto p=fs::path(entry.first).parent_path();while(!p.empty()){if(paths.count(p.wstring())){nested=true;break;}auto next=p.parent_path();if(next==p)break;p=next;}if(!nested)total+=entry.second;}
 return summaries[f]=L"分组："+f+L" / "+std::to_wstring(count)+L" 项\r\n同组已知工作集合计："+bytes((unsigned long long)total)+L"\r\n（共享内存仍可能重复）\r\n\r\n";}
std::wstring environmentWarning(const fs::path&,const fs::path&);
#include "directory_inspection.h"
#include "async_details.h"
void showDetails(){if(rebuildingList)return;int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(selected<0||size_t(selected)>=visible.size())return;if(visible[selected]<0){invalidateDetailSelection();SetWindowTextW(details,systemView?L"点击返回全部应用。":L"系统组件集中显示在此入口。点击查看内部项目。");EnableWindow(GetDlgItem(win,21),FALSE);EnableWindow(GetDlgItem(win,22),FALSE);return;}if(size_t(visible[selected])>=rows.size())return;auto&e=rows[visible[selected]];selectDetail(e);EnableWindow(GetDlgItem(win,22),!busy&&!systemEntry(e));EnableWindow(GetDlgItem(win,21),!busy);}

void sortApps(int mode){if(busy){MessageBoxW(win,L"请先停止扫描或等待扫描完成，再排序。",L"扫描进行中",MB_OK);return;}std::stable_sort(rows.begin(),rows.end(),[mode](const Entry&a,const Entry&b){if(mode==2)return family(a)==family(b)?a.name<b.name:family(a)<family(b);return sizeValue(mode==0?a.memory:a.size)>sizeValue(mode==0?b.memory:b.size);});refresh();}
bool removable(const fs::path& raw){std::error_code ec;auto p=fs::absolute(raw,ec).lexically_normal();if(ec||p==p.root_path()||p.wstring().size()<5)return false;
 for(auto q=p;!q.empty()&&q!=q.root_path();q=q.parent_path()){DWORD a=GetFileAttributesW(q.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))return false;}
 wchar_t root[32768];const wchar_t* vars[]={L"SystemRoot",L"ProgramFiles",L"ProgramFiles(x86)",L"ProgramData",L"USERPROFILE",L"PUBLIC",L"APPDATA",L"LOCALAPPDATA"};for(auto v:vars){if(GetEnvironmentVariableW(v,root,32768)){auto r=fs::path(root).lexically_normal().wstring();if(inside(r,p.wstring()))return false;if(std::wstring(v)==L"SystemRoot"&&inside(p.wstring(),r))return false;}}
 auto low=lower(p.wstring());if(low.find(LR"(\common files)")!=std::wstring::npos||low.find(LR"(\windowsapps)")!=std::wstring::npos||low.find(LR"(\windows defender)")!=std::wstring::npos)return false;
 wchar_t profile[32768];if(GetEnvironmentVariableW(L"USERPROFILE",profile,32768)){auto users=fs::path(profile).parent_path();if(inside(users.wstring(),p.wstring())||lower(p.parent_path().wstring())==lower(users.wstring()))return false;}
 wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);if(inside(self,p.wstring()))return false;return true;}
void removeTree(const fs::path& p,unsigned long long& removed,unsigned long long& failed){DWORD a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES){if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)failed++;return;}
 if(a&FILE_ATTRIBUTE_REPARSE_POINT){failed++;return;}std::error_code ec;if(a&FILE_ATTRIBUTE_DIRECTORY){fs::directory_iterator it(p,ec),end;if(ec){failed++;return;}while(it!=end){auto child=it->path();it.increment(ec);removeTree(child,removed,failed);if(ec){failed++;break;}}}
 if(a&FILE_ATTRIBUTE_READONLY)SetFileAttributesW(p.c_str(),a&~FILE_ATTRIBUTE_READONLY);
 if(fs::remove(p,ec))removed++;else if(ec)failed++;
}

// Deletion can remove a confirmed component or link entry; migration keeps its stricter rules.
bool deletionPathAllowed(const fs::path& raw){std::error_code ec;auto p=fs::absolute(raw,ec).lexically_normal();if(ec||!raw.is_absolute()||p==p.root_path()||p.wstring().size()<4)return false;for(auto&part:raw)if(part==L"..")return false;
 wchar_t windows[32768];GetWindowsDirectoryW(windows,32768);if(inside(p.wstring(),windows))return false;
 for(const auto* variable:{L"ProgramFiles",L"ProgramFiles(x86)",L"ProgramData",L"USERPROFILE",L"PUBLIC",L"APPDATA",L"LOCALAPPDATA"}){wchar_t value[32768];if(GetEnvironmentVariableW(variable,value,32768)){auto root=fs::path(value).lexically_normal();if(inside(root.wstring(),p.wstring()))return false;if(std::wstring(variable).find(L"ProgramFiles")==0){if(lower(p.wstring())==lower((root/L"Common Files").wstring())||inside(p.wstring(),(root/L"WindowsApps").wstring())||inside(p.wstring(),(root/L"Windows Defender").wstring()))return false;}}}
 wchar_t profile[32768];if(GetEnvironmentVariableW(L"USERPROFILE",profile,32768)){auto users=fs::path(profile).parent_path();if(inside(users.wstring(),p.wstring())||lower(p.parent_path().wstring())==lower(users.wstring()))return false;}
 for(auto q=p.parent_path();!q.empty()&&q!=q.root_path();q=q.parent_path()){DWORD a=GetFileAttributesW(q.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))return false;}
 wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);return !inside(self,p.wstring());}
void deleteConfirmedTarget(const fs::path& p,unsigned long long& removed,unsigned long long& failed){if(!deletionPathAllowed(p)){failed++;return;}DWORD attrs=GetFileAttributesW(p.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){bool ok=(attrs&FILE_ATTRIBUTE_DIRECTORY)?RemoveDirectoryW(p.c_str()):DeleteFileW(p.c_str());if(ok)removed++;else failed++;return;}removeTree(p,removed,failed);}

void askRemoveEmptyParents(const std::vector<fs::path>& targets,unsigned long long& removed,unsigned long long& failed){std::set<std::wstring> seen;std::vector<fs::path> empty;for(auto&target:targets){auto parent=target.parent_path();std::error_code ec;DWORD attributes=GetFileAttributesW(parent.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)||!deletionPathAllowed(parent)||!seen.insert(lower(parent.wstring())).second)continue;if(fs::is_directory(parent,ec)&&fs::is_empty(parent,ec)&&!ec)empty.push_back(parent);}if(empty.empty())return;std::wstring prompt=L"清理后，下列文件夹已经为空：\n\n";for(auto&path:empty)prompt+=path.wstring()+L"\n";prompt+=L"\n是否删除这些空文件夹？仅删除空目录，不递归删除内容。";if(deleteDialog(win,prompt.c_str(),L"是否删除空文件夹",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;for(auto&path:empty){std::error_code ec;DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES)continue;if((attr&FILE_ATTRIBUTE_REPARSE_POINT)||!deletionPathAllowed(path)||!fs::is_empty(path,ec)||ec){failed++;continue;}if(fs::remove(path,ec))removed++;else failed++;}}
#include "removal_feedback.h"
#include "cleanup_plan.h"
#include "elevated_delete.h"
#include "direct_delete.h"
void deleteSelected(){
 if(busy){MessageBoxW(win,L"请先停止扫描或等待当前操作完成。",L"删除",MB_OK);return;}
 int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(i<0||size_t(i)>=visible.size()||visible[i]<0)return;Entry e=rows[visible[i]];
 if(ownershipDirty)rebuildOwnership();if(ambiguousRoots.count(lower(fs::path(e.path).lexically_normal().wstring()))){deleteWholeDirectory(e.path);return;}
 if(!deletionPathAllowed(e.path)){MessageBoxW(win,L"此系统/共享根目录或本工具目录不能作为删除目标。",L"无法删除",MB_OK|MB_ICONWARNING);return;}
 if(e.cache.empty()||e.cache==L"点选后检查"||e.cache==L"待检查")e.cache=cachePaths(e);
 auto targets=relatedFiles(e);auto uninstall=e.quietUninstallCommand.empty()?e.uninstallCommand:e.quietUninstallCommand;
 bool component=systemEntry(e)||lower(e.path).find(L"common files")!=std::wstring::npos;
 std::wstring message=L"应用："+e.name+L"\n\n包含的主程序和辅助组件：\n"+componentSummary(e,5)+L"\n永久删除范围（包括个人配置和已识别缓存）：\n";
 for(const auto& path:targets)message+=path.wstring()+L"\n";
 for(const auto& other:rows)if(other.kind==L"已安装应用"&&other.name!=e.name&&(inside(other.path,e.path)||inside(e.path,other.path))){message+=L"可能影响："+other.name+L"\n";component=true;}
 if(!uninstall.empty())message+=L"\n优先卸载命令：\n"+uninstall+L"\n";
 message+=unifiedDeleteNotice();
 if(MessageBoxW(win,message.c_str(),L"卸载并清理：一次确认",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;
 launchElevatedDelete(targets,component,uninstall,true,true);
}

fs::path chooseFolder(const wchar_t* title){BROWSEINFOW bi{};bi.hwndOwner=win;bi.lpszTitle=title;bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;auto id=SHBrowseForFolderW(&bi);if(!id)return {};wchar_t path[MAX_PATH];bool ok=SHGetPathFromIDListW(id,path);CoTaskMemFree(id);return ok?fs::path(path):fs::path();}
void inspectChosenDirectory(){auto path=chooseFolder(L"选择要检查的应用目录（先查看内容与使用记录，不会删除）");if(path.empty())return;Entry e;e.name=path.filename().wstring();e.kind=L"目录检查";e.path=path.wstring();e.permission=e.cache=L"点选后检查";inspectedDirectories.erase(std::remove_if(inspectedDirectories.begin(),inspectedDirectories.end(),[&](const Entry&item){return item.path==e.path;}),inspectedDirectories.end());inspectedDirectories.push_back(e);rows.push_back(e);preferredRow=(int)rows.size()-1;invalidateOwnership();systemView=systemEntry(e);developerView=false;unknownView=false;suppressSearch=true;SetWindowTextW(searchBox,L"");suppressSearch=false;refresh();for(size_t i=0;i<visible.size();i++)if(visible[i]==(int)rows.size()-1){ListView_SetItemState(list,(int)i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,(int)i,FALSE);break;}}
std::wstring environmentWarning(const fs::path& old,const fs::path& dest){std::wstring result;
 for(int scope=0;scope<2;scope++){HKEY k;auto key=scope?L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment":L"Environment";if(RegOpenKeyExW(scope?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,key,0,KEY_READ,&k)!=ERROR_SUCCESS)continue;
 for(auto variable:{L"Path",L"JAVA_HOME"}){auto value=regstr(k,variable);std::wstringstream parts(value);std::wstring part;while(std::getline(parts,part,L';')){part=clean(part);if(part.empty())continue;auto check=old;std::error_code ec;if(fs::is_regular_file(old,ec))check=old.parent_path();if(inside(part,check.wstring())||inside(check.wstring(),part)){result+=(scope?L"系统 ":L"用户 ")+std::wstring(variable)+L"："+part+L"\r\n";auto replacement=dest;if(fs::is_regular_file(old,ec))replacement=dest.parent_path();result+=L"  请核对新位置："+replacement.wstring()+L"\r\n";}}}RegCloseKey(k);}
 return result.empty()?L"未发现 PATH / JAVA_HOME 的直接路径匹配（变量嵌套和自定义配置仍需核实）。":L"注意：迁移后需要手动核对环境变量：\r\n"+result+L"通过 Windows「编辑系统环境变量」修改，随后重新打开终端。";}
std::string utf8(const std::wstring& s){int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),out.data(),n,nullptr,nullptr);return out;}
struct PathEdit{fs::path path;std::string before,after;};
std::vector<PathEdit> referencePlan(const fs::path& root,const fs::path& old,const fs::path& dest){std::vector<PathEdit> edits;std::error_code ec;std::vector<fs::path> dirs{root};const std::set<std::wstring> extensions={L".txt",L".ini",L".conf",L".config",L".properties",L".json",L".xml",L".yaml",L".yml",L".cmd",L".bat",L".ps1"};
 auto from=utf8(old.wstring()),to=utf8(dest.wstring());
 while(!dirs.empty()){auto dir=dirs.back();dirs.pop_back();fs::directory_iterator it(dir,ec),end;if(ec){ec.clear();continue;}for(;it!=end;it.increment(ec)){if(ec){ec.clear();break;}auto p=it->path();DWORD a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES||a&FILE_ATTRIBUTE_REPARSE_POINT)continue;if(a&FILE_ATTRIBUTE_DIRECTORY){if(p.filename()!=L".git")dirs.push_back(p);continue;}if(!extensions.count(lower(p.extension().wstring()))||it->file_size(ec)>2*1024*1024||ec){ec.clear();continue;}
 std::ifstream input(p,std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});if(text.find('\0')!=std::string::npos||text.empty())continue;int valid=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);if(!valid)continue;
 auto after=text;size_t at=0;while((at=after.find(from,at))!=std::string::npos){size_t endpos=at+from.size();bool left=at==0||std::string("\"' =\t\r\n;(").find(after[at-1])!=std::string::npos;bool right=endpos==after.size()||std::string("\\/\"' ;\t\r\n)").find(after[endpos])!=std::string::npos;if(left&&right){after.replace(at,from.size(),to);at+=to.size();}else at+=from.size();}if(after!=text)edits.push_back({p,text,after});}}
 return edits;}
bool runningAt(const fs::path& root){HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return true;PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);bool found=false;if(Process32FirstW(snap,&pe))do{HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pe.th32ProcessID);if(!p)continue;wchar_t path[32768];DWORD n=32768;if(QueryFullProcessImageNameW(p,0,path,&n)&&inside(path,root.wstring()))found=true;CloseHandle(p);}while(!found&&Process32NextW(snap,&pe));CloseHandle(snap);return found;}
void migrateSelected(){if(busy){MessageBoxW(win,L"请先停止扫描或等待完成。",L"迁移位置",MB_OK);return;}int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(i<0||visible[i]<0)return;Entry entry=rows[visible[i]];fs::path old=entry.path;
 if(systemEntry(entry)||!removable(old)||entry.kind==L"已安装应用"){MessageBoxW(win,L"此位置属于受保护目录、链接或登记安装的应用，不能按便携文件迁移。Oracle java8path 属于共享入口，应迁移真实的便携 JDK 并核对 PATH。",L"不能直接迁移",MB_OK|MB_ICONWARNING);return;}
 if(runningAt(old)){MessageBoxW(win,L"发现该位置内的运行进程，请先关闭应用。",L"迁移前检查",MB_OK);return;}
 auto parent=chooseFolder(L"选择目标目录：在其中保留原文件或文件夹名称");if(parent.empty())return;auto dest=parent/old.filename();std::error_code ec;
 if(!removable(dest)||fs::exists(dest,ec)||inside(dest.wstring(),old.wstring())||inside(old.wstring(),dest.wstring())){MessageBoxW(win,L"目标已存在、与源位置重叠或属于受保护位置，请重新选择。",L"目标不可用",MB_OK);return;}
 // Reject all reparse points before any move, including descendants.
 if(fs::is_directory(old,ec)){fs::recursive_directory_iterator it(old,ec),end;for(;it!=end&&!ec;it.increment(ec)){DWORD a=GetFileAttributesW(it->path().c_str());if(a==INVALID_FILE_ATTRIBUTES||a&FILE_ATTRIBUTE_REPARSE_POINT){MessageBoxW(win,L"源目录包含链接或无法读取的项目，已取消迁移。",L"无法完整迁移",MB_OK);return;}}if(ec)return;}
 auto root=chooseFolder(L"选择要扫描路径引用的项目/配置目录（取消则只迁移文件）");std::vector<PathEdit> edits;if(!root.empty()){if(!removable(root)){MessageBoxW(win,L"配置扫描范围不能是系统、共享或磁盘根目录。",L"范围受保护",MB_OK);return;}edits=referencePlan(root,old,dest);}
 auto warning=environmentWarning(old,dest);std::wstring preview=L"源位置："+old.wstring()+L"\n目标位置："+dest.wstring()+L"\n\n"+warning+L"\n\n将直接修改以下 UTF-8 文本配置（每个文件先备份）：\n";for(auto&e:edits)preview+=e.path.wstring()+L"\n";if(edits.empty())preview+=L"无精确匹配\n";
 preview+=L"\n仅替换所选目录中大小不超过 2 MB、受支持文本格式的精确路径。二进制、转义路径、注册表、快捷方式、环境变量及未匹配引用不会修改。不能保证软件迁移后可运行。\n\n确认迁移并执行以上修改？";
 if(MessageBoxW(win,preview.c_str(),L"迁移预览 / PATH 提醒",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;
 // Back up and validate every text file before starting the move.
 std::wstring suffix=L".devscope-backup-"+std::to_wstring(GetTickCount64());for(auto&e:edits){std::ifstream in(e.path,std::ios::binary);std::string now((std::istreambuf_iterator<char>(in)),{});if(now!=e.before||!CopyFileW(e.path.c_str(),(e.path.wstring()+suffix).c_str(),TRUE)){MessageBoxW(win,L"配置文件已变化或备份失败，未执行迁移。已创建的备份可保留。",L"取消迁移",MB_OK);return;}}
 std::wstring source=old.wstring(),target=dest.wstring();source.push_back(L'\0');target.push_back(L'\0');SHFILEOPSTRUCTW operation{};operation.hwnd=win;operation.wFunc=FO_MOVE;operation.pFrom=source.c_str();operation.pTo=target.c_str();operation.fFlags=FOF_NOCONFIRMATION|FOF_NOCONFIRMMKDIR;
 int code=SHFileOperationW(&operation);if(code||operation.fAnyOperationsAborted||fs::exists(old,ec)||!fs::exists(dest,ec)){MessageBoxW(win,L"迁移未完整完成，未改写配置。请检查源和目标，可能存在部分迁移；配置备份已保留。",L"迁移未完成",MB_OK|MB_ICONWARNING);return;}
 size_t done=0,failed=0;for(auto&e:edits){auto path=e.path;if(inside(path.wstring(),old.wstring()))path=dest/path.lexically_relative(old);std::ifstream in(path,std::ios::binary);std::string current((std::istreambuf_iterator<char>(in)),{});in.close();if(current!=e.before){failed++;continue;}auto temp=path.wstring()+L".devscope-writing";HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE){failed++;continue;}DWORD written=0;bool ok=WriteFile(f,e.after.data(),(DWORD)e.after.size(),&written,nullptr)&&written==e.after.size();ok=FlushFileBuffers(f)&&ok;CloseHandle(f);if(ok&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))done++;else{failed++;DeleteFileW(temp.c_str());}}
 invalidateOwnership();clearDetailCache();rows[visible[i]].path=dest.wstring();rows[visible[i]].familyCache.clear();rows[visible[i]].searchCache.clear();rows[visible[i]].permission=accessHint(dest.wstring());refresh();auto report=L"迁移目标："+dest.wstring()+L"\n配置已修改："+std::to_wstring(done)+L"；失败："+std::to_wstring(failed)+L"\n备份后缀："+suffix+L"\n\n"+warning+L"\n\n请检查应用能否运行，并重新扫描。未匹配的引用需手动核对。";MessageBoxW(win,report.c_str(),L"迁移结果",MB_OK);}

void loadVisibleIcons(){drainDetailResults();loadAsyncIcons();}
Game2048 game;SnakeGame snake;MemoryGame memoryGame;int gameMode=0;
void resetMiniGame(HWND h){KillTimer(h,7);KillTimer(h,8);game.reset();snake.reset();memoryGame.reset();if(gameMode==1)SetTimer(h,7,160,nullptr);for(int id=10;id<=13;id++)ShowWindow(GetDlgItem(h,id),gameMode==2?SW_HIDE:SW_SHOW);SetWindowTextW(GetDlgItem(h,2),L"暂停 / 继续");ShowWindow(GetDlgItem(h,2),gameMode==1?SW_SHOW:SW_HIDE);InvalidateRect(h,nullptr,FALSE);SetFocus(h);}
void gameStep(HWND h,int direction){if(gameMode==0)game.move(direction);else if(gameMode==1)snake.turn(direction);else{int& i=memoryGame.focus;int x=i%4,y=i/4;x=std::clamp(x+(direction==1?1:direction==3?-1:0),0,3);y=std::clamp(y+(direction==2?1:direction==0?-1:0),0,3);i=y*4+x;}InvalidateRect(h,nullptr,FALSE);SetFocus(h);}
void flipCard(HWND h,int index){if(memoryGame.flip(index))SetTimer(h,8,750,nullptr);InvalidateRect(h,nullptr,FALSE);SetFocus(h);}
LRESULT CALLBACK gameProc(HWND h,UINT msg,WPARAM w,LPARAM l){switch(msg){
 case WM_CREATE:{auto button=[&](const wchar_t* text,int x,int y,int width,int id){HWND b=CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP,x,y,width,40,h,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);};button(L"2048",24,16,128,21);button(L"贪吃蛇",164,16,128,22);button(L"记忆翻牌",304,16,128,23);button(L"←",24,584,48,10);button(L"↑",80,584,48,11);button(L"↓",136,584,48,12);button(L"→",192,584,48,13);button(L"重新开始",272,584,160,1);button(L"暂停 / 继续",272,636,160,2);resetMiniGame(h);return 0;}
 case WM_COMMAND:if(LOWORD(w)>=21&&LOWORD(w)<=23){gameMode=LOWORD(w)-21;resetMiniGame(h);}if(LOWORD(w)==1)resetMiniGame(h);if(LOWORD(w)==2){snake.paused=!snake.paused;InvalidateRect(h,nullptr,FALSE);SetFocus(h);}if(LOWORD(w)>=10&&LOWORD(w)<=13){int directions[]={3,0,2,1};gameStep(h,directions[LOWORD(w)-10]);}return 0;
 case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE&&gameMode==1){snake.paused=true;InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_TIMER:if(w==7&&gameMode==1){if(!snake.paused&&!snake.over){snake.tick();InvalidateRect(h,nullptr,FALSE);}}if(w==8){KillTimer(h,8);memoryGame.conceal();InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_LBUTTONUP:if(gameMode==2){int x=(short)LOWORD(l)-24,y=(short)HIWORD(l)-156;if(x>=0&&x<408&&y>=0&&y<408&&x%104<96&&y%104<96){memoryGame.focus=(y/104)*4+x/104;flipCard(h,memoryGame.focus);}}return 0;
 case WM_KEYDOWN:if(w==VK_ESCAPE){DestroyWindow(h);return 0;}if(w=='R'){resetMiniGame(h);return 0;}if(w==VK_SPACE){if(gameMode==1){snake.paused=!snake.paused;InvalidateRect(h,nullptr,FALSE);}if(gameMode==2)flipCard(h,memoryGame.focus);return 0;}if(w==VK_RETURN&&gameMode==2){flipCard(h,memoryGame.focus);return 0;}if(w==VK_LEFT||w==VK_UP||w==VK_DOWN||w==VK_RIGHT)gameStep(h,w==VK_UP?0:w==VK_RIGHT?1:w==VK_DOWN?2:3);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps);RECT area;GetClientRect(h,&area);HDC dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,area.right,area.bottom);auto oldBitmap=SelectObject(dc,bitmap);FillRect(dc,&area,bg);SetBkMode(dc,TRANSPARENT);
 auto text=[&](std::wstring value,RECT r,HFONT f,COLORREF color,UINT flags){SelectObject(dc,f);SetTextColor(dc,color);DrawTextW(dc,value.c_str(),-1,&r,flags);};auto fill=[&](RECT r,COLORREF color){HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);};
 text(gameMode==0?L"2048":gameMode==1?L"贪吃蛇":L"记忆翻牌",{24,64,260,108},titleFont,Theme::text,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
 text(gameMode==2?L"配对 "+std::to_wstring(memoryGame.pairs)+L"/8":L"分数 "+std::to_wstring(gameMode==0?game.score:snake.score),{260,64,432,108},font,Theme::accent,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
 std::wstring hint=gameMode==0?(game.over()?L"本局结束 · 点击重新开始":game.won()?L"已达 2048！可以继续挑战":L"方向键 / 下方按钮移动，相同数字合并"):gameMode==1?(snake.won?L"恭喜填满棋盘！":snake.over?L"本局结束 · 点击重新开始":snake.paused?L"已暂停 · 方向键开始，空格暂停 / 继续":L"吃到橙色食物得分，避开边界与蛇身"):(memoryGame.pairs==8?L"全部配对成功！尝试次数："+std::to_wstring(memoryGame.attempts):L"点击翻牌；也可用方向键 + Enter / 空格");
 text(hint,{24,112,440,144},font,Theme::muted,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
 if(gameMode==1){fill({24,156,432,564},RGB(231,237,242));for(int i=0;i<20;i++)for(int j=0;j<20;j++)fill({28+i*20,160+j*20,47+i*20,179+j*20},RGB(241,245,248));if(snake.food>=0){int x=28+snake.food%20*20,y=160+snake.food/20*20;fill({x,y,x+19,y+19},RGB(194,119,44));}for(size_t k=0;k<snake.body.size();k++){int x=28+snake.body[k]%20*20,y=160+snake.body[k]/20*20;fill({x,y,x+19,y+19},k?RGB(75,160,164):Theme::accent);}}
 else for(int i=0;i<16;i++){int x=24+(i%4)*104,y=156+(i/4)*104;RECT tile{x,y,x+96,y+96};int value=gameMode==0?game.cells[i]:memoryGame.cards[i];bool show=gameMode==0||memoryGame.matched[i]||i==memoryGame.first||i==memoryGame.second;COLORREF color=gameMode==2?(memoryGame.matched[i]?RGB(179,222,207):show?RGB(215,233,238):RGB(27,106,128)):value>=128?RGB(27,106,128):value>=16?RGB(168,218,222):value?RGB(215,233,238):RGB(231,237,242);fill(tile,color);if(value)text(show?std::to_wstring(value):L"?",tile,value>=1024?font:titleFont,(gameMode==2?!show:value>=128)?RGB(255,255,255):Theme::text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(gameMode==2&&i==memoryGame.focus){RECT focus=tile;InflateRect(&focus,-4,-4);DrawFocusRect(dc,&focus);}}
 text(L"R 重新开始 · Esc 关闭 · 扫描不受影响",{24,636,260,676},font,Theme::muted,DT_LEFT|DT_WORDBREAK);
 BitBlt(screen,0,0,area.right,area.bottom,dc,0,0,SRCCOPY);SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);return 0;}
 case WM_DESTROY:KillTimer(h,7);KillTimer(h,8);gameWindow=nullptr;return 0;
 }return DefWindowProcW(h,msg,w,l);}
void openGame(){if(gameWindow){ShowWindow(gameWindow,SW_RESTORE);SetForegroundWindow(gameWindow);return;}WNDCLASSW wc{};wc.lpfnWndProc=gameProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DevScope2048";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(101));RegisterClassW(&wc);RECT r{0,0,456,692};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);gameWindow=CreateWindowW(wc.lpszClassName,scanning?L"休息空间 · 后台扫描继续进行":L"休息空间 · 三款离线小游戏",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,win,nullptr,wc.hInstance,nullptr);ShowWindow(gameWindow,SW_SHOW);SetFocus(gameWindow);}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){switch(msg){case WM_CREATE:{win=h;startDetailWorkers();font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");titleFont=CreateFontW(-32,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){HWND c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,0,0,h,(HMENU)(INT_PTR)id,0,0);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};HWND title=control(L"STATIC",L"应用控制中心",0,10);SendMessageW(title,WM_SETFONT,(WPARAM)titleFont,TRUE);control(L"STATIC",L"掌握每个应用的位置、环境与占用。",0,11);searchBox=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,12);SendMessageW(searchBox,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索名称、类型或安装路径…");scanBtn=control(L"BUTTON",L"重新扫描",BS_OWNERDRAW,13);sizeBtn=control(L"BUTTON",L"统计所选文件大小",BS_OWNERDRAW,14);control(L"BUTTON",L"内存 ↓",BS_OWNERDRAW,18);control(L"BUTTON",L"磁盘 ↓",BS_OWNERDRAW,19);control(L"BUTTON",L"JDK / SDK 归组",BS_OWNERDRAW,20);control(L"BUTTON",L"永久删除…",BS_OWNERDRAW,21);control(L"BUTTON",L"迁移位置…",BS_OWNERDRAW,22);control(L"BUTTON",L"全部应用",BS_OWNERDRAW,30);control(L"BUTTON",L"开发环境",BS_OWNERDRAW,31);control(L"BUTTON",L"系统组件",BS_OWNERDRAW,32);control(L"BUTTON",L"休息一下 · 小游戏",BS_OWNERDRAW,41);control(L"BUTTON",L"检查目录…",BS_OWNERDRAW,42);control(L"BUTTON",L"待识别程序",BS_OWNERDRAW,43);control(L"BUTTON",L"强制删除文件…",BS_OWNERDRAW,44);control(L"BUTTON",L"强制删除文件夹…",BS_OWNERDRAW,45);control(L"STATIC",L"搜索应用、版本或位置",0,33);control(L"STATIC",L"应用详情",0,34);progressBar=control(PROGRESS_CLASSW,L"",PBS_MARQUEE,40);ShowWindow(progressBar,SW_HIDE);SetTimer(h,2,200,nullptr);statusLabel=control(L"STATIC",L"准备扫描",0,15);list=control(WC_LISTVIEWW,L"",LVS_ICON|LVS_AUTOARRANGE|LVS_SINGLESEL|LVS_SHOWSELALWAYS,16);icons=ImageList_Create(32,32,ILC_COLOR32|ILC_MASK,64,64);ListView_SetImageList(list,icons,LVSIL_NORMAL);ListView_SetIconSpacing(list,144,112);details=control(L"EDIT",L"选择应用图标\r\n\r\n查看位置、缓存与权限详情",ES_MULTILINE|ES_READONLY|WS_VSCROLL,17);ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(list,RGB(255,255,255));ListView_SetTextColor(list,RGB(27,43,61));ListView_SetTextBkColor(list,RGB(255,255,255));const wchar_t* headers[]={L"名称",L"类别",L"版本",L"磁盘占用",L"运行内存（工作集）",L"安装 / 可执行文件位置",L"修改权限 / 建议",L"缓存位置（当前用户）"};int widths[]={200,180,130,190,180,550,300,650};for(int i=0;i<8;i++){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(LPWSTR)headers[i];c.cx=widths[i];ListView_InsertColumn(list,i,&c);}startScan();return 0;}case WM_SIZE:{int width=LOWORD(l),height=HIWORD(l);int left=216,right=width-344,content=right-left-24;
MoveWindow(GetDlgItem(h,10),left,24,content,44,TRUE);MoveWindow(GetDlgItem(h,11),left,76,content,28,TRUE);
MoveWindow(scanBtn,width-184,32,152,44,TRUE);
MoveWindow(GetDlgItem(h,30),20,160,164,44,TRUE);MoveWindow(GetDlgItem(h,31),20,216,164,44,TRUE);MoveWindow(GetDlgItem(h,32),20,272,164,44,TRUE);MoveWindow(GetDlgItem(h,41),20,344,164,44,TRUE);MoveWindow(GetDlgItem(h,42),20,400,164,44,TRUE);MoveWindow(GetDlgItem(h,43),20,456,164,44,TRUE);MoveWindow(GetDlgItem(h,44),20,512,164,44,TRUE);MoveWindow(GetDlgItem(h,45),20,568,164,44,TRUE);
MoveWindow(GetDlgItem(h,33),left,218,content,24,TRUE);MoveWindow(searchBox,left,246,content,44,TRUE);
MoveWindow(GetDlgItem(h,18),left,306,112,44,TRUE);MoveWindow(GetDlgItem(h,19),left+124,306,112,44,TRUE);MoveWindow(GetDlgItem(h,20),left+248,306,160,44,TRUE);
MoveWindow(list,left,370,content,std::max(80,height-426),TRUE);
MoveWindow(GetDlgItem(h,34),right,224,312,28,TRUE);MoveWindow(details,right,264,312,std::max(80,height-484),TRUE);
MoveWindow(sizeBtn,right,height-204,312,44,TRUE);MoveWindow(GetDlgItem(h,22),right,height-148,312,44,TRUE);MoveWindow(GetDlgItem(h,21),right,height-92,312,44,TRUE);
MoveWindow(progressBar,left,height-48,width-left-24,6,TRUE);MoveWindow(statusLabel,left,height-36,width-left-24,24,TRUE);InvalidateRect(h,nullptr,TRUE);return 0;}
case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT rect;GetClientRect(h,&rect);auto fill=[&](RECT r,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);};
fill({0,0,200,rect.bottom},RGB(235,241,246));fill({200,0,201,rect.bottom},RGB(211,222,231));
SetBkMode(dc,TRANSPARENT);auto draw=[&](std::wstring text,RECT r,COLORREF color,HFONT f){SelectObject(dc,f);SetTextColor(dc,color);DrawTextW(dc,text.c_str(),-1,&r,DT_LEFT|DT_SINGLELINE|DT_VCENTER);};
draw(L"DEV SCOPE",{24,28,190,70},Theme::accent,font);draw(L"应用与环境管理",{24,76,192,100},Theme::muted,font);draw(L"工作空间",{24,116,184,144},Theme::muted,font);draw(L"本地扫描 · 离线运行",{24,rect.bottom-72,196,rect.bottom-40},Theme::muted,font);
size_t systems=0,dev=0,appCount=0;for(size_t i=0;i<rows.size();i++){if(i<applicationOwner.size()&&applicationOwner[i]!=(int)i)continue;const auto&e=rows[i];if(systemEntry(e))systems++;else if(!unidentifiedApplication(e)){appCount++;if(family(e)!=L"应用与可执行文件")dev++;}}
int cardW=(rect.right-248-32)/3;std::wstring labels[]={L"已识别应用",L"开发环境",L"受保护系统项"};size_t counts[]={appCount,dev,systems};
for(int i=0;i<3;i++){int x=216+i*(cardW+16);fill({x,120,x+cardW,200},Theme::panel);fill({x,120,x+3,200},i==2?RGB(167,119,45):Theme::accent);draw(labels[i],{x+20,128,x+cardW-16,152},Theme::muted,font);draw(std::to_wstring(counts[i]),{x+20,152,x+cardW-16,194},Theme::text,titleFont);}
EndPaint(h,&ps);return 0;}
case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)l;if(d->CtlType==ODT_BUTTON){HBRUSH b=CreateSolidBrush((d->itemState&ODS_SELECTED)?RGB(216,234,242):Theme::panel);FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);HPEN pen=CreatePen(PS_SOLID,1,(d->CtlID==21?Theme::danger:RGB(178,197,210)));auto old=SelectObject(d->hDC,pen);auto brush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));Rectangle(d->hDC,1,1,d->rcItem.right-1,d->rcItem.bottom-1);SelectObject(d->hDC,brush);SelectObject(d->hDC,old);DeleteObject(pen);wchar_t text[128];GetWindowTextW(d->hwndItem,text,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,(d->itemState&ODS_DISABLED)?RGB(102,117,130):(d->CtlID==21?Theme::danger:Theme::text));DrawTextW(d->hDC,text,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS){RECT focus=d->rcItem;InflateRect(&focus,-4,-4);DrawFocusRect(d->hDC,&focus);}return TRUE;}break;}case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={1100,760};return 0;case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:SetBkColor((HDC)w,RGB(245,247,250));SetTextColor((HDC)w,Theme::text);return (LRESULT)bg;case WM_COMMAND:if(LOWORD(w)==43){unknownView=true;systemView=developerView=false;suppressSearch=true;SetWindowTextW(searchBox,L"");suppressSearch=false;refresh();SetWindowTextW(details,L"尚未可靠确认所属应用的程序候选，不代表独立应用。");return 0;}if(LOWORD(w)==44)chooseForceDelete();if(LOWORD(w)==45)chooseForceDelete(true);if(LOWORD(w)==42)inspectChosenDirectory();if(LOWORD(w)==41)openGame();if(LOWORD(w)>=30&&LOWORD(w)<=32){unknownView=false;systemView=LOWORD(w)==32;developerView=LOWORD(w)==31;suppressSearch=true;SetWindowTextW(searchBox,L"");suppressSearch=false;KillTimer(h,4);refresh();SetWindowTextW(details,L"选择应用查看位置、缓存与权限。");return 0;}if(LOWORD(w)==22)migrateSelected();if(LOWORD(w)==18)sortApps(0);if(LOWORD(w)==19)sortApps(1);if(LOWORD(w)==20)sortApps(2);if(LOWORD(w)==21)deleteSelected();if(LOWORD(w)==13)startScan();if(LOWORD(w)==14)calc();if(LOWORD(w)==12&&HIWORD(w)==EN_CHANGE&&!suppressSearch)SetTimer(h,4,160,nullptr);return 0;case WM_APP+2:systemView=!systemView;developerView=false;unknownView=false;unknownView=false;suppressSearch=true;SetWindowTextW(searchBox,L"");suppressSearch=false;KillTimer(h,4);refresh();SetWindowTextW(details,systemView?L"系统组件 / 选择图标查看详情。":L"全部应用 / 选择图标查看详情。");return 0;case WM_NOTIFY:if(((NMHDR*)l)->idFrom==16&&((NMHDR*)l)->code==LVN_KEYDOWN){auto key=(NMLVKEYDOWN*)l;int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(key->wVKey==VK_RETURN&&i>=0&&visible[i]<0)PostMessageW(h,WM_APP+2,0,0);return 0;}if(((NMHDR*)l)->idFrom==16&&((NMHDR*)l)->code==NM_CLICK){auto click=(NMITEMACTIVATE*)l;if(click->iItem>=0&&size_t(click->iItem)<visible.size()&&visible[click->iItem]<0)PostMessageW(h,WM_APP+2,0,0);return 0;}if(((NMHDR*)l)->idFrom==16&&((NMHDR*)l)->code==LVN_ITEMCHANGED){auto change=(NMLISTVIEW*)l;if(!rebuildingList&&(change->uChanged&LVIF_STATE)&&(change->uNewState&LVIS_SELECTED)&&!(change->uOldState&LVIS_SELECTED))showDetails();return 0;}if(((NMHDR*)l)->idFrom==16&&((NMHDR*)l)->code==NM_RCLICK&&!busy){int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(i>=0&&visible[i]>=0){auto&e=rows[visible[i]];auto detail=e.name+L"\n\n位置："+e.path+L"\n\n权限："+e.permission+L"\n\n缓存："+e.cache;MessageBoxW(h,detail.c_str(),L"应用详情 / 缓存位置",MB_OK);}return 0;}if(((NMHDR*)l)->idFrom==16&&((NMHDR*)l)->code==NM_DBLCLK){int s=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(s>=0&&visible[s]>=0){fs::path p(rows[visible[s]].path);std::error_code ec;if(fs::is_regular_file(p,ec))p=p.parent_path();if(fs::is_directory(p,ec))ShellExecuteW(h,L"open",p.c_str(),0,0,SW_SHOWNORMAL);}}return 0;case WM_TIMER:if(w==5){renderBatch();return 0;}if(w==4){KillTimer(h,4);refresh();return 0;}if(w==3){pollElevatedDelete();return 0;}if(w==2){loadVisibleIcons();return 0;}if(scanning&&busy){size_t before=rows.size();drainRows();if(rows.size()!=before){refresh(true);RECT cards{216,120,32767,200};InvalidateRect(h,&cards,FALSE);}auto t=L"扫描中  //  已检查 "+std::to_wstring(visited.load())+L" 个文件/目录  ·  已显示 "+std::to_wstring(visible.empty()?0:visible.size()-1)+L" 项  ·  跳过 "+std::to_wstring(skipped.load())+L" 处  ·  可点击停止保留结果";SetWindowTextW(statusLabel,t.c_str());if(shouldOfferGame(scanning,cancelScan,gameOffered,GetTickCount64()-scanStarted)){gameOffered=true;if(MessageBoxW(h,L"扫描已超过 30 秒。要玩一下小游戏吗？扫描会继续在后台进行。",L"等待时放松一下",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES)openGame();}}return 0;case DONE:invalidateOwnership();clearSummaries();scanning=false;SendMessageW(progressBar,PBM_SETMARQUEE,FALSE,0);ShowWindow(progressBar,SW_HIDE);if(gameWindow)SetWindowTextW(gameWindow,cancelScan?L"休息空间 · 扫描已停止，可继续游戏":L"休息空间 · 扫描已完成，可继续游戏");KillTimer(h,1);SetWindowTextW(scanBtn,L"重新扫描");if(worker.joinable())worker.join();{std::lock_guard<std::mutex> guard(lockRows);if(!finalRows.empty()){rows=std::move(finalRows);for(const auto&e:inspectedDirectories)rows.push_back(e);}pendingRows.clear();}busy=false;EnableWindow(scanBtn,TRUE);EnableWindow(sizeBtn,TRUE);refresh();return 0;case WM_DESTROY:KillTimer(h,5);closing=true;if(removalWindow)DestroyWindow(removalWindow);stopDetailWorkers();KillTimer(h,4);KillTimer(h,3);if(elevatedDeleteProcess){CloseHandle(elevatedDeleteProcess);elevatedDeleteProcess=nullptr;}KillTimer(h,2);if(gameWindow)DestroyWindow(gameWindow);closing=true;if(worker.joinable())worker.join();PostQuitMessage(0);return 0;}return DefWindowProcW(h,msg,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int show){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);SetProcessDPIAware();int helper=handleElevatedDeleteCommand();if(helper>=0){CoUninitialize();return helper;}INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&ic);WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=proc;wc.lpszClassName=L"DevScopeWindow";wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=bg;wc.hIcon=LoadIconW(inst,MAKEINTRESOURCEW(101));RegisterClassW(&wc);win=CreateWindowW(wc.lpszClassName,L"DevScope · 应用与开发环境",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,780,0,0,inst,0);ShowWindow(win,show);MSG m;while(GetMessageW(&m,0,0,0)>0){if(gameWindow&&(m.hwnd==gameWindow||IsChild(gameWindow,m.hwnd))){TranslateMessage(&m);DispatchMessageW(&m);}else if(!IsDialogMessageW(win,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}ImageList_Destroy(icons);DeleteObject(font);DeleteObject(titleFont);DeleteObject(bg);CoUninitialize();return 0;}








