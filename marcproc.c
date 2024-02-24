/************************************************************************
* marcproc.c                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Custom procedures used in Marc->Marc processing.                *
*                                                                       *
*       This file contains the tables for procedure name/pointer        *
*       conversions.  Actual procedures may reside here or in           *
*       additional files.                                               *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcproc.c,v 1.18 2000/01/06 15:58:33 vereb Exp vereb $
 * $Log: marcproc.c,v $
 * Revision 1.18  2000/01/06 15:58:33  vereb
 * Updated for Y2K compatibility
 *
 * Revision 1.17  1999/10/21 19:50:31  vereb
 * accommodated CM_CONTINUE in cmp_log procedure
 *
 * Revision 1.16  1999/09/13 15:06:43  vereb
 * changed cm_error messages in cmp_buf_write to identify
 * iddestp for debugging purposes
 *
 * Revision 1.15  1999/09/07 22:57:25  meyer
 * Major revision of the cmp_buf_write() code to correctly handle record
 * updates specified with '+' (MARC_REF_NEW) in the subfield occ reference.
 *
 * Revision 1.14  1999/07/22 03:15:25  meyer
 * Possible fix for negative start position in substr func
 * Fixed Microsoft compiler warnings.
 *
 * Revision 1.13  1999/06/17 15:53:15  meyer
 * Amplified and corrected some comments.
 * No changes to code.
 *
 * Revision 1.12  1999/06/14 21:44:38  meyer
 * Fixed bug where failed to null terminate search string passed to istr...
 *
 * Revision 1.11  1999/06/14 15:42:10  meyer
 * Fixed bug in cmp_log pointing to wrong buffer.
 *
 * Revision 1.10  1999/04/06 15:19:18  meyer
 * Bug, left off trailing comment delimiter
 *
 * Revision 1.9  1999/04/05 19:20:58  meyer
 * Modified cmp_log to support severity levels in call to cm_error
 * Added strip_quotes() function
 *
 * Revision 1.8  1999/03/09 01:26:02  meyer
 * Fixed bug in get_fixed_num().  '+' should've been '-'.
 *
 * Revision 1.7  1999/02/20 22:47:30  meyer
 * Made get_fixed_num() external.
 *
 * Revision 1.6  1999/02/08 15:17:08  meyer
 * Fixed two bugs - get_fixed_num termination test and last_op initialization.
 *
 * Revision 1.5  1998/12/31 21:14:15  meyer
 * Added cmp_call() function.
 * Fixed bug declaring extern as pointer when it should have been array.
 *
 * Revision 1.4  1998/12/29 02:37:18  meyer
 * Removed S_proc_list table.  Now global defined in marcproclist.c
 *
 * Revision 1.3  1998/12/29 01:33:14  meyer
 * Small bug fixes.
 *
 * Revision 1.2  1998/12/17 16:50:46  meyer
 * Added numeric comparison operators '<', '>'
 * Changed old '<' to '^'
 *
 * Revision 1.1  1998/12/14 19:46:21  meyer
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include "marcproclist.h"   /* Includes marcconv.h and marc.h */
#include "istrstr.h"        /* Case insensitive strstr()      */

/* Non-standard case insensitive string compare */
#if defined (_MSC_VER)
#define istrncmp strnicmp
#else
#define istrncmp strncasecmp
#endif

void *marc_alloc(int, int);
void *marc_realloc(void*, int, int);

/*********************************************************************
* qual_tbl                                                           *
*   Sorted array of these structures used to convert 2 char          *
*   MeSH qualifiers to expanded strings via binary search.           *
*********************************************************************/
typedef struct qual_tbl {
    char   qual[2];     /* Two char abbreviation        */
    char   *string;     /* Expanded string              */
    size_t lenexp;      /* Pre-computed string length   */
} QUAL_TBL;

/*********************************************************************
*   Prototypes for internal subroutines                              *
*********************************************************************/

static CM_ID get_src_type(char *);
static void load_quals    (QUAL_TBL **, size_t *);
static int  compare_quals (const void *, const void *);
static int  lookup_qual   (QUAL_TBL *, size_t, unsigned char *,
                           unsigned char **, size_t *);


/************************************************************************
* cmp_lookup_proc ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Lookup a procedure by its external name.                        *
*                                                                       *
*       Return all info from the table.                                 *
*                                                                       *
*       The lookup table is defined in the file "marcproclist.c".       *
*                                                                       *
*   PASS                                                                *
*       Pointer to name of procedure.                                   *
*       Pointer to place to put condition code indicating if/else/endif.*
*       Pointer to place to put minimum number of arguments.            *
*       Pointer to place to put max number of allowable args.           *
*       Pointer to place to put validation flags.                       *
*                                                                       *
*   RETURN                                                              *
*       Pointer to function.                                            *
*       NULL if not found.                                              *
************************************************************************/

CM_FUNCP cmp_lookup_proc (
    char      *pname,       /* Name of procedure        */
    CM_CND    *condition,   /* Put condition type here  */
    int       *min_args,    /* Put min arg count here   */
    int       *max_args,    /* Put max here             */
    int       *flags        /* Put valid position flags */
) {
    CMP_TABLE *pp;          /* Ptr into table           */

    extern CMP_TABLE G_proc_list[];

    /* Brute force search of global proc list */
    pp = G_proc_list;
    while (pp->pname) {
        if (!strcmp (pp->pname, pname)) {
            *condition = pp->condition;
            *min_args  = pp->min_args;
            *max_args  = pp->max_args;
            *flags     = pp->position;
            return (pp->proc);
        }
        ++pp;
    }

    return NULL;

} /* cmp_lookup_proc */


/************************************************************************
* cmp_if ()                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       Evaluate a condition.                                           *
*                                                                       *
*       Conditions are of several types:                                *
*           Does a data element or buffer have any data in it (*)?      *
*           Does it equal a certain value (=)?                          *
*           Is it greater or less than a certain value (> or <)?        *
*           Same for (>= or <=)?                                        *
*           Does it contain a certain leading substring (^)?            *
*           Does it contain a certain substring anywhere (?)?           *
*           Is it numeric (9)?                                          *
*                                                                       *
*       Conditions may be modified as follows:                          *
*           Negate condition (!).                                       *
*           Test condition case insensitively (~).                      *
*                                                                       *
*       The greater/less tests (>, <, >=, <=) convert their             *
*       arguments to integers and then perform the tests.  If           *
*       one of the arguments is not an integer an error message         *
*       is written to the log file and the test is considered to        *
*       have failed.                                                    *
*                                                                       *
*       The ignore case modifier (~) is ignored for numeric tests.      *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*           Data element or buffer id to evaluate.                      *
*           Operator to test, one of:                                   *
*                   * = > < >= <= ^ ? 9                                 *
*               or modified versions:                                   *
*                   !* != !> !? ~= !~= etc.                             *
*           Value                                                       *
*               Ignored for operator *, tested in all other cases.      *
*                                                                       *
*   RETURN                                                              *
*       0                  = Success, condition passed.                 *
*       CM_STAT_IF_FAILED  = Success, condition failed.                 *
*       Else fatal error.                                               *
************************************************************************/

