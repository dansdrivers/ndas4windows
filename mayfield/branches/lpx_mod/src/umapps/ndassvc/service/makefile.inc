mofcomp: ndassvc.bmf

ndassvc.bmf: ndassvc.mof
        mofcomp -B:ndassvc.bmf ndassvc.mof
        wmimofck ndassvc.bmf
