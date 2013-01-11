
    CONTROL         "Byte (8 bit)",IDC_RADIO1,"Button",BS_AUTORADIOBUTTON,11,
                    88,49,10
    CONTROL         "Word (16 bit)",IDC_RADIO2,"Button",BS_AUTORADIOBUTTON,
                    63,88,57,10
    CONTROL         "Longword (32 bit)",IDC_RADIO3,"Button",
                    BS_AUTORADIOBUTTON,122,88,71,10
    DEFPUSHBUTTON   "OK",IDOK,147,39,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,147,56,50,14
END

IDD_PASTEDIALOG DIALOGEX 0, 0, 291, 145
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Paste with dialogue"
FONT 8, "MS Shell Dlg", 0, 0, 0x1
BEGIN
    EDITTEXT        IDC_EDIT1,7,18,277,65,ES_MULTILINE | WS_VSCROLL,
                    WS_EX_TRANSPARENT
    CONTROL         "Overwrite",IDC_RADIO1,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,14,98,46,10
    CONTROL         "Insert",IDC_RADIO2,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,14,109,33,10
    EDITTEXT        IDC_EDIT2,79,96,67,12,ES_AUTOHSCROLL
    CONTROL         "Paste coded binary values as text",IDC_CHECK1,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,80,115,122,10
    DEFPUSHBUTTON   "OK",IDOK,234,96,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,234,112,50,14
    LTEXT           "Clipboard content (text):",IDC_STATIC,7,7,76,8
    GROUPBOX        "Paste mode",IDC_STATIC,7,86,64,40
    LTEXT           "Paste data how many times:",IDC_STATIC,80,86,89,8
    EDITTEXT        IDC_EDIT3,168,126,60,12,ES_AUTOHSCROLL
    LTEXT           " Skip how many bytes between inserts/overwrites",
                    IDC_STATIC,7,129,160,8
END

IDD_CUTDIALOG DIALOG  0, 0, 169, 159
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Cut"
FONT 8, "MS Shell Dlg"
BEGIN
    EDITTEXT        IDC_EDIT1,7,16,136,12,ES_AUTOHSCROLL
    CONTROL         "Cut up to and including offset:",IDC_RADIO1,"Button",
                    BS_AUTORADIOBUTTON | WS_TABSTOP,15,48,110,10
    EDITTEXT        IDC_EDIT2,16,59,136,12,ES_AUTOHSCROLL
    CONTROL         "Number of bytes to cut:",IDC_RADIO2,"Button",
                    BS_AUTORADIOBUTTON | WS_TABSTOP,15,80,89,10
    EDITTEXT        IDC_EDIT3,15,91,136,12,ES_AUTOHSCROLL
    CONTROL         "Cut to clipboard",IDC_CHECK1,"Button",BS_AUTOCHECKBOX | 
                    WS_TABSTOP,7,120,65,10
    DEFPUSHBUTTON   "OK",IDOK,112,122,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,112,138,50,14
    LTEXT           "Start cutting at offset: (prefix offsets with x for hex)",
                    IDC_STATIC,7,7,157,8
    GROUPBOX        "Cut how many bytes",IDC_STATIC,7,33,155,81
END

IDD_COPYDIALOG DIALOG  0, 0, 158, 154
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Copy"
FONT 8, "MS Shell Dlg"
BEGIN
    EDITTEXT        IDC_EDIT1,7,29,130,13,ES_AUTOHSCROLL
    CONTROL         "Copy up to and including offset:",IDC_RADIO1,"Button",
                    BS_AUTORADIOBUTTON | WS_TABSTOP,14,63,115,10
    EDITTEXT        IDC_EDIT2,14,73,130,13,ES_AUTOHSCROLL
    CONTROL         "Number of bytes to copy:",IDC_RADIO2,"Button",
                    BS_AUTORADIOBUTTON | WS_TABSTOP,13,93,95,10
    EDITTEXT        IDC_EDIT3,14,103,130,13,ES_AUTOHSCROLL
    DEFPUSHBUTTON   "OK",IDOK,25,131,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,82,131,50,14
    LTEXT           "Start copying at offset:",IDC_STATIC,7,20,72,8
    GROUPBOX        "Copy how many bytes",IDC_STATIC,7,51,144,73
    LTEXT           "Prefix offsets with x for hex.",IDC_STATIC,7,7,86,8
END

IDD_VIEWSETTINGSDIALOG DIALOG  0, 0, 229, 197
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "View Settings"
FONT 8, "MS Shell Dlg"
BEGIN
    EDITTEXT        IDC_EDIT1,7,16,68,12,ES_AUTOHSCROLL
    EDITTEXT        IDC_EDIT2,7,61,68,12,ES_AUTOHSCROLL
    CONTROL         "unsigned",IDC_RADIO1,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,14,86,45,10
    CONTROL         "signed",IDC_RADIO2,"Button",BS_AUTORADIOBUTTON | 
                    WS_TABSTOP,65,86,37,10
    CONTROL         "Automatically adjust number of bytes per line (uncheck this if you want frhed to use your own choice for bytes per line)",
                    IDC_CHECK1,"Button",BS_AUTOCHECKBOX | BS_MULTILINE | 
                    WS_TABSTOP,7,31,214,19
    CONTROL         "Set read-only mode on opening files",IDC_CHECK5,"Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP,7,106,128,10
    EDITTEXT        IDC_EDIT3,7,132,214,13,ES_AUTOHSCROLL
    DEFPUSHBUTTON   "OK",IDOK,171,80,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,171,97,50,14
    LTEXT           "Number of bytes to display per line:",IDC_STATIC,7,7,
                    110,8
    LTEXT           "Display length of offset in how many characters:",
                    IDC_STATIC,7,51,151,8
    GROUPBOX        "Display values at caret position as:",IDC_STATIC,7,76,
                    128,25
    LTEXT           "Path and filename of the text editor to call:",
                    IDC_STATIC,7,122,133,8
    EDITTEXT        IDC_EDIT4,7,161,214,13,ES_AUTOHSCROLL
    LTEXT           "Path and filename of the Internet browser:",IDC_STATIC,
                    7,150,132,8
    CONTROL         "Adjust offset len to that of the max offset",IDC_CHECK2,
                    "Button",BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP,80,
                    61,140,12
END

IDD_APPENDDIALOG DIALOG  0, 0, 192, 46
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Append"
FONT 8, "MS Shell Dlg"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,135,7,50,14
 