CM_STAT cmp_if (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    int  negate,            /* True=Negate meaning of operator      */
         insensitive,       /* True=Search/compare case insensitive */
         op,                /* Operator   * = > or ?                */
         last_op,           /* Previous operator character          */
         num1,              /* Numeric conversion of datap          */
         num2,              /* Same for data2p                      */
         rc;                /* Return from strncmp                  */
    char *p,                /* Ptr into operator string             */
         *rp;               /* Return from strstr                   */
    unsigned char *datap,   /* Ptr to data in source elem or buf    */
                  *data2p,  /* Ptr to data to compare against       */
                  *holdp,   /* Ptr to end of data2p, terminator     */
                  empty[1], /* Nullstring standing for dummy data   */
                  hold;     /* Char at holdp                        */
    size_t        datalen,  /* Length of data at datap              */
                  data2len; /* Length of data at data2p             */



    /* Find the data pointed to by data element/buffer id
     * If not found, cmp_buf_find returns us a pointer to null string
     *   which works just fine for our purposes.
     */
    cmp_buf_find (pp, pp->args[0], &datap, &datalen);

    /* Same for data to compare to, if any */
    if (pp->args[2])
        cmp_buf_find (pp, pp->args[2], &data2p, &data2len);
    else {
        data2p   = empty;
        *data2p  = '\0';
        data2len = 0;
    }

    /* Initial assumptions about operator modifiers */
    negate      =
    insensitive =
    last_op     = 0;

    /* Parse operator */
    p = pp->args[1];
    while (*p) {

        /* Look for single char op code */
        op = *p;
        switch (op) {
            case CM_OP_NOT:
                /* It's a prefix to op code */
                negate = 1;
                break;
            case CM_OP_NOCASE:
                /* Another prefix */
                insensitive = 1;
                break;
            case CM_OP_EQ:
                /* If part of ">=" or "<=" */
                if (last_op == CM_OP_GT)
                    op = CM_OP_GTE;
                else if (last_op == CM_OP_LT)
                    op = CM_OP_LTE;
                break;
            case CM_OP_EXISTS:
            case CM_OP_BEGINS:
            case CM_OP_CONTAINS:
            case CM_OP_NUMERIC:
            case CM_OP_GT:
            case CM_OP_LT:
                break;
            default:
                cm_error (CM_FATAL, "Unknown operator '%c' in if test", op);
        }
        last_op = op;
        ++p;
    }

    /* If no data found, all tests fail unless negated */
    if (!datalen)
        return (negate ? CM_STAT_OK : CM_STAT_IF_FAILED);

    /* Execute operation */
    switch (op) {
        case CM_OP_EXISTS:
            /* Already taken care of 0 datalen, hence assume data found */
            return negate ? CM_STAT_IF_FAILED : CM_STAT_OK;

        case CM_OP_EQ:
            /* Compare data to passed string */
            if ((rc = data2len - datalen) == 0) {
                if (insensitive)
                    rc = istrncmp ((char *) datap, (char *) data2p, datalen);
                else
                    rc = strncmp ((char *) datap, (char *) data2p, datalen);
            }
            if (!rc)
                return negate ? CM_STAT_IF_FAILED : CM_STAT_OK;
            return negate ? CM_STAT_OK : CM_STAT_IF_FAILED;

        case CM_OP_NUMERIC:
            /* Is passed data numeric? */
            if (get_fixed_num ((char *) datap, datalen, &num1))
                return negate ? CM_STAT_IF_FAILED : CM_STAT_OK;
            return negate ? CM_STAT_OK : CM_STAT_IF_FAILED;

        case CM_OP_GT:
        case CM_OP_LT:
        case CM_OP_GTE:
        case CM_OP_LTE:
            /* If we've got integers, prepare for numeric compare */
            if (get_fixed_num ((char *) datap, datalen, &num1)) {
                if (get_fixed_num ((char *) data2p, data2len, &num2)) {

                    switch (op) {
                        case CM_OP_GT:
                            rc = (num1 > num2);
                            break;
                        case CM_OP_LT:
                            rc = (num1 < num2);
                            break;
                        case CM_OP_GTE:
                            rc = (num1 >= num2);
                            break;
                        case CM_OP_LTE:
                            rc = (num1 <= num2);
                            break;
                    }

                    if (rc)
                        return negate ? CM_STAT_IF_FAILED : CM_STAT_OK;
                    return negate ? CM_STAT_OK : CM_STAT_IF_FAILED;
                }
            }

            /* Something was not an integer */
            cm_error (CM_ERROR,
                      "Data \"%*s\" or \"%*s\" not numeric in compare",
                      datalen, datap, data2len, data2p);
            return CM_STAT_IF_FAILED;

        case CM_OP_BEGINS:
        case CM_OP_CONTAINS:
            /* Case insensitive or not substring search
             * Requires null terminated search string, which
             *   we guarantee here
             */
            holdp  = data2p + data2len;
            hold   = *holdp;
            *holdp = '\0';
            if (insensitive)
                rp = instrstr ((char *) datap, (char *) data2p, datalen);
            else
                rp = nstrstr ((char *) datap, (char *) data2p, datalen);
            *holdp = hold;

            /* Do we need substring at start of data? */
            if (op == CM_OP_BEGINS) {
                if (rp != (char *) datap)
                    rp = NULL;
            }

            if (rp)
                return negate ? CM_STAT_IF_FAILED : CM_STAT_OK;
            return negate ? CM_STAT_OK : CM_STAT_IF_FAILED;
    }
    return CM_STAT_OK;
} /* cmp_if */


/************************************************************************
* get_fixed_num ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Convert a fixed length string to an integer.                    *
*                                                                       *
*       Subroutine of cmp_if() and possibly other routines.             *
*                                                                       *
*   PASS                                                                *
*       Pointer to string to convert.                                   *
*       Length of string.                                               *
*       Pointer to place to put integer conversion.                     *
*                                                                       *
*   RETURN                                                              *
*       Non-zero = Successful numeric conversion.                       *
*       0        = At least one char in string is non-numeric.          *
************************************************************************/

int get_fixed_num (
    char   *strp,       /* Ptr to string            */
    size_t numlen,      /* Examine this many bytes  */
    int    *nump        /* Return number here       */
) {
    int    sign,        /* +/-1 for numeric sign    */
           len,         /* Signed length of string  */
           num;         /* Target number            */
    char   *p;          /* Ptr into strp            */


    sign = 1;
    num  = 0;
    len  = (int) numlen;

    /* Test for leading unary minus */
    p = strp;
    if (*p == '-') {
        sign = -1;
        ++p;
        --len;
    }

    /* Test all chars for digits */
    while (len--) {
        if (!isdigit(*p))
            break;
        num *= 10;
        num += (*p - '0');
        ++p;

        /* Test for overflow */
        if (num < 0) {
            cm_error (CM_ERROR, "Numeric overflow converting \"%s\"", strp);
            break;
        }
    }

    /* Got all chars? */
    if (len >= 0)
        return 0;

    /* Success */
    *nump = num * sign;
    return 1;

} /* get_fixed_num */


/************************************************************************
* strip_quotes ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Strip quotation marks from a copy of a string, if they          *
*       are present.  Else just copies the string.                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to string to copy/strip.                                *
*       Pointer to place to copy to.                                    *
*       Length of output buffer.                                        *
*                                                                       *
*   RETURN                                                              *
*       Non-zero = Success.                                             *
*       0        = String truncated.                                    *
************************************************************************/

int strip_quotes (
    char *instr,        /* Input string     */
    char *outstr,       /* Output buffer    */
    size_t outlen       /* Output buf length*/
) {
    char *src,          /* Source of copy   */
         *dest;         /* Destination      */


    /* Point to start of start of buffer, after quote, if there is one */
    src  = instr;
    dest = outstr;
    if (*src == '"')
        ++src;

    /* Copy bytes until no more or buffer filled */
    while (*src && outlen > 1) {

        /* Test for non-escaped trailing quote */
        if (*src == '"' && (*(src-1) != '\\'))
            break;

        /* Copy and decrement buflen */
        *dest++ = *src++;
        --outlen;
    }

    /* Terminate */
    *dest = '\0';

    /* Return truncation or complete copy */
    if (outlen <= 1)
        return 0;
    return 1;

} /* strip_quotes */


/************************************************************************
* cmp_nop ()                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       No-op generated as a place holder when "else" or "endif"        *
*       instructions are encountered.                                   *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
************************************************************************/

CM_STAT cmp_nop (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    return CM_STAT_OK;
}


/************************************************************************
* cmp_indic ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Set an indicator for the current field.                         *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Indicator number, "1" or "2".                                   *
*       Single character indicator value.                               *
*           If value is " ", use trailing slash to delimit it, e.g.     *
*           indic/1/ /                                                  *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       CM_STAT_ERROR if failed.                                       *
************************************************************************/

CM_STAT cmp_indic (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    int indic,              /* Indicator code for marc_add_subfield */
        stat;               /* Return from func                     */


    /* Set indicator code */
    if (*pp->args[0] == '1')
        indic = MARC_INDIC1;
    else if (*pp->args[0] == '2')
        indic = MARC_INDIC2;

    /* If unrecognized parm, stop all processing */
    else {
        cm_error (CM_FATAL, "Bad indicator number = \"%c\"", *pp->args[0]);
        return CM_STAT_ERROR;
    }

    /* Install indicator, called func will validate value */
    if ((stat = marc_add_subfield (pp->outmp, indic,
                                  (unsigned char *) pp->args[1], 1)) != 0) {
        cm_error (CM_ERROR, "cm_indic: %d on indicator %c, value %c",
                            *pp->args[0], *pp->args[1]);
        return CM_STAT_ERROR;
    }

    return CM_STAT_OK;

} /* cmp_indic */


/************************************************************************
* cmp_clear ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Clear a buffer.                                                 *
*                                                                       *
*       Will work with a MARC field/subfield, but not likely to         *
*       be useful.                                                      *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Name of buffer to clear.                                        *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
************************************************************************/

CM_STAT cmp_clear (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    /* Universal routine does the work
     * Pass it a pointer to a null string as if coming from
     *   config file.
     */
    return (cmp_buf_copy (pp, pp->args[0], "\"\"", 0));

} /* cmp_clear */


/************************************************************************
* cmp_append ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Append data from somewhere to somewhere else.                   *
*                                                                       *
*       Front end to internal cmp_buf_copy().                           *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       ID of destination to append to.                                 *
*       ID of source from which append data comes.                      *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       Else return from lower level cmp_buf_copy().                    *
************************************************************************/

CM_STAT cmp_append (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    /* Pass through */
    return (cmp_buf_copy (pp, pp->args[0], pp->args[1], 1));

} /* cmp_append */


/************************************************************************
* cmp_copy ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Copy data from somewhere to somewhere else.                     *
*                                                                       *
*       Front end to internal cmp_buf_copy().                           *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       ID of destination to copy to.                                   *
*       ID of source from which copy data comes.                        *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       Else return from lower level cmp_buf_copy().                    *
************************************************************************/

CM_STAT cmp_copy (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    /* Pass through */
    return (cmp_buf_copy (pp, pp->args[0], pp->args[1], 0));

} /* cmp_copy */


