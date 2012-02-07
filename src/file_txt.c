/*

    File: file_txt.c

    Copyright (C) 2005-2009 Christophe GRENIER <grenier@cgsecurity.org>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <ctype.h>      /* tolower */
#include <stdio.h>
#include "types.h"
#include "common.h"
#include "filegen.h"
#include "log.h"
#include "memmem.h"
#include "file_txt.h"

extern const file_hint_t file_hint_doc;
extern const file_hint_t file_hint_jpg;
extern const file_hint_t file_hint_pdf;
extern const file_hint_t file_hint_tiff;
extern const file_hint_t file_hint_zip;

static inline int filtre(unsigned char car);

static void register_header_check_txt(file_stat_t *file_stat);
static int header_check_txt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new);
static void register_header_check_fasttxt(file_stat_t *file_stat);
static int header_check_fasttxt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new);
#ifdef UTF16
static int header_check_le16_txt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new);
#endif

static int data_check_txt(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery);
static void file_check_emlx(file_recovery_t *file_recovery);
static void file_check_ers(file_recovery_t *file_recovery);
static void file_check_svg(file_recovery_t *file_recovery);
static void file_check_smil(file_recovery_t *file_recovery);
static void file_check_xml(file_recovery_t *file_recovery);

const file_hint_t file_hint_fasttxt= {
  .extension="tx?",
  .description="Text files with header: rtf,xml,xhtml,mbox/imm,pm,ram,reg,sh,slk,stp,jad,url",
  .min_header_distance=0,
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_fasttxt
};

const file_hint_t file_hint_txt= {
  .extension="txt",
  .description="Other text files: txt,html,asp,bat,C,jsp,perl,php,py/emlx... scripts",
  .min_header_distance=0,
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_txt
};

static const unsigned char header_adr[25]	= "Opera Hotlist version 2.0";
static const unsigned char header_bash[11]  	= "#!/bin/bash";
static const unsigned char header_cls[24]	= {'V','E','R','S','I','O','N',' ','1','.','0',' ','C','L','A','S','S','\r','\n','B','E','G','I','N'};
static const unsigned char header_cue1[10]	= "REM GENRE ";
static const unsigned char header_cue2[6]	= { 'F', 'I', 'L', 'E', ' ', '"'};
static const unsigned char header_dc[6]		= "SC V10";
static const unsigned char header_dif[12]	= { 'T', 'A', 'B', 'L', 'E', '\r', '\n', '0', ',', '1', '\r', '\n'};
static const unsigned char header_emka[16]	= { '1', '\t', '\t', '\t', '\t', '\t', 't', 'h','i','s',' ','f','i','l','e','\t'};
static const unsigned char header_ers[19]	= "DatasetHeader Begin";
static const unsigned char header_html[14]	= "<!DOCTYPE HTML";
static const unsigned char header_ics[15]	= "BEGIN:VCALENDAR";
static const unsigned char header_imm[13]	= {'M','I','M','E','-','V','e','r','s','i','o','n',':'};
static const unsigned char header_jad[9]	= { 'M', 'I', 'D', 'l', 'e', 't', '-', '1', ':'};
static const unsigned char header_json[31]	= {
  '{', '"', 't', 'i', 't', 'l', 'e', '"',
  ':', '"', '"', ',', '"', 'i', 'd', '"',
  ':', '1', ',', '"', 'd', 'a', 't', 'e',
  'A', 'd', 'd', 'e', 'd', '"', ':' };
static const unsigned char header_ksh[10]  	= "#!/bin/ksh";
static const unsigned char header_ly[11]	= { '\n', '\\', 'v', 'e', 'r', 's', 'i', 'o', 'n', ' ', '"'};
static const unsigned char header_lyx[7]	= {'#', 'L', 'y', 'X', ' ', '1', '.'};
static const unsigned char header_m3u[7]	= {'#','E','X','T','M','3','U'};
static const unsigned char header_mail[19]	= {'F','r','o','m',' ','M','A','I','L','E','R','-','D','A','E','M','O','N',' '};
static const unsigned char header_mail2[5]	= {'F','r','o','m',' '};
static const unsigned char header_mdl[7]	= {'M','o','d','e','l',' ','{'};
static const unsigned char header_mnemosyne[48]	= {
  '-', '-', '-', ' ', 'M', 'n', 'e', 'm',
  'o', 's', 'y', 'n', 'e', ' ', 'D', 'a',
  't', 'a', ' ', 'B', 'a', 's', 'e', ' ',
  '-', '-', '-', ' ', 'F', 'o', 'r', 'm',
  'a', 't', ' ', 'V', 'e', 'r', 's', 'i',
  'o', 'n', ' ', '2', ' ', '-', '-', '-'
};
static const unsigned char header_msf[19]	= "// <!-- <mdb:mork:z";
static const unsigned char header_mysql[14]	= { '-', '-', ' ', 'M', 'y', 'S', 'Q', 'L', ' ', 'd', 'u', 'm', 'p', ' '};
static const unsigned char header_perlm[7] 	= "package";
static const unsigned char header_phpMyAdmin[22]= {
  '-', '-', ' ', 'p', 'h', 'p', 'M', 'y',
  'A', 'd', 'm', 'i', 'n', ' ', 'S', 'Q',
  'L', ' ', 'D', 'u', 'm', 'p'};
static const unsigned char header_postgreSQL[38]= {
  '-', '-', '\n', '-', '-', ' ', 'P', 'o',
  's', 't', 'g', 'r', 'e', 'S', 'Q', 'L',
  ' ', 'd', 'a', 't', 'a', 'b', 'a', 's',
  'e', ' ', 'c', 'l', 'u', 's', 't', 'e',
  'r', ' ', 'd', 'u', 'm', 'p'};
static const unsigned char header_postgreSQL_win[39]= {
  '-', '-', '\r', '\n', '-', '-', ' ', 'P',
  'o', 's', 't', 'g', 'r', 'e', 'S', 'Q',
  'L', ' ', 'd', 'a', 't', 'a', 'b', 'a',
  's', 'e', ' ', 'c', 'l', 'u', 's', 't',
  'e', 'r', ' ', 'd', 'u', 'm', 'p'};
