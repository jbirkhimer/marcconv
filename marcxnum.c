/************************************************************************
* marc_xnum ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Convert a number in the leader or directory of a marc record    *
*       from ASCII digits to binary.                                    *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Start pointer points to an array of right justified ASCII       *
*       digits of the proper length, zero filled on the left.           *
*       No checking is performed.                                       *
*                                                                       *
*   PASS                                                                *
*       Pointer to start of ASCII digits.                               *
*       Number of digits in the digit string.                           *
*                                                                       *
*   RETURN                                                              *
*       Unsigned integer conversion.                                    *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcxnum.c,v 1.1 1998/12/28 21:16:01 meyer Exp meyer $
 * $Log: marcxnum.c,v $
 * Revision 1.1  1998/12/28 21:16:01  meyer
 * Initial revision
 *
 */

#include "marcdefs.h"

unsigned int marc_xnum (
    unsigned char *startp,  /* Ptr to start of digit string */
    int           digits    /* Number of digits in string   */
) {
    unsigned char *digitp;  /* Ptr to single digit          */
    int           i;        /* Loop counter                 */
    unsigned int  mult,     /* Multiplier for digit position*/
                  num;      /* Return value                 */


    /* Point after last digit in string */
    digitp = startp + digits;

    /* Initial sum and multiplier */
    num  = 0;
    mult = 1;

    /* Loop through digits */
    for (i=0; i<digits; i++) {

        /* Move left and convert */
        --digitp;
        num += (*digitp - '0') * mult;
        mult *= 10;
    }

    return (num);

} /* marc_xnum */
