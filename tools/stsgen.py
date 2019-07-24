import os

class Catalog(object):
    _reserved = ('msg_code',)

    def __init__(self, mnemonic, code, errno_h, table_h):
        self._mnemonic = mnemonic
        self._code = code
        self._msg_code = 0
        self._errno_h = errno_h
        self._table_h = table_h
        self._defs = []

    @property
    def msg_code(self):
        v = self._msg_code
        self._msg_code += 1
        return v
        
    @msg_code.setter
    def msg_code(self, code):
        if code < self._msg_code:
            raise Exception('specified code:%d is less than %d' % (code, self._msg_code))
        self._msg_code = code

    def __setattr__(self, key, value):
        if key[0] == '_' or key in self._reserved:
            return super(Catalog, self).__setattr__(key, value)
        if isinstance(value, str):
            msg = value
        elif (isinstance(value, tuple) and
              len(value) == 2 and
              isinstance(value[0], str) and
              isinstance(value[1], int)):
            msg, self.msg_code = value
        else:
            raise Exception('assignment value must be a "message" or ("message", code)')
        self._defs.append((self.msg_code, key, msg))

    def __gen_errno_h(self, defs):
        with open(self._errno_h, 'w') as f:
            hn = os.path.splitext(os.path.basename(self._errno_h))[0]
            hnm = hn.upper() + '_H_LOADED__'
            f.write("#if !defined(%s)\n#define %s 1\n\n" % (hnm, hnm))
            for code, mnemonic, msg in defs:
                f.write("#define %s_E_%s ((0x%04x<<16)|0x%04x) // %s\n" %
                        (self._mnemonic.upper(), mnemonic,
                         self._code, code, msg))
            f.write("\n#endif /* %s.h */\n" % hn)

    def __gen_table_h(self, defs):
        with open(self._table_h, 'w') as f:
            table_varname = '_%s_table' % self._mnemonic.capitalize()
            catalog_varname = '_%s_catalog' % self._mnemonic.capitalize()
            f.write('#include <c7status.h>\n'
                    '#include "%s"\n\n'
                    'static c7_status_def_t %s[] = {\n' % (
                        os.path.basename(self._errno_h),
                        table_varname))
            for code, mnemonic, msg in defs:
                f.write('  { "%s", "%s", %s_E_%s },\n' %
                        (msg, mnemonic, self._mnemonic.upper(), mnemonic))
            f.write('};\n\n'
                    'static c7_status_catalog_t %s = {\n'
                    '  .cat = 0x%0x,\n'
                    '  .cat_id = "%s",\n'
                    '  .defv = %s,\n'
                    '  .defc = c7_numberof(%s),\n'
                    '};\n' % (catalog_varname,
                              self._code,
                              self._mnemonic.lower(),
                              table_varname,
                              table_varname))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type is not None:
            return
        self.__gen_errno_h(self._defs)
        self.__gen_table_h(self._defs)