static const unsigned char header_qgis[15]	= "<!DOCTYPE qgis ";
static const unsigned char header_ram[7]	= "rtsp://";
static const unsigned char header_ReceivedFrom[14]= {'R','e','c','e','i','v','e','d',':',' ','f','r','o','m'};
static const unsigned char header_reg[8]  	= "REGEDIT4";
static const unsigned char header_ReturnPath[13]= {'R','e','t','u','r','n','-','P','a','t','h',':',' '};
static const unsigned char header_rpp[16]	= { '<', 'R', 'E', 'A', 'P', 'E', 'R', '_', 'P', 'R', 'O', 'J', 'E', 'C', 'T', ' '};
static const unsigned char header_rtf[5]	= { '{','\\','r','t','f'};
static const unsigned char header_seenezsst[8]=  {
  0x23, 'S' , 'e' , 'e' , 'N' , 'e' , 'z' , ' ' ,
};
/* firefox session store */
static const unsigned char header_sessionstore[42]  	= "({\"windows\":[{\"tabs\":[{\"entries\":[{\"url\":\"";
static const unsigned char header_sh[9]  	= "#!/bin/sh";
static const unsigned char header_slk[10]  	= "ID;PSCALC3";
static const unsigned char header_smil[6]  	= "<smil>";
static const unsigned char header_snz_unix[8]=  {
  'D' , 'E' , 'F' , 'A' , 'U' , 'L' , 'T' ,'\n'
};
static const unsigned char header_snz_win[9]=  {
  'D' , 'E' , 'F' , 'A' , 'U' , 'L' , 'T' ,'\r', '\n'
};
static const unsigned char header_stl[6]	= "solid ";
static const unsigned char header_stp[13]  	= "ISO-10303-21;";
static const unsigned char header_ttd[55]	= "FF 09 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FFFF 00";
static const unsigned char header_url[18]  	= {
  '[', 'I', 'n', 't', 'e', 'r', 'n', 'e',
  't', 'S', 'h', 'o', 'r', 't', 'c', 'u',
  't', ']'
};
static const unsigned char header_wpl[21]	= { '<', '?', 'w', 'p', 'l', ' ', 'v', 'e', 'r', 's', 'i', 'o', 'n', '=', '"', '1', '.', '0', '"', '?', '>' };
static const unsigned char header_xml[14]	= "<?xml version=";
static const unsigned char header_xml_utf8[17]	= {0xef, 0xbb, 0xbf, '<', '?', 'x', 'm', 'l', ' ', 'v', 'e', 'r', 's', 'i', 'o', 'n', '='};
static const unsigned char header_xmp[35]	= {
  '<', 'x', ':', 'x', 'm', 'p', 'm', 'e',
  't', 'a', ' ', 'x', 'm', 'l', 'n', 's',
  ':', 'x', '=', '"', 'a', 'd', 'o', 'b',
  'e', ':', 'n', 's', ':', 'm', 'e', 't',
  'a', '/', '"'};
static const unsigned char header_vbookmark[10]	= { 'B', 'E', 'G', 'I', 'N', ':', 'V', 'B', 'K', 'M'};
static const char sign_java1[6]			= "class";
static const char sign_java3[15]		= "private static";
static const char sign_java4[17]		= "public interface";
static unsigned char ascii_char[256];
static const unsigned char hdr_header[17]=  {
  'E' , 'N' , 'V' , 'I' , 0x0d, 0x0a, 'd' , 'e' ,
  's' , 'c' , 'r' , 'i' , 'p' , 't' , 'i' , 'o' ,
  'n' 
};


static void register_header_check_txt(file_stat_t *file_stat)
{
  unsigned int i;
  for(i=0; i<256; i++)
    ascii_char[i]=i;
  for(i=0; i<256; i++)
  {
    if(filtre(i) || i==0xE2 || i==0xC2 || i==0xC3 || i==0xC5 || i==0xC6 || i==0xCB)
      register_header_check(0, &ascii_char[i], 1, &header_check_txt, file_stat);
  }
#ifdef UTF16
  register_header_check(1, &ascii_char[0], 1, &header_check_le16_txt, file_stat);
#endif
}

