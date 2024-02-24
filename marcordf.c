/************************************************************************
* marc_field_sort ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Turn field sorting on or off for all records controlled by      *
*       a given marcctl structure.  If sorting is on then, by default,  *
*       fields are output in numeric field tag order.  If off,          *
*       fields are output in the order entered.                         *
*                                                                       *
*       Sort orders may be changed by using marc_field_order(),         *
*       see below.                                                      *
*                                                                       *
*       Note: Sorting is only done during marc_get_record()             *
*       processing when fields are sorted for output into an            *
*       actual marc record.  Fields are never sorted in the             *
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

/* $Id: marcordf.c,v 1.1 1998/12/28 21:07:46 meyer Exp meyer $
 * $Log: marcordf.c,v $
 * Revision 1.1  1998/12/28 21:07:46  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_field_sort (
    MARCP mp,       /* Pointer to structure */
    int   enable    /* True=Enable sorting  */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    mp->field_sort = (enable) ? 1 : 0;

    return 0;

} /* marc_field_sort */


/************************************************************************
* marc_field_order ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Specify that fields in a certain range are protected from       *
*       sorting.                                                        *
*                                                                       *
*       This function only influences sorting when field sorting is     *
*       enabled.  See marc_field_sort() above.                          *
*                                                                       *
*       Normally, fields are output in numeric tag order.  However      *
*       the caller can specify that fields in a certain range are       *
*       not to be sorted into numeric tag order.  For example, in       *
*       authority records, fields 400-599 can be ordered by the         *
*       cataloger.                                                      *
*                                                                       *
*       marc_field_order() specifies a range of marc tags which are     *
*       to be protected from sorting.  If 400-599 are protected         *
*       then fields will be sorted as in the following example:         *
*                                                                       *
*       Input:                                                          *
*         400, 500, 450, 001, 008, 005, 670, 050                        *
*       Output:                                                         *
*         001, 005, 008, 050, 400, 500, 450, 670                        *
*                                                                       *
*       marc_field_order() sets the protected range for ALL subsequent  *
*       operations using the passed control structure.  If the          *
*       control structure is only used for one type of record, then     *
*       marc_field_order() need only be called once.                    *
*                                                                       *
*       To turn off sort protection, pass MARC_NO_ORDER for both        *
*       ends of the protected range.                                    *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Starting field tag in protected range, or MARC_NO_ORDER         *
*       Ending field tag number in protected range, or MARC_NO_ORDER.   *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

int marc_field_order (
    MARCP mp,           /* Pointer to structure         */
    int   start_tag,    /* First tag in protect range   */
    int   end_tag       /* Ending tag in protect range  */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* If either end of the range is no order, both must be */
    if (start_tag == MARC_NO_ORDER || end_tag == MARC_NO_ORDER) {
        if (!(start_tag == MARC_NO_ORDER && end_tag == MARC_NO_ORDER))
            return MARC_ERR_BAD_ORDER;
    }

    /* If not no order, then can only re-order variable fields */
    else if ( (start_tag < MARC_FIRST_VARFIELD) ||
              (end_tag > MARC_MAX_FIELDID) ||
              (start_tag >= end_tag) )
        return MARC_ERR_BAD_ORDER;

    /* Save requested range */
    mp->start_prot = start_tag;
    mp->end_prot   = end_tag;

    return 0;

} /* marc_field_order */
