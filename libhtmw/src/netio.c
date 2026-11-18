#include "netio.h"

typedef struct {
    char* data;
    size_t size;
} block;

static size_t write_mem_cb(void *contents, size_t size, size_t nmemb, block *userp) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(userp->data, userp->size + realsize + 1);
    
    if (!ptr) {
        // Out of memory
        //printf("Not enough memory (realloc returned NULL)\n");
        printf("[HTMW Network Fatal]: Re-allocation error\n");
        exit(1);
        return 0;
    }
    
    userp->data = ptr;
    memcpy(&(userp->data[userp->size]), contents, realsize);
    userp->size += realsize;
    userp->data[userp->size] = '\0'; // Null terminate
    
    return realsize;
}

static size_t net_read_to_string(const char *url, char **out) {
    CURL* handle;
    CURLcode x;
    block chunk;

    chunk.data = (char*)malloc(sizeof(char));
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    handle = curl_easy_init();

    if (handle == NULL) {
        printf("[HTMW Network Fatal]: CURL handle init error\n");
        exit(1);
    }

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_mem_cb);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    x = curl_easy_perform(handle);

    if (x != CURLE_OK) {
        curl_easy_cleanup(handle);
        curl_global_cleanup();
        printf("[HTMW Network Fatal]: CURL was not OK\n");
        exit(1);
    }

    long rc;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &rc);



    curl_easy_cleanup(handle);
    curl_global_cleanup();

    *out = chunk.data;

    //puts(chunk.data);

    if (!(rc >= 200 && rc < 300)) {
        printf("[HTMW Warning]: Response code (%ld) for \"%s\"\n", rc, url);
    }

    //printf("[HTMW Error]: Unexpected response code (%ld) for \"%s\"\n", rc, url);
    //exit(1);
    return chunk.size;
}

string read_file_network(const char* url) {
    char* data = NULL;
    size_t size = net_read_to_string(url, &data);

    if (!size) {
        printf("[HTMW Network Fatal]: Unexpected size set to zero\n");
        exit(1);
    }

    string out = str_new(size + 1);
#if WCHAR_ENABLED
    mbstowcs(out.c_str, data, size);
#else
    for (size_t i = 0; i < size; i++)
        out.c_str[i] = data[i];
#endif
    out.length = size;

    return out;
}