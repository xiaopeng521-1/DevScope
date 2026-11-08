#include "main.cpp"
#include <cassert>
int main(){assert(inside(L"C:\\Tools\\app.exe",L"C:\\Tools"));assert(!inside(L"C:\\ToolsOther\\app.exe",L"C:\\Tools"));assert(bytes(1024)==L"1.0 KB");assert(accessHint(L"Z:\\this-path-does-not-exist-1234")==L"未知 / 路径不可访问");return 0;}
