#!/usr/bin/env python3

from lxml import etree
from os import path, remove
import subprocess

FILE_JOIN = ' - '


def main():
    """Create and clean plantuml file."""
    tree = etree.parse('assistant.xml')
    root = tree.getroot()  # <assistants>
    for category in root.getchildren():
        category_name = category.get('name')
        for item in category.getchildren():
            item_name = item.get('name') or 'NONE'
            print('%s - %s' % (category_name, item_name))
            filename = category_name + FILE_JOIN + item_name + '.puml'
            filepath = path.join(path.curdir, 'icons', filename)
            cdata = item.text
            cdata = cdata.replace('\n' + (len(cdata[1:]) - len(cdata[1:].lstrip(' '))) * ' ', '\n')  # NOQA: E501
            with open(filepath, 'w') as source_fd:
                source_fd.write('@startuml'+cdata+'@enduml')

            if 'PNG' in category_name:
                subprocess.run(['java', '-jar',
                                '/usr/local/share/plantuml/plantuml.jar',
                                '-tpng',
                                path.join(path.curdir, 'icons', '*.puml')])
            else:
                subprocess.run(['java', '-jar',
                                '/usr/local/share/plantuml/plantuml.jar',
                                '-tsvg',
                                path.join(path.curdir, 'icons', '*.puml')])

            remove(filepath)


if __name__ == "__main__":
    main()

# vim:set et sts=4 ts=4 tw=80:
