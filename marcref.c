/************************************************************************
* marc_ref ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Convert an ASCII reference to a marc field and subfield         *
*       to a set of 4 integers representing field id, field occ,        *
*       sf id and sf occ.                                               *
*                                                                       *
*       References may be in any of the following forms:                *
*                                                                       *
*          650:2$x:0   - 3rd 650, first subfield x.                     *
*          650:+$x:0   - New 650, first subfield x.                     *
*          650:+$x:+   - New 650, first subfield x.                     *
*          008:0$3:5   - 5 bytes of first 008, starting at offset 3.    *
*          650:2       - 3rd 650, no subfield specified.                *
*          650$x       - Current 650, current subfield x.               *
*          650$x:1     - Current 650, second subfield x.                *
*          650:1$x     - Second 650, current subfield x.                *
*          $a:0        - Current field, first subfield a.               *
*          $a          - Current field, current subfield a (not useful).*
*          650@1       - Current 650, indicator 1.                      *
*          650:1@1     - Second 650, indicator 1.                       *
*          @2          - Current field, indicator 2.                    *
*                                                                       *
*       marc_ref does not try to find the object referenced, it         *
*       is purely and simply a string parser.  If a reference makes     *
*       no sense in some particular context, that is no concern of      *
*       marc_ref().  marc_ref() only cares if it is grammatically       *
*       correct.                                                        *
*                                                                       *
*   PASS                                                                *
*       Pointer to reference as null delimited string.                  *
*       Pointer to place to put field tag.                              *
*       Pointer to place to put field occ.                              *
*       Pointer to place to put subfield code (or fixed byte position)  *
*       Pointer to place to put subfield occ (or number of bytes)       *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcref.c,v 1.4 1999/06/14 21:43:21 meyer Exp $
 * $Log: marcref.c,v $
 * Revision 1.4  1999/06/14 21:43:21  meyer
 * Added support for negative occurrence numbers.
 *
 * Revision 1.3  1999/06/14 19:58:36  meyer
 * Added special case code to support cmp_buf_find in marcproc.c.
 *
 * Revision 1.2  1998/12/14 19:45:07  meyer
 * Added indicator parsing.
 *
 * Revision 1.1  1998/12/14 15:47:20  meyer
 * Initial revision
 *
 */


#include <ctype.h>
#include "marcdefs.h"

/* Types of legal objects in a pattern string */
typedef enum objtyp {
    number,         /* Field, occ, offset, len, etc.*/
    alpha,          /* Only used for subfield code  */
    dollar,         /* '$' before sf code           */
    atsign,         /* '@' before indicator number  */
    plus,           /* '+' occurrence indicator     */
    colon           /* Before occ, offset, len      */
} OBJTYP;

/* Put the objects into structures of this type */
typedef struct patpart {
    int    val;     /* Value, e.g., field id, occ   */
    OBJTYP otype;   /* Type of single piece in pat  */
} PATPART;

/* Max objects - ids, numbers, or delimiters, in a valid reference string */
#define MAX_REF_PARTS 7