/************************************************************************
* cmp_substr ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Extract a substring from someplace to someplace else.           *
*                                                                       *
*       The destination is cleared before copying.  If no copy          *
*       takes place, because the source is not found or is shorter      *
*       than the starting offset for the substring extract, then        *
*       no error is returned but the destination buffer will be         *
*       empty.                                                          *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       ID of destination to copy to.                                   *
*       ID of source from which to extract substring.                   *
*       Starting position in source, origin 0.                          *
*           If negative, we start from the end.                         *
*       Number of bytes.                                                *
*           If absent or larger than source, or 0, we copy the          *
*           entire source.                                              *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_substr (
    CM_PROC_PARMS *pp       /* Ptr to parameter structure   */
) {
    unsigned char *srcp;    /* Ptr to source of extract     */
    size_t        src_len,  /* Put data length here         */
                  sbs_len,  /* Substring length             */
                  max_len;  /* Max possible len to extract  */
    int           sbs_start;/* Substring starting offset    */


    /* Find source */
    cmp_buf_find (pp, pp->args[1], &srcp, &src_len);

    /* Clear destination */
    cmp_buf_copy (pp, pp->args[0], "\"\"", 0);

    /* If data found, extract part we want */
    if (src_len) {

        /* Get starting position and length */
        sbs_start = atoi (pp->args[2]);
        if (pp->args[3])
            sbs_len = (size_t) atoi (pp->args[3]);
        else
            sbs_len = 0;

        /* Adjust for negative offset */
        if (sbs_start < 0) {
            sbs_start = src_len + sbs_start;
            if (sbs_start < 0)
                sbs_start = 0;
        }

        /* Is starting position within the data? */
        if (sbs_start < (int) src_len) {

            /* Set new pointer to source */
            srcp += sbs_start;

            /* Adjust for 0 or too long length */
            max_len = src_len - sbs_start;
            if (sbs_len == 0 || sbs_len > max_len)
                sbs_len = max_len;

            /* Output to destination */
            return (cmp_buf_write (pp, pp->args[0], srcp, sbs_len, 0));
        }
    }

    /* If data not found or out of range, no error, but no copy either */
    return (CM_STAT_OK);

} /* cmp_substr */


/************************************************************************
* cmp_normalize ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Normalize data                                                  *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       ID of destination for normalized result.                        *
*       ID of source to be normalized.                                  *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       Else return from lower level cmp_buf_copy().                    *
************************************************************************/

CM_STAT cmp_normalize (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
  unsigned char *tsrcp;    /* Ptr to source data           */
  size_t        src_len;   /* Length source data, no null  */
  char          *srcp;    /* Ptr to temp data                    */
  char          *tmpp;     /* Ptr to temp data                    */
  char          *dest;     /* returned Ptr                        */
  int           numspaces;

  /* Find data source */

  cmp_buf_find (pp, pp->args[1], &tsrcp, &src_len);
  srcp=(char *)tsrcp;

  /*=========================================================================*/
  /* Allocate memory based on size of src...                                 */
  /*=========================================================================*/

#ifdef DEBUG
  if ((dest = (char *) marc_alloc(src_len+1, 600)) == NULL) {  //TAG:600
    cm_error(CM_FATAL, "Error allocating memory for normalization process.");
  }
#else
  if ((dest = (char *)malloc(src_len+1)) == NULL) {
    cm_error(CM_FATAL, "Error allocating memory for normalization process.");
  }
#endif

  /*=========================================================================*/
  /* Initialize the destination buffer...                                    */
  /*=========================================================================*/
  memset(dest, '\0', src_len+1);

  /*=========================================================================*/
  /* and begin copying over the contents of the source buffer                */
  /*=========================================================================*/
  for(tmpp=srcp;(*tmpp) && (tmpp<srcp+src_len);tmpp++) {
    /*=======================================================================*/
    /* If a character is alpha-numeric, copy it over...                      */
    /*=======================================================================*/
    if(isalnum(*tmpp) ||
       (*tmpp=='-'))
      strncat(dest, tmpp, 1);
    else {
      /*=====================================================================*/
      /* also copy over a single space but ignore any other non-alphanumerics*/
      /*=====================================================================*/
      numspaces = 0;
      for(;(*tmpp)&&(!isalnum(*tmpp))&&(tmpp<srcp+src_len); tmpp++) {
	if((*tmpp==' ')&&(numspaces==0)) {
	  strcat(dest, " ");
	  numspaces++;
	}
      }
      /*=====================================================================*/
      /* since we went forward, back up one to be properly placed            */
      /*=====================================================================*/
      tmpp--;
    }

  }

  /*=========================================================================*/
  /* and now lowercase everything...                                         */
  /*=========================================================================*/
  for(tmpp=dest;*tmpp;tmpp++) {
    *tmpp=tolower(*tmpp);
  }

  /*=========================================================================*/
  /* Place result in specified output buffer...                              */
  /*=========================================================================*/
  return (cmp_buf_write (pp, pp->args[0], (unsigned char *)dest, strlen(dest), 0));

} /* cmp_normalize */


/************************************************************************
* cmp_makefld ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Create a new, empty field and make it the current field.        *
*       All subsequent inserts will be into it.                         *
*                                                                       *
*       Note that subtle bugs are possible if a new field is            *
*       created and the old one is about to be created.  Hence          *
*       this is safest to use in certain special situations like        *
*       record pre-post process.                                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Field id.                                                       *
*                                                                       *
*   RETURN                                                              *
*       0 = Success                                                     *
*       Errors are fatal.                                               *
************************************************************************/

CM_STAT cmp_makefld (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    int field_id,               /* Field id/tag                     */
        stat;                   /* Return from marc_add_field()     */

    /* Get field id */
    field_id = atoi (pp->args[0]);
    if ((stat = marc_add_field (pp->outmp, field_id)) != 0)
        cm_error (CM_FATAL, "makefld: Error %d creating field %d",
                  stat, field_id);

    return CM_STAT_OK;

} /* cmp_makefld */


/************************************************************************
* cmp_makesf ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Insert the contents of the current buffer as a subfield.        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Subfield id.                                                    *
*           If the current field is fixed length, then we assume        *
*           that the subfield "id" is a position, origin 0.             *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       All errors are fatal.                                           *
************************************************************************/

CM_STAT cmp_makesf (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    int  sf_code,               /* Subfield code to insert          */
         field_id,              /* Id/tag of current field          */
         field_occ,             /* Occurrence number of current fld */
         field_pos,             /* Required parameter for cur_field */
         stat;


    /* Only do any of this if there is something to insert */
    if (*pp->bufp) {

        /* Fixed length field? */
        if ((stat = marc_cur_field (pp->outmp, &field_id, &field_occ,
                                    &field_pos)) != 0)
            cm_error (CM_FATAL, "makesf: Error %d getting current field id",
                      stat);

        /* Get numeric or char subfield code */
        if (field_id < 10)
            sf_code = atoi (pp->args[0]);
        else if (field_id < 1000)
            sf_code = *(pp->args[0]);

        /* Insert the new subfield */
        if ((stat = marc_add_subfield (pp->outmp, sf_code, pp->bufp,
                                       MARC_SF_STRLEN)) != 0)
            cm_error (CM_FATAL, "makesf: Error %d adding subfield %d",
                      stat, sf_code);
    }

    return CM_STAT_OK;

} /* cmp_makesf */


/************************************************************************
* cmp_today ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Create a string representing today's date and deposit           *
*       it where requested.                                             *
*                                                                       *
*       Currently two formats are supported.  More can be added if      *
*       required.                                                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Destination - buffer or data element id.                        *
*       Format, expressed as a string, one of:                          *
*           "YYYYMMDD"                                                  *
*           "YYMMDD"   (year 2000 here we come)                         *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_today (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    struct tm *ptm;             /* Ptr to time info from localtime()*/
    time_t ltime;               /* Time in seconds                  */
    char timebuf[20],           /* Create string here               */
         *fmtp;                 /* Ptr to sprintf format string     */


    /* Get current time info */
    time (&ltime);
    ptm = localtime (&ltime);

    /* Format as requested */
    if (!strcmp (pp->args[1], "\"YYYYMMDD\""))
        fmtp = "%Y%m%d";
    else if (!strcmp (pp->args[1], "\"YYMMDD\""))
        fmtp = "%y%m%d";
    else
        cm_error (CM_FATAL, "Unrecognized time format %s", pp->args[1]);

    strftime (timebuf, 20, fmtp, ptm);

    /* Copy to requested destination */
    return cmp_buf_write (pp, pp->args[0], (unsigned char *) timebuf,
                          strlen (timebuf), 0);

} /* cmp_today */


/************************************************************************
* cmp_y2toy4 ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Convert a two digit year to four digits.                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Destination of the converted data.  Put it here.                *
*       Source of the data - id, buffer, etc.                           *
*                                                                       *
*   RETURN                                                              *
*       0                                                               *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_y2toy4 (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    unsigned char *srcp,        /* Pointer to source of year data   */
                  inyear[3],    /* Copy year to here                */
                  outdata[10];  /* Construct converted date here    */
    int           year;         /* Converted to integer             */
    size_t        i,            /* Loop counter                     */
                  src_len;      /* Length of source data            */


    /* Find source data, wherever it is */
    cmp_buf_find (pp, pp->args[1], &srcp, &src_len);

    /* Validate it
     * No 2 digit date can have more than 6 digits
     * They all have to be ASCII digits
     */
    if (src_len > 6) {
        cm_error (CM_ERROR, "Expecting max 6 chars in \"%s\", got %u",
                  pp->args[1], src_len);
        return CM_STAT_ERROR;
    }
    for (i=0; i<src_len; i++) {
        if (!isdigit(*(srcp + i))) {
            cm_error (CM_ERROR, "Expecting digits in \"%s\" data, got %.*s",
                      pp->args[1], src_len, srcp);
            return CM_STAT_ERROR;
        }
    }

    /* Examine first two bytes */
    inyear[0] = *srcp;
    inyear[1] = *(srcp + 1);
    inyear[2] = '\0';
    year = atoi ((char *) inyear);

    /* Copy corrected data to temp buf */
    if (year < 35) {
        outdata[0] = '2';
        outdata[1] = '0';
    }
    else {
        outdata[0] = '1';
        outdata[1] = '9';
    }

    /* And original field after that */
    memcpy (outdata + 2, srcp, src_len);

    /* Send it to the output buffer */
    return (cmp_buf_write (pp, pp->args[0], outdata, src_len + 2, 0));

} /* cmp_y2toy4 */