static void register_header_check_fasttxt(file_stat_t *file_stat)
{
  register_header_check(0, header_adr, sizeof(header_adr), &header_check_fasttxt, file_stat);
  register_header_check(0, header_bash,sizeof(header_bash), &header_check_fasttxt, file_stat);
  register_header_check(0, header_cls,sizeof(header_cls), &header_check_fasttxt, file_stat);
  register_header_check(0, header_cue1,sizeof(header_cue1), &header_check_fasttxt, file_stat);
  register_header_check(0, header_cue2,sizeof(header_cue2), &header_check_fasttxt, file_stat);
  register_header_check(4, header_dc, sizeof(header_dc), &header_check_fasttxt, file_stat);
  register_header_check(0, header_dif, sizeof(header_dif), &header_check_fasttxt, file_stat);
  register_header_check(0, header_emka, sizeof(header_emka), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ers,sizeof(header_ers), &header_check_fasttxt, file_stat);
  register_header_check(0, hdr_header, sizeof(hdr_header), &header_check_fasttxt, file_stat);
  register_header_check(0, header_html, sizeof(header_html), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ics, sizeof(header_ics), &header_check_fasttxt, file_stat);
  register_header_check(0, header_imm,sizeof(header_imm), &header_check_fasttxt, file_stat);
  register_header_check(0, header_jad, sizeof(header_jad), &header_check_fasttxt, file_stat);
  register_header_check(0, header_json, sizeof(header_json), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ksh,sizeof(header_ksh), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ly, sizeof(header_ly), &header_check_fasttxt, file_stat);
  register_header_check(0, header_lyx,sizeof(header_lyx), &header_check_fasttxt, file_stat);
  register_header_check(0, header_m3u, sizeof(header_m3u), &header_check_fasttxt, file_stat);
  register_header_check(0, header_mail,sizeof(header_mail), &header_check_fasttxt, file_stat);
  register_header_check(0, header_mail2,sizeof(header_mail2), &header_check_fasttxt, file_stat);
  register_header_check(0, header_mdl, sizeof(header_mdl),  &header_check_fasttxt, file_stat);
  register_header_check(0, header_mnemosyne, sizeof(header_mnemosyne), &header_check_fasttxt, file_stat);
  register_header_check(0, header_msf, sizeof(header_msf), &header_check_fasttxt, file_stat);
  register_header_check(0, header_mysql, sizeof(header_mysql), &header_check_fasttxt, file_stat);
  register_header_check(0, header_perlm,sizeof(header_perlm), &header_check_fasttxt, file_stat);
  register_header_check(0, header_phpMyAdmin, sizeof(header_phpMyAdmin), &header_check_fasttxt, file_stat);
  register_header_check(0, header_postgreSQL, sizeof(header_postgreSQL), &header_check_fasttxt, file_stat);
  register_header_check(0, header_postgreSQL_win, sizeof(header_postgreSQL_win), &header_check_fasttxt, file_stat);
  register_header_check(0, header_qgis, sizeof(header_qgis), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ram,sizeof(header_ram), &header_check_fasttxt, file_stat);
  register_header_check(0, header_reg,sizeof(header_reg), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ReturnPath,sizeof(header_ReturnPath), &header_check_fasttxt, file_stat);
  register_header_check(0, header_rpp,sizeof(header_rpp), &header_check_fasttxt, file_stat);
  register_header_check(0, header_rtf,sizeof(header_rtf), &header_check_fasttxt, file_stat);
  register_header_check(0, header_seenezsst, sizeof(header_seenezsst), &header_check_fasttxt, file_stat);
  register_header_check(0, header_sessionstore, sizeof(header_sessionstore), &header_check_fasttxt, file_stat);
  register_header_check(0, header_sh,sizeof(header_sh), &header_check_fasttxt, file_stat);
  register_header_check(0, header_slk,sizeof(header_slk), &header_check_fasttxt, file_stat);
  register_header_check(0, header_smil,sizeof(header_smil), &header_check_fasttxt, file_stat);
  register_header_check(0, header_snz_unix, sizeof(header_snz_unix), &header_check_fasttxt, file_stat);
  register_header_check(0, header_snz_win, sizeof(header_snz_win), &header_check_fasttxt, file_stat);
  register_header_check(0, header_stl,sizeof(header_stl), &header_check_fasttxt, file_stat);
  register_header_check(0, header_stp,sizeof(header_stp), &header_check_fasttxt, file_stat);
  register_header_check(0, header_ttd, sizeof(header_ttd), &header_check_fasttxt, file_stat);
  register_header_check(0, header_url,sizeof(header_url), &header_check_fasttxt, file_stat);
  register_header_check(0, header_wpl,sizeof(header_wpl), &header_check_fasttxt, file_stat);
  register_header_check(0, header_xml,sizeof(header_xml), &header_check_fasttxt, file_stat);
  register_header_check(0, header_xml_utf8,sizeof(header_xml_utf8), &header_check_fasttxt, file_stat);
  register_header_check(0, header_xmp,sizeof(header_xmp), &header_check_fasttxt, file_stat);
  register_header_check(0, header_vbookmark, sizeof(header_vbookmark), &header_check_fasttxt, file_stat);
}

// #define DEBUG_FILETXT

/* return 1 if char can be found in text file */
static int filtre(unsigned char car)
{
  switch(car)
  {
    case 0x7c:  /* similar to | */
    case 0x80:
    case 0x92:
    case 0x99:
    case 0x9c:	/* '�' */
    case 0xa0:  /* nonbreaking space */
    case 0xa1:  /* '�' */
    case 0xa2:
    case 0xa3:	/* '�' */
    case 0xa7:	/* '�' */
    case 0xa8:
    case 0xa9:	/* '�' */
    case 0xab:	/* '�' */
    case 0xae:	/* '�' */
    case 0xb0:	/* '�' */
    case 0xb4:  /* '�' */
    case 0xb7:
    case 0xbb:  /* '�' */
    case 0xc0:  /* '�' */
    case 0xc7:  /* '�' */
    case 0xc9:  /* '�' */
    case 0xd6:  /* '�' */
    case 0xd7:
    case 0xd9:  /* '�' */
    case 0xdf:
    case 0xe0: 	/* '�' */
    case 0xe1: 	/* '�' */
    case 0xe2: 	/* '�' */
    case 0xe3:  /* '�' */
    case 0xe4: 	/* '�' */
    case 0xe6:  /* '�' */
    case 0xe7: 	/* '�' */
    case 0xe8: 	/* '�' */
    case 0xe9: 	/* '�' */
    case 0xea: 	/* '�' */
    case 0xeb: 	/* '�' */
    case 0xed:  /* '�' */
    case 0xee: 	/* '�' */
    case 0xef: 	/* '�' */
    case 0xf4: 	/* '�' */
    case 0xf6: 	/* '�' */
    case 0xf8:  /* '�' */
    case 0xf9: 	/* '�' */
    case 0xfa:  /* '�' */
    case 0xfb: 	/* '�' */
    case 0xfc: 	/* '�' */
      return 1;
  }
  if((car=='\b')||(car=='\t')||(car=='\r')||(car=='\n')
    ||((car>=' ')&&(car<='~'))
    ||((car>=0x82)&&(car<=0x8d))
    ||((car>=0x93)&&(car<=0x98))
    )
    return 1;
  return 0;
}

/* destination should have an extra byte available for null terminator
   return read size */
