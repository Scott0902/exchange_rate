#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include <wininet.h>

LPWSTR StringToLPWSTR(const char* s)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    LPWSTR buffer = (LPWSTR)malloc(length * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buffer, length);
    return buffer;
}


int display_message_dialog(const char* title, const char* message)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
    LPWSTR wideMessage = (LPWSTR)malloc(size * sizeof(WCHAR));
    LPWSTR title_str = StringToLPWSTR(title);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wideMessage, size);
    MessageBoxW(NULL, wideMessage, title_str, MB_ICONINFORMATION);
    free(wideMessage);
    free(title_str);
    return 0;
}

char* send_request(char* method, char* url, char* host_name, char* path_name, bool verbose)
{
    char* error_msg;
    HINTERNET Request;
    HINTERNET Connect;
    HINTERNET Session;
    Session = InternetOpenW(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/110.0.0.0 Safari/537.36", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!Session)
        {printf("InternetOpen() Error: %lu\n", GetLastError()); return NULL;}

    DWORD BytesRead;
    DWORD Flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    DWORD Timeout = 10000;
    bool is_https = (strncmp(url, "https://", 8) == 0);
    DWORD service_port = is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    
    InternetSetOption(Session, INTERNET_OPTION_RECEIVE_TIMEOUT, &Timeout, sizeof(DWORD));
    InternetSetOption(Session, INTERNET_OPTION_SEND_TIMEOUT, &Timeout, sizeof(DWORD));

    Connect = InternetConnect(Session, host_name, service_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!Connect)
        {sprintf(error_msg, "`InternetConnect` Error (%lu)", GetLastError());return error_msg;}
    
    InternetSetOption(Connect, 208, &Timeout, sizeof(DWORD));
    Request = HttpOpenRequest(Connect, method, path_name, NULL, NULL, NULL, Flags | (is_https ? INTERNET_FLAG_SECURE : 0), 0);
    if (!Request)
        {sprintf(error_msg, "`HttpOpenRequest` Error (%lu)", GetLastError());InternetCloseHandle(Connect);InternetCloseHandle(Session);return error_msg;}

    if (verbose){
        DWORD header_buffer_size = 4096;
        DWORD index = 0;
        char *header_buffer = (char *)malloc(header_buffer_size);
        HttpQueryInfo(Request, HTTP_QUERY_RAW_HEADERS_CRLF|HTTP_QUERY_FLAG_REQUEST_HEADERS, header_buffer, &header_buffer_size, &index);
        printf("Headers after HttpAddRequestHeaders:\n%s\n\n", header_buffer);
        free(header_buffer);
    }

    if (!HttpSendRequest(Request, NULL, 0, NULL, 0))
        {sprintf(error_msg, "`HttpSendRequestA` Error (%lu)", GetLastError());InternetCloseHandle(Request);InternetCloseHandle(Connect);InternetCloseHandle(Session);return error_msg;}

    char* htmltext = NULL;
    char* buffer = malloc(sizeof(char) * 4096);
    size_t buffer_size = 4096;
    size_t html_size = 0;

    while (InternetReadFile(Request, buffer, buffer_size, &BytesRead) && BytesRead) {
        buffer[BytesRead] = '\0';
        html_size += BytesRead;
        if (htmltext == NULL) {
            htmltext = malloc(sizeof(char) * (html_size + 1));
            strcpy(htmltext, buffer);
        } else {
            htmltext = realloc(htmltext, sizeof(char) * (html_size + 1));
            strcat(htmltext, buffer);
        }
    }
    free(buffer);

    if (verbose){
        DWORD header_buffer_size = 4096;
        char* header_buffer = (char*)malloc(header_buffer_size);
        if (HttpQueryInfo(Request, HTTP_QUERY_RAW_HEADERS_CRLF|HTTP_QUERY_FLAG_REQUEST_HEADERS, header_buffer, &header_buffer_size, NULL)) {
            printf("Headers after sending request:\n%s\n", header_buffer);
        } else {
            printf("failed retrieval of headers\n");
        }
        free(header_buffer);
    }

    InternetCloseHandle(Request);
    InternetCloseHandle(Connect);
    InternetCloseHandle(Session);
    return htmltext;
}


char* find_between(const char* string, const char* from_where, const char* start_str, const char* end_str, int* position) {
    const char* t1;
    if (from_where != NULL) {
        t1 = strstr(string + *position, from_where);
    } else {
        t1 = string + *position;
    }
    if (t1 == NULL) return "";
    char* t2 = strstr(t1 + (from_where ? strlen(from_where) : 0), start_str);
    if (t2 == NULL) return "";
    char* t3 = strstr(t2 + strlen(start_str), end_str);
    if (t3 == NULL) return "";
    size_t resultLen = t3 - (t2 + strlen(start_str));
    char* result = (char*)malloc((resultLen + 1) * sizeof(char));
    strncpy(result, t2 + strlen(start_str), resultLen);
    *position = (int)(t2 - string) + resultLen + strlen(end_str) - 1;
    // 2023-6-5新增：
    if (from_where != NULL) {
        *position += strlen(start_str);
    }
    // 新增结束
    result[resultLen] = '\0';
    return result;
}


int main() {
    char* out_str = malloc(sizeof(char)*1024);
    char* htmltext = send_request("GET", "https://www.boc.cn/sourcedb/whpj/", "www.boc.cn", "/sourcedb/whpj/", 0);
    if (htmltext == NULL) {
        printf("获取HTML源代码分配缓冲错误\n");
        return 0;
    }
    strcpy(out_str, "币种\t现汇\t现钞\t现汇\t现钞\t中行\n\t买入价\t买入价\t卖出价\t卖出价\t折算价\n—————————————————————————\n");
    char* currencies[] = {"美元", "港币", "欧元", "英镑", "卢布", "日元"};
    char my_str[32], temp[100];
    int i, j, position, length = sizeof(currencies) / sizeof(currencies[0]);
    for (i=0; i<length; i++)
    {
        position = 0;
        sprintf(temp, "<td>%s</td>", currencies[i]);
        strcpy(my_str, find_between(htmltext, temp, "<td>", "</td>", &position));
        sprintf(temp, "%s：\t%s\t", currencies[i], my_str);
        strcat(out_str, temp);
        for (j=0; j<4; j++)
        {
            strcpy(my_str, find_between(htmltext, "", "<td>", "</td>", &position));
            sprintf(temp, "%s\t", my_str);
            strcat(out_str, temp);
        }
        strcat(out_str, "\n");
    }
    display_message_dialog("今日汇率", out_str);
    free(htmltext);
    free(out_str);
    return 0;
}