/************************************************************************
* cmp_log ()                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Write something to the log file.                                *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Severity level, one of:                                         *
*           "info"                                                      *
*           "warn"                                                      *
*           "error"                                                     *
*           "fatal"                                                     *
*       Variable number of arguments.                                   *
*           Each is source of data to write, buffer, sf, literal, etc.  *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_log (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    unsigned char *srcp;        /* Pointer to source of year data   */
    char msgbuf[CM_MAX_LOG_MSG],/* Assemble message here            */
                  *msgp,        /* Ptr into msgbuf                  */
                  *sevp;        /* Ptr to severity string           */
    int           ac;           /* Count of args                    */
    size_t        src_len,      /* Length of source data            */
                  msg_len;      /* Total length of message          */
    CM_SEVERITY   severity;     /* Code for cm_error()              */


    /* First arg is severity code */
    sevp = pp->args[0];

    /* Strip optional quotes from it */
    strip_quotes ((char *) sevp, msgbuf, CM_MAX_LOG_MSG);

    /* Determine what it is */
    if (!istrncmp (msgbuf, "fatal", 5)) severity = CM_FATAL;
    else if (!istrncmp (msgbuf, "error", 5)) severity = CM_ERROR;
    else if (!istrncmp (msgbuf, "warn", 4))  severity = CM_WARNING;
    else if (!istrncmp (msgbuf, "info", 4))  severity = CM_NO_ERROR;
    else if (!istrncmp (msgbuf, "cont", 4))  severity = CM_CONTINUE;
    else {
        cm_error (CM_ERROR, "Bad internal error code \"%s\" from ctl table",
                  sevp);
        severity = CM_ERROR;
    }

    /* Assemble all message data to log */
    msgp    = msgbuf;
    msg_len = 0;
    for (ac=1; ac<pp->arg_count; ac++) {

        /* Find source data, wherever it is */
        cmp_buf_find (pp, pp->args[ac], &srcp, &src_len);

        /* Truncate it if it won't fit in the buffer */
        if (msg_len + src_len >= CM_MAX_LOG_MSG - 1)
            src_len = CM_MAX_LOG_MSG - (msg_len + 1);

        /* Copy it into message buffer */
        memcpy (msgp, srcp, src_len);

        /* Ready for next */
        msgp    += src_len;
        msg_len += src_len;
    }

    /* Point back to beginning */
    msgp = msgbuf;

    /* For debugging purposes, log something even if nothing there */
    if (msg_len == 0) {
        msgp    = "(empty)";
        msg_len = 7;
    }

    /* Log it using cm_error function */
    cm_error (severity, "%.*s", msg_len, msgp);

    return CM_STAT_OK;

} /* cmp_log */


/************************************************************************
* cmp_killrec() / cmp_killfld()                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Signal that the current output field is to be deleted, or the   *
*       current record is not to be output at all.                      *
*                                                                       *
*       All subsequent procedures will be short-circuited.              *
*                                                                       *
*       No work is done here.  We merely return codes indicating        *
*       the object to be killed.  Higher level exec_proc routine        *
*       insures that no more procs in the current chain will            *
*       execute and no more data added.  See marcconv.c::exec_proc()    *
*       for details.                                                    *
*                                                                       *
*       As of this time, killing a subfield is not allowed.             *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Completion code.                                                *
************************************************************************/

CM_STAT cmp_killrec (CM_PROC_PARMS *pp)
{
    return CM_STAT_KILL_RECORD;
}

CM_STAT cmp_killfld (CM_PROC_PARMS *pp)
{
    return CM_STAT_KILL_FIELD;
}


/************************************************************************
* cmp_donerec() / cmp_donefld() / cmp_donesf()                          *
*                                                                       *
*   DEFINITION                                                          *
*       Signal completion of a record, field or subfield.               *
*                                                                       *
*       No work is done here.  We merely return codes indicating        *
*       the work to be done.  Higher level exec_proc routine            *
*       insures that no more procs in the current chain will            *
*       execute and no more data added.  See marcconv.c::exec_proc()    *
*       for details.                                                    *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Completion code.                                                *
************************************************************************/

CM_STAT cmp_donerec (CM_PROC_PARMS *pp)
{
    return CM_STAT_DONE_RECORD;
}

CM_STAT cmp_donefld (CM_PROC_PARMS *pp)
{
    return CM_STAT_DONE_FIELD;
}

CM_STAT cmp_donesf (CM_PROC_PARMS *pp)
{
    return CM_STAT_DONE_SF;
}


/************************************************************************
* cmp_renfld ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Change the MARC field id/tag for the current field.             *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       New field ID, a number from 1..999                              *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_renfld (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    int  oldfid,                /* Field id of current field        */
         oldfocc,               /* Current occurrence, unused       */
         oldfpos,               /* Current position, unused         */
         newfid,                /* Change to this field id          */
         stat;                  /* Return from funcs                */


    /* Get id of current field */
    if ((stat = marc_cur_field (pp->outmp, &oldfid, &oldfocc, &oldfpos)) != 0)
        /* Can't happen */
        cm_error (CM_FATAL, "cmp_rename_field: Error %d from marc_cur_field",
                  stat);

    /* Old field id okay, i.e., we have a current field */
    if (oldfid < 1 || oldfid > 999)
        /* Also can't happen */
        cm_error (CM_FATAL, "cmp_rename_field: Old fieldid=%d, can't happen",
                  oldfid);

    /* Get id of new field from config file parm */
    newfid = atoi (pp->args[0]);

    /* Validate */
    if (newfid < 1 || newfid > 999)
        cm_error (CM_FATAL, "cmp_rename_field: Invalid field id %d", newfid);
    if (oldfid < 10 && newfid > 9)
        cm_error (CM_FATAL, "cmp_rename_field: Can't convert fixed field %d "
                  "to variable field %d", oldfid, newfid);
    if (oldfid > 9 && newfid < 10)
        cm_error (CM_FATAL, "cmp_rename_field: Can't convert variable "
                  "field %d to fixed field %d", oldfid, newfid);

    /* Do the deed */
    if ((stat = marc_rename_field (pp->outmp, newfid)) != 0)
        cm_error (CM_FATAL, "Error %d from marc_rename_field", stat);

    return CM_STAT_OK;

} /* cmp_renfld */


/************************************************************************
* cmp_rensf ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Change the subfield code for the current subfield.              *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       New subfield id, a-z or 1-9.                                    *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_rensf (
    CM_PROC_PARMS *pp           /* Pointer to parameter structure   */
) {
    int newsf,                  /* New subfield code                */
        stat;                   /* Return from called funcs         */


    /* Get new subfield code */
    newsf = *pp->args[0];

    /* Validate */
    if (!marc_ok_subfield (newsf))
        cm_error (CM_FATAL, "cmp_rename_sf: Invalid subfield code %c", newsf);

    if ((stat = marc_rename_subfield (pp->outmp, newsf)) != 0)
        cm_error (CM_FATAL, "cmp_rename_sf: Error %d from "
                  "marc_rename_subfield", stat);

    return CM_STAT_OK;

} /* cmp_rensf */


/************************************************************************
* cmp_call ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Call a marcproc function from outside, passing arguments        *
*       which are not in the cm_proc_parm structure.                    *
*                                                                       *
*       Normally, the args pointed to in the proc parm structure        *
*       were taken from the control table.   But an external            *
*       routine wanting to use the marcproc functions wants to          *
*       pass its own arguments.                                         *
*                                                                       *
*       This routine repackages the cm_proc_parms with the              *
*       new arguments and calls the routine.                            *
*                                                                       *
*   PASS                                                                *
*       Pointer to proc parm args from higher level routine.            *
*       Pointer to routine to call, one of cmp_ functions.              *
*       Variable argument 0.                                            *
*       Variable argument 1.                                            *
*        ...                                                            *
*       Variable argument 1.                                            *
*                                                                       *
*   RETURN                                                              *
*       Return code from called function.                               *
************************************************************************/

#define MAXARGS 50          /* Max passed args              */

