/************************************************************************
* custombib.c                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Custom procedures used in Marc->Marc processing.                *
*                                                                       *
*                                               Author: Mark Vereb      *
************************************************************************/

/* $Id: custombib.c,v 1.9 2000/10/05 14:29:37 meyer Exp vereb $
 * $Log: custombib.c,v $
 * Revision 1.9  2000/10/05 14:29:37  meyer
 * Added cmp_getUIfrom035.  Gets the 035$9 UI without the other things
 * done by proc_035.
 *
 * Revision 1.8  2000/10/04 17:16:02  vereb
 * 022 processing
 *
 * Revision 1.7  2000/04/26 18:53:35  vereb
 * Added move_0359 procedure
 * It move 035 $9 to 001 and performs same validations as
 * proc_035 and remarc_035
 *
 * Revision 1.6  1999/05/27 13:49:21  vereb
 * modifications to add remarc_035
 *
 * Revision 1.5  1999/05/27 13:13:38  vereb
 * Final version prior to re-marc additions
 *
 * Revision 1.4  1999/03/19 21:03:09  vereb
 * clarification of 010 error message for byte 11
 *
 * Revision 1.3  1999/03/18 16:47:04  meyer
 * Changed long to int.
 *
 * Revision 1.2  1999/03/11 21:17:47  vereb
 * *** empty log message ***
 *
 * Revision 1.1  1999/02/23 19:30:57  vereb
 * Initial revision
 *
 */

#include <string.h>
#include <ctype.h>
#include <time.h>
#include "marcproclist.h"
#include "mrv_util.h"

#define NUM_NUMERICS 6
#define YYYYMMDD "YYYYMMDD"
#define YYMMDD "YYMMDD"

#define MAX_TAGNUMS 100
static char *DupChkTbl[MAX_TAGNUMS];

unsigned char *uip;
size_t         ui_len;

void *marc_alloc(int, int);
void marc_dealloc(void *, int);

int str_date_format(unsigned char *tmpp, int tmp_len, char *fmt);

/************************************************************************
* cmp_proc_000 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process Leader -- recognized only in Record Prep/Post processing*
*       Leader processing must be forced as 'generic' field processing  *
*       begins with field 001.                                          *
*                                                                       *
*       Special processing is to occur for Leader byte 05               *
*       Last four bytes (20-23) are to be forced to '4500'              *
*       Calculations based on record size, etc. performed internally    *
*                                                                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       none                                                            *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_proc_000 (
    CM_PROC_PARMS *pp          /* Pointer to parameter structure      */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *tmpp,         /* Ptr to input data                   */
                *hlddata;      /* (movable) Ptr to data               */
  size_t         src_len,      /* Length of source field              */
                 tmp_len;      /* Length of temp field                */
  int            occ,          /* Field occurrence counter            */
                 flag5p,       /* ldr byte 05='p'                     */
                 flag,         /* Flag                                */
                 rc;           /* Return code                         */
  char  out_data[999];/* Output data                         */

  /* Extract Leader... */
  cmp_buf_find(pp, "000:0$0:24", &srcp, &src_len);

  if (src_len != 24) {
    cm_error(CM_ERROR, "Input Leader length not 24. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }

  flag = flag5p = 0;
  strcpy(out_data,"");

  tmpp=srcp+5;
  if(*tmpp=='p')
    flag5p=1;

  /* WE NEED TO GO IN REVERSE ORDER DUE TO 'SUBSTRING' WRITING PROBLEM */
  /* SO LEADER WILL BE DONE IN FOLLOWING ORDER... */
  /* 20 - 23 */
  /* 06 - 19 */
  /* 05      */
  /* 00 - 04 */

  /* Leader bytes 20-23 are to be forced to '4500' */
  cmp_buf_write(pp, "000:0$20:4", (unsigned char *) "4500", 4, 0);

  hlddata = srcp + 6;
  /* Leader bytes 06-19 can be copied over 'as-is'... */
  /* Now done in marcconv driver, not needed here     */
  /* cmp_buf_write(pp, "000:0$6:14", hlddata, 14, 0); */

  /* Leader byte 9 forced to ' '... */
  /* SHOULD NO LONGER BE FORCED
     cmp_buf_write(pp, "000:0$9:1", (unsigned char *)" ", 1, 0); */


  /* Leader byte 005 needs special processing */
  /* Case 1 -- if Ldr 05 = 'd', output 'd' */
  hlddata = srcp + 5;
  if (*hlddata == 'd')
    cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "d", 1, 0);
  else {
    /* First check to see if &998 is not "1" and 998$a is MNT */
    /* and 998$b is NOT 19981016... if so, reject the record  */
    cmp_buf_find(pp, "&998", &tmpp, &tmp_len);
    if (((*tmpp == '1') && (tmp_len > 0)) == 0) {
      cmp_buf_find(pp, "998:0$a:0", &tmpp, &tmp_len);
      if ((tmp_len>0) && ((strncmp((char *)tmpp,"mnt",3)==0)||
			  (strncmp((char *)tmpp,"MNT",3)==0))) {
	cmp_buf_find(pp, "998:0$b:0", &tmpp, &tmp_len);
	if ((tmp_len>0) && (strncmp((char *)tmpp,"19981016",8)!=0)) {
	  cm_error(CM_ERROR, "998 is MNT---check for maintenance.");
	  return CM_STAT_KILL_RECORD;
	}
      }
    }

    /* Case 2 -- if Ldr 17 = '8', output 'n' */
    hlddata = srcp + 17;
    if (*hlddata == '8')
      cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "n", 1, 0);
    else {
      /* 996       repeating           */
      /*    $a non-repeating/mandatory */
      /*    $b non-repeating/mandatory */

      /* 040   non-repeating           */
      /*    $d     repeating/optional  */

      /* 995   non-repeating           */
      /*    $b non-repeatin/mandatory  */
      /*    $d non-repeating/optional  */

      /* if 996$a = 'rev. CIP' exists, then... */

      for (occ=0; ; occ++) {
	if ((rc=marc_get_item(pp->inmp, 996, occ,
			      'a', 0, &tmpp, &tmp_len)) == 0) {
	  if ((strncmp((char *)tmpp,"rev. CIP",8) == 0) ||
	      (strncmp((char *)tmpp,"rev cip", 7) == 0)) {
	    flag = 1;
	    break;
	  }
	}
	else
	  break;
      }

      if (flag) {
	/* Case 3 -- output a 'c' unless the corresponding $b    */
	/* is in YYYYMMDD format and in the specified data range */
	/* in which case, output a 'p'                           */

	cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "c", 1, 0);

	if ((rc=marc_get_item(pp->inmp, 996, occ,
			      'b', 0, &tmpp, &tmp_len)) == 0) {

	  flag = str_date_format(tmpp, tmp_len, YYYYMMDD);

	  /* If in YYYYMMDD format, see if it's in specified date range */
	  if (flag) {
	    strncpy(out_data,(char *)tmpp, 8);

	    /* If numeric and in date range, then output 'p' */
	    cmp_buf_find(pp, "&Date1", &tmpp, &tmp_len);
	    if (strncmp(out_data, (char *)tmpp, 8) >= 0) {
	      cmp_buf_find(pp, "&Date2", &tmpp, &tmp_len);
	      if (strncmp(out_data, (char *)tmpp, 8) < 0) {
		cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "p", 1, 0);
	      }
	    }
	  } /* (flag) */
	} /* rc=marc_get_item... == 0 */
      } /* (flag) 996$a = 'rev. CIP' */
      else {
	  /* Case 3a -- no "rev. CIP", but ldr05='p'... */
	if(flag5p==1) {
	  cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "c", 1, 0);

	  /* if 995$b is in date range, date1 - date2... output 'p', else 'c' */
	  if ((rc=marc_get_item(pp->inmp,995, 0,
				'b',0, &tmpp, &tmp_len)) == 0) {
	    if (str_date_format(tmpp, tmp_len, YYYYMMDD)) {
	      strncpy(out_data,(char *)tmpp, 8);

	      /* If numeric and in date range, then output 'p' */
	      cmp_buf_find(pp, "&Date1", &tmpp, &tmp_len);
	      if (strncmp(out_data, (char *)tmpp, 8) >= 0) {
		cmp_buf_find(pp, "&Date2", &tmpp, &tmp_len);
		if (strncmp(out_data, (char *)tmpp, 8) < 0) {
		  cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "p", 1, 0);
		}
	      }
	    }
	  }
	}
	else {
	  /* Case 6 -- output a 'n' unless                  */
	  /* Case 4 -- 040$d = 'DNLM'... output 'c' or      */
	  /* Case 5 -- 995$d in YYYYMMDD format, output 'c' */
	  /* Case 5a - 995$b is present and contains a date */
	  /*           prior to date1, output 'c'           */


	  /* Case 6 -- default value 'n' */
	  cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "n", 1, 0);

	  /* Check for 040$d = 'DNLM'... */
	  for (occ=0; ; occ++) {
	    if ((rc=marc_get_item(pp->inmp, 40, 0,
				  'd', occ, &tmpp, &tmp_len)) == 0) {
	      if (strncmp((char *)tmpp,"DNLM",4) == 0) {
		flag = 1;
		break;
	      }
	    }
	    else
	      break;
	  }

	  /* Case 4 -- if 040$d, output 'c'... */
	  if (flag)
	    cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "c", 1, 0);
	  /* if 995$d in YYYYMMDD format, output 'c' */
	  else {
	    /* Case 5 -- 995$d present & in YYYYMMDD format, output 'c' */
	    /*        or 995$b present & in YYYYMMDD format and < Date1 */
	    if ((rc=marc_get_item(pp->inmp, 995, 0,
				  'd', 0, &tmpp, &tmp_len)) == 0) {
	      if (str_date_format(tmpp, tmp_len, YYYYMMDD))
		flag = 1;

	      cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "c", 1, 0);
	    }
	    if ((rc=marc_get_item(pp->inmp,995, 0,
				  'b',0, &tmpp, &tmp_len)) == 0) {
	      if (str_date_format(tmpp, tmp_len, YYYYMMDD)) {
		strncpy(out_data,(char *)tmpp, 8);

		/* If numeric and in date range, then output 'p' */
		cmp_buf_find(pp, "&Date1", &tmpp, &tmp_len);
		if (strncmp(out_data, (char *)tmpp, 8) < 0)
		  flag = 1;
	      }
	    }
	    if (flag)
	      cmp_buf_write(pp, "000:0$5:1", (unsigned char *) "c", 1, 0);
	  }
	}
      }
    }
  }

  /* Leader bytes 00-04 can be copied over 'as-is'... */
  cmp_buf_write(pp, "000:0$0:5", srcp, 5, 0);

  return CM_STAT_OK;
}/* cmp_proc_000 */


