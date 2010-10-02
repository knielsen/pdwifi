import re

class Processor:
    rexp= re.compile("Kristian")
    html_re= re.compile("^text/html")

    def init(self, ct):
        self.content_type= ct
        if re.search(self.html_re, ct):
            self.mangle= True
        else:
            self.mangle= False

    def process(self, s):
        if self.mangle:
            res= re.sub(self.rexp, "Christian", s)
        else:
            res= s
        return res