CM_STAT cmp_call (
    CM_PROC_PARMS *pp,      /* Ptr to parameter struct      */
    CM_FUNCP      procp,    /* Ptr to function to call      */
    int           argc,     /* Number of args passed        */
    ...                     /* Args to pass                 */
) {
    CM_PROC_PARMS cm;       /* Copy of parms with new args  */
    char *newargs[MAXARGS]; /* Copy all new args to here    */
    va_list       pvar;     /* Ptr to variable args         */
    int          i;         /* Loop counter                 */


    /* Copy everything to new version of proc parms */
    cm = *pp;

    /* Validate arg count */
    if (argc < 0 || argc > MAXARGS)
        cm_error (CM_FATAL, "Invalid arg count %d passed to cmp_call", argc);

    /* Override args */
    va_start(pvar, argc);
    for (i=0; i<argc; i++)
        newargs[i] = va_arg(pvar, char *);
    va_end(pvar);

    /* Set new args into new proc_parms */
    cm.args      = newargs;
    cm.arg_count = argc;

    /* Call desired function and hand the return code back to caller */
    return ((procp) (&cm));

} /* cmp_call */


/************************************************************************
* cmp_buf_find ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Get a pointer to and length of data from some source, an        *
*       input MARC record, an internal buffer, or a literal in the      *
*       control file.                                                   *
*                                                                       *
*       Internal subroutine of multiple higher level routines.          *
*                                                                       *
*   PASS                                                                *
*       Pointer to proc parm args from higher level routine.            *
*       Pointer to identifier string for data source.                   *
*       Pointer to place to put pointer to first byte of data.          *
*       Pointer to place to put length of data.                         *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
*       All errors are currently fatal.                                 *
*           Failure to find source data may or may not be an error,     *
*           see comments in the code.                                   *
*           Non-fatal failure results in returning pointer to           *
*           constant null string of 0 length.                           *
************************************************************************/

void cmp_buf_find (
    CM_PROC_PARMS *pp,      /* Ptr to parameter struct      */
    char          *idsrcp,  /* Ptr to source id string      */
    unsigned char **datapp, /* Put ptr to data here         */
    size_t        *lenp     /* Put ptr to data length here  */
) {
    unsigned char *p;       /* Temp pointer                 */
    size_t        buf_len;  /* Length of named buffer       */
    int           stat,     /* Return from called func      */
                  fid,      /* Field id                     */
                  focc,     /* Field occurrence number      */
                  sid,      /* Subfield id                  */
                  socc,     /* Occurrence number            */
                  id,       /* Generic field or sf id       */
                  occ,      /* Generic occurrence number    */
                  pos,      /* Unused parm for marc_cur...  */
                  bvar;     /* Value of builtin variable    */

    static char s_bvarbuf[20]; /* Put builtin variable here */


    /* Find source data using id */
    switch (get_src_type (idsrcp)) {

        case CM_ID_UNKNOWN:
            /* Error in control table */
            cm_error (CM_FATAL, "Can't parse reference %s from ctl table",
                      idsrcp);

        case CM_ID_MARC:
            /* Parse marc reference string */
            if ((stat = marc_ref (idsrcp, &fid, &focc, &sid, &socc)) != 0)

                /* Bad reference string, fatal error */
                cm_error (CM_FATAL, "cmp_buf_read: Error %d parsing marc "
                          "reference \"%s\"", stat, idsrcp);

            /* If referencing current occurrence, get it from input rec */
            if (fid == MARC_REF_CURRENT || focc == MARC_REF_CURRENT) {

                /* Get field stats */
                if ((stat = marc_cur_field (pp->inmp, &id, &occ, &pos)) != 0)
                    cm_error (CM_FATAL, "cmp_buf_find: Error %d getting "
                              "current field", stat);
                if (fid == MARC_REF_CURRENT)
                    fid = id;
                if (focc == MARC_REF_CURRENT)
                    focc = occ;
            }

            /* Process any symbolic parts of subfield reference */
            if (sid == MARC_REF_CURRENT || socc == MARC_REF_CURRENT) {

                /* Get field stats */
                if ((stat = marc_cur_subfield (pp->inmp, &id, &occ, &pos))!=0)
                    cm_error (CM_FATAL, "cmp_buf_find: Error %d getting "
                              "current subfield", stat);
                if (sid == MARC_REF_CURRENT)
                    sid = id;
                if (socc == MARC_REF_CURRENT)
                    socc = occ;
            }

            /* Can't request new when searching existing record */
            if (focc == MARC_REF_NEW || socc == MARC_REF_NEW)
                cm_error (CM_FATAL, "cmp_buf_find: Can't request new occ in"
                          " existing record - %s", idsrcp);

            /* Try to find it in input MARC record */
            if ((stat = marc_get_item (pp->inmp, fid, focc, sid, socc,
                                       datapp, lenp)) < 0)

                /* Fatal marc structure error */
                cm_error (CM_FATAL, "marc_get_item returned %d on input "
                          "fid=%d focc=%d sid=%d socc=%d", stat,
                          fid, focc, sid, socc);

            /* Input data not found is not an error */
            if (stat > 0) {
                /* If requested more than there, that's fine
                 * Otherwise (request off end, or request for field
                 *   which doesn't exist) use nullstring.
                 */
                if (stat != MARC_RET_FX_LENGTH) {
                    *datapp = (unsigned char *) "";
                    *lenp   = 0;
                }
            }

            /* Else we've found what we want, datapp and *lenp are set */
            break;

        case CM_ID_BUF:
            /* Get it from a named buffer */
            if (cmp_get_named_buf (idsrcp, datapp, 0, 0, &buf_len) != 0) {

                /* Request for non-existent switch same as if switch off */
                if (*idsrcp == '&')
                    *datapp = (unsigned char *) "";
                else
                    /* Attempt to read a non-existent buffer
                     *   is probably due to bug in control table.
                     * Anyway, we'll assume it is.
                     */
                    cm_error (CM_FATAL, "Buffer %s not found for reading",
                              idsrcp);
            }

            /* Length of data there */
            *lenp = strlen ((char *) (*datapp));
            break;

        case CM_ID_CURRENT:
            /* Data comes from proc buffer, may have originated in inmp */
            *datapp = pp->bufp;
            *lenp   = strlen ((char *) (*datapp));
            break;

        case CM_ID_BUILTIN:
            /* Get requested builtin, as an integer */
            bvar = cmp_get_builtin (pp, idsrcp);

            /* Return it as a string
             * ASSUMPTION:
             *   It's not possible to get two builtin variables before
             *   the first one is used.
             *   We're using just one buffer.
             */
            sprintf (s_bvarbuf, "%d", bvar);
            *datapp = (unsigned char *) s_bvarbuf;
            *lenp   = strlen (s_bvarbuf);
            break;

        case CM_ID_LITERAL:
            /* Data from control table passed directly by caller.
             * It starts after the " and continues till next quote.
             * We assume second quote has been found and verified at
             *   table load time and need not be checked for here.
             */
            p = (unsigned char *) idsrcp + 1;
            *datapp = p;
            while (*p != '"')
                ++p;
            *lenp = p - *datapp;
            break;
        default:
            break;
    }
} /* cmp_buf_find */


/************************************************************************
* cmp_buf_write ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Write data to an output MARC record, or internal buffer.        *
*                                                                       *
*       Internal subroutine of multiple higher level routines.          *
*                                                                       *
*   PASS                                                                *
*       Pointer to proc parm args from higher level routine.            *
*       Pointer to identifier string for data destination.              *
*       Pointer to data to be written.                                  *
*       Length of data to write.                                        *
*       Append indicator:                                               *
*           True  = append source to destination.                       *
*           False = copy, overwriting destination.                      *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
*       All errors are currently fatal.                                 *
************************************************************************/

#define TMPBUF_SIZE 10000   /* No marc field can be larger  */

