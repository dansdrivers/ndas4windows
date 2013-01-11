                   "Button",BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | 
                    WS_TABSTOP,165,20,105,10
    CONTROL         "Drag ""BinaryData""",IDC_DRAG_BIN_DATA,"Button",
                    BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | WS_TABSTOP,165,
                    55,75,10
    CONTROL         "Drag CF_TEXT as:",IDC_DRAG_CF_TEXT,"Button",
                    BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | WS_TABSTOP,165,
                    65,76,10
    CONTROL         "Hexdump",IDC_TEXT_HEXDUMP,"Button",BS_AUTORADIOBUTTON | 
                    BS_LEFT | BS_VCENTER,175,75,46,10
    CONTROL         "Special syntax (<bh:ff>...)",IDC_TEXT_SPECIAL,"Button",
                    BS_AUTORADIOBUTTON | BS_LEFT | BS_VCENTER,175,85,95,10
    CONTROL         "display (else digits)",IDC_TEXT_DISPLAY,"Button",
                    BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | WS_TABSTOP,225,
                    75,74,10
    CONTROL         "Drag ""Rich Text Format"" (as hexdump)",IDC_DRAG_RTF,
                    "Button",BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | 
                    WS_TABSTOP,165,95,138,10
    CONTROL         "Always give the opportunity to change between move and copy after a drop.",
                    IDC_ALWAYS_CHOOSE,"Button",BS_AUTOCHECKBOX | BS_LEFT | 
                    BS_VCENTER | BS_MULTILINE | WS_TABSTOP,10,50,135,20
    CONTROL         "Enable drag output",IDC_ENABLE_DRAG,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,10,20,76,10
    CONTROL         "Enable scroll-delaying for drag-drop",IDC_EN_SD_DD,
                    "Button",BS_AUTOCHECKBOX | WS_TABSTOP,10,30,126,10
    CONTROL         "Enable scroll-delaying for selecting",IDC_EN_SD_SEL,
                    "Button",BS_AUTOCHECKBOX | WS_TABSTOP,10,40,124,10
    CONTROL         "CF_HDROP opens the files dropped.",IDC_DROP_CF_HDROP,
                    "Button",BS_AUTOCHECKBOX | WS_TABSTOP,165,10,132,10
    CONTROL         "Drop CF_TEXT if present",IDC_DROP_CF_TEXT,"Button",
                    BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | WS_TABSTOP,165,
                    30,95,10
    GROUPBOX        "Input",IDC_STATIC,155,0,150,45
    GROUPBOX        "Output",IDC_STATIC,155,45,150,65
    GROUPBOX        "General",IDC_STATIC,5,0,145,75
    CONTROL         "Enable drop input",IDC_ENABLE_DROP,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,10,10,71,10
END


/////////////////////////////////////////////////////////////////////////////
//
// DESIGNINFO
//

#ifdef APSTUDIO_INVOKED
GUIDELINES DESIGNINFO 
BEGIN
    IDD_GOTODIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 237
        TOPMARGIN, 10
        BOTTOMMARGIN, 59
    END

    IDD_FINDDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 272
        TOPMARGIN, 7
        BOTTOMMARGIN, 131
    END

    IDD_ABOUTDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 205
        TOPMARGIN, 7
        BOTTOMMARGIN, 168
    END

    IDD_HEXDUMPDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 156
        TOPMARGIN, 7
        BOTTOMMARGIN, 154
    END

    IDD_DECIMALDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 197
        TOPMARGIN, 7
        BOTTOMMARGIN, 104
    END

    IDD_PASTEDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 284
        TOPMARGIN, 7
        BOTTOMMARGIN, 138
    END

    IDD_CUTDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 162
        TOPMARGIN, 7
        BOTTOMMARGIN, 152
    END

    IDD_COPYDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 151
        TOPMARGIN, 7
        BOTTOMMARGIN, 147
    END

    IDD_VIEWSETTINGSDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 221
        TOPMARGIN, 7
        BOTTOMMARGIN, 190
    END

    IDD_APPENDDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 185
        TOPMARGIN, 7
        BOTTOMMARGIN, 39
    END

    IDD_MANIPBITSDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 250
        TOPMARGIN, 7
        BOTTOMMARGIN, 77
    END

    IDD_CHARACTERSETDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 165
        TOPMARGIN, 7
        BOTTOMMARGIN, 74
    END

    IDD_CHOOSEDIFFDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 251
        TOPMARGIN, 7
        BOTTOMMARGIN, 224
    END

    IDD_BINARYMODEDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 176
        TOPMARGIN, 7
        BOTTOMMARGIN, 51
    END

    IDD_SELECT_BLOCK_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 200
        TOPMARGIN, 7
        BOTTOMMARGIN, 61
    END

    IDD_ADDBMK_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 186
        TOPMARGIN, 7
        BOTTOMMARGIN, 58
    END

    IDD_REMOVEBMK_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 202
        TOPMARGIN, 7
        BOTTOMMARGIN, 160
    END

    IDD_OPEN_PARTIAL_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 214
        TOPMARGIN, 7
        BOTTOMMARGIN, 128
    END

    IDD_FASTPASTE_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 260
        TOPMARGIN, 7
        BOTTOMMARGIN, 139
    END

    IDD_TMPL_RESULT_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 297
        TOPMARGIN, 7
        BOTTOMMARGIN, 168
    END

    IDD_REPLACEDIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 201
        TOPMARGIN, 7
        BOTTOMMARGIN, 219
    END

    IDD_FILL_WITH, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 375
        TOPMARGIN, 7
        BOTTOMMARGIN, 96
    END

    IDD_CHANGEINST, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 240
        TOPMARGIN, 7
        BOTTOMMARGIN, 69
    END

    IDD_ENCODE_DECODE_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 229
        TOPMARGIN, 7
        BOTTOMMARGIN, 162
    END

    IDD_OPEN_DRIVE_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 229
        TOPMARGIN, 7
        BOTTOMMARGIN, 126
    END

    IDD_GOTO_TRACK_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 237
        VERTGUIDE, 18
        TOPMARGIN, 10
        BOTTOMMARGIN, 272
    END

    IDD_MOVE_COPY, DIALOG
    BEGIN
        BOTTOMMARGIN, 140
    END

    IDD_DRAG_DROP, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 195
        TOPMARGIN, 7
        BOTTOMMARGIN, 114
    END

    IDD_DRAG_DROP_OPTIONS, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 305
        TOPMARGIN, 7
        BOTTOMMARGIN, 109
    END
