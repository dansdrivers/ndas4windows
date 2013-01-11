mofcomp: ndashlpsvc.bmf

ndashlpsvc.bmf: ndashlpsvc.mof
        mofcomp -B:ndashlpsvc.bmf ndashlpsvc.mof
        wmimofck ndashlpsvc.bmf

#deploy:
#	echo Deploying...
#	if exist deploy.cmd deploy.cmd