import string, codecs, sys

try:
    import win32com.client
except:
    pass

TARGET_DIRECTORY = "???"
PROJECT_NAME = "???"
PROJECT_SOURCE = "???"
GENERATE_DOC = 0
ADD_LINKS_TO_INDEX = 1
PAGE_FORMAT_LANDSCAPE = 0
USE_TOPLEVEL_PROJECT = 1
DEFAULT_TOPIC = "file0.htm"
USE_DOC_TEMPLATE = "pydocgen.dot"
DOC_PAGE_BREAKS = 0
HOMEPAGE = ""
HTML_FILE_START = """<html>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link rel="stylesheet" type="text/css" href="formate.css">
<body>"""
HTML_FILE_STOP = """</body></html>"""

if len(sys.argv) == 2:

    print "Reading settings from", sys.argv[1]
    exec(open(sys.argv[1]).read())

from htmlhelpgen import *

# needed for converting Unicode->Ansi (in local system codepage)
DecodeUnicodeString = lambda x: codecs.latin_1_encode(x)[0]

if GENERATE_DOC:
    word = win32com.client.Dispatch("Word.Application")
    doc = word.Documents.Add(USE_DOC_TEMPLATE)
    if PAGE_FORMAT_LANDSCAPE:
        doc.PageSetup.Orientation = win32com.client.constants.wdOrientLandscape
    else:
        doc.PageSetup.Orientation = win32com.client.constants.wdOrientPortrait
    


LINKS = {}
REVERSE_LINK_MAP = {}

class Table:
    def __init__(self):
        self.rows = []

    def Generate(self):
        global doc, doc_index, doc_index_list

        doc_index = doc.Range().End-1
        doc_index_list.append(doc_index)

        table = self.rows

        # eine Tabelle ans Textende einfügen 
        doc_table = doc.Tables.Add(doc.Range(doc_index, doc_index),len(table),len(table[0]))

        # Zeile 0 ist die Überschriftszeile
        doc_table.Rows(1).HeadingFormat = ~0

        # Alle Zellen mit Text füllen
        for sri in xrange(len(table)):
            row = table[sri]
            for sci in xrange(len(row)):
                cell = row[sci]
                r = doc_table.Cell(sri+1,sci+1).Range
                r.Style = cell.style
                r.InsertAfter(cell.text)
                if cell.texture:
                    doc_table.Cell(sri+1,sci+1).Shading.Texture = cell.texture

        doc_table.Columns.AutoFit()

        doc_index = doc.Range().End-1
        doc_index_list.append(doc_index)        

class TC:
    def __init__(self,text,style="TAB"):
        self.text = text
        self.style = style.upper()
        self.texture = None
        if self.style == "TABHEAD":
            self.texture = win32com.client.constants.wdTexture10Percent

doc_index_list = []
doc_index = 0
last_body = ""
output = None    

def isnumeric(token):
    try:
        int(token)
        return 1
    except:
        return 0

current_table = None
is_recording_preformatted = 0
is_recording_htag = 0