CM_STAT cmp_buf_write (
    CM_PROC_PARMS *pp,      /* Ptr to parameter struct      */
    char          *iddestp, /* Ptr to destination id string */
    unsigned char *datap,   /* Ptr to data to write         */
    size_t        data_len, /* Length of data to write      */
    int           append    /* Non-zero = append data       */
) {
    unsigned char *destp;   /* Ptr to destination data      */
    size_t        dest_len, /* Len data at destp            */
                  buf_len;  /* Length of named buffer       */
    int           stat,     /* Return from called func      */
                  find_stat,/* From attempt to find dest.   */
                  fid,      /* Field id                     */
                  focc,     /* Field occurrence number      */
                  sid,      /* Subfield id                  */
                  socc,     /* Occurrence number            */
                  new_field,/* True=Added a new field to rec*/
                  id,       /* Generic field or sf id       */
                  occ,      /* Generic occurrence number    */
                  pos;      /* Unused parm for marc_cur...  */

    /* Temporary copy buffer */
    static unsigned char s_tmpbuf[TMPBUF_SIZE];

    /* Output to destination */
    switch (get_src_type (iddestp)) {

        case CM_ID_LITERAL:
            /* Error in control table */
            cm_error (CM_FATAL,
                      "cmp_buf_write: Can't write to \"%s\" from ctl table",
                      iddestp);

        case CM_ID_MARC:
            /* Writing 0 bytes to a marc record ain't allowed */
            if (data_len < 1)
                break;

            /* Parse marc reference string */
            if ((stat = marc_ref (iddestp, &fid, &focc, &sid, &socc)) != 0)

                /* Bad reference string, fatal error */
                cm_error (CM_FATAL,
                        "cmp_buf_write: Error %d parsing marc reference = "
                        "\"%s\"", stat, iddestp);

            /* Assume we're positioned to the field we want */
            find_stat = 0;
            new_field = 0;

            /* If tain't so, then resolve symbolics and find field */
            if (fid != MARC_REF_CURRENT || focc != MARC_REF_CURRENT) {

                /* Resolve any symbolics */
                if (fid == MARC_REF_CURRENT || focc == MARC_REF_CURRENT) {
                    if ((stat = marc_cur_field (pp->outmp, &id, &occ, &pos))
                             != 0)
                        cm_error (CM_FATAL, "cmp_buf_write: Error %d getting "
                                  "current field stats for \"%s\"", stat, iddestp);
                    if (fid == MARC_REF_CURRENT)
                        fid = id;
                    if (focc == MARC_REF_CURRENT)
                        focc = occ;
                }

                /* If not definitely adding a new field, look for it */
                if (focc != MARC_REF_NEW) {

                    /* Search record */
                    if ((find_stat = marc_get_field (pp->outmp, fid, focc,
                                     &destp, &dest_len)) < 0)
                        cm_error (CM_FATAL, "cmp_buf_write: Error %d getting "
                                  "current field \"%s\"", find_stat, iddestp);
                }

                /* Or create it */
                if (focc == MARC_REF_NEW || find_stat != 0) {

                    /* Any error adding field is fatal.
                     * If caller asked for occ==2 and there aren't any
                     *   yet, it's a bug in his script.
                     * That's reason enough for a fatal error (so say I.)
                     */
                    if ((stat = marc_add_field (pp->outmp, fid)) != 0)
                        cm_error (CM_FATAL,
                                  "cmp_buf_write: Error %d adding field %d",
                                  stat, fid);

                    /* Created a new field */
                    new_field = 1;

                    /* If we created a new field, caller should be
                     *   trying to add subfield occ 0, or NEW
                     *   (assuming a variable length field.)
                     * Again, we'll call this script error fatal.
                     */
                    if (fid >= 10) {
                        /* Script may have implied occurrence.
                         * For new field, it's always 0.
                         */
                        if (socc == MARC_REF_CURRENT)
                            socc = 0;

                        /* Test */
                        if (socc != 0 && socc != MARC_REF_NEW) {
                            cm_error (CM_FATAL,
                              "cmp_buf_write: Attempt to add subfield"
                              " %d, invalid occurrence %d, to new field %d",
                              sid, socc, fid);
                        }
                    }
                }
            }

            /* We should now be sitting on the correct field
             * If working with variable length subfields, then if
             *   we have an existing field we may have to position
             *   to the subfield we want.
             */
            if (!new_field && fid >= 10) {

                /* Resolve any symbolics */
                if (sid == MARC_REF_CURRENT || socc == MARC_REF_CURRENT) {
                    if ((stat = marc_cur_subfield (pp->outmp, &id, &occ,
                                                   &pos))!=0)
                        cm_error (CM_FATAL, "cmp_buf_write: Error %d getting "
                                  "current subfield stats for \"%s\"", stat, iddestp);
                    if (sid == MARC_REF_CURRENT)
                        sid = id;
                    if (socc == MARC_REF_CURRENT)
                        socc = occ;
                }

                /* Position to subfield.
                 * We may in fact already be there, but it will be safe
                 *   to position again.
                 */
                if ((find_stat = marc_get_subfield (pp->outmp, sid, socc,
                                &destp, &dest_len)) < 0)
                    cm_error (CM_FATAL, "cmp_buf_write: Error %d getting "
                              "current subfield for \"%s\"", find_stat, iddestp);

                /* If appending to existing sf, save what we've got */
                if (append && find_stat == 0) {

                    /* Check lengths */
                    if (data_len + dest_len >= TMPBUF_SIZE)
                        cm_error (CM_FATAL,
                           "cmp_buf_write: Impossible output length=%u ",
                           "fid=%d, focc=%d, sid=%d, socc=%d",
                            data_len + dest_len, fid, focc, sid, socc);

                    /* Copy to temp buf */
                    memcpy (s_tmpbuf, destp, dest_len);

                    /* Append new data */
                    memcpy (s_tmpbuf + dest_len, datap, data_len);

                    /* Now have a new source */
                    datap     = s_tmpbuf;
                    data_len += dest_len;

                    /* Delete source */
                    if ((stat = marc_del_subfield (pp->outmp)) != 0)
                        cm_error (CM_FATAL,
                              "cmp_buf_write: Error %d deleting subfield "
                              "fid=%d, focc=%d, sid=%d, socc=%d",
                              stat, fid, focc, sid, socc);
                }
            }

            /* (Re-)add subfield with passed (possibly appended) data */
            if ((stat = marc_add_subfield (pp->outmp,sid,datap,data_len))!=0)
                cm_error (CM_FATAL,
                    "cmp_buf_write: Error %d adding subfield %c (decimal %d) to field %d",
                    stat, sid, sid, fid);

            /* That's it for most complicated case */
            break;

        case CM_ID_BUF:
            /* Find or create named buffer to receive output */
            if (cmp_get_named_buf (iddestp, &destp, 1, data_len+1,
                                   &buf_len) != 0)
                cm_error (CM_FATAL,
                      "cmp_buf_write: Could not find or create buffer \"%s\"",
                      iddestp);

            /* If appending, need to check for extra space */
            if (append) {
                dest_len = strlen ((char *) destp);
                if (buf_len < data_len + dest_len) {
                    if (cmp_get_named_buf (iddestp, &destp, 1, data_len +
                                       dest_len + 1, &buf_len) != 0)
                        cm_error (CM_FATAL,
                              "cmp_buf_write: Could not increase buffer size "
                              "to % for \"%s\"", data_len + dest_len + 1, iddestp);
                }

                /* New destination is after existing data */
                destp += dest_len;
            }

            /* Copy in data */
            memcpy (destp, datap, data_len);
            *(destp + data_len) = '\0';

            break;

        case CM_ID_CURRENT:
            /* Using caller supplied buffer */
            destp = pp->bufp;

            /* Copy after if so specified */
            if (append)
                destp += strlen ((char *) destp);

            /* Test for space
             * Shouldn't ever fail
             */
            if (data_len + (size_t) (destp - pp->bufp) >= pp->buflen)
                cm_error (CM_FATAL,
                          "cmp_buf_write: Data too large for internal buffer for \"%s\"", iddestp);

            /* Copy it in */
            memcpy (destp, datap, data_len);
            *(destp + data_len) = '\0';

            break;

        case CM_ID_BUILTIN:
            /* Illegal, can't write to these */
            cm_error (CM_FATAL, "cmp_buf_write: Can't write to \"%s\"",
                      iddestp);
            break;

        case CM_ID_UNKNOWN:
            /* Illegal */
            cm_error (CM_FATAL, "cmp_buf_write: destination \"%s\" "
                      "unrecognized", iddestp);
            break;
        default:
            break;
    }

    return CM_STAT_OK;

} /* cmp_buf_write */


/************************************************************************
* cmp_buf_copy ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Get data from some source, an input MARC record, an internal    *
*       buffer, or a literal in the control file, and copy or append    *
*       it to some destination.                                         *
*                                                                       *
*       Internal routine to do the real work of cmp_copy and cmp_append.*
*                                                                       *
*   PASS                                                                *
*       Pointer to proc parm args from higher level routine.            *
*       Pointer to identifier string for data destination.              *
*       Pointer to identifier string for data source.                   *
*       Append indicator:                                               *
*           True  = append source to destination.                       *
*           False = copy, overwriting destination.                      *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*           Failure to find source data may or may not be an error,     *
*           see comments in cmp_buf_find() subroutine.                  *
************************************************************************/

CM_STAT cmp_buf_copy (
    CM_PROC_PARMS *pp,  /* Ptr to parameter struct      */
    char *iddestp,      /* Ptr to destination id string */
    char *idsrcp,       /* Ptr to source id string      */
    int  append         /* Non-zero = append data       */
) {
    unsigned char *srcp;/* Ptr to source data           */
    size_t src_len;     /* Length source data, no null  */


    /* Find data source */
    cmp_buf_find (pp, idsrcp, &srcp, &src_len);

    /* Output to destination */
    return (cmp_buf_write (pp, iddestp, srcp, src_len, append));

} /* cmp_buf_copy */


/************************************************************************
* get_src_type ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Get the type of source to or from which data will be processed. *
*                                                                       *
*   PASS                                                                *
*       Pointer to identifier string for data source.                   *
*                                                                       *
*   RETURN                                                              *
*       One of CM_ID_... identifiers.                                   *
************************************************************************/

static CM_ID get_src_type (
    char *idstrp        /* Ptr to id string     */
) {
    int  c;             /* First char of id     */


    /* Categorization done partly by looking at first char */
    c = (int) *idstrp;

    /* MARC field or subfield identifier */
    if (isdigit(c) || c == '$' || c == '@')
        return CM_ID_MARC;

    /* Current data, or descriptor of same */
    if (c == '%') {
        if (!strcmp (idstrp, "%data"))
            return CM_ID_CURRENT;
        else
            return CM_ID_BUILTIN;
    }

    /* Literal text right here */
    if (c == '"')
        return CM_ID_LITERAL;

    /* Named buffer or switch (which is internally a named buffer) */
    if (isalpha(c) || c == '&')
        return CM_ID_BUF;

    /* That's all we allow */
    return CM_ID_UNKNOWN;

} /* get_src_type */


