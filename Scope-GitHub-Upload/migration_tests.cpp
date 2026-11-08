#include "main.cpp"
#include <cassert>
int main(){
 auto root=fs::current_path()/L"migration_test_fixture";assert(!fs::exists(root));fs::create_directory(root);
 fs::path old=LR"(C:\Tools\jdk8)",target=LR"(D:\Dev\jdk8)";
 {std::ofstream f(root/L"sample.ini");f<<"java=C:\\Tools\\jdk8\\bin\\java.exe\nother=C:\\Tools\\jdk80\n";}
 {std::ofstream f(root/L"binary.exe");f<<"C:\\Tools\\jdk8";}
 auto plan=referencePlan(root,old,target);assert(plan.size()==1);assert(plan[0].after.find("D:\\Dev\\jdk8\\bin\\java.exe")!=std::string::npos);assert(plan[0].after.find("C:\\Tools\\jdk80")!=std::string::npos);
 std::ifstream f(root/L"sample.ini",std::ios::binary);std::string text((std::istreambuf_iterator<char>(f)),{});assert(text==plan[0].before);f.close();
 assert(!removable(LR"(C:\Program Files (x86)\Common Files\Oracle\Java\java8path\java.exe)"));
 unsigned long long removed=0,failed=0;removeTree(root,removed,failed);assert(!failed);return 0;
}

