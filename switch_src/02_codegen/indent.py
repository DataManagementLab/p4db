import re
from collections.abc import Iterable


def indent(snippet):
    if isinstance(snippet, Iterable):
        lines = list(itertools.chain.from_iterable(x.__str__().split('\n') for x in snippet))
    else:
        lines = snippet.__str__().split('\n')

    indentation = 0
    in_params = False
    for i, line in enumerate(lines):
        line = line.strip()
        nc_line = line.split('//', 1)[0].strip()

        if re.search(r'{\s*}$', nc_line):
            pass
        elif re.search(r'^}', nc_line):
            indentation -= 1
        elif re.search(r'^#(?:else|endif)', nc_line):
            indentation -= 1
        elif re.search(r';$', nc_line) and in_params:
            in_params = False
            indentation -= 2

        newline = f'{"    "*indentation}{line}'
        #print(f'{indentation:2} {newline}')

        if re.search(r'\($', nc_line):
            in_params = True
            indentation += 2
        elif re.search(r'\)\s*{$', nc_line) and in_params:
            in_params = False
            indentation -= 1
        elif re.search(r'{$', nc_line):
            indentation += 1
        elif re.search(r'^#(?:if|else)', nc_line):
            indentation += 1
        

        lines[i] = newline

    lines.append('')
    return '\n'.join(lines)