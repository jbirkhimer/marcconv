/************************************************************************
* marc_set_collate ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Initialize the subfield collation table used for sorting        *
*       subfield output.                                                *
*                                                                       *
*       This is called at marc_init() time to initialize the table      *
*       with its default settings.  See marcdefs.h for the default      *
*       value.                                                          *
*                                                                       *
*       It may be called explicitly to change default collation.        *
*                                                                       *
*       Note that all fields may be protected from subfield sorting     *
*       by calling marc_subfield_sort(), or an individual range of      *
*       subfields may be protected within a single field by calling     *
*       marc_subfield_order().                                          *
*                                                                       *
*   PASS                                                                *
*       Pointer to initialization string containing subfield codes      *
*           in the order in which they should be collated, with         *
*           hyphens for ranges, e.g., "aqzb-pr-y1-9".                   *
*                                                                       *
*           Any subfield code which is not in the passed list will      *
*           automatically sort at the end of the subfields for a        *
*           field.                                                      *
*                                                                       *
*           Multiple occurences of the same subfield sort in entry      *
*           order, first in, first out.                                 *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marccoll.c,v 1.1 1998/12/28 20:30:42 meyer Exp meyer $
 * $Log: marccoll.c,v $
 * Revision 1.1  1998/12/28 20:30:42  meyer
 * Initial revision
 *
 */

#include <string.h>
#include <ctype.h>
#include "marcdefs.h"

#define SF_NONE    255  /* Value for unintialized subfield codes */

int marc_set_collate (
    MARCP mp,           /* Pointer to structure */
    char  *coll_str     /* Collation string     */
) {
    int   i,            /* Loop counter         */
          c,            /* Char in coll_str     */
          order;        /* Collation order      */
    unsigned char
          ttbl[MARC_SFTBL_SIZE]; /* Temp table  */


    /* Initialize entire table to illegal values */
    for (i=0; i<MARC_SFTBL_SIZE; i++)
        ttbl[i] = SF_NONE;

    /* Order all legal values starting from 0 */
    order = 0;

    /* For all values in the passed collation sequence */
    while ((c = *coll_str) != 0) {

        /* Non-printables are not allowed */
        if (!isprint(c))
            return MARC_ERR_SFCODE;

        /* Process mid-values in range */
        if (c == '-') {
            for (i=(*(coll_str-1)+1); i<(*(coll_str+1)); i++) {
                /* If not a duplicate, set collation */
                if (ttbl[i] != SF_NONE)
                    return MARC_ERR_DUP_COLL;
                ttbl[i] = (unsigned char) order++;
            }
        }

        else {
            /* Same processing as in range */
            if (ttbl[c] != SF_NONE)
                return MARC_ERR_DUP_COLL;
            ttbl[c] = (unsigned char) order++;
        }

        /* Next char */
        ++coll_str;
    }

    /* If we got here, we processed everything okay */
    memcpy (mp->sf_collate, ttbl, MARC_SFTBL_SIZE);

    return 0;

} /* marc_set_collate */