END
#endif    // APSTUDIO_INVOKED


/////////////////////////////////////////////////////////////////////////////
//
// Version
//

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,0
 PRODUCTVERSION 1,0,0,1
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040704b0"
        BEGIN
            VALUE "Comments", "Homepage: http://www.kibria.de, http://zip.to/pabs3"
            VALUE "CompanyName", "(c) Raihan Kibria 2000"
            VALUE "FileDescription", "frhed - free hex editor 1.1.0"
            VALUE "FileVersion", "1, 1, 0"
            VALUE "InternalName", "frhed"
            VALUE "LegalCopyright", "Copyright (c) 2000"
            VALUE "OriginalFilename", "frhed.exe"
            VALUE "ProductName", "frhed"
            VALUE "ProductVersion", "1, 1, 0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x407, 1200
    END
END


/////////////////////////////////////////////////////////////////////////////
//
// Icon
//

// Icon with lowest ID value placed first to ensure application icon
// remains consistent on all systems.
IDI_ICON1               ICON                    "icon1.ico"

/////////////////////////////////////////////////////////////////////////////
//
// Bitmap
//

IDB_TOOLBAR             BITMAP                  "Toolbar.bmp"

/////////////////////////////////////////////////////////////////////////////
//
// String Table
//

STRINGTABLE 
BEGIN
    IDM_OPEN                "Open"
    IDM_NEW                 "New"
END

STRINGTABLE 
BEGIN
    IDM_SAVE                "Save"
    IDM_FIND                "Find"
    IDM_EDIT_COPY           "Copy"
    IDM_EDIT_PASTE          "Paste"
    IDM_HELP_TOPICS         "Help"
    IDM_EDIT_CUT            "Cut"
END

STRINGTABLE 
BEGIN
    IDM_REPLACE             "Replace"
END

STRINGTABLE 
BEGIN
    ID_DISK_GOTONEXTTRACK   "Goto next sector"
    ID_DISK_GOTOPREVIOUSTRACK "Goto previous sector"
    ID_DISK_GOTOFIRSTTRACK  "Goto first sector"
    ID_DISK_GOTOLASTTRACK   "Goto last sector"
END

#endif    // German (Germany) resources
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// English (U.S.) resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_ENU)
#ifdef _WIN32
LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
#pragma code_page(1252)
#endif //_WIN32

/////////////////////////////////////////////////////////////////////////////
//
// Dialog
//

IDD_OPEN_ADDRESS_DIALOG DIALOG  0, 0, 219, 143
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Open NDAS"
FONT 8, "MS Shell Dlg"
BEGIN
    COMBOBOX        IDC_COMBO_ID,23,7,189,258,CBS_DROPDOWN | WS_VSCROLL | 
                    WS_TABSTOP
    CONTROL         "&READONLY",IDC_CHECK_READONLY,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,7,88,55,10
    EDITTEXT        IDC_EDIT_UNIT,55,102,40,14,ES_AUTOHSCROLL | ES_NUMBER
    DEFPUSHBUTTON   "OK",IDOK,7,122,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,65,122,50,14
    PUSHBUTTON      "&Delete",ID_BTN_DELETE_NDAS_ADDRESS,162,22,50,14
    LTEXT           "Form :\n000BD0000102 or\n00:0B:D0:00:01:02 or\n01234ABCDE567890FGHIJ or\n01234ABCDE567890FGHIJ01234 or\n01234-ABCDE5-67890-FGHIJ or\n01234-ABCDE5-67890-FGHIJ-01234",
                    IDC_STATIC,7,23,143,60
    LTEXT           "Unit Number",IDC_STATIC,7,106,40,8
    LTEXT           "Author : AINGOPPA",IDC_STATIC,148,128,64,8
    LTEXT           "ID :",IDC_STATIC,7,8,12,8
END


/////////////////////////////////////////////////////////////////////////////
//
// DESIGNINFO
//

#ifdef APSTUDIO_INVOKED
GUIDELINES DESIGNINFO 
BEGIN
    IDD_OPEN_ADDRESS_DIALOG, DIALOG
    BEGIN
        LEFTMARGIN, 7
        RIGHTMARGIN, 212
        VERTGUIDE, 212
        TOPMARGIN, 7
        BOTTOMMARGIN, 136
    END
END
#endif    // APSTUDIO_INVOKED

#endif    // English (U.S.) resources
/////////////////////////////////////////////////////////////////////////////



#ifndef APSTUDIO_INVOKED
/////////////////////////////////////////////////////////////////////////////
//
// Generated from the TEXTINCLUDE 3 resource.
//


/////////////////////////////////////////////////////////////////////////////
#endif    // not APSTUDIO_INVOKED

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           //=========================================================
// File: simparr.cpp

#include "precomp.h"
#include "simparr.h"

//-------------------------------------------------------------------
template