int UTF2Lat(unsigned char *buffer_lower, const unsigned char *buffer, const int buf_len)
{
  const unsigned char *p; 	/* pointers to actual position in source buffer */
  unsigned char *q;	/* pointers to actual position in destination buffer */
  int i; /* counter of remaining bytes available in destination buffer */
  for (i = buf_len, p = buffer, q = buffer_lower; p-buffer<buf_len && i > 0 && *p!='\0';) 
  {
    const unsigned char *p_org=p;
    if((*p & 0xf0)==0xe0 && (*(p+1) & 0xc0)==0x80 && (*(p+2) & 0xc0)==0x80)
    { /* UTF8 l=3 */
#ifdef DEBUG_TXT
      log_info("UTF8 l=3 0x%02x 0x%02x 0x02x\n", *p, *(p+1),*(p+2));
#endif
      *q = '\0';
      switch (*p)
      {
        case 0xE2 : 
          switch (*(p+1))
          { 
            case 0x80 : 
              switch (*(p+2))
              { 
                case 0x93 : (*q) = 150; break;
                case 0x94 : (*q) = 151; break;
                case 0x98 : (*q) = 145; break;
                /* case 0x99 : (*q) = 146; break; */
                case 0x99 : (*q) = '\''; break;
                case 0x9A : (*q) = 130; break;
                case 0x9C : (*q) = 147; break;
                case 0x9D : (*q) = 148; break;
                case 0x9E : (*q) = 132; break;
                case 0xA0 : (*q) = 134; break;
                case 0xA1 : (*q) = 135; break;
                case 0xA2 : (*q) = 149; break;
                case 0xA6 : (*q) = 133; break;
                case 0xB0 : (*q) = 137; break;
                case 0xB9 : (*q) = 139; break;
                case 0xBA : (*q) = 155; break;
              }
              break;
            case 0x82 : 
              switch (*(p+2))
              { 
                case 0xAC : (*q) = 128; break;
              }
              break;
            case 0x84 : 
              switch (*(p+2))
              { 
                case 0xA2 : (*q) = 153; break;
              }
              break;
          }
          break;
      }
      p+=3;
    }
    else if((*p & 0xe0)==0xc0 && (*(p+1) & 0xc0)==0x80)
    { /* UTF8 l=2 */
      *q = '\0';
      switch (*p)
      {
        case 0xC2 : 
          (*q) = ((*(p+1)) | 0x80) & 0xBF; /* A0-BF and a few 80-9F */
          if((*q)==0xA0)
            (*q)=' ';
          break;
        case 0xC3 : 
          switch (*(p+1))
	  { 
	    case 0xB3 : (*q) = 162; break;
	    default:
			(*q) = (*(p+1)) | 0xC0; /* C0-FF */
			break;
	  }
          break;
        case 0xC5 : 
          switch (*(p+1)) { 
            case 0x92 : (*q) = 140; break;
            case 0x93 : (*q) = 156; break;
            case 0xA0 : (*q) = 138; break;
            case 0xA1 : (*q) = 154; break;
            case 0xB8 : (*q) = 143; break;
            case 0xBD : (*q) = 142; break;
            case 0xBE : (*q) = 158; break;
          }
          break;
        case 0xC6: 
          switch (*(p+1)) { 
            case 0x92 : (*q) = 131; break;
          }
          break;
        case 0xCB : 
          switch (*(p+1)) { 
            case 0x86 : (*q) = 136; break;
            case 0x9C : (*q) = 152; break;
          }
          break;
      }
      p+=2;
    }
    else
    { /* Ascii UCS */
#ifdef DEBUG_TXT
      log_info("UTF8 Ascii UCS 0x%02x\n", *p);
#endif
      *q = tolower(*p++);
    }
    if (*q=='\0' || filtre(*q)==0)
    {
#ifdef DEBUG_TXT
      log_warning("UTF2Lat reject 0x%x\n",*q);
#endif
      *q = '\0';
      return(p_org-buffer);
    }
    q++;
    i--;
  }
  *q = '\0';
  return(p-buffer);
}

static int data_check_ttd(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  unsigned int i;
  for(i=buffer_size/2; i<buffer_size; i++)
  {
    const unsigned char car=buffer[i];
    if((car>='A' && car<='F') || (car >='0' && car <='9') || car==' ' || car=='\n')
      continue;
    file_recovery->calculated_file_size=file_recovery->file_size + i - buffer_size/2;
    return 2;
  }
  file_recovery->calculated_file_size=file_recovery->file_size+(buffer_size/2);
  return 1;
}

