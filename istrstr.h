/* Header for case insensitive string searches, like strstr().
 *
 * Source file is istrstr.c, currently in \re\src\misc
 */

#if defined __cplusplus
extern "C" {
#endif

char *istrstr (const char *datap, const char *srchp);
char *nstrstr (const char *datap, const char *srchp, const size_t datalen);
char *instrstr (const char *datap, const char *srchp, const size_t datalen);

#if defined __cplusplus
}
#endif