/************************************************************************
* cmp_proc_010 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 010 fields -- recognized only in Field Prep processing  *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       "NAM" for Name Authority file processing.                       *
*       "BIB" for Bibliographic  file processing.                       *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_proc_010 (
    CM_PROC_PARMS *pp          /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *indatap,      /* Ptr to output data                  */
                *hlddata,      /* Ptr to output data                  */
                *outdatap,     /* Ptr to output data                  */
                *tmpp,         /* Ptr to output data                  */
                *datatypep;    /* Ptr to "BIB" or "NAM"               */
  size_t         src_len,      /* Length of source field              */
                 type_len;     /* Length of data at datatypep         */
  int            occ,          /* Field occurrence counter            */
                 socc,         /* Subfield occurrence counter         */
                 sf_code,      /* Subfield code                       */
                 cnt_nums,     /* Count of numerics                   */
                 rectype_bib,  /* True=Bib record type                */
                 rectype_name, /* True=Name record type               */
                 prefixlen,    /* True=Name record type               */
                 errSeverity,  /* True=Name record type               */
                 y2kflag;      /* True=Name record type               */
 char  out_data[999];/* Output data                         */


  /*=========================================================================*/
  /* Create new output field and copy over the indicators...                 */
  /*=========================================================================*/
  if(add_field_and_indics(pp, 10)!=0)
    return CM_STAT_KILL_RECORD;

  /*=========================================================================*/
  /* Determine whether we've been called for bib or nam records...           */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &datatypep, &type_len);
  rectype_bib =(strncmp((char *)datatypep,"BIB",3)==0);
  rectype_name=(strncmp((char *)datatypep,"NAM",3)==0);


  /*=========================================================================*/
  /* Loop through subfields (start at 2 since 0 & 1 retrieve indicators)     */
  /*=========================================================================*/
  for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
      socc++) {

    /*=======================================================================*/
    /* We need to reformat $a & $z (and $b for BIB records)...               */
    /*=======================================================================*/
    if((sf_code=='a')
       /* ||(sf_code=='z')                 removed 12/03/18 */
       /* || ((sf_code=='b')&&rectype_bib) removed 11/25/02 */
       ) {

      if(sf_code=='a')
	errSeverity=CM_ERROR;
      else
	errSeverity=CM_WARNING;

      memset(out_data,'\0',999);

      outdatap = (unsigned char *)out_data;
      indatap = tmpp = srcp;

      y2kflag=0;
      prefixlen=3;
      /*=====================================================================*/
      /* We need to check for Y2K processing...                              */
      /*=====================================================================*/
      for(tmpp=srcp;(tmpp<srcp+src_len)&&!isdigit(*tmpp);tmpp++);

      if(isdigit(*tmpp))
	if((strncmp((char *)tmpp,"20",2)==0)&&
	   (strncmp((char *)tmpp,"20-",3)!=0)) {
	  y2kflag=1;
	  prefixlen=2;
	}

      for(;(*indatap==' ')&&(indatap<srcp+src_len);indatap++);

      /*=====================================================================*/
      /* For NAME AUTHORITY RECORDS ONLY, the first character MUST be 'n'    */
      /*=====================================================================*/
      if(rectype_name&&(*indatap!='n')) {
	cm_error(CM_ERROR, "First character of 010$%c is not 'n' -- '%.*s'."
		 "Record not processed.", sf_code, src_len, srcp);
	return CM_STAT_KILL_RECORD;
      }

      /*=====================================================================*/
      /* Copy over the prefixlen alphas or spaces, if present...             */
      /*=====================================================================*/
      for(occ=0;
	  (occ<=prefixlen-1)&&(indatap<srcp+src_len)&&
	    ((*indatap==' ')||isalpha(*indatap));
	  occ++) {
	*outdatap++ = *indatap++;
      }

      /*=====================================================================*/
      /* If short on alphas and spaces, pad with blanks...                   */
      /*=====================================================================*/
      while(occ<=prefixlen-1) {
	*outdatap++ = ' ';
	++occ;
      }

      /*=====================================================================*/
      /* If we have y2kflag set, we need to do special processing...         */
      /* What's remaining must match pattern 9999999999 or 9999-999999       */
      /*=====================================================================*/
      if(y2kflag) {
	for(tmpp=indatap;(indatap<srcp+src_len)&&(*indatap==' ');indatap++);

	for(tmpp=indatap;indatap<srcp+src_len;indatap++) {
	  if(isdigit(*indatap))
	    *outdatap++=*indatap;
	  else {
	    /*===============================================================*/
	    /* if '-' and between 4th and 5th digit, we're okay...           */
	    /*===============================================================*/
	    if((*indatap=='-')&&(indatap-tmpp==4))
	      continue;
	    else {
	      if(indatap==tmpp)
		cm_error(errSeverity,"bad 010 prefix");
	      else {
		for(tmpp=indatap;(tmpp<srcp+src_len)&&(*tmpp==' ');tmpp++);
		if(tmpp>=srcp+src_len)
		  break;
		else
		  cm_error(errSeverity,"bad 010 data");
	      }
	      if(sf_code=='a')
		return CM_STAT_KILL_RECORD;
	      else
		break;
	    }
	  }
	}

	if(strlen(out_data)!=12) {
	  cm_error(errSeverity,"bad 010 data");
	  if(sf_code=='a')
	    return CM_STAT_KILL_RECORD;
	}
      }
      /*=====================================================================*/
      /* If not y2kflag, we do "normal" processing...                        */
      /* Copy over first two numerics, stopping at hyphen and ignoring blanks*/
      /*=====================================================================*/
      else {
	hlddata = outdatap;
	while((indatap<srcp+src_len)&&(outdatap<hlddata+2)) {
	  if(isdigit(*indatap))
	    *outdatap++=*indatap++;
	  else
	    if(*indatap==' ')
	      indatap++;
	    else
	      if(*indatap=='-') {
		indatap++;
		break;
	      }
	      else {
		cm_error(errSeverity, "Improper format for 010$%c -- '%.*s'."
			 "(characters following initial alphabetics "
			 "are not numeric). ", sf_code, src_len, srcp);
		if(sf_code=='a')
		  return CM_STAT_KILL_RECORD;
		else
		  break;
	      }
	}

	/*===================================================================*/
	/* If there was only one numeric, provide leading zero               */
	/*===================================================================*/
	if(outdatap==hlddata+1) {
	  *outdatap++=*hlddata;
	  *hlddata='0';
	}

	/*===================================================================*/
	/* If we're at a hyphen, skip it...                                  */
	/*===================================================================*/
	if(*indatap=='-')
	  indatap++;

	/*===================================================================*/
	/* Check the rest of the field... All characters must be numeric     */
	/* There should be no more than that specified by NUM_NUMERICS       */
	/*===================================================================*/
	hlddata=indatap;
	cnt_nums=0;
	for (;indatap<srcp+src_len; indatap++) {
	  if(isdigit(*indatap))
	    cnt_nums++;
	  else
	    if(*indatap!=' ') {
	      if(rectype_name) {
		cm_error(errSeverity, "Improper format for 010$%c -- '%.*s'. "
			 "(non-numeric characters at end). "
			 "Record not processed.", sf_code, src_len, srcp);
		return CM_STAT_KILL_RECORD;
	      }
	      else
		break;
	    }
	}

	/*===================================================================*/
	/* If more than NUM_NUMERICS, log error...                           */
	/*===================================================================*/
	if(cnt_nums>NUM_NUMERICS) {
	  cm_error(errSeverity, "Improper format for 010$%c -- '%.*s'. "
		   "(more than 6 numeric characters at end). "
		   "Record not processed.", sf_code, src_len, srcp);
	  if(sf_code=='a')
	    return CM_STAT_KILL_RECORD;
	}

	/*===================================================================*/
	/* If there were less than NUM_NUMERICS, right pad with zeroes...    */
	/*===================================================================*/
	for (occ=0;occ<NUM_NUMERICS-(cnt_nums);occ++) {
	  *outdatap++ = '0';
	}

	/*===================================================================*/
	/* Copy the rest of the data over                                    */
	/*===================================================================*/
	while(hlddata<indatap) {
	  if(*hlddata!=' ')
	    *outdatap++ = *hlddata;
	  hlddata++;
	}

	/*===================================================================*/
	/* Output field byte 11 is blank                                     */
	/*===================================================================*/
	if(outdatap-(unsigned char *)out_data!=11) {
	  cm_error(errSeverity, "Byte 11 in 010 is NOT properly postioned.");
	  if(sf_code=='a')
	    return CM_STAT_KILL_RECORD;
	}

	*outdatap++=' ';

	/*===================================================================*/
	/* Copy the rest of the data over                                    */
	/*===================================================================*/
	if(rectype_bib) {
	  while(indatap-srcp<src_len) {
	    *outdatap++ = *indatap++;
	  }
	}
      }

      /*=====================================================================*/
      /* Send to output field                                                */
      /*=====================================================================*/
      marc_add_subfield(pp->outmp, sf_code, (unsigned char *)out_data, strlen(out_data));
    }
    else
      /*=====================================================================*/
      /* Copy any other subfield over as-is...                               */
      /*=====================================================================*/
      marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

  }

  return CM_STAT_OK;
} /* cmp_proc_010 */


