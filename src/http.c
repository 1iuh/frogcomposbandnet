#include "angband.h"
#include "http.h"

#ifdef USE_CURL
#include <curl/curl.h>

/* Callback function for processing HTTP response data */
static size_t _write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    http_response_t *mem = (http_response_t *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        /* Out of memory */
        msg_print("<color:r>Error: Not enough memory for HTTP response</color>");
        return 0;
    }
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;  /* Null-terminate the string */
    
    return realsize;
}

/* Function to make an HTTP request using libcurl */
bool make_http_request(const char *url, const char *post_data, http_response_t *response)
{
    CURL *curl;
    CURLcode res;
    bool success = FALSE;
    
    if (!response) return FALSE;
    
    /* Initialize response struct */
    response->data = malloc(1);
    response->size = 0;
    
    if (!response->data) return FALSE;
    
    /* In case this is the first time, initialize libcurl globally */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    /* Get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        /* Set the URL */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        /* If a POST request, set the data */
        if (post_data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        }
        
        /* Send all data to this function */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_callback);
        
        /* Pass our response struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
        
        /* Set a reasonable timeout */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
        
        /* Set a user agent */
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Frogcomposband/1.0");
        
        /* Follow redirects */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        /* Perform the request */
        res = curl_easy_perform(curl);
        
        /* Check for errors */
        if (res != CURLE_OK) {
            msg_format("<color:r>Error: HTTP request failed: %s</color>", curl_easy_strerror(res));
            if (response->data) {
                free(response->data);
                response->data = NULL;
                response->size = 0;
            }
        } else {
            success = TRUE;
        }
        
        /* Cleanup */
        curl_easy_cleanup(curl);
    }
    
    /* Always cleanup global initialization */
    curl_global_cleanup();
    
    return success;
}

#else /* USE_CURL */

/* Stub implementation when libcurl is not available */
bool make_http_request(const char *url, const char *post_data, http_response_t *response)
{
    msg_print("<color:r>Network features are not available. libcurl was not found during compilation.</color>");
    
    if (response) {
        response->data = NULL;
        response->size = 0;
    }
    
    return FALSE;
}

#endif /* USE_CURL */