static int header_check_fasttxt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  static const unsigned char spaces[16]={
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
  if(memcmp(buffer,header_cls,sizeof(header_cls))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="cls";
    return 1;
  }
  if(memcmp(buffer, header_html, sizeof(header_html))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
#ifdef DJGPP
    file_recovery_new->extension="htm";
#else
    file_recovery_new->extension="html";
#endif
    return 1;
  }
  if(memcmp(buffer,header_json,sizeof(header_json))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="json";
    return 1;
  }
  /* Incredimail has .imm extension but this extension isn't frequent */
  if(memcmp(buffer,header_imm,sizeof(header_imm))==0 ||
      memcmp(buffer,header_mail,sizeof(header_mail))==0 ||
      memcmp(buffer,header_ReturnPath,sizeof(header_ReturnPath))==0)
  {
    if(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
        file_recovery->file_stat->file_hint==&file_hint_fasttxt &&
        strcmp(file_recovery->extension,"mbox")==0)
      return 0;
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=NULL;
    file_recovery_new->extension="mbox";
    return 1;
  }
  if(memcmp(buffer,header_mail2,sizeof(header_mail2))==0)
  {
    unsigned int i;
    /* From someone@somewhere */
    for(i=sizeof(header_mail2); buffer[i]!=' ' && buffer[i]!='@' && i<200; i++);
    if(buffer[i]!='@')
      return 0;
    if(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
        file_recovery->file_stat->file_hint==&file_hint_fasttxt &&
        strcmp(file_recovery->extension,"mbox")==0)
      return 0;
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=NULL;
    file_recovery_new->extension="mbox";
    return 1;
  }
  if(memcmp(buffer,header_mdl,sizeof(header_mdl))==0)
  { /* Mathlab Model .mdl */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->extension="mdl";
    return 1;
  }
  if(memcmp(buffer,header_perlm,sizeof(header_perlm))==0 &&
      (buffer[sizeof(header_perlm)]==' ' || buffer[sizeof(header_perlm)]=='\t'))
  {
    char *buffer_lower=(char *)MALLOC(2048);
    const unsigned int buffer_size_test=(buffer_size < 2048-16 ? buffer_size : 2048-16);
    UTF2Lat((unsigned char*)buffer_lower, buffer, buffer_size_test);
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    if(strstr(buffer_lower, sign_java1)!=NULL ||
	strstr(buffer_lower, sign_java3)!=NULL ||
	strstr(buffer_lower, sign_java4)!=NULL)
    {
#ifdef DJGPP
      file_recovery_new->extension="jav";
#else
      file_recovery_new->extension="java";
#endif
    }
    else
      file_recovery_new->extension="pm";
    free(buffer_lower);
    return 1;
  }
  if(memcmp(buffer,header_rpp,sizeof(header_rpp))==0)
  {
    /* Reaper Project */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="rpp";
    return 1;
  }
  if(memcmp(buffer,header_rtf,sizeof(header_rtf))==0 &&
      ! (file_recovery!=NULL && file_recovery->file_stat!=NULL &&
	file_recovery->file_stat->file_hint==&file_hint_doc) &&
	  strstr(file_recovery->filename,".snt")!=NULL)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="rtf";
    return 1;
  }
  if(memcmp(buffer,header_reg,sizeof(header_reg))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="reg";
    return 1;
  }
  if(memcmp(buffer,header_sessionstore, sizeof(header_sessionstore))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
#ifdef DJGPP
    file_recovery_new->extension="js";
#else
    file_recovery_new->extension="sessionstore.js";
#endif
    return 1;
  }
  if(memcmp(buffer,header_sh,sizeof(header_sh))==0 ||
      memcmp(buffer,header_bash,sizeof(header_bash))==0 ||
      memcmp(buffer,header_ksh,sizeof(header_ksh))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="sh";
    return 1;
  }
  if(memcmp(buffer,header_slk,sizeof(header_slk))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="slk";
    return 1;
  }
  if(memcmp(buffer,header_seenezsst,sizeof(header_seenezsst))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="SeeNezSST";
    return 1;
  }
  if(memcmp(buffer,header_snz_unix,sizeof(header_snz_unix))==0 ||
      memcmp(buffer,header_snz_win,sizeof(header_snz_win))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="snz";
    return 1;
  }
  if(memcmp(buffer, header_mysql, sizeof(header_mysql))==0 ||
      memcmp(buffer, header_phpMyAdmin, sizeof(header_phpMyAdmin))==0 ||
      memcmp(buffer, header_postgreSQL, sizeof(header_postgreSQL))==0 ||
      memcmp(buffer, header_postgreSQL_win, sizeof(header_postgreSQL_win))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="sql";
    return 1;
  }
  if(memcmp(buffer, header_stl, sizeof(header_stl))==0 &&
      memcmp(buffer+0x40, spaces, sizeof(spaces))!=0)
  {
    /* StereoLithography - STL Ascii format
     * http://www.ennex.com/~fabbers/StL.asp	*/
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="stl";
    return 1;
  }
  if(memcmp(buffer, header_ers, sizeof(header_ers))==0)
  {
    /* ER Mapper Rasters (ERS) */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_ers;
    file_recovery_new->extension="ers";
    return 1;
  }
  if(memcmp(buffer, hdr_header, sizeof(hdr_header))==0)
  {
    /* ENVI */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="hdr";
    return 1;
  }
  if(memcmp(buffer, header_emka, sizeof(header_emka))==0)
  {
    /* EMKA IOX file */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
#ifdef DJGPP
    file_recovery_new->extension="emk";
#else
    file_recovery_new->extension="emka";
#endif
    return 1;
  }
  if(td_memmem(buffer, buffer_size, header_qgis, sizeof(header_qgis))!=NULL)
  {
    /* Quantum GIS (QGIS) is a user friendly Open Source Geographic Information System */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="qgs";
    return 1;
  }
  if(memcmp(buffer,header_stp,sizeof(header_stp))==0)
  {
    /* ISO 10303 is an ISO standard for the computer-interpretable
     * representation and exchange of industrial product data.
     * - Industrial automation systems and integration - Product data representation and exchange
     * - Standard for the Exchange of Product model data.
     * */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="stp";
    return 1;
  }
  if(memcmp(buffer, header_ttd, sizeof(header_ttd))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_ttd;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="ttd";
    return 1;
  }
  if(memcmp(buffer, header_url, sizeof(header_url))==0)
  {
    /* URL / Internet Shortcut */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="url";
    return 1;
  }
  if(memcmp(buffer,header_wpl,sizeof(header_wpl))==0)
  {
    /* Windows Play List*/
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="wpl";
    return 1;
  }
  if(memcmp(buffer,header_ram,sizeof(header_ram))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="ram";
    return 1;
  }
  if(memcmp(buffer, header_xml, sizeof(header_xml))==0 ||
      memcmp(buffer, header_xml_utf8, sizeof(header_xml_utf8))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    if(td_memmem(buffer, buffer_size, "Version_grisbi", 14)!=NULL)
    {
      /* Grisbi - Personal Finance Manager XML data */
      file_recovery_new->extension="gsb";
    }
    else if(td_memmem(buffer, buffer_size, "QBFSD", 5)!=NULL)
      file_recovery_new->extension="fst";
    else if(td_memmem(buffer, buffer_size, "<collection type=\"GC", 20)!=NULL)
    {
      /* GCstart, personal collections manager, http://www.gcstar.org/ */
      file_recovery_new->extension="gcs";
    }
    else if(td_memmem(buffer, buffer_size, "<html", 5)!=NULL)
    {
#ifdef DJGPP
      file_recovery_new->extension="htm";
#else
      file_recovery_new->extension="html";
#endif
    }
    else if(td_memmem(buffer, buffer_size, "<svg", 4)!=NULL)
    {
      /* Scalable Vector Graphics */
      file_recovery_new->extension="svg";
      file_recovery_new->file_check=&file_check_svg;
      return 1;
    }
    else if(td_memmem(buffer, buffer_size, "<!DOCTYPE plist ", 16)!=NULL)
    {
      /* Mac OS X property list */
#ifdef DJGPP
      file_recovery_new->extension="pli";
#else
      file_recovery_new->extension="plist";
#endif
    }
    else if(td_memmem(buffer, buffer_size, "<PremiereData Version=", 22)!=NULL)
    {
      file_recovery_new->extension="prproj";
    }
    else
      file_recovery_new->extension="xml";
    file_recovery_new->file_check=&file_check_xml;
    return 1;
  }
  if(buffer[0]=='0' && buffer[1]=='0' && memcmp(&buffer[4],header_dc,sizeof(header_dc))==0)
  { /*
       TSCe Survey Controller DC v10.0
     */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="dc";
    return 1;
  }
  if(memcmp(buffer,header_dif,sizeof(header_dif))==0)
  { /*
       Lotus Data Interchange Format
     */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="dif";
    return 1;
  }
  if(memcmp(buffer, header_ics, sizeof(header_ics))==0)
  {
    const char *date_asc;
    char *buffer2;
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="ics";
    /* DTSTART:19970714T133000            ;Local time
     * DTSTART:19970714T173000Z           ;UTC time
     * DTSTART;TZID=US-Eastern:19970714T133000    ;Local time and time
     */
    buffer2=(char *)MALLOC(buffer_size+1);
    buffer2[buffer_size]='\0';
    memcpy(buffer2, buffer, buffer_size);
    date_asc=strstr(buffer2, "DTSTART");
    if(date_asc!=NULL)
      date_asc=strchr(date_asc, ':');
    if(date_asc!=NULL && date_asc-buffer2<=buffer_size-14)
    {
      struct tm tm_time;
      memset(&tm_time, 0, sizeof(tm_time));
      date_asc++;
      tm_time.tm_sec=(date_asc[13]-'0')*10+(date_asc[14]-'0');      /* seconds 0-59 */
      tm_time.tm_min=(date_asc[11]-'0')*10+(date_asc[12]-'0');      /* minutes 0-59 */
      tm_time.tm_hour=(date_asc[9]-'0')*10+(date_asc[10]-'0');      /* hours   0-23*/
      tm_time.tm_mday=(date_asc[6]-'0')*10+(date_asc[7]-'0');	/* day of the month 1-31 */
      tm_time.tm_mon=(date_asc[4]-'0')*10+(date_asc[5]-'0')-1;	/* month 0-11 */
      tm_time.tm_year=(date_asc[0]-'0')*1000+(date_asc[1]-'0')*100+
	(date_asc[2]-'0')*10+(date_asc[3]-'0')-1900;        	/* year */
      tm_time.tm_isdst = -1;		/* unknown daylight saving time */
      file_recovery_new->time=mktime(&tm_time);
    }
    free(buffer2);
    return 1;
  }
  /* Java Application Descriptor
   * http://en.wikipedia.org/wiki/JAD_%28file_format%29 */
  if(memcmp(buffer, header_jad, sizeof(header_jad))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="jad";
    return 1;
  }
  /* LilyPond http://lilypond.org*/
  if(memcmp(buffer, header_ly, sizeof(header_ly))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="ly";
    return 1;
  }
  /* Lyx http://www.lyx.org */
  if(memcmp(buffer, header_lyx, sizeof(header_lyx))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="lyx";
    return 1;
  }
  /* Moving Picture Experts Group Audio Layer 3 Uniform Resource Locator */
  if(memcmp(buffer, header_m3u, sizeof(header_m3u))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="m3u";
    return 1;
  }
  /* http://www.mnemosyne-proj.org/
   * flash-card program to help you memorise question/answer pairs */
  if(memcmp(buffer, header_mnemosyne, sizeof(header_mnemosyne))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="mem";
    return 1;
  }
  /* Mozilla, firefox, thunderbird msf (Mail Summary File) */
  if(memcmp(buffer, header_msf, sizeof(header_msf))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="msf";
    return 1;
  }
  /* Opera Hotlist bookmark/contact list/notes */
  if(memcmp(buffer, header_adr, sizeof(header_adr))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="adr";
    return 1;
  }
  /* Cue sheet often begins by the music genre
   * or by the filename
   * http://wiki.hydrogenaudio.org/index.php?title=Cue_sheet */
  if(memcmp(buffer, header_cue1, sizeof(header_cue1))==0 ||
      memcmp(buffer, header_cue2, sizeof(header_cue2))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="cue";
    return 1;
  }
  /* Synchronized Multimedia Integration Language
   * http://en.wikipedia.org/wiki/Synchronized_Multimedia_Integration_Language */
  if(memcmp(buffer, header_smil, sizeof(header_smil))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_smil;
    file_recovery_new->extension="smil";
    return 1;
  }
  if(memcmp(buffer,header_xmp,sizeof(header_xmp))==0 &&
      !(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
	(file_recovery->file_stat->file_hint==&file_hint_pdf ||
	 file_recovery->file_stat->file_hint==&file_hint_tiff)))
  {
    /* Adobe's Extensible Metadata Platform */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="xmp";
    return 1;
  }
  if(memcmp(buffer, header_vbookmark, sizeof(header_vbookmark))==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="url";
    return 1;
  }
  return 0;
}