/************************************************************************
* cmp_naco_010 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 010 fields -- recognized only in Field Prep processing  *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       "NAM" for Name Authority file processing.                       *
*       "BIB" for Bibliographic  file processing.                       *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_naco_010 (
    CM_PROC_PARMS *pp          /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *indatap,      /* Ptr to output data                  */
                *outdatap;     /* Ptr to output data                  */
  size_t         src_len;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                 icurryr,      /* current year                        */
                 inputyr;      /* input year                          */

  char input_year[5];/* Output data                         */
  char current_yr[5];
  struct tm *ptm;             /* Ptr to time info from localtime()*/
  time_t ltime;               /* Time in seconds                  */

  memset(input_year,'\0',5);
  memset(current_yr,'\0',5);

  time (&ltime);
  ptm = localtime (&ltime);
  strftime (current_yr, 5, "%Y", ptm);

  /*=========================================================================*/
  /* We need to check the format of the $a...                                */
  /*=========================================================================*/
  cmp_buf_find(pp, "%data", &srcp, &src_len);

  if(src_len!=12) {
    cm_error(CM_ERROR, "010$a does not match 'n yyyy999999' format: '%.*s'. "
	     "Record not processed.", src_len, srcp);
    return CM_STAT_KILL_RECORD;
  }

  indatap = srcp;

  /*=========================================================================*/
  /* the first character MUST be 'n'                                         */
  /*=========================================================================*/
  if(*indatap++!='n') {
    cm_error(CM_ERROR, "First character of 010$a is not 'n' -- '%.*s'. "
	     "Record not processed.", src_len, srcp);
    return CM_STAT_KILL_RECORD;
  }

  /*=====================================================================*/
  /* the second character MUST be ' '                                    */
  /*=====================================================================*/
  if(*indatap++!=' ') {
    cm_error(CM_ERROR, "Second character of 010$a is not blank -- '%.*s'. "
	     "Record not processed.", src_len, srcp);
    return CM_STAT_KILL_RECORD;
  }

  /*=====================================================================*/
  /* Copy over the next 4 characters...                                  */
  /*=====================================================================*/
  outdatap = (unsigned char *)input_year;

  for(occ=0;occ<4;occ++) {
    if(isdigit(*indatap))
      *outdatap++ = *indatap++;
    else {
      cm_error(CM_ERROR, "010$a does not match 'n yyyy999999' format: '%.*s'. "
	       "Record not processed.", src_len, srcp);
      return CM_STAT_KILL_RECORD;
    }
  }

  /*=====================================================================*/
  /* Should be current year (or last year if we're still in January)...  */
  /*=====================================================================*/
  if(!get_fixed_num(input_year,strlen(input_year),&inputyr)) {
    cm_error(CM_FATAL,"Fatal error determining year for 010$a: '%s'",input_year);
  }

  if(!get_fixed_num(current_yr,strlen(current_yr),&icurryr)) {
    cm_error(CM_FATAL,"Fatal error determining computer's system year");
  }

  if(icurryr!=inputyr) {
    if((ptm->tm_mon!=0)||(icurryr-inputyr!=1)) {
      cm_error(CM_ERROR,"First 4 numeric characters of 010$a are not the "
	       "current year -- '%.*s'. Record not processed.",
	       src_len, srcp);
      return CM_STAT_KILL_RECORD;
    }
  }

  /*=====================================================================*/
  /* the last 6 characters MUST be numeric...                            */
  /*=====================================================================*/
  for(;(indatap<srcp+src_len)&&isdigit(*indatap);indatap++);

  if(indatap<srcp+src_len){
    cm_error(CM_ERROR, "Last 6 characters of 010$a are not numeric: '%.*s'. "
	     "Record not processed.", src_len, srcp);
    return CM_STAT_KILL_RECORD;
  }

  return CM_STAT_OK;
} /* cmp_naco_010 */


/************************************************************************
* cmp_isbn_13 ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_isbn_13 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,
                *src2;        /* Ptr to data                         */
  char         *tmpp,
               *tsrc2;
  size_t        src_len,      /* Length of source field              */
                src2len;
  int           occ,          /* Field occurrence counter            */
                socc,         /* Subfield occurrence counter         */
                occ2,         /* Subfield occurrence counter         */
                num10s,
                num13s,
                charcnt,
                sf_code,
                matchfound;
  int multiplier[]={3,1,3,1,3,1,3,1,3};

  num10s=num13s=matchfound=charcnt=0;
  /*=========================================================================*/
  /* Loop through successive 020's...                                        */
  /*=========================================================================*/
  for (occ=0;marc_get_field(pp->inmp,20,occ,&srcp,&src_len)==0;occ++) {
    /*=======================================================================*/
    /* We're only interested in $a's...                                      */
    /*=======================================================================*/
    if (marc_get_subfield(pp->inmp, 'a', 0, &srcp,&src_len)==0) {
      /*=====================================================================*/
      /* Find out how many characters in the ISBN...                         */
      /*=====================================================================*/
      tsrc2=(char *)srcp;
      charcnt=get_isbn_length(srcp, src_len);

      /*=====================================================================*/
      /* and keep track of how many 10s and 13s...                           */
      /*=====================================================================*/
      if(charcnt==10)
	num10s++;
      else if(charcnt==13) {
	/*===================================================================*/
	/* For 13s, we also want to make sure there is a corresponding 10... */
	/*===================================================================*/
	num13s++;

	matchfound=0;
	/*===================================================================*/
	/* So loop through them again, checking for a matching ISBN10...     */
	/*===================================================================*/
	for (occ2=0;
	     ((marc_get_field(pp->inmp,20,occ2,&src2,&src2len)==0) &&
	      (matchfound==0));
	     occ2++) {
	  if(occ2!=occ)
	    if (marc_get_subfield(pp->inmp, 'a', 0, &src2,&src2len)==0)
	      if(strncmp((char *)(src2+3),tsrc2,charcnt-1)==0)
		matchfound=1;
	}

	/*===================================================================*/
	/* If we don't find one, this is okay... leave record alone...       */
	/*===================================================================*/
	if(matchfound==0) {
	  return CM_STAT_OK;/*ILL_RECORD;*/
	}
      }
    }

    /*=======================================================================*/
    /* If we have any 13's, we can ignore this record...                     */
    /*=======================================================================*/
    if(num13s>0) {
      /*=====================================================================*/
      /* Either, there are problems with them matching up -> send err msg... */
      /*=====================================================================*/
      if(num13s!=num10s) {
	if(num10s>0)
	  cm_error(CM_ERROR,"Record contains differing number of ISBN13s (%d) and ISBN10s (%d). "
		   "Record not processed...", num13s,num10s);
	return CM_STAT_KILL_RECORD;
      }
      /*=====================================================================*/
      /* OR... They all match up fine -> nothing else to do...               */
      /*=====================================================================*/
      else {
	return CM_STAT_OK;/*ILL_RECORD;*/
      }
    }
  }

  /*=========================================================================*/
  /* Loop through successive 020's again...                                  */
  /*=========================================================================*/
  for (occ=0;marc_get_field(pp->inmp,20,occ,&srcp,&src_len)==0;occ++) {

    /*=======================================================================*/
    /* If we have an ISBN10, generate ISBN13 in new 020 $a...                */
    /*=======================================================================*/
    if (marc_get_subfield(pp->inmp, 'a', 0, &srcp,&src_len)==0) {

      if(get_isbn_length(srcp, src_len)==10) {
	if (add_field_and_indics(pp, 20) != 0 )
	  return CM_STAT_KILL_RECORD;

	/*===================================================================*/
	/* First, allocate space for ISBN13...                               */
	/*===================================================================*/


#ifdef DEBUG
	if ((tsrc2 = (char *) marc_alloc(src_len+4, 100)) == NULL) {   // TAG:100
	    cm_error(CM_ERROR, "Error allocating memory for ISBN13 processing. " "Record not processed...");
	    return CM_STAT_KILL_RECORD;
	}
#else
	if ((tsrc2 = (char *)malloc(src_len+4)) == NULL) {
	  cm_error(CM_ERROR, "Error allocating memory for ISBN13 processing. "
		   "Record not processed...");
	  return CM_STAT_KILL_RECORD;
	}
#endif

	memset(tsrc2, '\0', src_len+4);

	/*===================================================================*/
	/* ISBN13 starts w/"978" and checksum of 38 (9*1  +  7*3  +  8*1)... */
	/*===================================================================*/
	charcnt=38;

	strcpy(tsrc2, "978");

	for(tmpp=(char *)srcp;tmpp<(char *)srcp+9;tmpp++) {
	  strncat(tsrc2, tmpp, 1);
	  charcnt=charcnt + ((int)(*tmpp-48) * multiplier[strlen(tsrc2)-3-1]);
	}

	/*===================================================================*/
	/* Ignore 10th char. and add checksum byte                           */
	/*===================================================================*/
	tmpp++;
	if(charcnt%10 == 0)
	  strcat(tsrc2,"0");
	else
	  tsrc2[strlen(tsrc2)]=(char)(10-(charcnt%10)+48);

	/*===================================================================*/
	/* Followed up with whatever follows the ISBN10 and send to new $a   */
	/*===================================================================*/
	strncat(tsrc2,tmpp,(char *)srcp+src_len-tmpp);

	marc_add_subfield(pp->outmp, 'a', (unsigned char *)tsrc2, strlen(tsrc2));
	cmp_buf_write(pp,"modified",(unsigned char *)"1",1,0);
      }
    }

    /*=======================================================================*/
    /* Now, output the field...                                              */
    /*=======================================================================*/
    if (add_field_and_indics(pp, 20) != 0 )
      return CM_STAT_KILL_RECORD;

    /*=======================================================================*/
    /* Loop through all subfields...                                         */
    /*=======================================================================*/
    for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	socc++) {

      /*=====================================================================*/
      /* If $a is not isbn 10, then convert to $z. Otherwise, output as-is...*/
      /*=====================================================================*/
      if((sf_code=='a') && (get_isbn_length(srcp, src_len)!=10)) {
	marc_add_subfield(pp->outmp, 'z', srcp, src_len);
	cmp_buf_write(pp,"modified",(unsigned char *)"1",1,0);
	cm_error(CM_WARNING,"ISBN (#%d) is of improper length (%d): '%*.*s'. "
		 "Will be moved to $z...",
		 occ,get_isbn_length(srcp,src_len),src_len,src_len,(char *)srcp);
      }
      else
	marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
    }
  }

  return CM_STAT_OK;

} /* cmp_isbn_13 */


int get_isbn_length(unsigned char *tsrc2, size_t src_len) {
  unsigned char *tmpp;
  int charcnt;

  charcnt=0;

  for(tmpp=tsrc2;tmpp<tsrc2+src_len;tmpp++) {
    if((*tmpp>='0')&&(*tmpp<='9')) {
      charcnt++;
    }
    else if((*tmpp=='x')||(*tmpp=='X')) {
      *tmpp='X';
      tmpp++;
      if((*tmpp==' ')||
	 ((tmpp-tsrc2)==src_len)) {
	charcnt++;
      }
      else {
	tmpp--;
	break;
      }
    }
    else break;
  }
  return charcnt;
}


