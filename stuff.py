import re

rexp= re.compile("Kristian")

def process(s):
    res= re.sub(rexp, "Christian", s)
    return res
