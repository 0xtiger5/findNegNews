#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <regex>
#include <unistd.h>
#include "/opt/sogou/include/workflow/HttpMessage.h"
#include "/opt/sogou/include/workflow/HttpUtil.h"
#include "/opt/sogou/include/workflow/WFServer.h"
#include "/opt/sogou/include/workflow/WFHttpServer.h"
#include "/opt/sogou/include/workflow/WFFacilities.h"

double judgeEmo(std::string str) {
	std::string command = "curl -s -X POST 'http://baobianapi.pullword.com:9091/get.php' -d'" + str + "' --compressed";
	FILE* fp = popen(command.c_str(), "r");
	char buffer[40] = { 0 };
	if (fp != NULL) {
		fgets(buffer, sizeof(buffer), fp);
	}
	else {
		return -3;
	}
	std::string jsonres(buffer);
	// std::cout << jsonres << std::endl;
	const std::regex pattern("\\{\"result\":(.*)\\}");
	std::smatch result;
	if (std::regex_match(jsonres, result, pattern)) {
		return stod(std::string(result.str(1)));
	}
	return -2;
}

void parse(std::vector<std::vector<std::string>>& ans) {
	const std::regex pattern("<a href=\"(http.*?)\".*>(.{20,})</a>");
	char* filename = "/tmp/get.html";
	std::fstream input_file(filename);
	if (!input_file.is_open()) {
		std::cerr << "Could not open the file - '"
			<< filename << "'" << std::endl;
		exit(EXIT_FAILURE);
	}
	std::stringstream ss;
	ss << input_file.rdbuf();
	const std::string text = ss.str();
	// std::cout << text << std::endl;

	std::smatch result;
	auto begin = text.begin();
	auto end = text.end();
	for (; std::regex_search(begin, end, result, pattern); begin = result.suffix().first) {
		std::vector<std::string> temp;
		//std::cout << result.str() << std::endl;
		temp.push_back(result.str(1));
		temp.push_back(result.str(2));
		ans.push_back(temp);
	}
}

bool getUrl()
{
	char* filename = "/tmp/get.html";
	CURL* curl;
	CURLcode res;
	FILE* fp;
	if ((fp = fopen(filename, "w")) == NULL)  // 返回结果用文件存储
		return false;
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Accept: Agent-007");
	curl = curl_easy_init();    // 初始化
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);   // 改协议头
		curl_easy_setopt(curl, CURLOPT_URL, "http://news.baidu.com");
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);         //将返回的内容输出到fp指向的文件
		res = curl_easy_perform(curl);   // 执行
		if (res != 0) {
			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
		}
		return true;
	}
	fclose(fp);
	return false;
}

std::string genMessage() {
	std::string ans;
	getUrl();
	std::vector<std::vector<std::string>> result;
	parse(result);

	/* ans += "<h1>负面新闻</h1>";
	ans += "来源：<a href=\"https://news.baidu.com/\" target=\"_blank\">百度新闻</a>";
	ans += "<ol>";
	for (auto news : result) {
		double num = judgeEmo(news[1]);
		if (num < -1) {
			std::cout << "an error occurred when judgeEmo()" << std::endl;
		}
		else if (num < -0.5) {
			ans += ("<li><a href=\"" + news[0] + "\" target=\"_blank\">" + news[1] + "</a></li>");
		}
	}
	ans += "</ol>"; */

	ans += "[";
	for (auto news : result) {
		double num = judgeEmo(news[1]);
		if (num < -1) {
			std::cout << "an error occurred when judgeEmo()" << std::endl;
		}
		else if (num < -0.5) {
			ans += ("{title:\"" + news[1] + "\", url:\"" + news[0] + "\"},");
		}
	}
	ans += "]";
	return ans;
}

void process(WFHttpTask* server_task)
{
	protocol::HttpRequest* req = server_task->get_req();
	protocol::HttpResponse* resp = server_task->get_resp();
	long long seq = server_task->get_task_seq();
	protocol::HttpHeaderCursor cursor(req);
	std::string name;
	std::string value;
	char buf[8192];
	int len;

	/* Set response message body. */
	resp->append_output_body_nocopy("<head>", 6);
	resp->append_output_body_nocopy("<title>", 7);
	std::string title = "负面新闻";
	resp->append_output_body(title.c_str(), title.length());
	resp->append_output_body_nocopy("</title>", 8);
	resp->append_output_body_nocopy("</head>", 7);
	resp->append_output_body_nocopy("<html>", 6);
	std::string message = genMessage();
	// std::cout << message.c_str() << std::endl;
	resp->append_output_body(message.c_str(), message.length());
	resp->append_output_body_nocopy("</html>", 7);

	/* Set status line if you like. */
	resp->set_http_version("HTTP/1.1");
	resp->set_status_code("200");
	resp->set_reason_phrase("OK");

	resp->add_header_pair("Content-Type", "text/html; charset=utf-8");
	if (seq == 9) /* no more than 10 requests on the same connection. */
		resp->add_header_pair("Connection", "close");

	/* print some log */
	char addrstr[128];
	struct sockaddr_storage addr;
	socklen_t l = sizeof addr;
	unsigned short port = 0;

	server_task->get_peer_addr((struct sockaddr*)&addr, &l);
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in* sin = (struct sockaddr_in*)&addr;
		inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
		port = ntohs(sin->sin_port);
	}
	else if (addr.ss_family == AF_INET6)
	{
		struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&addr;
		inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
		port = ntohs(sin6->sin6_port);
	}
	else
		strcpy(addrstr, "Unknown");

	fprintf(stderr, "Peer address: %s:%d, seq: %lld.\n",
		addrstr, port, seq);
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}
int main(int argc, char* argv[])
{
	unsigned short port;

	if (argc != 2)
	{
		fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
		exit(1);
	}

	signal(SIGINT, sig_handler);

	WFHttpServer server(process);
	port = atoi(argv[1]);
	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
	{
		perror("Cannot start server");
		exit(1);
	}

	return 0;
}