# this function gets called for all tokens the parser finds
def Pass1_OnToken(token):
    global FILENAMES, project, output, last_body, file_index, is_recording_preformatted
    global doc_index_list, doc_index, GENERATE_DOC, current_table, DOC_PAGE_BREAKS, is_recording_htag

    token_string = token
    token = map(string.lower,token.split())
    
    # end of header token -------------------------------------------------------------
    if token[0][:2] == '/h' and isnumeric(token[0][2:]):
        
        project.last_subject.topic += last_body
        project.last_subject.keywords[0] += last_body
        is_recording_htag = 0

        if GENERATE_DOC:
            doc.Content.InsertAfter("\n")
            doc_index += 1
            doc.Range(doc_index_list[-1],doc_index).Style = getattr(win32com.client.constants,"wdStyleHeading%d" % project.last_subject.level)
            doc_index_list.append(doc_index)       

    elif token[0] == "a" and token[1][:5] == "href=":
        link_to = token[1]
        x = link_to.find('"')
        if x >= 0:
            link_to = link_to[x+1:]
        x = link_to.rfind('"')
        if x >= 0:
            link_to = link_to[:x]        
        if output:
            try:
                output.write('<a href="%s%s">' % (LINKS[link_to],link_to))
            except:
                print "Warning, link '%s' invalid or external." % link_to
                output.write("<"+token_string+">")
            return

    # table handling BEGIN ----------------------------------
    elif token[0] == "table" and GENERATE_DOC:
        current_table = Table()

    elif token[0] == "tr" and GENERATE_DOC and current_table:
        if len(current_table.rows) > 1:
            for item in current_table.rows[-1][:-1]:
                item.style = "TABC"

        current_table.rows.append([])

    elif token[0] == "/td" and GENERATE_DOC and current_table:
        style = "TAB"
        if len(current_table.rows) == 1:
            style = "TABHEAD"
        
        current_table.rows[-1].append(TC(last_body,style))

    elif token[0] == "/table" and GENERATE_DOC and current_table:
        if len(current_table.rows) > 1:
            for item in current_table.rows[-1][:-1]:
                item.style = "TABC"
        
        current_table.Generate()
        current_table = None

    # table handling END ----------------------------------    
    elif token[0] == "pre":
        is_recording_preformatted = 1

    # preprocessor define
    elif token[0] == '/pre':
        is_recording_preformatted = 0
        if GENERATE_DOC:
            doc.Content.InsertAfter("\n")
            doc_index += 1
            doc.Range(doc_index_list[-1],doc_index).Style = "sourcecode"
            doc_index_list.append(doc_index)          

    # bullet-style list entry
    elif token[0] == '/li':
        if GENERATE_DOC:
            doc.Content.InsertAfter("\n")
            doc_index += 1
            doc.Range(doc_index_list[-1],doc_index).Style = win32com.client.constants.wdStyleListBullet
            doc_index_list.append(doc_index)        

    # end of paragraph
    elif token[0] == '/p':
        if GENERATE_DOC and not current_table:
            doc.Content.InsertAfter("\n")
            doc_index += 1
            doc_index_list.append(doc_index)

    # font-style BOLD
    elif token[0] == '/b':
        if GENERATE_DOC:
            doc.Content.InsertAfter(" ")
            doc.Range(doc_index_list[-1],doc_index).Bold = 1
            doc_index += 1
            doc_index_list = doc_index_list[:-1]

    # font-style ITALIC
    elif token[0] == '/i':
        if GENERATE_DOC:
            doc.Content.InsertAfter(" ")
            doc.Range(doc_index_list[-1],doc_index).Italic = 1
            doc_index += 1
            doc_index_list = doc_index_list[:-1]

    elif is_recording_htag and (token[0] in ("dfn","/dfn")):
        project.last_subject.topic += last_body
        project.last_subject.keywords[0] += last_body

    elif token[0] == "img":
        filename = token[1]
        x = filename.find('"')
        if x > 0:
            filename = filename[x+1:]
        x = filename.rfind('"')
        if x > 0:
            filename = filename[:x]
        filename = os.path.join(TARGET_DIRECTORY,filename)
        if GENERATE_DOC:
            doc.Content.InsertAfter("\n")
            picture = doc.InlineShapes.AddPicture( filename, 1, 0,Range=doc.Range(doc_index,doc_index) )
            doc.Content.InsertAfter("\n")
            doc_index = doc.Range().End-1
            doc_index_list.append(doc_index)

    # start of header token
    elif token[0][:1] == 'h' and isnumeric(token[0][1:]):

        token = token[0]
        is_recording_htag = 1

        # close old topic        
        if output:
            output.write(HTML_FILE_STOP)
            output.close()

        # generate new topic      
        if RENAME_DEFAULT_FOR_WEB and file_index == 0:
            filename = "index.htm"
        else:
            filename = "file%d.htm" % file_index
            
        output = open( os.path.join(TARGET_DIRECTORY,filename), "w" )
        file_index += 1
        output.write(HTML_FILE_START)

        subject = HTMLHelpSubject(filename, filename)
        project.last_subject = subject
        project.last_subject.topic = ""
        project.last_subject.keywords[0] = ""

        if ADD_LINKS_TO_INDEX:
            try:
                for keyword in REVERSE_LINK_MAP[filename]:
                    subject.keywords.append(keyword[1:])
            except:
                pass

        subject.level = topic_level = int(token[1:])
        
        if topic_level == 1:
            # this is a root project
            project.root.subjects.append(subject)
            project.levels[1] = subject
        else:
            parent_topic_level = topic_level-1
            while (parent_topic_level > 0) and not project.levels.has_key(parent_topic_level):
                parent_topic_level = parent_topic_level-1

            if parent_topic_level == 0:
                project.root.subjects.append(subject)
                project.levels[1] = subject
            else:
                project.levels[parent_topic_level].subjects.append(subject)

            project.levels[topic_level] = subject

        if GENERATE_DOC and DOC_PAGE_BREAKS:
            doc.Range(doc_index,doc_index).InsertBreak(win32com.client.constants.wdPageBreak)
            doc_index = doc.Range().End-1
            doc_index_list.append(doc_index)

    if output:
        output.write("<"+token_string+">")