/************************************************************************
* cmp_proc_022 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       prep_022 is custom procedure designed to:                       *
*          1) suppress and/or rearrange the incoming 022's              *
*       Recognized only in FIELD PREP processing                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_022 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *tsrcp;       /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                srctlen;      /* Length of temp source field         */
  char          *tmpp;        /* Field occurrence counter            */
  int           occ,          /* Field occurrence counter            */
                socc,         /* Subfield occurrence counter         */
                sf_code,      /* Subfield code                       */
                rc,           /* Return code                         */
                cnty,         /* field position w/$9                 */
                cnta,         /* Count of $9's                       */
                cnt9,         /* Count of $9's                       */
                cnt22;        /* Count of $9's                       */

  cnta = cnt9 = cnty = cnt22 = 0;
  tmpp = NULL;
  /*=========================================================================*/
  /* Loop through successive 022's...                                        */
  /*=========================================================================*/
  for (occ=0;marc_get_field(pp->inmp,22,occ,&srcp,&src_len)==0;occ++) {

    /*=======================================================================*/
    /* Keep track of those have $a but no $9...                              */
    /*=======================================================================*/
    if (marc_get_item(pp->inmp,22,occ,'a',0,&srcp,&src_len)==0)
      cnta++;

    if((rc=marc_get_item(pp->inmp,22,occ,'9',0,&tsrcp,&srctlen))==0)
      cnt9++;

    /*=======================================================================*/
    /* Send error msg if there at least 2 022's and at least one missing a $9*/
    /*=======================================================================*/
    if((cnta>1)&&(cnt9<cnta)) {
      cm_error(CM_ERROR,"multiple 022s with $a; $9 is lacking");
      return CM_STAT_KILL_RECORD;
    }

    /*=======================================================================*/
    /* If there is a $9...                                                   */
    /*=======================================================================*/
    if(rc==0) {
      /*=====================================================================*/
      /* The second character must be 'Y' or 'N' (mixed case)                */
      /*=====================================================================*/
      if(srctlen<2) {
	cm_error(CM_ERROR,"invalid 022 $9 present");
	return CM_STAT_KILL_RECORD;
      }
      else {
	tmpp=(char *)tsrcp+1;
	if((*tmpp!='y')&&
	   (*tmpp!='n')&&
	   (*tmpp!='Y')&&
	   (*tmpp!='N')) {
	  cm_error(CM_ERROR,"invalid 022 $9 present");
	  return CM_STAT_KILL_RECORD;
	}
	else {
	  /*=================================================================*/
	  /* Also, only one may have value of 'Y' (mixed case)               */
	  /*=================================================================*/
	  if((*tmpp=='y')||
	     (*tmpp=='Y'))
	    cnty++;
	  if(cnty>1) {
	    cm_error(CM_ERROR,"multiple 022's with $9 containing 'Y'");
	    return CM_STAT_KILL_RECORD;
	  }
	}
      }
    }

    /*=======================================================================*/
    /* Process any remaining records as follows...                           */
    /*=======================================================================*/

    /*=======================================================================*/
    /* If no $9, exclude $7 and output everything else as-is...              */
    /*=======================================================================*/
    if(rc!=0) {
      /*=====================================================================*/
      /* Only 1 022 is allowed in output record...                           */
      /*=====================================================================*/
      if(cnt22) {
	cm_error(CM_ERROR,"Two 022's remain after processing--check record.");
	return CM_STAT_KILL_RECORD;
      }

      /*=====================================================================*/
      /* Create new 022 output field and copy over the indicators...         */
      /*=====================================================================*/
      if (add_field_and_indics(pp, 22) != 0 )
	return CM_STAT_KILL_RECORD;

      cnt22++;
      /*=====================================================================*/
      /* Read successive subfields (start at 2 since 0 & 1 are indicators)   */
      /*=====================================================================*/
      for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	  socc++) {
	if (sf_code != '7')
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
      }
    }
    else {
      /*=====================================================================*/
      /* If $9 = 'Y', exclude $7 & $9 and output everything else as-is...    */
      /*=====================================================================*/
      if((*tmpp=='y') || (*tmpp=='Y')) {

	/*===================================================================*/
	/* Only 1 022 is allowed in output record...                         */
	/*===================================================================*/
	if(cnt22) {
	  cm_error(CM_ERROR,"Two 022's remain after processing--check record");
	  return CM_STAT_KILL_RECORD;
	}

	/*===================================================================*/
	/* Create new 022 output field and copy over the indicators...       */
	/*===================================================================*/
	if (add_field_and_indics(pp, 22) != 0 )
	  return CM_STAT_KILL_RECORD;

	cnt22++;
	/*===================================================================*/
	/* Read successive subfields (start at 2 since 0 & 1 are indicators) */
	/*===================================================================*/
	for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	    socc++) {
	  if ((sf_code != '7') && (sf_code != '9'))
	    marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
	}
      }
      else {
	/*===================================================================*/
	/* $9 must be 'N'... send $a to 776 $x with @1 = '0'...              */
	/*===================================================================*/
	cmp_buf_write(pp,"776:+$x:0",srcp,src_len,0);
	cmp_buf_write(pp,"776:-1@1",(unsigned char *)"0",1,0);
      }
    }
  } /* end of "for (occ=0; ; occ++)" with Leader 07 not 's' or 'b' */

  return CM_STAT_OK;

} /* cmp_proc_022 */


/************************************************************************
* cmp_proc_035 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       prep_035 is custom procedure designed to:                       *
*          1) place $9 in 001 (and verify there is only one $9)         *
*          2) depending on value of Leader 07, suppress or re-arrange   *
*             the 035's that encountered                                *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_035 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *tmpp,        /* Ptr to move through data            */
                *srctmp;      /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                tmp_len,      /* Length of temp source field         */
                srctlen;      /* Length of temp source field         */
  int           occ,          /* Field occurrence counter            */
                socc,         /* Subfield occurrence counter         */
                dcnt,         /* Data value counter                  */
                outputsfld,   /* flag to output subfield             */
                rc,           /* Return code                         */
                sf_code,      /* Subfield code                       */
                switch035,    /* boolean indicating Switch035 value  */
                pattern_match,/* boolean indicating $9 pattern match */
                DNLM,         /* field position w/$a=(DNLM)*(s)      */
                ocolc,        /* field position w/$a=(DNLM)*(s)      */
                ocolccnt,     /* field position w/$a=(DNLM)*(s)      */
                sf9,          /* field position w/$9                 */
                cnt9;         /* Count of $9's                       */

  /* Hold on to switch value */
  cmp_buf_find(pp, "&035", &srcp, &src_len);
  switch035 = (*srcp == '1') && (src_len > 0);

/*===========================================================================*/
/* CAPTURE UI 1st...                                                         */
/*===========================================================================*/
  for (occ=0;
       marc_get_field(pp->inmp, 35, occ, &srcp, &src_len)==0;
       occ++) {
    
    for(socc=0;marc_get_subfield(pp->inmp,'9',socc,&tmpp,&tmp_len)==0;socc++) {
      cmp_buf_write(pp, "ui", tmpp, tmp_len, 0);
      break;
    }
  }
    
  /* Extract Leader 7... */
  cmp_buf_find(pp, "000:0$7:1", &srcp, &src_len);

  cnt9 = 0;
  ocolccnt = 0;

  /* LEADER 07 exists, needs to be tested */
  if ((*srcp != 's') &&
      (*srcp != 'b') &&
      (*srcp != 'i')) {


/*===========================================================================*/
/* LEADER 07 IS NOT 's' OR 'b' or 'i'                                        */
/*===========================================================================*/

    /* Leader 07 not 's' or 'b' or 'i'*/
    /* Changed 09/04/18 to include 'i' in check */
    /* Changed 01/29/18 per Diane Boehr to output all $a=(DNLM)* with */
    /*    a warning if it ends in (s) */
    /* AND to output first $a=(OCoLC)*, stripping any 'ocm' or 'ocn' that */
    /*    immediately follows */ 

    /* Read successive 035's */
    for (occ=0;
	 marc_get_field(pp->inmp, 35, occ, &srcp, &src_len)==0;
	 occ++) {
      
      /* Create new output field and copy over the indicators...       */
      if (add_field_and_indics(pp, 35) != 0 )
	return CM_STAT_KILL_RECORD;
      
      /* Read successive subfields                    */
      /* (start at 2 since 0 & 1 retrieve indicators) */
      for (socc=2;
	   marc_pos_subfield(pp->inmp, socc, &sf_code, &srcp, &src_len)==0;
	   socc++) {

	/* default to output */
	outputsfld=1;
	
	if (sf_code == 'a') {
	  
	  /* Send message if $a starts with "(DNLM)" and ends in "(s)" */
	  srctmp = srcp + src_len - 3;
	  if ((strncmp((char *)srcp,"(DNLM)",6) == 0) &&
	      (strncmp((char *)srctmp,"(s)",3) == 0)) {
	    cm_error(CM_WARNING,"LDR 07 not 's' or 'b' or 'i' but 035$a='(DNLM)...(s)': %*.*s", src_len, src_len, srcp);
	  }

	  else {
	    /* If $a starts with "(OCoLC)", output first one */
	    /* sending message for subsequent ones */
	    if (strncmp((char *)srcp,"(OCoLC)",7)==0) {
	      ocolccnt++;
	      
	      if(ocolccnt==1) {
		/* If "(OCoLC)" is immediately followed by 'ocm' or 'ocn' */
		/* strip those characters from output */
		srctmp=srcp+7;
		if((strncmp((char *)srctmp,"ocm",3)==0) ||
		   (strncmp((char *)srctmp,"ocn",3)==0)) {
		  srctmp += 3;
		  cmp_buf_write(pp,"035:-1$a:+", (unsigned char *)"(OCoLC)", 7, 0);
		  cmp_buf_write(pp,"035:-1$a:-1",(unsigned char *)srctmp,src_len-10, 1);
		  outputsfld=0; /* set to 0 since we've just output what we wanted */
		}
	      }
	      else {
		cm_error(CM_WARNING,"Multiple OCLC#s: %*.*s", src_len, src_len, srcp);
		outputsfld=0;
	      }
	    }
	  }
	} /* End of $a */
	 
	else {
	  /* Drop the $9's but first copy it to the ui buffer */
	  /* and count it to make sure there's only one of them */
	  if (sf_code == '9') {
	    cnt9++;
	    cmp_buf_write(pp, "ui", srcp, src_len, 0);
	    outputsfld=0;
	  }
	}
	
	/* if ok to output */
	if(outputsfld!=0)
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

      } /* End of subfields */
    } /* End of 035's */
  }

