_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Paste"
FONT 8, "MS Shell Dlg"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,210,11,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,210,28,50,14
    CONTROL         "Overwrite",IDC_RADIO1,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,14,18,46,10
    CONTROL         "Insert",IDC_RADIO2,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,14,30,33,10
    EDITTEXT        IDC_EDIT2,79,17,67,12,ES_AUTOHSCROLL
    CONTROL         "Paste coded binary values as text",IDC_CHECK1,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,80,36,122,10
    GROUPBOX        "Paste mode",IDC_STATIC,7,7,64,35
    LTEXT           "Paste data how many times:",IDC_STATIC,80,7,89,8
    EDITTEXT        IDC_EDIT3,170,49,35,12,ES_AUTOHSCROLL
    LTEXT           " Skip how many bytes between inserts/overwrites",
                    IDC_STATIC,7,49,156,12,SS_CENTERIMAGE
    LISTBOX         IDC_LIST,5,75,255,35,LBS_NOINTEGRALHEIGHT | WS_VSCROLL | 
                    WS_HSCROLL | WS_TABSTOP
    LTEXT           "Clipboard format to use:",IDC_STATIC,5,65,75,8
    LTEXT           " - CF_TEXT was used by previous versions of frhed\n - Those starting w CF_ are standard formats && others are registered/private ones\n - Some formats may have been synthesized from the original.",
                    IDC_STATIC,5,115,255,25
    PUSHBUTTON      "Refresh",IDC_REFRESH,210,45,50,15
END

IDD_TMPL_RESULT_DIALOG DIALOG  0, 0, 304, 175
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Template"
FONT 8, "MS Shell Dlg"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,99,154,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,154,154,50,14
    EDITTEXT        IDC_EDIT1,7,17,290,132,ES_MULTILINE | ES_AUTOVSCROLL | 
                    ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL
    LTEXT           "Result of template application:",IDC_STATIC,7,7,96,8
END

IDD_REPLACEDIALOG DIALOG  0, 0, 208, 226
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Replace"
FONT 8, "MS Shell Dlg"
BEGIN
    PUSHBUTTON      "Cancel",IDCANCEL,147,205,54,14
    LTEXT           "Find what:",IDC_STATIC,7,7,34,8
    EDITTEXT        IDC_TO_REPLACE_EDIT,7,16,194,62,ES_MULTILINE | 
                    ES_AUTOHSCROLL | WS_VSCROLL
    LTEXT           "Replace with:",IDC_STATIC,7,98,44,8
    EDITTEXT        IDC_REPLACEWITH_EDIT,7,108,194,58,ES_AUTOHSCROLL
    PUSHBUTTON      "Find next",IDC_FINDNEXT_BUTTON,7,81,39,14
    PUSHBUTTON      "Find previous",IDC_FINDPREVIOUS_BUTTON,51,81,54,14
    CONTROL         "Match case",IDC_MATCHCASE_CHECK,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,146,81,53,10
    PUSHBUTTON      "Replace",IDC_REPLACE_BUTTON,148,186,53,14
    PUSHBUTTON      "... following occurances",IDC_FOLLOCC_BUTTON,13,180,90,
                    14
    PUSHBUTTON      "... previous occurances",IDC_PREVOCC_BUTTON,14,196,89,
                    14
    GROUPBOX        "Replace all...",IDC_STATIC,7,1