static int is_ini(const char *buffer)
{
  const char *src=buffer;
  if(*src!='[')
    return 0;
  src++;
  while(1)
  {
    if(*src==']')
    {
      if(src > buffer + 3)
	return 1;
      return 0;
    }
    if(!isalnum(*src) && *src!=' ')
      return 0;
    src++;
  }
}

#ifdef UTF16
static int header_check_le16_txt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  unsigned int i;
  for(i=0; i+1 < buffer_size; i+=2)
  {
    if(!( buffer[i+1]=='\0' && (isprint(buffer[i]) || buffer[i]=='\n' || buffer[i]=='\r' || buffer[i]==0xbb)))
    {
      if(i<40)
	return 0;
      reset_file_recovery(file_recovery_new);
      file_recovery_new->calculated_file_size=i;
      file_recovery_new->data_check=&data_check_size;
      file_recovery_new->file_check=&file_check_size;
      file_recovery_new->extension="utf16";
      return 1;
    }
  }
  reset_file_recovery(file_recovery_new);
  file_recovery_new->calculated_file_size=i;
  file_recovery_new->data_check=&data_check_size;
  file_recovery_new->file_check=&file_check_size;
  file_recovery_new->extension="utf16";
  return 1;
}
#endif

static int header_check_txt(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  static char *buffer_lower=NULL;
  static unsigned int buffer_lower_size=0;
  unsigned int l=0;
  const char sign_h[]			= "/*";
  const char sign_html[]		= "<html";
  const unsigned int buffer_size_test=(buffer_size < 2048 ? buffer_size : 2048);
  {
    unsigned int i;
    unsigned int tmp=0;
    for(i=0;i<10 && isdigit(buffer[i]);i++)
      tmp=tmp*10+buffer[i]-'0';
    if(buffer[i]==0x0a &&
      (memcmp(buffer+i+1, header_ReturnPath, sizeof(header_ReturnPath))==0 ||
       memcmp(buffer+i+1, header_ReceivedFrom, sizeof(header_ReceivedFrom))==0) &&
        !(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
          file_recovery->file_stat->file_hint==&file_hint_fasttxt &&
          strcmp(file_recovery->extension,"mbox")==0))
    {
      reset_file_recovery(file_recovery_new);
      file_recovery_new->calculated_file_size=tmp+i+1;
      file_recovery_new->data_check=NULL;
      file_recovery_new->file_check=&file_check_emlx;
      file_recovery_new->extension="emlx";
      return 1;
    }
  }
  if(buffer_lower_size<buffer_size_test+16)
  {
    free(buffer_lower);
    buffer_lower=NULL;
  }
  /* Don't malloc/free memory every time, small memory leak */
  if(buffer_lower==NULL)
  {
    buffer_lower_size=buffer_size_test+16;
    buffer_lower=(char *)MALLOC(buffer_lower_size);
  }
  l=UTF2Lat((unsigned char*)buffer_lower, buffer, buffer_size_test);
  if(l<10)
    return 0;
  /* strncasecmp */
  if(memcmp(buffer_lower, "@echo off", 9)==0 ||
      memcmp(buffer_lower, "rem ", 4)==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="bat";
    return 1;
  }
  if(memcmp(buffer_lower, "<%@ language=\"vbscript", 22)==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="asp";
    return 1;
  }
  if(memcmp(buffer_lower, "version 4.00\r\nbegin", 20)==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="vb";
    return 1;
  }
  if(memcmp(buffer_lower, "begin:vcard", 11)==0)
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    file_recovery_new->extension="vcf";
    return 1;
  }
  if(buffer[0]=='#' && buffer[1]=='!')
  {
    unsigned int ll=l-2;
    const unsigned char *haystack=(const unsigned char *)buffer_lower+2;
    const unsigned char *res;
    res=(const unsigned char *)memchr(haystack,'\n',ll);
    if(res!=NULL)
      ll=res-haystack;
    if(td_memmem(haystack, ll, "perl", 4) != NULL)
    {
      reset_file_recovery(file_recovery_new);
      file_recovery_new->data_check=&data_check_txt;
      file_recovery_new->file_check=&file_check_size;
      file_recovery_new->extension="pl";
      return 1;
    }
    if(td_memmem(haystack, ll, "python", 6) != NULL)
    {
      reset_file_recovery(file_recovery_new);
      file_recovery_new->data_check=&data_check_txt;
      file_recovery_new->file_check=&file_check_size;
      file_recovery_new->extension="py";
      return 1;
    }
    if(td_memmem(haystack, ll, "ruby", 4) != NULL)
    {
      reset_file_recovery(file_recovery_new);
      file_recovery_new->data_check=&data_check_txt;
      file_recovery_new->file_check=&file_check_size;
      file_recovery_new->extension="rb";
      return 1;
    }
  }
  if(safe_header_only!=0)
  {
    return 0;
  }
  /* Don't search text in the beginning of JPG or inside pdf */
  if(file_recovery!=NULL && file_recovery->file_stat!=NULL &&
      ((file_recovery->file_stat->file_hint==&file_hint_jpg && file_recovery->file_size < file_recovery->min_filesize) ||
       file_recovery->file_stat->file_hint==&file_hint_pdf))
  {
    return 0;
  }
  {
    const char *ext=NULL;
    /* ind=~0: random
     * ind=~1: constant	*/
    double ind=1;
    unsigned int nbrf=0;
    unsigned int is_csv=1;
    /* Detect Fortran */
    {
      char *str=buffer_lower;
      while((str=strstr(str, "\n      "))!=NULL)
      {
	nbrf++;
	str++;
      }
    }
    /* Detect csv */
    {
      unsigned int csv_per_line_current=0;
      unsigned int csv_per_line=0;
      unsigned int line_nbr=0;
      unsigned int i;
      for(i=0;i<l && is_csv>0;i++)
      {
	if(buffer_lower[i]==';')
	{
	  csv_per_line_current++;
	}
	else if(buffer_lower[i]=='\n')
	{
	  if(line_nbr==0)
	    csv_per_line=csv_per_line_current;
	  if(csv_per_line_current!=csv_per_line)
	    is_csv=0;
	  line_nbr++;
	  csv_per_line_current=0;
	}
      }
      if(csv_per_line<1 || line_nbr<10)
	is_csv=0;
    }
    /* if(l>1) */
    {
      unsigned int stats[256];
      unsigned int i;
      memset(&stats, 0, sizeof(stats));
      for(i=0;i<l;i++)
	stats[(unsigned char)buffer_lower[i]]++;
      ind=0;
      for(i=0;i<256;i++)
	if(stats[i]>0)
	  ind+=stats[i]*(stats[i]-1);
      ind=ind/l/(l-1);
    }
    /* Detect .ini */
    if(buffer[0]=='[' && is_ini(buffer_lower) && l>50)
      ext="ini";
    else if(strstr(buffer_lower, "<?php")!=NULL)
      ext="php";
    else if(strstr(buffer_lower, sign_java1)!=NULL ||
	strstr(buffer_lower, sign_java3)!=NULL ||
	strstr(buffer_lower, sign_java4)!=NULL)
    {
#ifdef DJGPP
      ext="jav";
#else
      ext="java";
#endif
    }
    else if(nbrf>10 && ind<0.9 && strstr(buffer_lower, "integer")!=NULL)
      ext="f";
    else if(is_csv>0)
      ext="csv";
    /* Detect LaTeX, C, PHP, JSP, ASP, HTML, C header */
    else if(strstr(buffer_lower, "\\begin{")!=NULL)
      ext="tex";
    else if(strstr(buffer_lower, "#include")!=NULL)
      ext="c";
    /* Windows Autorun */
    else if(strstr(buffer_lower, "[autorun]")!=NULL)
      ext="inf";
    else if(strstr(buffer_lower, "<%@")!=NULL)
      ext="jsp";
    else if(strstr(buffer_lower, "<%=")!=NULL)
      ext="jsp";
    else if(strstr(buffer_lower, "<% ")!=NULL)
      ext="asp";
    else if(strstr(buffer_lower, sign_html)!=NULL)
      ext="html";
    else if(strstr(buffer_lower, "\\score {")!=NULL)
      ext="ly"; 	/* LilyPond http://lilypond.org*/
    else if(strstr(buffer_lower, sign_h)!=NULL && l>50)
      ext="h";
    else if(l<100 || ind<0.03 || ind>0.90)
      ext=NULL;
    else if(memcmp(buffer_lower, "{\"", 2)==0)
      ext="json";
    else
      ext=file_hint_txt.extension;
    if(ext==NULL)
      return 0;
    if(strcmp(ext,"txt")==0 &&
	(strstr(buffer_lower,"<br>")!=NULL || strstr(buffer_lower,"<p>")!=NULL))
    {
      ext="html";
    }
    if(file_recovery!=NULL && file_recovery->file_stat!=NULL)
    {
      const unsigned char zip_header[4]  = { 'P', 'K', 0x03, 0x04};
      if(strcmp(ext,"html")==0 &&
	  file_recovery->file_stat->file_hint==&file_hint_txt &&
	  strstr(file_recovery->filename,"")!=NULL)
      {
	return 0;
      }

      /* Special case: doc, texte files
Unix: \n (0xA)
Dos: \r\n (0xD 0xA)
Doc: \r (0xD)
*/
      if(file_recovery->file_stat->file_hint==&file_hint_doc &&
	  strstr(file_recovery->filename,".doc")!=NULL)
      {
	unsigned int i;
	unsigned int txt_nl=0;
	if(ind>0.20)
	  return 0;
	for(i=0;i<l-1;i++)
	  if(buffer_lower[i]=='\r' && buffer_lower[i+1]!='\n')
	  {
	    return 0;
	  }
	for(i=0;i<l && i<512;i++)
	  if(buffer_lower[i]=='\n')
	    txt_nl++;
	if(txt_nl==0)
	  return 0;
	/* log_trace(">%s<\ndoc => %s\n",buffer_lower,ext); */
	reset_file_recovery(file_recovery_new);
	file_recovery_new->data_check=&data_check_txt;
	file_recovery_new->file_check=&file_check_size;
	file_recovery_new->extension=ext;
	return 1;
      }
      buffer_lower[511]='\0';
      /* Special case: two consecutive HTML files */
      if((strcmp(ext,"html")==0 &&
	    strstr(buffer_lower,sign_html)!=NULL &&
	    strstr(file_recovery->filename,".html")!=NULL) ||
	  /* Text should not be found in JPEG */
	  (file_recovery->file_stat->file_hint==&file_hint_jpg &&
	   td_memmem(buffer, buffer_size_test, "8BIM", 4)==NULL &&
	   td_memmem(buffer, buffer_size_test, "adobe", 5)==NULL &&
	   td_memmem(buffer, buffer_size_test, "exif:", 5)==NULL &&
	   td_memmem(buffer, buffer_size_test, "<rdf:", 5)==NULL &&
	   td_memmem(buffer, buffer_size_test, "<?xpacket", 9)==NULL &&
	   td_memmem(buffer, buffer_size_test, "<dict>", 6)==NULL) ||
	  /* Text should not be found in zip because of compression except .sh3d */
	  (file_recovery->file_stat->file_hint==&file_hint_zip &&
	   td_memmem(buffer, buffer_size_test, zip_header, 4)==NULL &&
	   strstr(file_recovery->filename, ".sh3d")==NULL))
      {
	reset_file_recovery(file_recovery_new);
	file_recovery_new->data_check=&data_check_txt;
	file_recovery_new->file_check=&file_check_size;
	file_recovery_new->extension=ext;
	return 1;
      }
      return 0;
    }
    /*    log_trace("ext=%s\n",ext); */
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension=ext;
    file_recovery_new->data_check=&data_check_txt;
    file_recovery_new->file_check=&file_check_size;
    return 1;
  }
}

