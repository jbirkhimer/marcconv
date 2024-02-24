/************************************************************************
* marc_subfield_sort ()                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Turn subfield sorting on or off for all records controlled by   *
*       a given marcctl structure.  If sorting is on then, by default,  *
*       subfields are output in the order given in the subfield         *
*       collation sequence.                                             *
*                                                                       *
*       Sort orders may be changed by using marc_subfield_order(),      *
*       see below.                                                      *
*                                                                       *
*       Note: Sorting is only done during marc_get_record()             *
*       processing when subfields are sorted for output into an         *
*       actual marc record.  Subfields are never sorted in the          *
*       internal input buffers.                                         *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Enable sorting indicator:                                       *
*           True (non-zero) = Sorting enabled (the default).            *
*           False (zero)    = Sorting disabled.                         *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcords.c,v 1.1 1998/12/28 21:08:43 meyer Exp meyer $
 * $Log: marcords.c,v $
 * Revision 1.1  1998/12/28 21:08:43  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_subfield_sort (
    MARCP mp,       /* Pointer to structure */
    int   enable    /* True=Enable sorting  */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    mp->sf_sort = (enable) ? 1 : 0;

    return 0;

} /* marc_subfield_sort */


/************************************************************************
* marc_subfield_order ()                                                *
*                                                                       *
*   DEFINITION                                                          *
*       Specify that subfields in a certain range are protected from    *
*       sorting.                                                        *
*                                                                       *
*       This function only influences sorting when subfield sorting is  *
*       enabled.  See marc_subfield_sort() above.                       *
*                                                                       *
*       By default, subfields are output in the order:                  *
*           a,q,z,b..p,r..y,1..9                                        *
*                                                                       *
*       If subfields have been added in a different order, they will    *
*       be sorted on output.  For example:                              *
*                                                                       *
*       Input:                                                          *
*           2, a, b, c, b, c, d                                         *
*       Output:                                                         *
*           a, b, b, c, c, d, 2                                         *
*                                                                       *
*       If some fields are to be output in caller specified order,      *
*       the calling program must use marc_subfield order to protect     *
*       a range of subfields.  For example, after:                      *
*                                                                       *
*           marc_subfield_order (mp, 'b', 'c');                         *
*       Output:                                                         *
*           a, b, c, b, c, d, 2                                         *
*                                                                       *
*       To turn off sort protection, pass MARC_NO_ORDER for both        *
*       ends of the protected range.                                    *
*                                                                       *
*       Sorting may be enabled or disabled for the current field only,  *
*       all fields in the current record, or for all records controlled *
*       by this marcctl pointer.                                        *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Starting subfield tag in protected range, or MARC_NO_ORDER.     *
*       Ending subfield tag number in protected range, or MARC_NO_ORDER.*
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

#include "marcdefs.h"

int marc_subfield_order (
    MARCP mp,           /* Pointer to structure         */
    int   start_tag,    /* First tag in protect range   */
    int   end_tag       /* Ending tag in protect range  */
) {
    /* Cast to mp and test, return if failed */
    MARC_XCHECK(mp)

    /* If either end of the range is no order, both must be */
    if (start_tag == MARC_NO_ORDER || end_tag == MARC_NO_ORDER) {
        if (!(start_tag == MARC_NO_ORDER && end_tag == MARC_NO_ORDER))
            return MARC_ERR_BAD_ORDER;
    }

    /* If not no order, then can only re-order valid subfields */
    else if ( (start_tag < '1') || (end_tag > 'z') )
        return MARC_ERR_BAD_ORDER;

    /* Save requested range */
    mp->fdirp[mp->cur_field].startprot = (unsigned char) start_tag;
    mp->fdirp[mp->cur_field].endprot   = (unsigned char) end_tag;

    return 0;

} /* marc_subfield_order */
