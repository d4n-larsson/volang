/* Plain HTTP for std.http (spec 007 US5, research R5; ported from src/net.s, spec 008):
 * __http_exchange(host, port, request). */
#include "vo.h"

/* result[str]: the raw response, or err with the reason. */
Variant *io_http_exchange(Str *host, long port, Str *request)
{
    (void)host;
    (void)port;
    (void)request;
    return make_result(1, (long)str_from_cstr("network access is not available"));
}
