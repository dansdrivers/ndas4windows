function externlink(file,changeloc) {
	var X, sl, ra, link, oldproto, newproto, path;
	ra = /:/;
	oldproto = location.href.substring(0,location.href.search(ra));
	if(oldproto=="mk"){
		//"mk:@MSITStore:"
		X=14;
		sl="\\";
		newproto="file:///";
	} else if(oldproto=="ms-its"){
		X=7;
		sl="\\";
		newproto="file:///";
	} else if(oldproto=="http"||oldproto=="file"){
		X=7;
		sl="/";
		newproto=oldproto+"://";
	}
	link = newproto + location.href.substring(X, location.href.lastIndexOf(sl) + 1) + file;
	if(changeloc)
		location.href = link;
	return link;
}