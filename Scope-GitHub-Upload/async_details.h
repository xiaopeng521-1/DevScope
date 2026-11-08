#pragma once
struct ExtraDetails {std::wstring permission,cache,environment,directory;};
struct DetailTask {Entry entry;std::wstring key;unsigned long long generation;};
struct DetailReply {std::wstring key;unsigned long long generation;ExtraDetails extra;};
struct IconReply {std::wstring key;HICON icon;};
std::mutex asyncMutex;std::condition_variable detailWake,iconWake;
std::optional<DetailTask> latestDetail;std::deque<Entry> iconTasks;std::deque<DetailReply> detailReplies;std::deque<IconReply> iconReplies;
std::atomic<bool> asyncStop{false};std::atomic<unsigned long long> detailGeneration{0};
std::thread detailThread,iconThread;
std::unordered_map<std::wstring,ExtraDetails> detailCache;
std::unordered_map<std::wstring,int> imageCache;std::unordered_set<std::wstring> iconsRequested;
std::wstring activeDetailKey;size_t iconCursor=0;
std::wstring entryKey(const Entry&e){return e.path+L"|"+e.name+L"|"+e.version+L"|"+e.kind;}
std::wstring imageKey(const Entry&e){return entryKey(e)+L"|"+e.iconPath;}
void invalidateDetailSelection(){++detailGeneration;activeDetailKey.clear();std::lock_guard<std::mutex> guard(asyncMutex);latestDetail.reset();}
void clearDetailCache(){invalidateDetailSelection();detailCache.clear();}
void renderDetail(Entry&e,const ExtraDetails* extra){
 if(extra){e.permission=extra->permission;e.cache=extra->cache;e.searchCache.clear();}
 auto text=groupSummary(e)+e.name+L"\r\n────────────────────\r\n"+e.kind+L" / "+e.version+L"\r\n\r\n包含的程序组件\r\n"+componentSummary(e)+L"\r\n安装目录 / 程序位置\r\n"+e.path+L"\r\n定位依据："+(e.locationSource.empty()?L"磁盘扫描 / PATH":e.locationSource)+L"\r\n\r\n缓存位置\r\n"+(extra?extra->cache:L"正在后台查询…")+L"\r\n\r\n当前权限\r\n"+(extra?extra->permission:L"正在后台查询…")+L"\r\n\r\n位置修改建议\r\n已安装应用建议通过安装器迁移；便携应用需确认支持。开发环境迁移后核对 PATH / JAVA_HOME。\r\n\r\n磁盘占用\r\n"+e.size+L"\r\n\r\n运行内存\r\n"+e.memory+L"\r\n\r\n环境变量引用\r\n"+(extra?extra->environment:L"正在后台查询…")+(systemEntry(e)?L"\r\n\r\n此项属于系统或共享组件，删除前会说明类型与影响。":L"");if(e.kind==L"目录检查")text=L"目录检查："+e.path+L"\r\n\r\n"+(extra?extra->directory:L"正在后台枚举程序并查询使用记录…")+L"\r\n────────────────────\r\n"+text;SetWindowTextW(details,text.c_str());
}
void selectDetail(Entry&e){auto key=entryKey(e);if(activeDetailKey==key){auto found=detailCache.find(key);renderDetail(e,found==detailCache.end()?nullptr:&found->second);return;}
 activeDetailKey=key;auto generation=++detailGeneration;auto cached=detailCache.find(key);renderDetail(e,cached==detailCache.end()?nullptr:&cached->second);if(cached!=detailCache.end())return;
 {std::lock_guard<std::mutex> guard(asyncMutex);latestDetail=DetailTask{e,key,generation};}detailWake.notify_one();
}
void startDetailWorkers(){asyncStop=false;
 detailThread=std::thread([]{while(true){DetailTask job;{std::unique_lock<std::mutex> guard(asyncMutex);detailWake.wait(guard,[]{return asyncStop||latestDetail.has_value();});if(asyncStop)break;job=std::move(*latestDetail);latestDetail.reset();}auto cancelled=[&]{return asyncStop||detailGeneration.load()!=job.generation;};if(cancelled())continue;ExtraDetails extra;try{extra.permission=accessHint(job.entry.path);if(cancelled())continue;extra.cache=cachePaths(job.entry,cancelled);if(cancelled())continue;extra.environment=environmentWarning(job.entry.path,job.entry.path);if(job.entry.kind==L"目录检查"&&!cancelled())extra.directory=inspectDirectory(job.entry.path,cancelled);}catch(...){extra={L"查询失败，可重新扫描后重试",L"查询失败",L"查询失败"};}if(!cancelled()){std::lock_guard<std::mutex> guard(asyncMutex);detailReplies.push_back({job.key,job.generation,std::move(extra)});}}});
 iconThread=std::thread([]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);while(true){Entry entry;{std::unique_lock<std::mutex> guard(asyncMutex);iconWake.wait(guard,[]{return asyncStop||!iconTasks.empty();});if(asyncStop)break;entry=std::move(iconTasks.front());iconTasks.pop_front();}HICON icon=nullptr;try{icon=appIcon(entry);}catch(...){}std::lock_guard<std::mutex> guard(asyncMutex);iconReplies.push_back({imageKey(entry),icon});}CoUninitialize();});
}
void stopDetailWorkers(){asyncStop=true;++detailGeneration;detailWake.notify_all();iconWake.notify_all();if(detailThread.joinable())detailThread.join();if(iconThread.joinable())iconThread.join();std::lock_guard<std::mutex> guard(asyncMutex);for(auto&r:iconReplies)if(r.icon)DestroyIcon(r.icon);iconReplies.clear();iconTasks.clear();detailReplies.clear();latestDetail.reset();}
void drainDetailResults(){std::deque<DetailReply> replies;std::deque<IconReply> images;{std::lock_guard<std::mutex> guard(asyncMutex);replies.swap(detailReplies);images.swap(iconReplies);}for(auto&r:images){int index=0;if(r.icon){index=ImageList_AddIcon(icons,r.icon);DestroyIcon(r.icon);}imageCache[r.key]=index;iconsRequested.erase(r.key);}for(auto&r:replies){if(r.generation!=detailGeneration||r.key!=activeDetailKey)continue;detailCache[r.key]=r.extra;int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(selected>=0&&size_t(selected)<visible.size()&&visible[selected]>=0&&size_t(visible[selected])<rows.size()){auto&e=rows[visible[selected]];if(entryKey(e)==r.key)renderDetail(e,&detailCache[r.key]);}}}
void loadAsyncIcons(){if(!list||visible.empty())return;RECT view;GetClientRect(list,&view);size_t checks=std::min<size_t>(visible.size(),96);while(checks--){if(iconCursor>=visible.size())iconCursor=0;int i=(int)iconCursor++;if(visible[i]<0||loadedIcons.count(i))continue;RECT item{};if(!ListView_GetItemRect(list,i,&item,LVIR_BOUNDS)||item.top>view.bottom||item.bottom<0)continue;auto&e=rows[visible[i]];auto key=imageKey(e);auto cached=imageCache.find(key);if(cached!=imageCache.end()){LVITEMW update{};update.mask=LVIF_IMAGE;update.iItem=i;update.iImage=cached->second;ListView_SetItem(list,&update);loadedIcons[i]=cached->second;}else if(!iconsRequested.count(key)){std::lock_guard<std::mutex> guard(asyncMutex);if(iconTasks.size()<32){iconsRequested.insert(key);iconTasks.push_back(e);iconWake.notify_one();}}}}
