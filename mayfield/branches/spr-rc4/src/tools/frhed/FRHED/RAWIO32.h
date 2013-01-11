TTON   "OK",IDOK,184,8,50,14
    PUSHBUTTON      "Cancel",IDCANCEL,184,26,50,14
    LTEXT           "Drive Information:",IDC_STATIC,4,4,168,12
    EDITTEXT        IDC_EDIT3,4,20,172,80,ES_MULTILINE | ES_AUTOHSCROLL | 
                    ES_READONLY
    LTEXT           "(prefix 0x for hexadecimal)",IDC_STATIC,114,118,82,8
    GROUPBOX        "NDAS",IDC_STATIC,7,143,230,129
    EDITTEXT        IDC_EDIT_NDAS_BITMAP,106,222,54,14,ES_AUTOHSCROLL
    PUSHBUTTON      "MBR",IDC_BTN_NDAS_MBR,18,157,86,10
    PUSHBUTTON      "1st Partition",IDC_BTN_NDAS_PARTITION_1,18,167,86,10
    PUSHBUTTON      "2nd Partition",IDC_BTN_NDAS_PARTITION_2,18,177,86,10,
                    WS_DISABLED
    PUSHBUTTON      "3rd Partition",IDC_BTN_NDAS_PARTITION_3,18,187,86,10,
                    WS_DISABLED
    PUSHBUTTON      "4th Partition",IDC_BTN_NDAS_PARTITION_4,18,197,86,10,
                    WS_DISABLED
    PUSHBUTTON      "Last Written Sector",IDC_BTN_NDAS_LAST_WRITTEN_SECTOR,
                    18,216,86,10
    PUSHBUTTON      "Corruption bitmap for",IDC_BTN_NDAS_BITMAP,18,226,86,10
    PUSHBUTTON      "NDAS_DIB_V2",IDC_BTN_NDAS_DIB_V2,18,236,86,10
    PUSHBUTTON      "NDAS_DIB_V1",IDC_BTN_NDAS_DIB_V1,18,246,86,10
    PUSHBUTTON      "Logical Disk Manager",IDC_BTN_NDAS_LDM,18,206,86,10,
                    WS_DISABLED
    PUSHBUTTON      "NDAS_RMD",IDC_BTN_NDAS_RMD,18,256,86,10
END

IDD_SHORTCUTS DIALOGEX 0, 0, 217, 166
STYLE DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Shortcuts"
FONT 8, "MS Shell Dlg", 0, 0, 0x1
BEGIN
    DEFPUSHBUTTON   "&Close",IDCANCEL,165,115,45,20
    PUSHBUTTON      "<< &Add...",IDC_ADD,115,65,45,20
    PUSHBUTTON      ">> &Delete",IDC_DELETE,115,90,45,20
    PUSHBUTTON      "&Move",IDC_MOVE,165,90,45,20,BS_CENTER | BS_VCENTER | 
                    BS_MULTILINE
    PUSHBUTTON      "&Update list",IDC_UPDATE,165,65,45,20,BS_CENTER | 
                    BS_VCENTER | BS_MULTILINE
    PUSHBUTTON      "&Find links to any frhed.exe, fix them and list them here.",
                    IDC_FIND_AND_FIX,5,140,205,20,BS_CENTER | BS_VCENTER | 
                    BS_MULTILINE
    PUSHBUTTON      "&Send To",IDC_SENDTO,115,35,40,15,BS_CENTER | 
                    BS_VCENTER | BS_MULTILINE
    PUSHBUTTON      "&Desktop"