int marc_ref (
    char *refp,     /* Ptr to reference as string   */
    int  *fidp,     /* Put field id here            */
    int  *foccp,    /* Put field occ here           */
    int  *sidp,     /* Put subfield id here         */
    int  *soccp     /* Put subfield occ here        */
) {
    char *p;        /* Ptr into reference           */
    int  pcnt,      /* Pattern part counter         */
         fidx,      /* Index into rpat for field val*/
         foccx,     /* Index to field occurrence    */
         sfx,       /* Index to subfield code       */
         sfoccx,    /* Index to subfield occurrence */
         indic,     /* True='sf' is really indicator*/
         stat;      /* Return from called func      */

    /* Array of pattern elements parsed from reference string */
    PATPART rpat[MAX_REF_PARTS],  /* Pattern found  */
            *rpp;                 /* Ptr to rpat    */


    /* Parse into up to fields */
    p     = refp;
    rpp   = rpat;
    pcnt  = 0;
    indic = 0;
    while (*p) {

        /* Too many parts in pattern? */
        if (pcnt > MAX_REF_PARTS)
            return MARC_ERR_REF_PARTS;

        /* Numeric field or subfield */
        if (isdigit(*p) || *p == '-') {
            rpp->val   = atoi (p);
            rpp->otype = number;
            if (*p == '-')
                ++p;
            while (isdigit(*p))
                ++p;
        }

        else if (isalpha(*p)) {
            rpp->val   = (int) *p;
            rpp->otype = alpha;
            ++p;
        }

        else {
            rpp->val = *p++;
            switch (rpp->val) {
                case ':':
                    rpp->otype = colon;
                    break;
                case '$':
                    rpp->otype = dollar;
                    break;
                case '@':
                    rpp->otype = atsign;
                    indic = 1;
                    break;
                case '+':
                    rpp->otype = plus;
                    break;
                default:
                    return MARC_ERR_BAD_REF;
            }
        }

        /* Count and point */
        ++pcnt;
        ++rpp;
    }

    /* Search for one of the valid patterns */
    switch (pcnt) {

        case 7:
            /* Pattern = fid:focc$sfid:sfocc */
            if (! (rpat[1].otype == colon &&
                   rpat[3].otype == dollar &&
                   rpat[5].otype == colon) )
                return MARC_ERR_BAD_REF;

            /* Indicate where values are in array */
            fidx   = 0;
            foccx  = 2;
            sfx    = 4;
            sfoccx = 6;
            break;

        case 5:
            /* Pattern = fid:focc$sf or fid:focc@indic */
            if (rpat[1].otype == colon &&
                    (rpat[3].otype == dollar || rpat[3].otype == atsign)) {
                fidx   = 0;
                foccx  = 2;
                sfx    = 4;
                sfoccx = -1;
            }

            /* Pattern = fid$sf:sfocc */
            else if (rpat[1].otype == dollar && rpat[3].otype == colon) {
                fidx   = 0;
                foccx  = -1;
                sfx    = 2;
                sfoccx = 4;
            }

            else
                return MARC_ERR_BAD_REF;
            break;

        case 4:
            /* Pattern = $sf:occ */
            if (rpat[0].otype == dollar && rpat[2].otype == colon) {
                fidx   = -1;
                foccx  = -1;
                sfx    = 1;
                sfoccx = 3;
            }
            else
                return MARC_ERR_BAD_REF;
            break;

        case 3:
            /* Pattern = fid:occ */
            if (rpat[1].otype == colon) {
                fidx   = 0;
                foccx  = 2;

                /* We need to reference some sort of subfield in order
                 *   to avoid breaking attempts to retrieve an item
                 *   using this reference.
                 * Since none was provided, we'll reference the first
                 *   byte of the field.
                 * This is really a special case supporting the function
                 *   marcproc.c::cmp_buf_find().
                 * We might want it otherwise for other applications.
                 */
                rpat[3].otype = number;
                rpat[4].otype = number;
                sfx    = 3;
                sfoccx = 4;
                if (rpat[0].val < MARC_FIRST_VARFIELD) {
                    rpat[3].val = 0;
                    rpat[4].val = 1;
                }
                else {
                    rpat[3].val = MARC_INDIC1;
                    rpat[4].val = 0;
                }
            }
            /* Pattern = fid:occ or fid$sf or fid@indic */
            else if (rpat[1].otype == dollar || rpat[1].otype == atsign) {
                fidx   = 0;
                foccx  = -1;
                sfx    = 2;
                sfoccx = -1;
            }
            else
                return MARC_ERR_BAD_REF;
            break;

        case 2:
            /* Pattern = $sf or @indic */
            if (rpat[0].otype == dollar || rpat[0].otype == atsign) {
                fidx   = -1;
                foccx  = -1;
                sfx    = 1;
                sfoccx = -1;
            }
            else
                return MARC_ERR_BAD_REF;
            break;

        default:
            /* We've exhausted all legal patterns */
            return MARC_ERR_BAD_REF;
    }


    /* If we got here, we have a valid pattern
     * Now have to interpret the values in the slots in the pattern
     * Errors may still be encountered
     */

    /* Field id */
    if (fidx < 0)

        /* Default is the current field */
        *fidp = MARC_REF_CURRENT;

    else {
        *fidp = rpat[fidx].val;
        if (*fidp<0 || *fidp>MARC_MAX_FIELDID || rpat[fidx].otype!=number)
            return MARC_ERR_REF_FIELD;
    }

    /* Field occurrence */
    if (foccx < 0)

        /* Default is the current field */
        *foccp = MARC_REF_CURRENT;

    else {
        /* '+' means a new occurrence */
        if (rpat[foccx].otype == plus)
            *foccp = MARC_REF_NEW;
        else {
            *foccp = rpat[foccx].val;
            if (rpat[foccx].otype != number)
                return (MARC_ERR_REF_FOCC);
        }
    }

    /* Subfield id, never defaulted */
    *sidp = rpat[sfx].val;

    /* Variable fields only allow a-z, 1-9
     * For fixed fields, treat the subfield "id" as an offset
     */
    if (*fidp >= MARC_FIRST_VARFIELD) {

        /* Indicators */
        if (indic) {

            switch (*sidp) {

                /* MARC_INDICx is probably identical to 1 and 2
                 *   but we won't depend on that.
                 * Set it here.
                 */
                case 1:
                    *sidp = MARC_INDIC1;
                    break;
                case 2:
                    *sidp = MARC_INDIC2;
                    break;
                default:
                    return MARC_ERR_REF_INDIC;
            }
        }

        else {
            /* Numeric subfield code got converted to a number
             * Have to convert it back
             */
            if (*sidp >= 0 && *sidp <= 9)
                *sidp += '0';

            /* Test for legal sf code */
            if ((stat = marc_ok_subfield (*sidp)) != 0)
                return stat;
        }
    }

    /* Subfield occurrence */
    if (*fidp < MARC_FIRST_VARFIELD) {

        /* "Fixed" fields don't have subfields, use offset + length */
        if (sfoccx < 1)
            return MARC_ERR_REF_FIXLEN;
        *soccp = rpat[sfoccx].val;
    }

    else {
        /* Default is current occurrence */
        if (sfoccx < 0)
            *soccp = MARC_REF_CURRENT;

        /* '+' = next occurrence */
        else if (rpat[sfoccx].otype == plus)
            *soccp = MARC_REF_NEW;

        else if (rpat[sfoccx].otype != number)
            return MARC_ERR_REF_SFOCC;

        else
            *soccp = rpat[sfoccx].val;
    }

    return 0;

} /* marc_ref */
