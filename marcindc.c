/************************************************************************
* marc_set_indic ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Set an indicator for the current field.                         *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Indicator number, 1 or 2.                                       *
*       Indicator value - printable ASCII character.                    *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcindc.c,v 1.1 1998/12/28 21:01:58 meyer Exp meyer $
 * $Log: marcindc.c,v $
 * Revision 1.1  1998/12/28 21:01:58  meyer
 * Initial revision
 *
 */

#include <ctype.h>
#include "marcdefs.h"

int marc_set_indic (
    MARCP mp,       /* Pointer to structure     */
    int   indic,    /* Indicator number, 1 or 2 */
    unsigned char
          value     /* Indicator value          */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Other parameters in range? */
    if (indic != 1 && indic != 2)
        return MARC_ERR_BAD_INDIC;
    if (!isprint(value))
        return MARC_ERR_INDIC_VAL;

    /* Are we positioned on a variable length field? */
    if (mp->cur_field < 1 || MARC_CUR_TAG(mp) < MARC_FIRST_VARFIELD)
        return MARC_ERR_INDIC_FLD;

    /* Set indicator */
    MARC_INDIC(mp,indic) = value;

    return 0;

} /* marc_set_indic */


/************************************************************************
* marc_get_indic ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Retrieve an indicator of the current field.                     *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Indicator number, 1 or 2.                                       *
*       Pointer to place to put indicator value.                        *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

int marc_get_indic (
    MARCP mp,       /* Pointer to structure     */
    int   indic,    /* Indicator number, 1 or 2 */
    unsigned char
          *valp     /* Put indic value here     */
) {
    /* Test, return if failed */
    MARC_XCHECK(mp)

    /* Indicator in range? */
    if (indic != 1 && indic != 2)
        return MARC_ERR_BAD_INDIC;

    /* Are we positioned on a variable length field? */
    if (mp->cur_field < 1 || MARC_CUR_TAG(mp) < MARC_FIRST_VARFIELD)
        return MARC_ERR_INDIC_FLD;

    /* Get indicator */
    *valp = MARC_INDIC(mp,indic);

    return 0;

} /* marc_get_indic */