/*===========================================================================*/
/* LEADER 07 IS 's' OR 'b' or 'i' -- FIRST PASS                              */
/*===========================================================================*/

  else {
    /* Leader 07 IS 's' or 'b' or 'i'                                     */
    /* if Switch is NOT set and 035$9 is NOT of pattern 10#######         */
    /* if 035$a exists '(DNLM)...(s)', put it out first followed by rest  */
    /* if switch IS set and 35$9 is of pattern 10####### and there are    */
    /* no $a's of '(DNLM)...(s)', put $9 out as '(DNLM)$9(s)              */
    /* Loop through all 035's, snatching $9 and (DNLM)...(s)              */
    /* then do testing to determine which needs to be output first        */
    /* Loop through second time, skipping over what's already been output */

    sf9 = -1;
    DNLM = -1;

    /* Read successive 035's */
    for (occ=0; 
	 marc_get_field(pp->inmp, 35, occ, &srcp, &src_len) == 0;
	 occ++) {
      
      /* Read successive subfields                    */
      /* (start at 2 since 0 & 1 retrieve indicators) */
      for (socc=2;
	   marc_pos_subfield(pp->inmp, socc, &sf_code, &srcp, &src_len) == 0;
	   socc++) {
	
	/* Identify $a that starts with "(DNLM)" and ends with "(s)" */
	if (sf_code == 'a') {
	  srctmp = srcp;
	  srctmp = srctmp + src_len - 3;
	  
	  if ((strncmp((char *)srcp,"(DNLM)",6) == 0) &&
	      (strncmp((char *)srctmp,"(s)", 3) == 0)) {
	    /* store occurrence number in DNLM for reference below */
	    DNLM = occ;
	    continue;
	  }
	  
	  if(strncmp((char *)srcp,"(OCoLC)",7)==0) {
	    cmp_buf_find(pp, "&PULL", &tmpp, &tmp_len);
	    if((strncmp((char *)tmpp,"CONSER",tmp_len)==0)&&(tmp_len==6)) {
	      cm_error(CM_ERROR,"record with OCLC# cannot be CONSER NEW");
	      return CM_STAT_KILL_RECORD;
	    }
	    ocolccnt++;
	  }
	}
	
	/* Ignore the $9's but first copy it to the ui buffer */
	/* and count it to make sure there's only one of them */
	else if (sf_code == '9') {
	  /* also store occurrence number in case needed below */
	  sf9 = occ;
	  cnt9++;
	  cmp_buf_write(pp, "ui", srcp, src_len, 0);
	  
	  /* Determine if $9 is of pattern "10#######"...        */
	  /* pattern_match will tell us how many numerics are in */
	  /* $9 (with first two being '1' & '0'                  */
	  if (strncmp((char *)srcp,"10",2) == 0){
	    pattern_match = 2;
	    for (dcnt=2; dcnt <= src_len; dcnt++) {
	      srctmp = srcp + dcnt;
	      if (isdigit(*srctmp))
		pattern_match++;
	    }
	  }
	  break;
	}
      } /* End of subfields */
    } /* End of 035's */

/*===========================================================================*/
/* LEADER 07 IS 's' OR 'b' -- SECOND PASS                                    */
/*===========================================================================*/

    /* If we have an $a starting with (DNLM) and ending in (s) */
    /* then it is to be the first one out...                   */

    if (DNLM > -1) {

      if ((rc=marc_get_field(pp->inmp, 35, DNLM, &srcp, &src_len)) == 0) {
	/* Create new output field and copy over the indicators...       */
	if (add_field_and_indics(pp, 35) != 0 )
	  return CM_STAT_KILL_RECORD;
	
	/* Read successive subfields                    */
	/* (start at 2 since 0 & 1 retrieve indicators) */
	for (socc=2;
	     marc_pos_subfield(pp->inmp, socc, &sf_code, &srcp, &src_len) == 0;
	     socc++) {
	  
	  /* Copy everything to the output field */
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
	}
      }
    }

    /* If we have a pattern match and if the switch is set AND     */
    /* we DO NOT have a $a starting with (DNLM) and ending in (s) */
    /* then we are to create the first 035 from the $9            */
    else  if ((sf9 > -1) && (pattern_match == 9) && (switch035)) {
      if ((rc=marc_get_item(pp->inmp,35,sf9,'9',0,&srcp,&src_len)) == 0) {
	cmp_buf_write(pp, "035:0$a:0", (unsigned char *) "(DNLM)",  6,    0);
	cmp_buf_write(pp, "035:0$a:0", srcp,                     src_len, 1);
	cmp_buf_write(pp, "035:0$a:0", (unsigned char *) "(s)",     3,    1);
      }
    }
    
    ocolc=0;

    /* Now loop through the 035's again */
    for (occ=0;
	 marc_get_field(pp->inmp, 35, occ, &srcp, &src_len)==0;
	 occ++) {

      /* skip any that have already gone out */
      if ((occ != DNLM) && (occ != sf9) ){

	/* Create new output field and copy over the indicators...       */
	if (add_field_and_indics(pp, 35) != 0 )
	  return CM_STAT_KILL_RECORD;
	
	/* Read successive subfields                    */
	/* (start at 2 since 0 & 1 retrieve indicators) */
	for (socc=2;
	     marc_pos_subfield(pp->inmp, socc, &sf_code, &srcp, &src_len) == 0;
	     socc++) {

	  outputsfld = 1;

	  if (sf_code == 'a') {
	    
	    //	  /* Ignore any $a's that start with "(DNLM)" but do not end with "(s)" */
	    //	    srctmp = srcp;
	    //	    srctmp = srctmp + src_len - 3;
	    //	    if ((strncmp((char *)srcp,"(DNLM)",6) == 0) &&
	    //		(strncmp((char *)srctmp,"(s)", 3) != 0)) {
	    //	      outputsfld=0;
	    //	    }
	    
	    /* If $a starts with "(OCoLC)", output first one */
	    /* sending message for subsequent ones */
	    if (strncmp((char *)srcp,"(OCoLC)",7)==0) {
	      if(ocolc==0) {
		ocolc=1;
		if(ocolccnt>1)
		  cm_error(CM_WARNING,"Multiple OCLC#s: %*.*s",src_len,src_len,srcp);
		
		/* If "(OCoLC)" is immediately followed by 'ocm' or 'ocn' */
		/* strip those characters from output */
		srctmp=srcp+7;
		if((strncmp((char *)srctmp,"ocm",3)==0) ||
		   (strncmp((char *)srctmp,"ocn",3)==0)) {
		  srctmp += 3;
		  cmp_buf_write(pp,"035:-1$a:+", (unsigned char *)"(OCoLC)", 7, 0);
		  cmp_buf_write(pp,"035:-1$a:-1",(unsigned char *)srctmp,src_len-10, 1);
		  outputsfld=0; /* set to 0 since we've just output what we wanted */
		}
	      }
	      else {
		cm_error(CM_WARNING,"Multiple OCLC#s: %*.*s",src_len,src_len,srcp);
		outputsfld=0;
	      }
	    }
	  }
	  
	  /* Copy anything else to the output field */
	  if(outputsfld!=0)
	    marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

	} /* End of subfields */
      } /* End of check for DNLM or SF9 */
    } /* End of 035's (may have been none) */
  } /* End of Processing for LDR 7 's' or 'b' */


  /* Additional QA checks */
  cmp_buf_find(pp, "ui", &srctmp, &srctlen);

  /* Check length to make sure from 7 to 19 characters */
  if((srctlen<7)||(srctlen>20)) {
    cm_error(CM_ERROR,"UI(035$9) is incorrect length, %d", srctlen);
    return CM_STAT_KILL_RECORD;
  }
  
  /* Check to make sure cnt9 is exactly 1 */
  if (cnt9 == 1)
    return CM_STAT_OK;
  else {
    cm_error(CM_ERROR, "There are %d 035$9's for this record. "
	     "Record not processed.", cnt9);
    return CM_STAT_KILL_RECORD;
  }
  
} /* cmp_proc_035 */


/************************************************************************
* cmp_proc_035_remarc ()                                                *
*                                                                       *
*   DEFINITION                                                          *
*       remarc_035 is custom procedure designed to:                     *
*       place $9 in 001 (and verify there is only one $9)               *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_035_remarc (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *srctmp;      /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                srctlen;      /* Length of temp source field         */
  int           occ,          /* Field occurrence counter            */
                socc,         /* Subfield occurrence counter         */
                rc,           /* Return code                         */
                sf_code,      /* Subfield code                       */
                cnt9,         /* Count of $9's                       */
                l001,         /* numeric value of 001                */
                l0359;        /* numeric value of 035$9              */

  cnt9 = 0;

  /* Loop through the 035's */
  for (occ=0; ; occ++) {

    if ((rc=marc_get_field(pp->inmp, 35, occ, &srcp, &src_len)) == 0) {

      /* Create new output field and copy over the indicators...       */
      if (add_field_and_indics(pp, 35) != 0 )
	return CM_STAT_KILL_RECORD;

      /* Read successive subfields                    */
      /* (start at 2 since 0 & 1 retrieve indicators) */
      for (socc=2; ; socc++) {

	if ((rc=marc_pos_subfield(pp->inmp, socc,
				  &sf_code, &srcp, &src_len)) == 0) {

	  /* Copy $9 to ui buffer and increment counter */
	  if (sf_code == '9') {
	    cnt9++;
	    cmp_buf_write(pp, "ui", srcp, src_len, 0);
	  }

	  /* Copy everything to the output field */
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
	}
	else /* End of marc_pos_subfield */
	  break;
      } /* end of "for (socc=2; ; socc++)" */
    }
    else /* End of marc_get_field for 035's (may have been none) */
      break;

  } /* end of "for (occ=0; ; occ++)" */

  /* Additional QA checks */
  cmp_buf_find(pp, "001:0$0:99", &srcp, &src_len);
  cmp_buf_find(pp, "ui", &srctmp, &srctlen);

  /* Check length to make sure from 7 to 9 characters */
  if((srctlen<7)||(srctlen>9)) {
    cm_error(CM_ERROR,"UI(035$9) is incorrect length, %d", srctlen);
    return CM_STAT_KILL_RECORD;
  }

  /* and make sure 001 = 035$9 + 100,000,000 */
  if (get_fixed_num ((char *) srcp, src_len, &l001)) {
    if (l001 > 883200) {

      if (get_fixed_num ((char *) srctmp, srctlen, &l0359)) {
   	if (l0359 != l001 + 100000000) {
	  cm_error(CM_ERROR,"035$9(%d) does not equal 001(%d) + 100,000,000."
		   "Record not processed.", l0359, l001);
	  return CM_STAT_KILL_RECORD;
	}
      }
    }
  }

  /* Check to make sure cnt9 is exactly 1 */
  if (cnt9 == 1)
    return CM_STAT_OK;
  else {
    cm_error(CM_ERROR, "There are %d 035$9's for this record. "
	     "Record not processed.", cnt9);
    return CM_STAT_KILL_RECORD;
  }

} /* cmp_proc_035_remarc */


