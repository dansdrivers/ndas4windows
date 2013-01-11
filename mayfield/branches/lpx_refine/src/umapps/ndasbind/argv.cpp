#include "stdafx.h"

VOID pParseCmdlineA (
    LPSTR cmdstart,
    LPSTR*argv,
    LPSTR lpstr,
    INT *numargs,
    INT *numbytes)
{
    LPSTR p;
    WCHAR c;
    INT inquote;                    /* 1 = inside quotes */
    INT copychar;                   /* 1 = copy char to *args */
    WORD numslash;                  /* num of backslashes seen */

    *numbytes = 0;
    *numargs = 1;                   /* the program name at least */

    /* first scan the program name, copy it, and count the bytes */
    p = cmdstart;
    if (argv)
        *argv++ = lpstr;

    /* A quoted program name is handled here. The handling is much
       simpler than for other arguments. Basically, whatever lies
       between the leading double-quote and next one, or a terminal null
       character is simply accepted. Fancier handling is not required
       because the program name must be a legal NTFS/HPFS file name.
       Note that the double-quote characters are not copied, nor do they
       contribute to numbytes. */
    if (*p == '\"')
    {
        /* scan from just past the first double-quote through the next
           double-quote, or up to a null, whichever comes first */
        while ((*(++p) != '\"') && (*p != '\0'))
        {
            *numbytes += sizeof(CHAR);
            if (lpstr)
                *lpstr++ = *p;
        }
        /* append the terminating null */
        *numbytes += sizeof(CHAR);
        if (lpstr)
            *lpstr++ = '\0';

        /* if we stopped on a double-quote (usual case), skip over it */
        if (*p == '\"')
            p++;
    }
    else
    {
        /* Not a quoted program name */
        do {
            *numbytes += sizeof(CHAR);
            if (lpstr)
                *lpstr++ = *p;

            c = (CHAR) *p++;

        } while (c > ' ');

        if (c == '\0')
        {
            p--;
        }
        else
        {
            if (lpstr)
                *(lpstr - 1) = '\0';
        }
    }

    inquote = 0;

    /* loop on each argument */
    for ( ; ; )
    {
        if (*p)
        {
            while (*p == ' ' || *p == '\t')
                ++p;
        }

        if (*p == '\0')
            break;                  /* end of args */

        /* scan an argument */
        if (argv)
            *argv++ = lpstr;         /* store ptr to arg */
        ++*numargs;

        /* loop through scanning one argument */
        for ( ; ; )
        {
            copychar = 1;
            /* Rules: 2N backslashes + " ==> N backslashes and begin/end quote
                      2N+1 backslashes + " ==> N backslashes + literal "
                      N backslashes ==> N backslashes */
            numslash = 0;
            while (*p == '\\')
            {
                /* count number of backslashes for use below */
                ++p;
                ++numslash;
            }
            if (*p == '\"')
            {
                /* if 2N backslashes before, start/end quote, otherwise
                   copy literally */
                if (numslash % 2 == 0)
                {
                    if (inquote)
                        if (p[1] == '\"')
                            p++;    /* Double quote inside quoted string */
                        else        /* skip first quote char and copy second */
                            copychar = 0;
                    else
                        copychar = 0;       /* don't copy quote */

                    inquote = !inquote;
                }
                numslash /= 2;          /* divide numslash by two */
            }

            /* copy slashes */
            while (numslash--)
            {
                if (lpstr)
                    *lpstr++ = '\\';
                *numbytes += sizeof(CHAR);
            }

            /* if at end of arg, break loop */
            if (*p == ('\0') || (!inquote && (*p == (' ') || *p == ('\t'))))
                break;

            /* copy character into argument */
            if (copychar)
            {
                if (lpstr)
                        *lpstr++ = *p;
                *numbytes += sizeof(CHAR);
            }
            ++p;
        }

        /* null-terminate the argument */

        if (lpstr)
            *lpstr++ = ('\0');         /* terminate string */
        *numbytes += sizeof(CHAR);
    }

}


LPSTR* CommandLineToArgvA(LPCSTR lpCmdLine, int* pNumArgs)
{
    LPSTR *argv;
    LPSTR cmdstart;                 /* start of command line to parse */
    INT   numbytes;
    CHAR pgmname[MAX_PATH];

    if (pNumArgs == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* Get the program name pointer from Win32 Base */

    GetModuleFileNameA(NULL, pgmname, sizeof(pgmname) / sizeof(CHAR));

    /* if there's no command line at all (won't happen from cmd.exe, but
       possibly another program), then we use pgmname as the command line
       to parse, so that argv[0] is initialized to the program name */
    cmdstart = (*lpCmdLine == TEXT('\0')) ? pgmname : (LPSTR) lpCmdLine;

    /* first find out how much space is needed to store args */
    pParseCmdlineA(cmdstart, NULL, NULL, pNumArgs, &numbytes);

    /* allocate space for argv[] vector and strings */
    argv = (LPSTR*) LocalAlloc(LMEM_ZEROINIT,
                                   (*pNumArgs+1) * sizeof(LPSTR) + numbytes);
    if (!argv) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return (NULL);
    }

    /* store args and argv ptrs in just allocated block */
    pParseCmdlineA(cmdstart, argv,
                   (LPSTR) (((LPBYTE)argv) + *pNumArgs * sizeof(LPSTR)),
                   pNumArgs, &numbytes);

    return (argv);
}
