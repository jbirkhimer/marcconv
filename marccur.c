/************************************************************************
* marc_cur_...                                                          *
*                                                                       *
*   These functions return information about the current contents       *
*   of a record.                                                        *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marccur.c,v 1.4 1999/10/01 17:03:14 meyer Exp $
 * $Log: marccur.c,v $
 * Revision 1.4  1999/10/01 17:03:14  meyer
 * Fixed off by one error computing field occ.
 *
 * Revision 1.3  1999/02/14 19:44:09  meyer
 * Fixed bug in marc_get_subfield
 *
 * Revision 1.2  1998/12/31 19:17:17  meyer
 * Fixed bug, updating occp instead of occ.
 *
 * Revision 1.1  1998/12/28 20:31:28  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

/************************************************************************
* marc_cur_field_count ()                                               *
*                                                                       *
*   DEFINITION                                                          *
*       Returns the number of fields in a record.                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to return count.                               *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_cur_field_count (
    MARCP mp,       /* Pointer to structure     */
    int   *countp   /* Put count here           */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    *countp = mp->field_count;

    return 0;

} /* marc_cur_field_count */


/************************************************************************
* marc_cur_subfield_count ()                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Returns the number of subfields in the current field.           *
*                                                                       *
*       Only works for variable length fields.                          *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to return count.                               *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

#include "marcdefs.h"

int marc_cur_subfield_count (
    MARCP mp,       /* Pointer to structure     */
    int   *countp   /* Put count here           */
) {
    int   stat;     /* Return from marc_xsfdir()*/


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Ensure subfield directory is up to date */
    if ((stat = marc_xsfdir (mp)) == 0)
        *countp = mp->sf_count;

    return (stat);

} /* marc_cur_sf_count */


/************************************************************************
* marc_cur_field_len ()                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Returns the length of data in the current field, as it          *
*       would appear in the directory.                                  *
*                                                                       *
*       This length includes any indicators, subfield codes and         *
*       delimiters, and the terminating field delimiter.                *
*                                                                       *
*       Returns an error if there is no current field.                  *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to return length.                              *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_cur_field_len (
    MARCP  mp,      /* Pointer to structure */
    size_t *lenp    /* Put length here      */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Point to current field, if there is one */
    if (mp->cur_field > 0) {
        *lenp = (mp->fdirp + mp->cur_field)->len + 1;
        return 0;
    }

    return MARC_ERR_LEN_FLDPOS;

} /* marc_cur_field_len */


/************************************************************************
* marc_cur_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Returns the identifier and position of the current field.       *
*                                                                       *
*       All positional information is origin 0.                         *
*                                                                       *
*       Treats the leader as field 0, occurrence 0, position 0.         *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to put current field tag.                      *
*       Pointer to place to put current field occurrence number.        *
*       Pointer to place to put current field ordinal position.         *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           All return values set to -1.                                *
************************************************************************/

#include "marcdefs.h"

int marc_cur_field (
    MARCP mp,       /* Pointer to structure     */
    int   *tagp,    /* Put field tag/id here    */
    int   *occp,    /* Put field occurrence here*/
    int   *posp     /* Put field position here  */
) {
    int   i,        /* Loop counter             */
          id,       /* Local tag/id             */
          occ,      /* Local occurrence         */
          pos;      /* Local position           */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Info */
    if (mp->cur_field >= 0) {

        /* ID and position are known immediately */
        id  = (mp->fdirp + mp->cur_field)->tag;
        pos = mp->cur_field;

        /* Count occurrences from start of field
         * Counts up to, but not including, the current field
         * Thus if 0 found, this will be the 0th occurrence.  It's
         *   a way of adjusting to origin 0.
         */
        occ = 0;
        for (i=0; i<pos; i++)
            if (mp->fdirp[i].tag == id)
                    ++occ;

        /* Info to caller */
        *tagp = id;
        *occp = occ;
        *posp = pos;

        return 0;
    }

    /* No position established */
    *tagp =
    *occp =
    *posp = -1;

    return MARC_ERR_CUR_FIELD;

} /* marc_cur_field */


/************************************************************************
* marc_cur_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Returns the identifier and position of the current subfield     *
*       within the current field.                                       *
*                                                                       *
*       All positional info is origin 0.                                *
*                                                                       *
*       Returns errors if there is no current field, or if it is        *
*       not a variable length field - see marc_xsfdir().                *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to put current subfield code/id.               *
*       Pointer to place to put current subfield occurrence number.     *
*       Pointer to place to put current subfield ordinal position.      *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_cur_subfield (
    MARCP mp,       /* Pointer to structure     */
    int   *idp,     /* Put sf code/id here      */
    int   *occp,    /* Put sf occurrence here   */
    int   *posp     /* Put sf position here     */
) {
    int   i,        /* Loop counter             */
          id,       /* Local tag/id             */
          occ,      /* Local occurrence         */
          pos,      /* Local position           */
          stat;     /* Return from marc_xsfdir()*/


    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Insure varlen field and subfield directory is current */
    if ((stat = marc_xsfdir (mp)) == 0) {

        /* Info */
        id  = (mp->sdirp + mp->cur_sf)->id;
        pos = mp->cur_sf;

        /* Count occurrences from start of subfield
         * Counts up to, but not including, the current subfield
         * Thus if 0 found, this will be the 0th occurrence.  It's
         *   a way of adjusting to origin 0.
         */
        occ = 0;
        for (i=0; i<pos; i++)
            if (mp->sdirp[i].id == id)
                ++occ;

        /* Return values */
        *idp  = id;
        *occp = occ;
        *posp = pos;

        return 0;
    }

    /* No position established */
    *idp  =
    *occp =
    *posp = -1;

    return (stat);

} /* marc_cur_subfield */


/************************************************************************
* marc_cur_item ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Package combination of marc_cur_field() and marc_cur_subfield().*
*                                                                       *
*       All occurrence/position info is origin 0.                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Pointer to place to put current field tag.                      *
*       Pointer to place to put current field occurrence number.        *
*       Pointer to place to put current field ordinal position.         *
*       Pointer to place to put current subfield code/id.               *
*       Pointer to place to put current subfield occurrence number.     *
*       Pointer to place to put current subfield ordinal position.      *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_cur_item (
    MARCP mp,       /* Pointer to structure     */
    int   *tagp,    /* Put field tag here       */
    int   *foccp,   /* Put field occ here       */
    int   *fposp,   /* Put field position here  */
    int   *idp,     /* Put sf code/id here      */
    int   *soccp,   /* Put sf occ here          */
    int   *sposp    /* Put sf position here     */
) {
    int   stat;     /* Return from marc_ funcs  */


    /* Get field info */
    if ((stat = marc_cur_field (mp, tagp, foccp, fposp)) == 0) {

        /* Only get subfield info if we are on a subfield */
        if (*tagp >= MARC_FIRST_VARFIELD)
            stat = marc_cur_subfield (mp, idp, soccp, sposp);

        /* Else set subfields to invalid ids */
        else {
            *idp   = MARC_NO_SUBFIELD;
            *soccp = MARC_NO_SUBFIELD;
            *sposp = MARC_NO_SUBFIELD;
        }
    }

    return (stat);

} /* marc_cur_item */