/************************************************************************
* cmp_generic_035_processing ()                                         *
*                                                                       *
*   DEFINITION                                                          *
*       remarc_035 is custom procedure designed to:                     *
*       move $9 to 001 (and verify there is only one $9)                *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_generic_035_processing (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *srctmp;      /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                srctlen;      /* Length of temp source field         */
  char          *tmpp;
  int           occ,          /* Field occurrence counter            */
                socc,rc,         /* Subfield occurrence counter         */
                messages,     /* Subfield occurrence counter         */
                sf_code,      /* Subfield code                       */
                cnt9,         /* Count of $9's                       */
                cnta;         /* Count of $a's                       */

  cnta = cnt9 = messages = 0;

  /*=========================================================================*/
  /* Determine whether we're supposed to send out messages...                */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &srcp, &src_len);
  messages = (*srcp == '1');


  /*=========================================================================*/
  /* First, we loop through the 035's looking for $9's...                    */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,35,occ,&srcp,&src_len)==0;occ++) {

    /*=======================================================================*/
    /* For each $9...                                                        */
    /*=======================================================================*/
    for (socc=0;marc_get_item(pp->inmp,35,occ,'9',socc,&srcp,&src_len)==0;
	 socc++) {

      /*=====================================================================*/
      /* Increment the counter (but we shouldn't have more than one...       */
      /*=====================================================================*/
      cnt9++;

      if(cnt9>1) {
	cm_error(CM_ERROR, "There are %d 035$9's for this record. "
		 "Record not processed.", cnt9);
	return CM_STAT_KILL_RECORD;
      }

      /*=====================================================================*/
      /* Copy the value to the UI buffer for future reference...             */
      /*=====================================================================*/
      cmp_buf_write(pp, "ui", srcp, src_len, 0);
      srctmp=srcp;
      srctlen=src_len;

    } /* end of $9 loop */

  } /* end of 035 loop */


  /*=========================================================================*/
  /* If we don't have exactly one $9, exit with error message...             */
  /*=========================================================================*/
  if(cnt9!=1) {
    cm_error(CM_ERROR, "There are %d 035$9's for this record. "
	     "Record not processed.", cnt9);
    return CM_STAT_KILL_RECORD;
  }

  /*=========================================================================*/
  /* Then, loop through them again for output...                             */
  /*=========================================================================*/
  for(occ=0;(rc=marc_get_field(pp->inmp,35,occ,&srcp,&src_len))==0;occ++) {

    /*=======================================================================*/
    /* For each one, create a new output occurrence...                       */
    /*=======================================================================*/
    if (add_field_and_indics(pp, 35) != 0 )
	return CM_STAT_KILL_RECORD;

    /*=======================================================================*/
    /* and begin copying over contents of subfields (starting at 2)          */
    /*=======================================================================*/
    for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	socc++) {

      tmpp=(char *)srcp;
      /*=====================================================================*/
      /* We need to check the $a's starting with (DNLM)...                   */
      /*=====================================================================*/
      if((sf_code=='a')&&(strncmp(tmpp,"(DNLM)",6)==0)) {

	tmpp+=6;
	/*===================================================================*/
	/* If $a = $9, drop the $a...                                        */
	/*===================================================================*/
	if((strncmp(tmpp,(char *)srctmp,srctlen)==0)&&(srctlen==src_len-6)) {
	  cnta++;
	  continue;
	}
      }

      /*=====================================================================*/
      /* Send everything that gets to here over 'as-is'...                   */
      /*=====================================================================*/
      marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

    } /* end of subfield loop */

  } /* end of 035 loop */

  if(cnta>1)
    cm_error(CM_WARNING,"There were %d 035 $a's matching the 035$9. "
	     "All have been removed.",cnta);

  return CM_STAT_OK;

} /* cmp_generic_035_processing */


/************************************************************************
* cmp_proc_041 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 041 fields -- recognized only in Field Prep processing  *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       none                                                            *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_041 (
  CM_PROC_PARMS *pp          /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *tmpp,         /* Ptr to output data                  */
                 ind1;         /* Ptr to input data                   */
  size_t         src_len,      /* Length of source field              */
                 tmp_len;      /* Length of data at datatypep         */
  int            occ,          /* Field occurrence counter            */
                 socc,         /* Subfield occurrence counter         */
                 sf_code,      /* Subfield code                       */
                 cnt_nums,     /* Count of numerics                   */
                 have_a,       /* True=Bib record type                */
                 skip_b;       /* True=Bib record type                */


  /*=========================================================================*/
  /* Create new output field and copy over the indicators...                 */
  /*=========================================================================*/
  if (marc_add_field(pp->outmp, 41) != 0) {
    cm_error(CM_ERROR,"Error adding new 041 field. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }

  /*=========================================================================*/
  /* Indicator 1 must be non-blank; indicator 2 forced to blank              */
  /*=========================================================================*/
  marc_get_indic(pp->inmp,1,&ind1);
  if(ind1==' ') {
    cm_error(CM_ERROR, "041 Ind1 not coded. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }

  marc_set_indic(pp->outmp,1,ind1);
  marc_set_indic(pp->outmp,2,' ');

  have_a=skip_b=cnt_nums=0;
  /*=========================================================================*/
  /* Loop through subfields (start at 2 since 0 & 1 retrieve indicators)     */
  /*=========================================================================*/
  for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
      socc++) {

    cnt_nums++;
    /*=======================================================================*/
    /* Count each subfield going out...                                      */
    /*=======================================================================*/
    if(sf_code=='a') {
      have_a=1;
    }

    if(sf_code=='b') {
      skip_b=0;
      for(occ=0;marc_get_subfield(pp->inmp,'a',occ,&tmpp,&tmp_len)==0;occ++) {
	if(tmp_len==src_len) {
	  if(strncmp((char *)srcp,(char *)tmpp,tmp_len)==0) {
	    skip_b=1;
	    break;
	  }
	}
      }

      if(skip_b==1)
	continue;
    }

    marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
  }

  if(cnt_nums-have_a>0)
    return CM_STAT_OK;
  else {
    marc_del_field (pp->outmp);
    return CM_STAT_KILL_FIELD;
  }

} /* cmp_proc_041 */


/************************************************************************
* cmp_proc_066 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       proc_066 is custom procedure designed to:                       *
*          1) kill record if 066 $c contains "(Q" or "(4"               *
*       Recognized only in FIELD PREP processing                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_066 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp;        /* Ptr to data                         */
  char          *tmpp,
                *tsrc2;
  size_t         src_len;     /* Length of source field              */
  int            occ;         /* Field occurrence counter            */

  /*===================================================================*/
  /* Only 1 066 allowed but multiple $c's could exist so...            */
  /* Loop through successive $c's...                                   */
  /*===================================================================*/
  for (occ=0;marc_get_item(pp->inmp,66,0,'c',occ,&srcp,&src_len)==0;
       occ++) {
    tsrc2=(char *)srcp;

    for(tmpp=tsrc2;tmpp<tsrc2+src_len;tmpp++) {
      if((strncmp(tmpp,"(Q",2)==0) ||
	 (strncmp(tmpp,"(4",2)==0) ||
	 (strncmp(tmpp,"(N",2)==0) ||
	 (strncmp(tmpp,"(3",2)==0)) {

	cmp_buf_write(pp, "dropem", (unsigned char *) "1", 1, 0);
      }


      /*
      if(*tmpp=='(') {
	tmpp++;

	if(*tmpp=='Q') {
	  cm_error(CM_ERROR,"Field 066 $c contains '(Q'. "
		   "Record blocked...");
	  return CM_STAT_KILL_RECORD;
	}

	if(*tmpp=='4') {
	  cm_error(CM_ERROR,"Field 066 $c contains '(4'. "
		   "Record blocked...");
	  return CM_STAT_KILL_RECORD;
	}

	if(*tmpp=='N') {
	  cm_error(CM_ERROR,"Field 066 $c contains '(N'. "
		   "Record blocked...");
	  return CM_STAT_KILL_RECORD;
	}

	if(*tmpp=='3') {
	  cm_error(CM_ERROR,"Field 066 $c contains '(3'. "
		   "Record blocked...");
	  return CM_STAT_KILL_RECORD;
	}
      }
      */
    }
  }

  return CM_STAT_OK;

} /* cmp_proc_066 */


/************************************************************************
* cmp_punc_245 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       punc_245 is custom procedure designed to:                       *
*          1) place proper punctuation in the 245                       *
*                                                                       *
*             $a [$h] [ :$b] [ /$c] .                                   *
*             sf preceding $b get ' :'                                  *
*             sf preceding $c get ' /'                                  *
*             last sf ends in '.'                                       *
*                                                                       *
*       This should be the only process run against the 245             *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_punc_245 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *tsrcp;
  char          *tmpp,
                *tmp2;
  size_t         src_len,     /* Length of source field              */
                 tsrclen;     /* Length of source field              */
  int            socc,        /* Subfield occurrence counter         */
                 sf_code,     /* Subfield code                       */
                 sf_code2;    /* Subfield code                       */

  /*=========================================================================*/
  /* Create new output field and copy over the indicators...                 */
  /*=========================================================================*/
  if(add_field_and_indics(pp, 245)!=0)
    return CM_STAT_KILL_RECORD;

  /*=========================================================================*/
  /* Loop through subfields (start at 2 since 0 & 1 retrieve indicators)     */
  /*=========================================================================*/
  for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
      socc++) {

    /*=======================================================================*/
    /* Allocate memory based on size of source string...                     */
    /*=======================================================================*/

#ifdef DEBUG
    if ((tmpp = (char *) marc_alloc(src_len+4, 101)) == NULL) {   // TAG:101
        cm_error(CM_ERROR, "Error allocating memory for 245 processing. " "Record not processed");
        return CM_STAT_KILL_RECORD;
    }
#else
    if ((tmpp = (char *)malloc(src_len+4)) == NULL) {
      cm_error(CM_ERROR, "Error allocating memory for 245 processing. " "Record not processed");
      return CM_STAT_KILL_RECORD;
    }
#endif

    memset(tmpp, '\0', src_len+4);

    /*=======================================================================*/
    /* ...and then copy source over to destination                           */
    /*=======================================================================*/
    strncpy(tmpp, (char *)srcp, src_len);

    src_len = strlen(tmpp);

    /*=======================================================================*/
    /* Remove all trailing punctuation and blanks                            */
    /*=======================================================================*/
    for(tmp2=tmpp+src_len-1;
	(tmp2>=tmpp)&&
	  ((*tmp2==' ')||
	   (ispunct(*tmp2)&&(strchr(")]",*tmp2)==NULL))
	   );
	tmp2--);
    if(tmp2!=tmpp)
      tmp2++;
    *tmp2='\0';
    src_len = strlen(tmpp);


    /*=======================================================================*/
    /* Then check to see what the next subfield is...                        */
    /*=======================================================================*/
    if(marc_pos_subfield(pp->inmp,socc+1,&sf_code2,&tsrcp,&tsrclen)==0) {
      /*=====================================================================*/
      /* If it's $h, do nothing...                                           */
      /*=====================================================================*/
      if(sf_code2=='h') {
	/* nop; */
      }
      /*=====================================================================*/
      /* $b is preceded by " :"                                              */
      /*=====================================================================*/
      else if(sf_code2=='b') {
	strcat(tmpp," :");
      }
      /*=====================================================================*/
      /* $c is preceded by " /"                                              */
      /*=====================================================================*/
      else if(sf_code2=='c') {
	strcat(tmpp," /");
      }
    }
    /*=======================================================================*/
    /* Last subfield ends in "."                                             */
    /*=======================================================================*/
    else
      strcat(tmpp,".");

    marc_add_subfield(pp->outmp, sf_code, (unsigned char *)tmpp, strlen(tmpp));

  }

  return CM_STAT_OK;

} /* cmp_punc_245 */