/************************************************************************
* cmp_get_named_buf ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Internal routine to find or create a named buffer.              *
*                                                                       *
*   PASS                                                                *
*       Pointer to buffer name string, max 32 chars.                    *
*       Pointer to place to put pointer to buffer.                      *
*       Create indicator:                                               *
*           True  = Create buffer if not found.                         *
*           False = Don't create.                                       *
*       Minimum desired buffer length.                                  *
*           Pass 0 if reading.                                          *
*           Pass length needed if writing.                              *
*           Will realloc if need.                                       *
*       Pointer to place to put current buffer length.                  *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

#define MAX_NAME_BUFS 60
#define MAX_BNAME     32

CM_STAT cmp_get_named_buf (
    char   *namep,              /* Ptr to name          */
    unsigned char **bufpp,      /* Put ptr to buf here  */
    int    create,              /* True=Create if need  */
    size_t minlen,              /* Min if create        */
    size_t *lenp                /* Current buffer len   */
) {
    static struct {
        char name[MAX_BNAME];   /* Buffer name          */
        unsigned char *bufp;    /* Buffer pointer       */
        size_t size;            /* Num bytes            */
    } s_bufs[MAX_NAME_BUFS];
    static int s_bufcnt;        /* Num in use           */

    int i,                      /* Loop counter         */
        found;                  /* True=have a buffer   */

    /* Search for existing buffer */
    found = 0;
    for (i=0; i<s_bufcnt; i++) {

        /* Fast, then slow compare */
        if (*namep == s_bufs[i].name[0] && !strcmp (namep, s_bufs[i].name)) {

            /* Found it.  Enlarge if necessary */
            if (s_bufs[i].size < minlen) {

#ifdef DEBUG
                if ((s_bufs[i].bufp = marc_realloc(s_bufs[i].bufp, minlen, 601)) == NULL) {  //TAG:601
                    cm_error (CM_FATAL, "Unable to realloc for %u bytes " "for named buffer %s", minlen, namep);
                }
#else
                if ((s_bufs[i].bufp = realloc(s_bufs[i].bufp, minlen)) == NULL) {
                    cm_error (CM_FATAL, "Unable to realloc for %u bytes " "for named buffer %s", minlen, namep);
                }
#endif
                s_bufs[i].size = minlen;
            }
            found = 1;
            break;
        }
    }

    /* If not found, then create if desired */
    if (i == s_bufcnt && create) {

        /* Too many? */
        if (s_bufcnt >= MAX_NAME_BUFS)
            cm_error (CM_FATAL, "Too many buffers, adding name=%s", namep);

        /* Create */
#ifdef DEBUG
        if ((s_bufs[i].bufp = marc_alloc(minlen, 602)) == NULL) {  //TAG:602
            cm_error (CM_FATAL, "Unable to malloc for %u bytes " "for named buffer %s", minlen, namep);
        }
#else
        if ((s_bufs[i].bufp = malloc (minlen)) == NULL) {
            cm_error (CM_FATAL, "Unable to malloc for %u bytes " "for named buffer %s", minlen, namep);
        }
#endif
        strncpy (s_bufs[i].name, namep, MAX_BNAME - 1);
        *s_bufs[i].bufp = '\0';
        s_bufs[i].size = minlen;

        ++s_bufcnt;
        found = 1;
    }

    /* Data for caller */
    if (found) {
        *bufpp = s_bufs[i].bufp;
        *lenp  = s_bufs[i].size;
        return CM_STAT_OK;
    }
    return CM_STAT_ERROR;

} /* cmp_get_named_buf */


/************************************************************************
* cmp_get_builtin ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Internal routine to derive the value of a builtin variable.     *
*                                                                       *
*       All builtins are integers.                                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to current context to access input record.              *
*       Pointer to name of builtin, e.g., %focc.                        *
*                                                                       *
*   RETURN                                                              *
*       Value of builtin.                                               *
*       -1 if builtin not accessible.                                   *
************************************************************************/

int cmp_get_builtin (
    CM_PROC_PARMS *pp,      /* Ptr to parameter struct  */
    char          *namep    /* Ptr to name              */
) {
    int           okay,     /* No error in passed name  */
                  id,       /* Field or subfield id     */
                  occ,      /* Field or subfield occ    */
                  pos,      /* Field or subfield pos    */
                  stat,     /* From called subroutine   */
                  value;    /* Return value for caller  */


    okay = 1;

    /* We handle specific cases, with minimal error checking on names
     *   %fid, %focc, %fpos
     */
    if (namep[1] == 'f') {

        /* Get info from input record */
        if ((stat = marc_cur_field (pp->inmp, &id, &occ, &pos)) != 0)
            cm_error (CM_FATAL, "cmp_get_builtins: Error %d getting curfield",
                      stat);

        /* What did caller want? */
        switch (namep[2]) {
            case 'i': value = id;  break;
            case 'o': value = occ; break;
            case 'p': value = pos; break;
            default:
                okay = 0;
        }
    }

    /*   %sid, %socc, %spos */
    else if (namep[1] == 's') {

        /* Get info from input record */
        if ((stat = marc_cur_subfield (pp->inmp, &id, &occ, &pos)) != 0)
            cm_error (CM_FATAL, "cmp_get_builtins: Error %d getting curfield",
                      stat);

        switch (namep[2]) {
            case 'i': value = id;  break;
            case 'o': value = occ; break;
            case 'p': value = pos; break;
            default:
                okay = 0;
        }
    }

    else
        okay = 0;

    if (!okay)
        cm_error (CM_FATAL, "Unknown builtin %s requested", namep);

    return value;

} /* cmp_get_builtin */


/*********************************************************************
*                      Specialized Subroutines                       *
*                                                                    *
* Subroutines above this point are generalized routines which may    *
* apply to multiple input and output fields.  The routines below     *
* are specialized for converting specific Medline input fields.      *
*********************************************************************/


/************************************************************************
* cmp_enMesh ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Process a Medline MeSH heading to produce the following         *
*       outputs:                                                        *
*           650$a = Mesh heading.                                       *
*              i1 = 0 for regular heading, 1 for starred heading.       *
*              i2 = 2 indicating NLM subject heading.                   *
*       For each subheading/qualifier:                                  *
*           Create another MeSH heading, indic2 = 2.                    *
*           Create a $x subheading for qualifier, value is expanded     *
*               text of the qualifier, in lower case.                   *
*           If qualifier is starred, set indic1 = 1, else 0.            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   ASSUMPTIONS                                                         *
*    1. Must have an accurate, sorted, lowercased, qualifier table      *
*       in form:                                                        *
*           XX=expanded qualifier                                       *
*       one per line.                                                   *
*                                                                       *
*    2. This function is assumed to be attached as a post process       *
*       as follows:                                                     *
*           field 351=650                                               *
*           1=a                                                         *
*           post=indic/2/2                                              *
*           post=mesh                                                   *
*                                                                       *
*        Input pp->bufp therefor = ptr to heading.                      *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_enMesh (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
    int     sfocc,          /* Subfield 4 occurrence counter        */
            starred,        /* True=starred heading or subheading   */
            stat;           /* Return from marc calls               */
    unsigned char *insfp,   /* Ptr to subfield data from get subf   */
           *exp;            /* Ptr to subfield expansion string     */
    size_t insflen,         /* Length of input subfield             */
           explen;          /* Length subfield expansion string     */


    static QUAL_TBL *s_quals = NULL;    /* Qual->string lookup table   */
    static size_t   s_qual_cnt;         /* Num entries in table        */


    /* Load qualifier table if not yet done */
    if (!s_quals)
        load_quals (&s_quals, &s_qual_cnt);

    /* We should be positioned at '1' subfield (heading)
     *   of the input MeSH record, and the 'a' subfield of
     *   the output record.
     * Default indicators already filled in.
     * Now process any occurrences of subfield 4, qualifier
     */
    sfocc = 0;
    while (marc_get_subfield (pp->inmp, '4', sfocc, &insfp, &insflen) == 0) {

        /* If '*' prefixes subfield, this is a primary heading */
        if (*insfp == '*') {
            starred = 1;
            ++insfp;
            --insflen;
        }
        else
            starred = 0;

        /* If qualifier present, not just single '*' */
        if (insflen == 2) {

            /* Create a new heading + subheading combination */
            if ((stat = marc_add_field (pp->outmp, 650)) != 0) {
                cm_error (CM_ERROR, "Error %d creating 650", stat);
                return CM_STAT_ERROR;
            }

            /* Subfield $a = heading */
            if ((stat = marc_add_subfield (pp->outmp, 'a', pp->bufp,
                                       strlen ((char *) pp->bufp))) != 0) {
                cm_error (CM_ERROR, "Error %d creating 650$a", stat);
                return CM_STAT_ERROR;
            }

            /* Subfield $x = expanded qualifier */
            if (lookup_qual (s_quals, s_qual_cnt, insfp, &exp, &explen) != 0) {
                cm_error (CM_ERROR, "Couldn't find qualifier %c%c",
                          *insfp, *(insfp + 1));
                return CM_STAT_ERROR;
            }
            if ((stat = marc_add_subfield (pp->outmp, 'x', exp, explen))
                     != 0) {
                cm_error (CM_ERROR, "Error %d creating 650$x", stat);
                return CM_STAT_ERROR;
            }
        }

        /* Starred main or sub heading requires indic1=1 */
        if (starred) {
            if ((stat = marc_set_indic (pp->outmp, 1, '1')) != 0) {
                cm_error (CM_ERROR, "Error %d setting 650 indic 1", stat);
                return CM_STAT_ERROR;
            }
        }

        /* Indicator 2=2 indicates NLM */
        if ((stat = marc_set_indic (pp->outmp, 2, '2')) != 0) {
            cm_error (CM_ERROR, "Error %d setting 650 indic 2", stat);
            return CM_STAT_ERROR;
        }

        /* Next occurrence of subheading subfield */
        ++sfocc;
    }

    return CM_STAT_OK;

} /* cmp_enMesh */


