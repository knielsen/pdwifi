import re

class Processor:
    rexp= re.compile("Kristian")

    def process(self, s):
        res= re.sub(self.rexp, "Hugo", s)
        return res
