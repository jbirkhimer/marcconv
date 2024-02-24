/************************************************************************
* marc_next_item ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Position to the next data element after the current one.        *
*                                                                       *
*       Packages marc_next_field() and marc_next_subfield() to          *
*       walk through a record, picking up the next field, or            *
*       subfield, whatever it is.                                       *
*                                                                       *
*       After the last subfield of a field, it moves on to the          *
*       first subfield of the next one (which may be the first          *
*       indicator "subfield" - not the way marc does it but             *
*       a simplification of the API.                                    *
*                                                                       *
*       Caller can tell if we have started a new field occurrence       *
*       by comparing the returned field position to the previous        *
*       one, or more simply by checking for subfield position==0,       *
*       which always indicates a new field.                             *
*                                                                       *
*       To process a record sequentially, call marc_get_field()         *
*       for the leader (field 0), and then call marc_next_item()        *
*       repeatedly until it returns MARC_RET_END_REC.                   *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure returned by marc_init.                     *
*       Direction to move.                                              *
*           True (non-zero) = Forward, next data element.               *
*           False (zero)    = Backward, previous element.               *
*       Pointer to place to put field tag/id number.                    *
*           0 = Leader.                                                 *
*       Pointer to place to put subfield code/id number.                *
*           Undefined for leader and control fields.                    *
*       Ptr to place to put pointer to data element data.               *
*           For control fields, this is the whole field.                *
*           For variable length fields, it's a pointer to a subfield.   *
*           We point to actual data, not to a leading subfield          *
*           delimiter or subfield code.                                 *
*       Ptr to place to put length of data element.                     *
*           Length does NOT include subfield code or delimiter.         *
*       Ptr to place to put field position.                             *
*           In case the caller wants to know that this is the Nth field.*
*       Ptr to place to put subfield position.                          *
*           In case the caller wants to know that this is the Mth sf.   *
*           Note: position 0 may be first indicator, position 2 is      *
*           first variable length subfield.                             *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Negative number indicates serious, structural error.        *
*           Positive number indicates data not found.                   *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcnxti.c,v 1.2 2000/03/20 15:54:16 meyer Exp $
 * $Log: marcnxti.c,v $
 * Revision 1.2  2000/03/20 15:54:16  meyer
 * Added initialization of retcode variable.
 *
 * Revision 1.1  1998/12/28 21:04:05  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

int marc_next_item (
    MARCP         mp,           /* Pointer to structure     */
    int           forward,      /* True=next item else prev */
    int           *field_idp,   /* Put Field tag/id here    */
    int           *sf_idp,      /* Put subfield code/id here*/
    unsigned char **datap,      /* Put data pointer here    */
    size_t        *datalenp,    /* Put data only length here*/
    int           *fposp,       /* Put field position here  */
    int           *sposp        /* Put sf position here     */
) {
    int           direction,    /* +/- 1 = forward/reverse  */
                  move,         /* True=move to next field  */
                  fid,          /* Current field id         */
                  spos,         /* Sf position we want      */
                  retcode;      /* Return code              */


    /* Test, return if failed */
    MARC_XCHECK(mp)

    retcode = 0;

    /* Direction of motion relative to current pos */
    direction = forward ? 1 : -1;

    /* Assume we'll move to the next field, unless we find out otherwise */
    move = 1;

    /* Set current field id */
    fid = (mp->fdirp + mp->cur_field)->tag;

    /* If on a variable field and not at end, we don't move */
    if (fid >= MARC_FIRST_VARFIELD) {

        /* Make sure subfield directory is up to date */
        if ((retcode = marc_xsfdir (mp)) != 0)
            return (retcode);

        /* Is there room to move within this field? */
        spos = mp->cur_sf + direction;
        if (spos >= 0 && spos < mp->cur_sf)
            move = 0;
    }

    /* Change fields if necessary */
    if (move) {
        if ((retcode = marc_next_field (mp, forward, &fid, datap,
                                        datalenp, fposp)) != 0)
            return (retcode);

        /* Update subfield directory if necessary */
        if ( (fid >= MARC_FIRST_VARFIELD) &&
             (retcode = marc_xsfdir (mp)) != 0)
            return (retcode);
    }

    /* If no subfields in current field, set default return values */
    if (fid < MARC_FIRST_VARFIELD) {
        *sf_idp = 0;
        *sposp  = 0;
    }

    else {
        /* Set actual subfield values */
        if (move)
            spos = forward ? 0 : mp->sf_count - 1;
        else
            spos = mp->cur_sf + direction;
        retcode = marc_pos_subfield (mp, spos, sf_idp, datap, datalenp);
    }

    /* Returned field id */
    *field_idp = fid;

    /* This may also already have been set, but in case it wasn't */
    *fposp = mp->cur_field;

    return (retcode);

} /* marc_next_item */
