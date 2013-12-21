import gdb.printing
import clogs_printing
gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    clogs_printing.build_pretty_printer())

