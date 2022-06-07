from .snippet import Snippet
from .indent import indent


class C(Snippet):
    def __init__(self, code, **kwargs):
        self.__doc__ = code
        super().__init__(**kwargs)


def C_for(code, range, **kwargs):
    return '\n'.join(map(str, (C(code, i=i, **kwargs) for i in range)))



def write(output, code):
    code = indent(code)
    output.write(code)
    return code.count('\n') + 1
