import gdb.printing
import gdb.types

class TypedParameterPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val['value']

class ParameterPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        dtype = self.val.dynamic_type
        if gdb.types.get_basic_type(dtype) != gdb.types.get_basic_type(self.val.type):
            return self.val.cast(dtype)
        else:
            return self.val.address

class ParameterPointerPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val.dereference()

class Printer(gdb.printing.RegexpCollectionPrettyPrinter):
    def __call__(self, val):
        t = gdb.types.get_basic_type(val.type)
        if t.code == gdb.TYPE_CODE_PTR:
            trg = gdb.types.get_basic_type(t.target()).tag
            if trg == 'clogs::detail::Parameter':
                return ParameterPointerPrinter(val)
        return super(Printer, self).__call__(val)

def build_pretty_printer():
    pp = Printer("clogs")
    pp.add_printer('TypedParameter', '^clogs::detail::TypedParameter<.*>$', TypedParameterPrinter)
    pp.add_printer('Parameter', '^clogs::detail::Parameter$', ParameterPrinter)
    return pp
