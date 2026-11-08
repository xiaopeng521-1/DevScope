#pragma once
bool ownershipDirty=true,ownershipVisibilityChanged=false;
std::vector<int> applicationOwner;
std::unordered_map<int,std::vector<int>> applicationMembers;
std::unordered_map<std::wstring,int> applicationRoots;
std::unordered_set<std::wstring> ambiguousRoots;
void invalidateOwnership(){ownershipDirty=true;}
std::wstring ownershipPath(const std::wstring& path){auto key=lower(fs::path(path).lexically_normal().wstring());while(key.size()>3&&key.back()==wchar_t(92))key.pop_back();return key;}
bool rootCandidate(const Entry&e){if(e.kind!=L"已安装应用"&&e.kind!=L"应用主程序"&&e.kind!=L"Java 环境")return false;fs::path p(e.path);if(!p.is_absolute()||p==p.root_path())return false;auto leaf=lower(p.filename().wstring());return leaf!=L"windows"&&leaf!=L"program files"&&leaf!=L"program files (x86)"&&leaf!=L"common files"&&leaf!=L"users";}
void rebuildOwnership(){auto previous=applicationOwner;applicationOwner.resize(rows.size());applicationMembers.clear();applicationRoots.clear();ambiguousRoots.clear();
 for(size_t i=0;i<rows.size();i++){applicationOwner[i]=(int)i;auto&e=rows[i];if(!rootCandidate(e))continue;auto key=ownershipPath(e.path);auto found=applicationRoots.find(key);if(found==applicationRoots.end()){applicationRoots[key]=(int)i;continue;}auto&prior=rows[found->second];bool same=lower(prior.name)==lower(e.name)||e.kind==L"应用主程序"||prior.kind==L"应用主程序"||e.kind==L"Java 环境"||prior.kind==L"Java 环境";
 if(same){if(prior.kind==L"应用主程序"&&e.kind==L"已安装应用"){if(e.iconPath.empty())e.iconPath=prior.iconPath;found->second=(int)i;}else if(e.kind==L"应用主程序"&&!e.iconPath.empty()){prior.iconPath=e.iconPath;}if(prior.uninstallCommand.empty())prior.uninstallCommand=e.uninstallCommand;if(prior.quietUninstallCommand.empty())prior.quietUninstallCommand=e.quietUninstallCommand;}
 else ambiguousRoots.insert(key);}
 for(auto&key:ambiguousRoots)applicationRoots.erase(key);
 for(size_t i=0;i<rows.size();i++){if(rows[i].kind==L"目录检查")continue;auto key=ownershipPath(rows[i].path);while(!key.empty()){auto root=applicationRoots.find(key);if(root!=applicationRoots.end()){applicationOwner[i]=root->second;break;}auto at=key.find_last_of(wchar_t(92));if(at==std::wstring::npos||at<3)break;key.resize(at);}if(applicationOwner[i]!=(int)i)applicationMembers[applicationOwner[i]].push_back((int)i);}
 ownershipVisibilityChanged=false;for(size_t i=0;i<previous.size()&&i<applicationOwner.size();i++)if(previous[i]!=applicationOwner[i]){ownershipVisibilityChanged=true;break;}loadedIcons.clear();ownershipDirty=false;
}
std::wstring componentSummary(const Entry&e,size_t limit){if(ownershipDirty)rebuildOwnership();auto root=applicationRoots.find(ownershipPath(e.path));if(root==applicationRoots.end())return L"未识别到可确认归属的辅助程序。\r\n";auto member=applicationMembers.find(root->second);std::wstring text;size_t count=0;if(member!=applicationMembers.end())for(int i:member->second){const auto&child=rows[i];if(child.path==e.path)continue;if(count++<limit)text+=child.name+L"  —  "+child.path+L"\r\n";}if(count>limit)text+=L"另有 "+std::to_wstring(count-limit)+L" 项，目录内文件将随安装目录处理。\r\n";return text.empty()?L"尚未发现辅助程序；扫描仍可能继续补充。\r\n":text;}

bool unidentifiedApplication(const Entry&e){return e.kind==L"可执行文件（待识别）"||!fs::path(e.path).is_absolute()||ambiguousRoots.count(ownershipPath(e.path));}
