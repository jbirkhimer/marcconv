/************************************************************************
* marc_dup ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Create a duplicate marcctl structure, set to read only.         *
*                                                                       *
*       Used by conversion programs which want to provide read only     *
*       access to a record.                                             *
*                                                                       *
*   PASS                                                                *
*       Pointer to marcctl structure to duplicate.                      *
*       Pointer to place to put pointer to copy of structure.           *
*           If initial value pointed to is NULL, then allocated         *
*               a new structure.                                        *
*           Else copy into the object pointed to.                       *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcdup.c,v 1.1 1998/12/28 20:31:54 meyer Exp meyer $
 * $Log: marcdup.c,v $
 * Revision 1.1  1998/12/28 20:31:54  meyer
 * Initial revision
 *
 */

#include <string.h>
#include "marcdefs.h"

void *marc_alloc(int, int);

int marc_dup (
    MARCP mp,       /* Pointer to structure     */
    MARCP *dpp      /* Pointer to ptr to copy   */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* If structure exists, make sure it is a marcctl */
    if (*dpp) {
        if (marc_xcheck (*dpp))
            return MARC_ERR_NON_DUP;
    }

    else {
        /* Create a new structure */
#ifdef DEBUG
        if ((*dpp = (MARCCTL *) marc_alloc(sizeof(MARCCTL), 300)) == NULL) {   //TAG:300
            return MARC_ERR_ALLOC;
        }
#else
        if ((*dpp = (MARCCTL *) malloc (sizeof(MARCCTL))) == NULL) {
            return MARC_ERR_ALLOC;
        }
#endif
    }

    /* Duplicate as requested */
    memcpy (*dpp, mp, sizeof(MARCCTL));

    /* Make copy read only */
    (*dpp)->read_only = 1;

    return 0;

} /* marc_dup */
