n to the current version.\n'Read' reads data from the selected version and the selected instance of that version.\n'Delete' removes the checked instances of the checked versions.",
                    IDC_STATIC,45,215,270,25
    CONTROL         "Data",IDC_INSTDATA,"SysListView32",LVS_REPORT | 
                    LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,215,15,
                    155,80,WS_EX_CLIENTEDGE
    LTEXT           "Instance data:",IDC_STATIC,215,5,46,8
    PUSHBUTTON      "Copy",IDC_COPY,5,195,75,15
    CONTROL         "Links",IDC_LINKS,"SysListView32",LVS_REPORT | 
                    LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | 
                    WS_TABSTOP,5,100,110,90,WS_EX_CLIENTEDGE
    CONTROL         "MRU Files",IDC_MRU,"SysListView32",LVS_REPORT | 
                    LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | 
                    WS_TABSTOP,240,100,130,90,WS_EX_CLIENTEDGE
    CONTROL         "frhed Display",IDC_DISPLAY,"frhed display",WS_SYSMENU | 
                    WS_GROUP | WS_TABSTOP,120,100,115,90,WS_EX_DLGMODALFRAME
END

IDD_MOVE_COPY DIALOG  0, 0, 163, 156
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Move/Copy bytes"
FONT 8, "MS Shell Dlg"
BEGIN
    DEFPUSHBUTTON   "OK",IDOK,5,136,70,14
    PUSHBUTTON      "Cancel",IDCANCEL,90,136,65,