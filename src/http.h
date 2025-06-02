#ifndef INCLUDED_HTTP_H
#define INCLUDED_HTTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Structure to store HTTP response data */
typedef struct {
    char *data;
    size_t size;
} http_response_t;

/* Function to make an HTTP request using libcurl
   url: target URL
   post_data: if not NULL, the request will be a POST with this data
   response: buffer to store the response (will be allocated)
   Returns: TRUE on success, FALSE on failure
   
   The caller is responsible for freeing the response data
   with free(response->data) when done with it.
*/
bool make_http_request(const char *url, const char *post_data, http_response_t *response);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_HTTP_H */
