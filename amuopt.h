/* SCCS: %M%  VER: %I%  MOD: %G%  EXTRACT: %H% */

/*********************************************************************
*   amuopt.h                                                         *
*                                                                    *
*   PURPOSE                                                          *
*       Header for command line option parser.                       *
*                                                                    *
*                                   Copyright 1991, AM Systems, Inc. *
*                                   All rights reserved              *
*********************************************************************/

#ifndef AMUOPT_H
#define AMUOPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Command line amuopt() return codes */
#define AMU_OPT_DONE       0        /* No more command line args    */
#define AMU_OPT_BAD      (-1)       /* Unrecognized option char     */
#define AMU_OPT_NO_ARG   (-2)       /* Missing required opt argument*/

/* Function prototype */
int  amuopt (int, char **, char *, char **);

#ifdef  __cplusplus
}
#endif

#endif /* AMUOPT_H */
