#include "MyProxy.h"
#include <tchar.h>

int _tmain(int argc, _TCHAR* argv[])
{
	setlocale(LC_ALL, "");
	vector<string> blackList;
	blackList.push_back("bbs.3dmgame.com");
	unordered_map<string, string> redirect;
	redirect.insert(unordered_map<string, string>::value_type("jwc.hit.edu.cn", "today.hit.edu.cn"));
	cout << "╔════════════════════════════════════════════╗" << endl;
	cout << "║                               HTTP代理服务器的简单实现                                 ║" << endl;
	cout << "║                                                                                        ║" << endl;
	cout << "║                                                                           作者：率怀一 ║" << endl;
	cout << "║                                                                           2017年5月6日 ║" << endl;
	cout << "╚════════════════════════════════════════════╝" << endl;
	MyProxy my_proxy(10240, blackList, redirect);
	return 0;
}
