#include "main.cpp"
#include <cassert>
#include <chrono>
#include <iostream>
int main(){
 Game2048 g;g.cells={2,2,2,2};assert(g.move(3,false));assert(g.cells[0]==4&&g.cells[1]==4&&g.cells[2]==0&&g.score==8);
 g.cells={2,0,0,0};assert(!g.move(3,false));assert(g.move(1,false)&&g.cells[3]==2);assert(g.move(2,false)&&g.cells[15]==2);assert(g.move(0,false)&&g.cells[3]==2);
 g.cells={2,4,2,4,4,2,4,2,2,4,2,4,4,2,4,2};assert(g.over());g.cells[0]=4;assert(!g.over());g.cells[0]=2048;assert(g.won());
 assert(!shouldOfferGame(true,false,false,29999));assert(shouldOfferGame(true,false,false,30000));assert(!shouldOfferGame(true,false,true,60000));assert(!shouldOfferGame(false,false,false,60000));assert(!shouldOfferGame(true,true,false,60000));
 auto root=fs::current_path()/L"scan_speed_fixture";assert(!fs::exists(root));fs::create_directory(root);
 for(int d=0;d<60;d++){auto folder=root/std::to_wstring(d);fs::create_directory(folder);for(int f=0;f<40;f++)std::ofstream(folder/(std::to_wstring(f)+(f?L".txt":L".exe")))<<"fixture";}
 fs::create_directories(root/L"jdk17"/L"bin");std::ofstream(root/L"jdk17"/L"release")<<"JAVA_VERSION=\"17.0.8\"\n";std::ofstream(root/L"jdk17"/L"bin"/L"java.exe")<<"fixture";std::ofstream(root/L"jdk17"/L"bin"/L"javac.exe")<<"fixture";
 auto start=std::chrono::steady_clock::now();size_t oldCount=0;for(int pass=0;pass<5;pass++)for(auto&entry:fs::recursive_directory_iterator(root)){auto a=GetFileAttributesW(entry.path().c_str());if(!(a&FILE_ATTRIBUTE_DIRECTORY)&&lower(entry.path().extension().wstring())==L".exe")oldCount++;}auto oldTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();assert(oldCount==62*5);
 start=std::chrono::steady_clock::now();for(int pass=0;pass<5;pass++){std::vector<Entry> entries;published=0;pendingRows.clear();deepScan(entries,{root});assert(entries.size()==63);assert(std::any_of(entries.begin(),entries.end(),[](auto&e){return e.name==L"JDK 17"&&e.version==L"17.0.8";}));}auto newTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 cancelScan=true;std::vector<Entry> cancelled;deepScan(cancelled,{root});assert(cancelled.empty());cancelScan=false;
 unsigned long long removed=0,failed=0;removeTree(root,removed,failed);assert(!failed&&!fs::exists(root));
 std::cout<<"PASS: game, prompt threshold, discovery, JDK, cancellation\nFixture (5 warm passes): old="<<oldTime<<"ms native="<<newTime<<"ms\n";return 0;
}
