import os

# generate a tree of HTML files to be used in the helpfile.

class HTMLHelpSubject:
    "a help subject consists of a topic, and a filename"
    def __init__( self, topic, filename, keywords = [] ):
        self.topic, self.filename = topic, filename
        self.keywords = keywords + [topic]
        self.subjects = []

    def AsHHCString(self,tab):
        return '''%s<LI> <OBJECT type="text/sitemap">
%s    <param name="Name" value="%s">
%s    <param name="Local" value="%s">
%s</OBJECT>''' % (tab, tab, self.topic, tab, self.filename, tab )

    def AsHHKString(self):
        result = ""
        for keyword in self.keywords:
            result += '''    <LI> <OBJECT type="text/sitemap">
        <param name="Name" value="%s">
        <param name="Local" value="%s">
    </OBJECT>
''' % (keyword, self.filename)
        return result

class HTMLHelpProject:
    """a help project is a collection of topics and options that will be compiled
    to a HTMLHelp file (.chm)"""
    def __init__(self,subject,filename="default.htm",title=None):

        if title is None:
            title = subject

        # this is the root node for the help project. add subprojects here.
        if filename is not None:
            self.root = HTMLHelpSubject(subject, filename)
        else:
            self.root = HTMLHelpSubject(None, None)
        self.language = "0x407 German (Germany)"
        self.index_file = subject + ".hhk"
        self.default_topic = filename
        self.toc_file = subject + ".hhc"
        self.compiled_file = subject + ".chm"
        self.project_file = subject + ".hhp"
        self.title = title
        self.index = {}
        self.use_toplevel_project = 0
        self.homepage = ""

    def Generate(self,directory):
        # make sure, target directory exists
        try:
            os.makedirs(directory)
        except:
            pass

        # generate files
        self.GenerateHHP( os.path.join( directory, self.project_file ) )
        self.GenerateHHK( os.path.join( directory, self.index_file ) )
        self.GenerateTOC( os.path.join( directory, self.toc_file ) )

    def GenerateHHP(self,filename):
        "Helper function: Generate HtmlHelp Project."
        assert(self.root is not None)

        file = open(filename,"w")
        print >>file, """[OPTIONS]
Compatibility=1.1 or later
Compiled file=%s
Contents file=%s
Default topic=%s
Default Window=main
Display compile progress=No
Full-text search=Yes
Index file=%s
Language=%s
Title=%s

[FILES]
""" % (self.compiled_file, self.toc_file, self.default_topic, self.index_file, self.language, self.title)
        print >>file, """[WINDOWS]
main="%s","%s","%s","index.htm","%s",,,,,0x2520,,0x304e,,,,,,,,0
""" % (self.title, self.toc_file, self.index_file, self.homepage )

        print >>file, "[INFOTYPES]\n\n"

    def GenerateTOCRecursive(self,file,subject,indent):
        tab = "\t" * indent
        for item in subject.subjects:
            print >>file, item.AsHHCString(tab)
            if item.subjects:
                print >>file, tab + "<UL>"
                self.GenerateTOCRecursive(file, item, indent+1)
                print >>file, tab + "</UL>"

    def GenerateTOC(self,filename):
        "Helper function: Generate Table-of-contents."
        assert(self.root is not None)

        file = open(filename,"w")
        print >>file, """<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML>
<HEAD>
<meta name="GENERATOR" content="python">
</HEAD><BODY>
<OBJECT type="text/site properties">
    <param name="ImageType" value="Folder">
</OBJECT>
"""
        if self.use_toplevel_project:
            print >>file, "<UL>"
            print >>file, self.root.AsHHCString("\t")
            self.GenerateTOCRecursive(file,self.root,2)
            print >>file, "</UL>"
        else:
            print >>file, "<UL>"
            self.GenerateTOCRecursive(file,self.root,1)
            print >>file, "</UL>"
        print >>file, "</BODY></HTML>"
        file.close()

    def GenerateHHKRecursive(self,file,subject):
        for item in subject.subjects:
            print >>file, item.AsHHKString()
            if item.subjects:
                self.GenerateHHKRecursive(file, item)

    def GenerateHHK(self,filename):
        "Helper function: Generate Index file."
        assert(self.root is not None)

        file = open(filename,"w")
        print >>file, """<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML>
<HEAD>
<meta name="GENERATOR" content="Python">
<!-- Sitemap 1.0 -->
</HEAD><BODY>
<UL>"""
        if self.root.topic:
            print >>file, self.root.AsHHKString()
        self.GenerateHHKRecursive(file,self.root)
        print >>file, "</UL>\n</BODY></HTML>"
        file.close()

