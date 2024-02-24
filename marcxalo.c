/************************************************************************
* marc_xallocate ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Ensure that some buffer is large enough for a contemplated      *
*       operation.                                                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to object to check.                                     *
*           Object is itself a pointer.                                 *
*           If it must be reallocated, we replace it.                   *
*       Pointer to pointer after end of object.                         *
*           If we reallocate, we update the pointer to the end also.    *
*           If there is no end pointer, pass NULL.                      *
*       Pointer to current allocation size.                             *
*       New size required.                                              *
*       Size of individual element within object.                       *
*                                                                       *
*   RETURN                                                              *
*       0 = Success                                                     *
*           May mean either that reallocation was successful, or        *
*           that no reallocation was required.                          *
*       Else error.                                                     *
*           If reallocation failed, existing buffer is NOT              *
*           destroyed.                                                  *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcxalo.c,v 1.1 1998/12/28 21:14:37 meyer Exp meyer $
 * $Log: marcxalo.c,v $
 * Revision 1.1  1998/12/28 21:14:37  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

void *marc_realloc(void *, int, int);

int marc_xallocate (
    void   **bufpp,         /* Ptr to object to check   */
    void   **endpp,         /* Ptr to after-end ptr     */
    size_t *bufsizep,       /* Put new size here        */
    size_t new_size,        /* How much we need         */
    size_t elem_size        /* Bytes in one element     */
) {
    void   *srcp;           /* Ptr to new allocation    */

    /* Only do anything if there's no room */
    if (*bufsizep < new_size) {
        /* Try to reallocate whatever it was */

#ifdef DEBUG
        if ((srcp = marc_realloc(*bufpp, new_size * elem_size, 700)) == NULL) { //TAG:700
            return MARC_ERR_ALLOC;
        }
#else
        if ((srcp = realloc (*bufpp, new_size * elem_size)) == NULL) {
            return MARC_ERR_ALLOC;
        }
#endif

        /* Return new pointer information to caller's structure */
        *bufpp = srcp;
        if (endpp)
            *endpp = (void *) ((unsigned char *) srcp + (new_size*elem_size));
        *bufsizep = new_size;
    }

    return 0;

} /* marc_xallocate */
