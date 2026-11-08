#include "main.cpp"
#include <cassert>
#include <iostream>
int main(){
 fs::path root=LR"(C:\Program Files (x86)\Wukong)";
 assert(serviceBelongsTo(LR"("C:\Program Files (x86)\Wukong\deepscan\zhudongfangyu.exe" -service)",root));
 assert(serviceBelongsTo(LR"(C:\Program Files (x86)\Wukong\safemon\wscreg.exe)",root));
 assert(!serviceBelongsTo(LR"("C:\Program Files (x86)\WukongOther\service.exe")",root));
 assert(!serviceBelongsTo(LR"(C:\Windows\System32\svchost.exe -k Shared)",root));
 assert(!serviceBelongsTo(LR"(C:\Program Files (x86)\Wukong\..\Other\service.exe)",root));
 assert(!serviceBelongsTo(LR"(C:\Windows\System32\drivers\360FsFlt.sys)",root));
 assert(!serviceBelongsTo(LR"(service.exe)",root));
 std::cout<<"PASS: exact installation service association; excludes shared hosts, sibling directories, traversal and drivers. No services changed.\n";
}