/************************************************************************
* cmp_checkdups ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Recognized only in FIELD PREP processing                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Field to be checked for duplicates                              *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_checkdups (
    CM_PROC_PARMS *pp               /* Pointer to parameter struct  */
) {

  unsigned char *tsrcp,       /* Ptr to data w/o spaces collapsed    */
                *tmp1;
  char          *srcp,        /* Ptr to temp data                    */
                *src2,        /* Ptr to data with spaces collapsed   */
                *tmpp;        /* Ptr to data with spaces collapsed   */
  size_t        tsrc_len,     /* Length of source (collapsed)        */
                tmp1len,
                src_len;      /* Length of source (collapsed)        */
  int           focc,         /* Field occurrence counter            */
                rc,           /* Field occurrence counter            */
                tagno,        /* Field occurrence counter            */
                entry_cnt;    /* Field occurrence counter            */

  /*=========================================================================*/
  /* For debugging purposes...                                               */
  /*=========================================================================*/
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"101311145",9)==0)
    uip=uip;

  tagno = cmp_get_builtin(pp, "%fid");
  entry_cnt = 0;

  /*=========================================================================*/
  /* Loop through all the fld's...                                           */
  /*=========================================================================*/
  for(focc=0;marc_get_item(pp->inmp,tagno,focc,'a',0,&tsrcp,&tsrc_len)==0;
      focc++){

    /*=======================================================================*/
    /* We want to look at the $a data...                                     */
    /*=======================================================================*/
    if(allocate_n_ncopy(&src2,tsrc_len,1,(char *)tsrcp,
			NO_COLLAPSE_SPACES,pp->args[0])!=0)
      return CM_STAT_KILL_RECORD;

    srcp = (char *)tsrcp;
    src_len = strlen(srcp);

    /*=======================================================================*/
    /* and the $x data...                                                    */
    /*=======================================================================*/
    marc_get_item(pp->inmp,tagno,focc,'x',0,&tmp1,&tmp1len);

    /*=======================================================================*/
    /* concatenate $a & $x together with '!' separator                       */
    /*=======================================================================*/
    if(tmp1len>0) {

#ifdef DEBUG
        marc_dealloc(src2, 102);  //TAG:102
#else
        free(src2);
#endif

		src2 = NULL;

        if(allocate_n_ncopy(&src2,tsrc_len,tmp1len+1,(char *)tsrcp, NO_COLLAPSE_SPACES,pp->args[0])!=0)
	        return CM_STAT_KILL_RECORD;

        strcat(src2,"!");
        strncat(src2,(char *)tmp1,tmp1len);
    }

    src_len=strlen(src2);
    srcp=src2;

    /*=======================================================================*/
    /* See if this $a$x combo already exists...                              */
    /*=======================================================================*/
    if((entry_cnt) && (find_array_entry(DupChkTbl,(char *)srcp,src_len,&tmpp,
					EXACT_MATCH, NO_NORMALIZE_DATA))) {
      cm_error(CM_ERROR,"Duplicate %d found. Record not processed.", tagno);
      return CM_STAT_KILL_RECORD;
    }
    else {
      /*=====================================================================*/
      /* If not, add it to the list...                                       */
      /*=====================================================================*/

      /*=======================================================================*/
      /* Make sure we don't have too many                                      */
      /*=======================================================================*/
      if (entry_cnt >= MAX_TAGNUMS)
	cm_error (CM_FATAL, "Number of %d Tags exceeds programmed maximum of %d.",
		  tagno, MAX_TAGNUMS);

      /*=======================================================================*/
      /* Allocate space for each part and place them into decode element       */
      /*=======================================================================*/
      if ((rc=allocate_n_copy(&tmpp, srcp, "")) == 0)
	DupChkTbl[entry_cnt++] = tmpp;
    }

    /*=========================================================================*/
    /* "terminate" array elements                                              */
    /*=========================================================================*/
    if (entry_cnt < MAX_TAGNUMS)
      DupChkTbl[entry_cnt] = '\0';
  }

  for (;entry_cnt > 0; entry_cnt--) {
      printf ("ENTRY #%d\tENTRY CONTENT: '%s'\n", entry_cnt, DupChkTbl[entry_cnt]);

#ifdef DEBUG
      marc_dealloc(DupChkTbl[entry_cnt], 103);  //TAG:103
#else
      free(DupChkTbl[entry_cnt]);
#endif

      DupChkTbl[entry_cnt] = '\0';
  }

  return CM_STAT_OK;

} /* cmp_checkdups */



/************************************************************************
* cmp_proc_659 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 659 recognized only in Field prep                       *
*       Ignore if switch is set                                         *
*       Otherwise output as 655 after insuring:                         *
*         Indicator 1 = blank                                           *
*         Indicator 2 = '7'                                             *
*         $a exists and ends with a period '.'                          *
*         $2 exists                                                     *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       none                                                            *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_proc_659 (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *hlddata;      /* Ptr to output data                  */
  size_t         src_len;      /* Length of source field              */
  int            sf_code,      /* Subfield code                       */
                 rc;           /* Return code                         */
  char  out_data[999];/* Output data                         */


  /* if the switch is set... ignore this field */
  cmp_buf_find(pp, "&659S", &srcp, &src_len);
  if (*srcp == '1')
    return CM_STAT_KILL_FIELD;


  /* Make sure first indicator is blank */
  if ((rc=marc_pos_subfield(pp->inmp, 0, &sf_code, &srcp, &src_len)) != 0) {
    cm_error(CM_ERROR, "Retrieving first indicator from 659. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }
  /*!!!*/
  if (*srcp != ' ') {
    cm_error(CM_ERROR, "First indicator of 659 is not blank. Field not processed.");
    return CM_STAT_KILL_FIELD;
  }


  /* Create new output field and copy over the indicators...       */
  if (add_field_and_indics(pp, 655) != 0 )
    return CM_STAT_KILL_RECORD;

  /* do $a first... */
  if ((rc = marc_get_subfield(pp->inmp, 'a', 0, &srcp, &src_len)) != 0) {
    cm_error(CM_ERROR, "659 missing $a. Field not processed.");
    return CM_STAT_KILL_FIELD;
  }
  else {
    /* make sure it has a period at the end... */

    memset(out_data,'\0',999);
    strncpy(out_data,(char *)srcp,src_len);
    hlddata = srcp + src_len -1 ;
    if (*hlddata != '.') {
      strcat(out_data, ".");
      src_len++;
    }

    marc_add_subfield(pp->outmp, 'a', (unsigned char *)out_data, src_len);
  }

  /* Make sure second indicator is '7' */
  if ((rc=marc_pos_subfield(pp->inmp, 1, &sf_code, &srcp, &src_len)) != 0) {
    cm_error(CM_ERROR, "Retrieving second indicator from 659. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }

  if (*srcp == '7') {
    /* now do $2 */
    if ((rc = marc_get_subfield(pp->inmp, '2', 0, &srcp, &src_len)) != 0) {
      cm_error(CM_ERROR, "659 missing $2. Field not processed.");
      return CM_STAT_KILL_FIELD;
    }
    else
      marc_add_subfield(pp->outmp, '2', srcp, src_len);
  }

  /* if ind2 is not '7', it must be '2' and omit $2... */
  else {
    if(*srcp!='2') {
      cm_error(CM_ERROR, "Second indicator of 659 is not '7' or '2'. "
	       "Field not processed.");
      return CM_STAT_KILL_FIELD;
    }

    /* if '2', we should not have $2... */
    if ((rc = marc_get_subfield(pp->inmp, '2', 0, &srcp, &src_len)) == 0)
      cm_error(CM_WARNING, "659 has $2 when indicator 2 ='2'. $2 not output.");

  }

  return CM_STAT_OK;
} /* cmp_proc_659 */


/************************************************************************
* cmp_proc_76X ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 760 - 787 fields -- recognized only in Field Prep       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       Field id                                                        *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_proc_76X (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *indatap,      /* Ptr to output data                  */
                *outdatap;     /* Ptr to output data                  */
  size_t         src_len;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                 socc,         /* Subfield occurrence counter         */
                 sf_code;      /* Subfield code                       */
  char  out_data[999];/* Output data                         */


  /* Create new output field and copy over the indicators...       */
  sf_code = cmp_get_builtin(pp, "%fid");

  if (add_field_and_indics(pp, sf_code) != 0 )
    return CM_STAT_KILL_RECORD;

  /* (start at 2 since 0 & 1 retrieve indicators) */
  for (socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
       socc++) {

      /* If we have $w... */
      if (sf_code == 'w') {

	strcpy(out_data,"");
	/* outdatap & indatap are "movable" pointers    */
	/* so start them at their respective beginnings */
	outdatap = (unsigned char *)out_data;
	indatap = srcp;

	/* Only process those $w's that don't start with "(DNLM)" */
	if (strncmp((char *)srcp,"(DNLM)",6) != 0) {

	  /* If it starts with (DLC), then start numerics in column 4 */
	  if (strncmp((char *)srcp,"(DLC)",5) == 0) {

	    /* First, copy over anything in parentheses... */
	    if (strncmp((char *)srcp,"(",1) == 0) {
	      while (((indatap - srcp) < src_len) && (*indatap != ')')) {
		*outdatap++ = *indatap++;
	      }
	      *outdatap++ = *indatap++;
	    }

	    /* Copy over the next 3 alphas or spaces, if present... */
	    for (occ=0; occ<=2 && (indatap-srcp < src_len) &&
		   ((*indatap == ' ') || isalpha(*indatap)); occ++) {
	      *outdatap++ = *indatap++;
	    }

	    /* If there weren't two alphas or spaces, pad with blanks... */
	    while (occ <= 2) {
	      *outdatap++ = ' ';
	      ++occ;
	    }

	    /* Copy the rest of the data over */
	    while (indatap - srcp < src_len) {
	      *outdatap++ = *indatap++;
	    }

	    /* Send to output field */
	    marc_add_subfield(pp->outmp, sf_code, (unsigned char *)out_data,
			      outdatap-(unsigned char *)out_data);
	  }
	  else
	    /* If it doesn't start with (DLC), just copy it over */
	    marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
	}
      }
      else
	/* Copy everything else except $9 to the output field */
	if (sf_code != '9')
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

  } /* end of "for (socc=2; ; socc++)" */

  return CM_STAT_OK;
} /* cmp_proc_76X */


/************************************************************************
* cmp_proc_856 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Process 856 field        -- recognized only in Field Prep       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       none                                                            *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_proc_856 (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
  unsigned char *srcp,         /* Ptr to input data                   */
                *outdatap,     /* Ptr to output data                  */
                *indatap;      /* Ptr to input data                   */
  size_t         src_len;      /* Length of source field              */
  int            socc,         /* Subfield code                       */
                 sf_code,      /* Subfield code                       */
                 rc;           /* Return code                         */
  char  out_data[999];/* Output data                         */


  if (add_field_and_indics(pp, 856) != 0 )
    return CM_STAT_KILL_RECORD;

  /* (start at 2 since 0 & 1 retrieve indicators) */
  for (socc=2; ; socc++) {

    /* Loop through all subfields */
    if ((rc=marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)) == 0) {

      /* Suppressing all $z's */
      /* 05/04/21 suppress $n's from output */
      if ((sf_code != 'z') &&
	  (sf_code != 'n')) {

	/* converting all _'s to %5F and all ~'s to %7E */
	memset(out_data, '\0', 999);

	/* outdatap & indatap are "movable" pointers    */
	/* so start them at their respective beginnings */
	indatap = srcp;
	outdatap = (unsigned char *)out_data;

	while ((indatap - srcp) < src_len) {
	  if(*indatap == '_') {
	    *outdatap++ = '%';
	    *outdatap++ = '5';
	    *outdatap++ = 'F';
	  }
	  else
	    if(*indatap == '~') {
	      *outdatap++ = '%';
	      *outdatap++ = '7';
	      *outdatap++ = 'E';
	    }
	    else
	      *outdatap++ = *indatap++;
	}
	/* sen $x's to $z */
	if(sf_code == 'x')
	  marc_add_subfield(pp->outmp, 'z',     (unsigned char *)out_data, strlen(out_data));
	else
	/* Send to output field */
	  marc_add_subfield(pp->outmp, sf_code, (unsigned char *)out_data, strlen(out_data));
      }
    }
    else
      /* End of subfields */
      break;
  } /* end of "for (socc=2; ; socc++)" */

  return CM_STAT_OK;
} /* cmp_proc_856 */


