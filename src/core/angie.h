
/*
 * Copyright (C) 2022 Web Server LLC
 */


#ifndef _ANGIE_H_INCLUDED_
#define _ANGIE_H_INCLUDED_

#define ANGIE_NAME         "FreeAngie"


#define ANGIE_VERSION      "1.6.0"
#define ANGIE_VER          "Angie/" ANGIE_VERSION

#define ANGIE_VER          ANGIE_NAME "/" ANGIE_VERSION

#ifdef NGX_BUILD
#define ANGIE_VER_BUILD    ANGIE_VER " (" NGX_BUILD ")"
#else
#define ANGIE_VER_BUILD    ANGIE_VER
#endif

#define ANGIE_VAR          "FreeANGIE"
#define NGX_OLDPID_EXT     ".oldbin"

#define ngx_angie_sign     ('A' + 'n' + 'g' + 'i' + 'e')


#endif /* _ANGIE_H_INCLUDED_ */