/************************************************************************
* load_quals ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Load a table of MeSH qualifiers.                                *
*                                                                       *
*       Qualifiers are loaded into an area of contiguous memory         *
*       with the following structure:                                   *
*                                                                       *
*           Array of:                                                   *
*               2 byte qualifier.                                       *
*               Pointer to expanded string.                             *
*                                                                       *
*       Followed immediately by:                                        *
*           Contiguous null terminated strings.                         *
*                                                                       *
*   PASS                                                                *
*       Pointer to place to put pointer to array of qual structures.    *
*       Pointer to place to put number of qualifiers loaded.            *
*                                                                       *
*   ASSUMPTIONS                                                         *
*    1. Qualifiers are in the file named by the environment variable    *
*       MESHQUALFILE.  If the variable is not set, we use the           *
*       filename "meshqual" in the current directory.                   *
*                                                                       *
*    2. Qualifiers should be presorted, in the following format,        *
*       without leading or trailing spaces.                             *
*          QA=expanded form of this qualifier                           *
*                                                                       *
*   NOTES                                                               *
*       There is no requirement at this time for a general purpose      *
*       lookup table.  If one is needed, we should change this          *
*       load a more general structure which uses a char ptr instead     *
*       of a fixed 2 char key.                                          *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
*       Exit with fatal error if we cannot return loaded qualifiers.    *
************************************************************************/

#define QLSIZE 128      /* More than max size 1 line*/

void load_quals (
    QUAL_TBL **qtpp,    /* Put ptr to table here    */
    size_t   *countp    /* Put num entries here     */
) {
    FILE     *fp;       /* Ptr to qualifier file    */
    char     *fname,    /* Filename                 */
             *textp,    /* Ptr to text of exp. qual */
          buf[QLSIZE];  /* For one line from file   */
    size_t   entries,   /* Number of entries in tbl */
             bytes;     /* Total bytes to alloc     */
    QUAL_TBL *qtp;      /* Ptr into table           */


    /* Try to get filename from environment */
    if ((fname = getenv ("MESHQUALFILE")) == NULL)
        fname = "meshqual";

    if ((fp = fopen (fname, "r")) == NULL) {
        perror (fname);
        cm_error (CM_FATAL, "Can't open qualifier file %s", fname);
    }

    /* Get statistics on file:
     *   Number of lines
     *   Size of all strings
     *   Total number of bytes of memory we need
     * Makes a pass through file
     */
    entries = 0;
    bytes   = 0;
    while (fgets (buf, sizeof(buf)-1, fp) != NULL) {

        /* Read one entry from file */
        ++entries;

        /* Check format is XX=xxxxx... */
        if (buf[2] != '=')
            cm_error (CM_FATAL, "Missing '=' qual file line %u", entries);
        if (!isupper(buf[0]) || !isupper(buf[1]))
            cm_error (CM_FATAL, "Expecting uppercase qualifier, "
                                "qual file line %u", entries);
        if (!islower(buf[3]))
            cm_error (CM_FATAL, "Expecting lowercase string, "
                                "qual file line %u", entries);

        /* Add to num bytes required
         * Bytes read
         * -1 for equal sign which we don't keep
         * -2 for qualifier abbreviation, moved to structure
         * Plus size of structure itself
         */
        bytes += (strlen (buf) -3 + sizeof(QUAL_TBL));
    }

    /* Allocate space, table at front, strings at end */

#ifdef DEBUG
    if ((qtp = (QUAL_TBL *) marc_alloc(bytes, 603)) == NULL) { //TAG:603
        cm_error (CM_FATAL, "Qualifier table memory");
    }
#else
    if ((qtp = (QUAL_TBL *) malloc (bytes)) == NULL) {
        cm_error (CM_FATAL, "Qualifier table memory");
    }
#endif

    *qtpp = qtp;
    textp = (char *) (qtp + entries);

    /* Re-read file, loading the table */
    rewind (fp);
    while (fgets (buf, sizeof(buf)-1, fp) != NULL) {

        /* Qualifier */
        qtp->qual[0] = buf[0];
        qtp->qual[1] = buf[1];

        /* Expansion starts after "XX=" */
        qtp->string  = textp;
        strcpy (qtp->string, buf + 3);

        /* Remove trailing newline & blanks */
        textp += strlen (textp) - 1;
        while (isspace(*textp))
            *textp-- = '\0';

        /* Point after null delimiter for next string */
        textp      += 2;
        qtp->lenexp = (size_t) (textp - qtp->string);

        /* Next struct */
        ++qtp;
    }

    /* Array is probably already sorted, but just in case */
    qsort ((void *) *qtpp, entries, sizeof(QUAL_TBL), compare_quals);

    fclose (fp);

    *countp = entries;

} /* load_quals */


/************************************************************************
* lookup_qual ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Find the expanded string corresponding to a 2 char qualifier    *
*       in the table loaded by load_quals().                            *
*                                                                       *
*   PASS                                                                *
*       Ptr to qualifier table.                                         *
*       Number of entries in table.                                     *
*       Ptr to 2 char qualifier abbreviation.                           *
*       Ptr to to place to put pointer to expanded string.              *
*       Ptr to place to put length of string.                           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success                                                     *
*       Else qualifier not found.                                       *
************************************************************************/

static int lookup_qual (
    QUAL_TBL *qtp,          /* Pointer to table             */
    size_t   tbllen,        /* Num entries in table         */
    unsigned char *qualp,   /* Pointer to qualifier to find */
    unsigned char **expp,   /* Put pointer to expansion here*/
    size_t   *explen        /* Put length here              */
) {
    QUAL_TBL *qp;           /* Pointer to entry found in tbl*/


    /* Binary search the table */
    if ((qp = (QUAL_TBL *) bsearch ((void *) qualp, (void *) qtp,
              tbllen, sizeof(QUAL_TBL), compare_quals)) == NULL) {
        *expp = NULL;
        return -1;
    }

    /* Set return values */
    *expp   = (unsigned char *) qp->string;
    *explen = qp->lenexp;

    return 0;

} /* lookup_qual */


/************************************************************************
* compare_quals ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Compare two qualifiers for qsort or bsearch.                    *
*                                                                       *
*   PASS                                                                *
*       Ptr to first.                                                   *
*       Ptr to second.                                                  *
*                                                                       *
*   RETURN                                                              *
*       See qsort() or bsearch().                                       *
************************************************************************/

static int compare_quals (
    const void *q1p,
    const void *q2p
) {
    /* Only need to compare first two chars
     * They must be unique
     */
    if (*((char *) q1p) != (*((char *) q2p)))
        return (*((char *) q1p) - (*((char *) q2p)));
    return (*((char *) q1p + 1) - (*((char *) q2p + 1)));

} /* compare_quals */


/************************************************************************
* cmp_f606 ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       If an input 606 field is found then                             *
*           If there is a slash embedded in the data:                   *
*               Data after the slash becomes 693$a.                     *
*               Data before the slash becomes 693$z.                    *
*                                                                       *
*       Note: if more than 30 606's, the last field has the single      *
*       value '+'.  We may want special processing for this, but        *
*       we're not doing any at this time.                               *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success, no errors possible.                                *
************************************************************************/

CM_STAT cmp_f606 (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure   */
) {
    unsigned char *srcp,    /* Ptr to 606 data                  */
                  *slashp;  /* Ptr to slash in midst of data    */
    size_t        alen,     /* Length of $a subfield            */
                  zlen,     /* Length of $z subfield            */
                  src_len;  /* Length of source 606 field       */
    int           occ;      /* Field occurrence counter         */
    char          idbuf[40];/* Construct reference to field here*/


    /* Read each 606 field */
    for (occ=0; ; occ++) {

        /* Try to fetch 606 */
        sprintf (idbuf, "606:%d$1:0", occ);
        cmp_buf_find (pp, idbuf, &srcp, &src_len);

        /* If we got one, parse on / */
        if (src_len) {
            if ((slashp = (unsigned char *)
                           nstrstr ((char *) srcp, "/", src_len)) == NULL) {

                /* No slash found, add entire field as subfield z */
                sprintf (idbuf, "693:%d$z:0", occ);
                cmp_buf_write (pp, idbuf, srcp, src_len, 0);
            }
            else {
                /* Slash found, add a and z fields */
                zlen = (size_t) (slashp - srcp);
                alen = src_len - (zlen + 1);
                sprintf (idbuf, "693:%d$a:0", occ);
                cmp_buf_write (pp, idbuf, slashp+1, alen, 0);
                sprintf (idbuf, "693:%d$z:0", occ);
                cmp_buf_write (pp, idbuf, srcp, zlen, 0);
            }
        }
        else
            /* End of 606's (may have been none) */
            break;
    }

    return CM_STAT_OK;

} /* cmp_f606 */