/************************************************************************
* cmp_proc_880 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       proc_880 is custom procedure designed to:                       *
*          1) kill record if 880 is missing hex 1b                      *
*       Recognized only in FIELD PREP processing                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_880 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp;        /* Ptr to data                         */
  char         *tmpp,
               *tsrc2;
  size_t        src_len;      /* Length of source field              */
  int           occ,          /* Field occurrence counter            */
                flag;         /* Subfield code                       */

  /*=========================================================================*/
  /* Loop through successive 880's...                                        */
  /*=========================================================================*/
  for (occ=0;marc_get_field(pp->inmp,880,occ,&srcp,&src_len)==0;occ++) {
    flag=0;
    tsrc2=(char *)srcp;
    for(tmpp=tsrc2;(tmpp<tsrc2+src_len)&&(flag==0);tmpp++)
      if(*tmpp=='\x1b')
	flag=1;
    if(flag==0) {
      cm_error(CM_ERROR,"Corrupted field 880[%d]: "
	       "Escape sequence(s) missing. Record blocked...", occ+1);
      return CM_STAT_KILL_RECORD;
    }

  }

  return CM_STAT_OK;

} /* cmp_proc_880 */


/************************************************************************
* cmp_proc_998 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       prep_998 is custom procedure designed to:                       *
*          1) check for $a valid of passed argument and                 *
*          2) insure $b is in YYYYMMDD format                           *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_998 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *tsrcp;       /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                srctlen;      /* Length of temp source field         */
  int           occ;          /* Field occurrence counter            */

  /*=========================================================================*/
  /* First retrieve specified argument...                                    */
  /*=========================================================================*/
  cmp_buf_find(pp,pp->args[0],&tsrcp,&srctlen);

  /*=========================================================================*/
  /* Loop through successive 998's...                                        */
  /*=========================================================================*/
  for (occ=0;marc_get_item(pp->inmp,998,occ,'a',0,&srcp,&src_len)==0;occ++) {

    /*=======================================================================*/
    /* If $a = argument...                                                   */
    /*=======================================================================*/
      if((strncmp((char *)srcp,(char *)tsrcp,srctlen)==0)&&
	 (src_len==srctlen)) {

	/*===================================================================*/
	/* Make sure $b is YYYYMMDD format...                                */
	/*===================================================================*/
	if (marc_get_item(pp->inmp,998,occ,'b',0,&srcp,&src_len)==0) {

	  if (str_date_format(srcp, src_len, YYYYMMDD)) {
	    return CM_STAT_OK;
	  }
	}
      }
  }

  return CM_STAT_KILL_RECORD;

} /* cmp_proc_998 */


/************************************************************************
* cmp_checklen ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Check field length for multiple of specified argument...        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       none                                                            *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/

CM_STAT cmp_checklen (
    CM_PROC_PARMS *pp       /* Pointer to parameter structure       */
) {
  unsigned char *srcp;         /* Ptr to input data                   */
  char          *tmpp;
  size_t         src_len;      /* Length of source field              */
  int            fldlen,       /* Field length passed from argument   */
                 tag,          /* Field tag id                        */
                 sf_code;      /* subfield  id                        */


  /*=========================================================================*/
  /* First retrieve specified argument...                                    */
  /*=========================================================================*/
  cmp_buf_find(pp,pp->args[0],&srcp,&src_len);

  /*=========================================================================*/
  /* Make sure we can convert it to a number...                              */
  /*=========================================================================*/
  if (get_fixed_num ((char *) srcp, src_len, &fldlen)) {

    /*=======================================================================*/
    /* Then retrieve incoming data...                                        */
    /*=======================================================================*/
    cmp_buf_find(pp, "%data", &srcp, &src_len);


    tmpp=(char *)srcp+src_len-1;
    /*=======================================================================*/
    /* and check to see if it's length is a multiple of the argument...      */
    /*=======================================================================*/
    if((src_len % fldlen == 0)
       && (*tmpp!='.')
       && (*tmpp!=']')
       ){

      /*=====================================================================*/
      /* If so, send out a message along with the data itself...             */
      /*=====================================================================*/
      tag = cmp_get_builtin(pp, "%fid");
      sf_code = cmp_get_builtin(pp,"%sid");

      cm_error(CM_WARNING,"Length of %d $%c data (%d) "
	       "is a multiple of %d (%d x %d):",
	       tag,sf_code,src_len,fldlen,fldlen,src_len/fldlen);
      if(src_len<256)
	cm_error(CM_CONTINUE,"%*.*s",src_len,src_len,(char *)srcp);
      else {
	tmpp=tmpp-255;
	cm_error(CM_CONTINUE,"%*.*s",256,256,tmpp);
	cm_error(CM_CONTINUE,"Last 256 characters displayed...");
      }

    }
  }

  return CM_STAT_OK;
} /* cmp_checklen */


/************************************************************************
* add_field_and_indics                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       add a new occurrence of a field (field_id)                      *
*       copy over the indicators from the input field identified        *
*            by pp->inmp (assumes already populated)                    *
*       performs error checking at each step of the process.            *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       field_id      field identifier                                  *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problems encountered                                       *
************************************************************************/

CM_STAT add_field_and_indics (
    CM_PROC_PARMS *pp,       /* Pointer to parameter structure      */
    int     field_id         /* Field tag/identifier                */
) {
  unsigned char *srcp;       /* Ptr to data                         */
  size_t         src_len;    /* Length of source field              */
  int            socc,       /* Subfield occurrence counter         */
                 sf_code,    /* Subfield code                       */
                 rc;         /* Return code                         */

  /* Create new output field and copy over the indicators...       */
  /* If we end up with no subfields, the driver will automatically */
  /* remove this field from output                                 */
  if ((rc=marc_add_field(pp->outmp, field_id)) != 0) {
    cm_error(CM_ERROR,"Error adding new %d field. Record not processed.",
	     field_id);
    return CM_STAT_KILL_RECORD;
  }

  for (socc=0; socc<2; socc++) {
    if((rc=marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len))==0){
      if((rc=marc_set_indic(pp->outmp,sf_code,*srcp))!=0){
	cm_error(CM_ERROR, "Error setting indicator %d for %d. "
		 "Record not processed.", socc, field_id);
	return CM_STAT_KILL_RECORD;
      }
    }
    else {
      cm_error(CM_ERROR, "Error retrieving indicator %d for %d. "
	       "Record not processed.", socc, field_id);
      return CM_STAT_KILL_RECORD;
    }
  }

  return CM_STAT_OK;
}

/************************************************************************
* str_date_format                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       determines whether a specified date is in a specific date format*
*                                                                       *
*   PARAMETERS                                                          *
*       tmpp    ptr to data                                             *
*       tmp_len length of data                                          *
*       fmt     date format to check for                                *
*                                                                       *
*   RETURN                                                              *
*       1 data is in specified date format                              *
*       0 data is *NOT* in specified date format                        *
************************************************************************/

int str_date_format(unsigned char *tmpp, int tmp_len, char *fmt)
{
  unsigned char *hlddata;      /* (movable) Ptr to data               */

 /* if not proper length, don't go any further */
  if (tmp_len == strlen(fmt)) {

    /* first, let's make sure all are numeric */
    for (hlddata=tmpp; hlddata-tmpp < tmp_len; hlddata++) {
      if (!isdigit(*hlddata)) {
	return 0;
      }
    }
    return 1;
  }
  return 0;
}



/************************************************************************
* cmp_getUIfrom035()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Get the 035$9 UI.  Put it in buffer "ui".                       *
*       Don't do anything else, unlike proc_035.                        *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       CM_STAT_OK = Success.                                           *
*       Else CM_STAT_ERROR                                              *
************************************************************************/

CM_STAT cmp_getUIfrom035 (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
    int           focc,         /* Field occurrence number  */
                  stat;         /* Return code              */
    unsigned char *datap;       /* Pointer to data          */
    size_t        datalen;      /* Data length              */


    /* Get 035$9 */
    focc = 0;
    while ((stat = marc_get_field (pp->inmp, 35, focc, &datap, &datalen))
                 == 0) {
        if ((marc_get_subfield (pp->inmp, '9', 0, &datap, &datalen)) == 0) {

            /* Copy it to the ui buffer */
	        cmp_buf_write(pp, "ui", datap, datalen, 0);
            break;
        }
        ++focc;
    }

    if (stat) {
        cm_error (CM_ERROR, "Error %d getting 035$9", stat);
        return CM_STAT_ERROR;
    }

    return CM_STAT_OK;
}


/************************************************************************
* cmp_getUIfrom035()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Get the 035$9 UI.  Put it in buffer "ui".                       *
*       Don't do anything else, unlike proc_035.                        *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       CM_STAT_OK = Success.                                           *
*       Else CM_STAT_ERROR                                              *
************************************************************************/

CM_STAT cmp_naco_cleanup (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
    unsigned char *srcp;       /* Ptr to data                         */
    char          *tmpp;
    size_t         src_len;    /* Length of source field              */
    int            socc,       /* Subfield occurrence counter         */
                   fld,        /* Field id                            */
                   sf_code;    /* Subfield code                       */


    /*=======================================================================*/
    /* Determine which field we're working on next...                        */
    /*=======================================================================*/
    fld = cmp_get_builtin(pp, "%fid");

    /*=========================================================================*/
    /* Create new output field and copy over the indicators...                 */
    /*=========================================================================*/
    if(add_field_and_indics(pp, fld)!=0)
      return CM_STAT_KILL_RECORD;

    /*=========================================================================*/
    /* Loop through subfields (start at 2 since 0 & 1 retrieve indicators)     */
    /*=========================================================================*/
    for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	socc++) {

      /* Start walking through entire string */
      for(tmpp=(char *)srcp; tmpp < (char *)srcp + src_len-1; tmpp++) {

	/* We're looking for hex <CA><BE>, so look for <CA> first... */
	if(*tmpp=='\xca') {
	  /* If we find it, check to see if next character is <BE> */
	  tmpp++;

	  if(*tmpp=='\xbe') {
	    /* If so, change it to <BC> */
	    *tmpp='\xbc';
	  }
	}
      }

      /* After processing entire string,  copy it over to the output field */
      marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
    }

    return CM_STAT_OK;
}