static int data_check_txt(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  unsigned int i;
  char *buffer_lower=(char *)MALLOC(buffer_size+16);
  i=UTF2Lat((unsigned char*)buffer_lower, &buffer[buffer_size/2], buffer_size/2);
  if(i<buffer_size/2)
  {
    const char sign_html_end[]	= "</html>";
    const char *pos;
    pos=strstr(buffer_lower,sign_html_end);
    if(strstr(file_recovery->filename,".html")!=NULL && pos!=NULL && i<((pos-buffer_lower)+sizeof(sign_html_end))-1+10)
    {
      file_recovery->calculated_file_size+=(pos-buffer_lower)+sizeof(sign_html_end)-1;
    }
    else if(i>=10)
      file_recovery->calculated_file_size=file_recovery->file_size+i;
    free(buffer_lower);
    return 2;
  }
  free(buffer_lower);
  file_recovery->calculated_file_size=file_recovery->file_size+(buffer_size/2);
  return 1;
}

static void file_check_emlx(file_recovery_t *file_recovery)
{
  const unsigned char emlx_footer[9]= {'<', '/', 'p', 'l', 'i', 's', 't', '>', 0x0a};
  if(file_recovery->file_size < file_recovery->calculated_file_size)
    file_recovery->file_size=0;
  else
  {
    if(file_recovery->file_size > file_recovery->calculated_file_size+2048)
      file_recovery->file_size=file_recovery->calculated_file_size+2048;
    file_search_footer(file_recovery, emlx_footer, sizeof(emlx_footer), 0);
  }
}

static void file_check_smil(file_recovery_t *file_recovery)
{
  file_search_footer(file_recovery, "</smil>", 7, 0);
  file_allow_nl(file_recovery, NL_BARENL|NL_CRLF|NL_BARECR);
}

static void file_check_xml(file_recovery_t *file_recovery)
{
  file_search_footer(file_recovery, ">", 1, 0);
  file_allow_nl(file_recovery, NL_BARENL|NL_CRLF|NL_BARECR);
}

static void file_check_ers(file_recovery_t *file_recovery)
{
  file_search_footer(file_recovery, "DatasetHeader End", 17, 0);
  file_allow_nl(file_recovery, NL_BARENL|NL_CRLF|NL_BARECR);
}

static void file_check_svg(file_recovery_t *file_recovery)
{
  file_search_footer(file_recovery, "</svg>", 6, 0);
  file_allow_nl(file_recovery, NL_BARENL|NL_CRLF|NL_BARECR);
}

