import re

class Processor:
    rexp= re.compile("Kristian")

    def process(self, s):
        res= re.sub(self.rexp, "Christian", s)
        return res
