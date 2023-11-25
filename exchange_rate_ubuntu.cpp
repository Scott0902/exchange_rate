#include <iostream>
#include <string>
#include <regex>
#include <curl/curl.h>
#include <vector>
using namespace std;

// Define a callback function to be called by curl when data is received
size_t curlWriteCallback(char* buf, size_t size, size_t nmemb, string* str)
{
    str->append(buf, size * nmemb);
    return size * nmemb;
}

// GET
int requests_get(string url, string cookies_file, string& htmlText, int timeout)
{
    htmlText = "";
    CURLcode res;
    CURL* curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init(); 
    if (!curl) {
        cout << "初始化CURL失败" << endl;
        curl_global_cleanup();
        return 1;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept-Encoding: utf-8");
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; rv:11.0) like Gecko");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);    // headers的设置只需一次，后续的requests不需要重复设置
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);     // 允许redirect
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookies_file.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &htmlText);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return 1;
    }else {
        return 0;
    }
}

int get_rates_now(vector<string> currencies)
{
    string cookies_file, htmlText, result1, s2;
    int ret = requests_get("https://www.boc.cn/sourcedb/whpj", cookies_file, htmlText, 10);
    if(ret) {
    	cout<< "Opening website error." <<endl;
        curl_global_cleanup();
        return 1;
    }
    cout<<"今天汇率：\n币种\t现汇买入价\t现钞买入价\t现汇卖出价\t现钞卖出价\t中行折算价\n"<<string (85, '-')<<endl;
    smatch match;
    for (string i : currencies)
    {
        cout << i << "：\t";
        if (regex_search(htmlText, match, regex("<td>"+i+"</td>([^]*?)<td class=\"pjrq\">")))
        {
            result1 = match[1].str();
            auto search_start = result1.cbegin();
            while (regex_search(search_start, result1.cend(), match, regex("\\d.+\\d+")))
            {
                cout << match[0].str()+"\t\t";
                search_start = match.suffix().first;
            }
            cout << endl;
        }
    }
    return 0;
}


int main() {
    string htmlText;
    vector<string> currencies = {"美元", "港币", "欧元", "英镑", "卢布", "日元"};
    get_rates_now(currencies);
    return 0;
}
