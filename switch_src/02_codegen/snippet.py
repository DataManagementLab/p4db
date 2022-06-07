import re
import textwrap
import types


RE_FORMAT = re.compile(r'\${(?P<name>\w+)(?:=(?P<default>.+?)(?=}))?}')


class Snippet:
    def __init__(self, **kwargs):
        code = self.__doc__.rstrip()
        self.__code = textwrap.dedent(code)
        for k, v in kwargs.items():
            setattr(self, k, v)

    def __str__(self):
        start = 0
        end = len(self.__code)
        parts = []

        for m in RE_FORMAT.finditer(self.__code):
            groups = m.groupdict()
            name = groups['name']
            end = m.start()
            parts.append(self.__code[start:end])

            if groups['default']:
                default = eval(groups['default'])
                attr = getattr(self, name, default)
            else:
                attr = getattr(self, name)

            if isinstance(attr, types.GeneratorType):
                variable = '\n'.join(map(str, attr))
            else:
                variable = str(attr)

            parts.append(variable)
            start = m.end()
            end = len(self.__code)

        if start != len(self.__code):
            parts.append(self.__code[start:end])

        return ''.join(parts)
