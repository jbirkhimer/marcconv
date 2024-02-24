/* SCCS: %M%  VER: %I%  MOD: %G%  EXTRACT: %H% */

/*********************************************************************
*   amuopt ()                                                        *
*                                                                    *
*   DESCRIPTION:                                                     *
*       Get the next option or argument from the command line.       *
*                                                                    *
*       Options have the following characteristics:                  *
*                                                                    *
*           Begin with an option indicator character:                *
*               '-' on UNIX                                          *
*               '-' or '/' on other systems which do not reserve     *
*                          the slash char for directory indication.  *
*           Followed by a single option character:                   *
*               Characters are case sensitive.                       *
*               Must be found in options control string as in UNIX.  *
*           May be followed by string argument, as in UNIX.          *
*                                                                    *
*       Option control strings contain:                              *
*           Representation of each option letter which is valid.     *
*           ':' after each option which requires an argument.        *
*           Leading '-' if errors are to be returned rather than     *
*           processed internally to amuopt().                        *
*                                                                    *
*       Non-option arguments are ordinary command line arguments,    *
*       not preceded by a '-' or '/'.                                *
*                                                                    *
*   PASS                                                             *
*       argc - from the command line.                                *
*       argv -   "   "     "      "                                  *
*       Option control string.                                       *
*       Pointer to place to return option string argument.           *
*                                                                    *
*   RETURNS:                                                         *
*       Option character, or:                                        *
*       Position on command line of non-option argument.             *
*           1 = First non-option argument.                           *
*           2 = Second non-option argument.                          *
*           etc., or                                                 *
*       AMU_OPT_DONE   = No more data on command line.               *
*       AMU_OPT_BAD    = Unrecognized option char.                   *
*       AMU_OPT_NO_ARG = Option char recognized, but missing arg.    *
*                                                                    *
*                                   Copyright 1991, AM Systems, Inc. *
*                                   All rights reserved              *
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "amuopt.h"

#if defined(__MSC_VER) || defined( __BORLANDC__) || defined(__WIN32__)
        /* Win32 or DOS compiler */
#define OPT_SWITCH(x) ((x) == '/' || (x) == '-')
#else
        /* UNIX compiler?, slash reserved for directories */
#define OPT_SWITCH(x) ((x) == '-')
#endif


static int nextc  = 1;      /* argv to use, starts with first one   */
static int nooptc = 0;      /* Count for non-option args            */

int amuopt (
    int  argc,      /* Arg count for command line   */
    char **argv,    /* Ptr array of arg ptrs        */
    char *optctl,   /* Options control string       */
    char **valptr   /* Value of option string       */
) {
    char opt_char,  /* Option character from cmdline*/
         *nextv,    /* argv we are processing       */
         *msg;      /* Error message                */
    int  abort_err, /* True=Abort on error          */
         retcode;   /* Return code                  */


    /* Nothing found yet */
    retcode = AMU_OPT_DONE;
    *valptr = NULL;

    /* Only continue if unprocessed arguments remain */
    if (nextc < argc) {

        /* Point to argument */
        nextv = argv[nextc];

        /* Does it have a leading option switch char? */
        if (OPT_SWITCH(*nextv)) {

            /* Get the option char */
            opt_char = nextv[1];

            /* Establish error flag */
            if (*optctl == '-') {
                abort_err = 0;
                ++optctl;
            }
            else
                abort_err = 1;

            /* Validate */
            while (*optctl && *optctl != opt_char)
                ++optctl;

            /* Found? */
            if (*optctl) {

                /* Set return code */
                retcode = (int) opt_char;

                /* Find required string, if any */
                if (*(optctl+1) == ':') {

                    /* Check for optional whitespace after opt */
                    if (nextv[2])
                        /* No whitespace - use remainder of this arg */
                        *valptr = nextv + 2;

                    /* Else use next arg as argument to current opt */
                    else {
                        nextv = argv[++nextc];

                        /* Make sure there is one, and not opt char */
                        if (argc <= nextc || OPT_SWITCH(*nextv))
                            retcode = AMU_OPT_NO_ARG;

                        else
                            *valptr = nextv;
                    }
                }
            }
            else
                /* Char is not in optctl string */
                retcode = AMU_OPT_BAD;
        }
        else {
            /* Found regular parm, not an option */
            *valptr = nextv;
            retcode = ++nooptc;
        }
    }

    /* Ready for next argument */
    ++nextc;

    /* Handle exits if need */
    if (abort_err && retcode < 0) {
        switch (retcode) {
            case AMU_OPT_BAD:
                msg = "Unrecognized option '%c'\n";
                break;
            case AMU_OPT_NO_ARG:
                msg = "Missing required argument for option '%c'\n";
        }
        fprintf (stderr, msg, opt_char);
        exit (-1);
    }

    /* Return option char or error code */
    return (retcode);

} /* amuopt */