# this function gets called for all text bodies the parser finds. the default is to write the token to a file
def Pass1_OnBody(body):
    global output, last_body, doc, doc_index, current_table, is_recording_preformatted
    
    last_body = body
    if output:
        output.write(body)
    if GENERATE_DOC and not current_table:
        if not is_recording_preformatted:
            body = body.replace("\n"," ")
        body = body.replace("&lt;","<")
        body = body.replace("&gt;",">").strip()
        if body:
            doc_index_list.append(doc_index)
            doc.Content.InsertAfter(body)
            doc_index += len(body)

# this function gets called for all tokens the parser finds
def Pass0_OnToken(token):
    global last_filename, LINKS, file_index

    if token[:7] == "a name=":
        x = token.find('"')
        if x >= 0:
            token = token[x+1:]
        x = token.rfind('"')
        if x >= 0:
            token = token[:x]
        if LINKS.has_key(token):
            print "Warning, link '%s' is used more than once." % token
            
        LINKS[token] = last_filename

    elif token[:1] == 'h' and isnumeric(token[1:]):

        # generate new topic      
        if RENAME_DEFAULT_FOR_WEB and file_index == 0:
            last_filename = "index.htm"
        else:
            last_filename = "file%d.htm" % file_index        
        file_index += 1

# this function gets called for all text bodies the parser finds. the default is to write the token to a file
def Pass0_OnBody(body):
    pass

def ParseData(onbody,ontoken):
    global output, output_index
    output, output_index = None, 0

    current_token, current_body = None, None

    for i in xrange(len(data)):
        c = data[i]
        if c == '<':
            if current_body is not None:
                onbody(data[current_body:i])
                current_body = None
            current_token = i+1
        elif c == '>':
            if current_token is not None:
                ontoken(data[current_token:i])
            current_token = None
        elif current_body is None and current_token is None:
            current_body = i

    if current_body:
        onbody(data[current_body:i])

project = HTMLHelpProject( PROJECT_NAME, DEFAULT_TOPIC )
project.homepage = HOMEPAGE
project.levels = {}
project.last_subject = None
project.use_toplevel_project = USE_TOPLEVEL_PROJECT
print "USE_TOPLEVEL_PROJECT=",USE_TOPLEVEL_PROJECT
data = open(PROJECT_SOURCE,"r").read()

file_index = 0
ParseData(Pass0_OnBody,Pass0_OnToken)

if ADD_LINKS_TO_INDEX:
    for key in LINKS:
        file = LINKS[key]
        try:
            REVERSE_LINK_MAP[file].append(key)
        except:
            REVERSE_LINK_MAP[file] = [key]

file_index = 0
ParseData(Pass1_OnBody,Pass1_OnToken)

if output:
    output.write(HTML_FILE_STOP)
    output.close()

project.Generate(TARGET_DIRECTORY)

