#!/usr/bin/env python
#
# D-Bus C Bindings library - XML2cdbus proxy generator
#
# Python 2.7+ required
#
# Copyright 2011 S.I.S.E. S.A.
# Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
#
#
import getopt, sys
import xml.parsers.expat

class DBusSignatureException(Exception):
    def __str__(self):
        return "Bad signature"

class DBusSignature:
    def __init__(self, signature):
        self.signature = signature
        self.subs = []
        self.ParseSignature()

    def ParseSignature(self):
        if self.IsDict():
            raise DBusSignatureException
        if self.IsArray():
            self.subs.append(DBusSignature(self.signature[1:]))
        if self.IsStruct():
            self.subs = self.Split(self.signature[1:-1])
        self.signature = self.signature[0]

    def Match(self, signature, char1, char2):
        level = 1
        for i in range(0, len(signature)):
            if signature[i] == char1:
                level += 1
            if signature[i] == char2:
                level -= 1
                if level == 0:
                    return i + 1
        return -1

    def Split(self, signature):
        e = []
        while len(signature) != 0:
            if signature[0] == "(":
                pos1 = 0
                pos2 = self.Match(signature[1:], "(", ")") + 1
            elif signature[0] == "{":
                pos1 = 0
                pos2 = self.Match(signature[1:], "{", "}") + 1
            elif signature[0] == "a":
                pos1 = 0
                if signature[1] == "(":
                    pos2 = self.Match(signature[2:], "(", ")") + 2
                elif signature[1] == "{":
                    pos2 = self.Match(signature[2:], "{", "}") + 2
                else:
                    pos2 = 2 # Position after the array
            else:
                pos1 = 0
                pos2 = 1
            e.append(DBusSignature(signature[pos1:pos2]))
            signature = signature[pos2:]
        return e
    
    def IsContainer(self):
        return self.IsStruct()

    def IsStruct(self):
        if self.signature[0] == "(": return True
        
    def IsDict(self):
        if self.signature[0] == "{": return True

    def IsArray(self):
        if self.signature[0] == "a": return True

    def IsPrimitive(self):
        return not (self.IsArray() or self.IsContainer())

    def DBusType(self):
        if self.signature == "y": return "DBUS_TYPE_BYTE"
        elif self.signature == "b": return "DBUS_TYPE_BOOLEAN"
        elif self.signature == "n": return "DBUS_TYPE_INT16"
        elif self.signature == "q": return "DBUS_TYPE_UINT16"
        elif self.signature == "i": return "DBUS_TYPE_INT32"
        elif self.signature == "u": return "DBUS_TYPE_UINT32"
        elif self.signature == "x": return "DBUS_TYPE_INT64"
        elif self.signature == "t": return "DBUS_TYPE_UINT64"
        elif self.signature == "d": return "DBUS_TYPE_DOUBLE"
        elif self.signature == "s": return "DBUS_TYPE_STRING"
        elif self.signature == "o": return "DBUS_TYPE_OBJECT_PATH"
        elif self.signature == "g": return "DBUS_TYPE_SIGNATURE"
        elif self.signature == "h": return "DBUS_TYPE_UNIX_FD"
        elif self.signature == "v": return "DBUS_TYPE_VARIANT"
        if self.IsArray(): return "DBUS_TYPE_ARRAY"
        if self.IsStruct(): return "DBUS_TYPE_STRUCT"

    def DBusSignature(self):
        return self.signature + self.SubSignature()

    def SubSignature(self):
        signature = ""
        for sub in self.subs:
            if sub.IsPrimitive():
                signature += sub.signature
            if sub.IsArray():
                signature += sub.signature
                signature += sub.SubSignature()
            if sub.IsStruct():
                signature += "(" + sub.SubSignature() + ")"
        return signature

    def CType(self, varname):
        if self.signature == "y": return "char"
        elif self.signature == "b": return "int"
        elif self.signature == "n": return "short"
        elif self.signature == "q": return "unsigned short"
        elif self.signature == "i": return "long"
        elif self.signature == "u": return "unsigned long"
        elif self.signature == "x": return "long long"
        elif self.signature == "t": return "unsigned long long"
        elif self.signature == "d": return "double"
        elif self.signature == "s": return "char *"
        elif self.signature == "o": return "char *"
        elif self.signature == "g": return "char *"
        elif self.signature == "h": return "int"
        elif self.signature == "v": raise DBusSignatureException
        if self.IsArray():     return self.CArrayType(varname)
        if self.IsStruct():     return self.CContainerType(varname)

    def CArrayType(self, varname):
        return self.subs[0].CType(varname) + " *"

    def CContainerType(self, varname):
        return "struct " + varname + "_t"

    def CVarProto(self, direction, varname):
        if direction != "in":
            add = "*"
        else:
            add = ""
        if self.IsArray():     return self.CArrayVarProto(direction, varname)
        elif self.IsContainer(): return self.CContainerVarProto(direction, varname)
        else:
            return self.CType(varname) + add + " " + varname

    def CArrayVarProto(self, direction, varname):
        if direction != "in":
            add = "*"
        else:
            add = ""
        return self.CType(varname) + add + " " + varname + ", int" + add + " " + varname + "_len"

    def CContainerVarProto(self, direction, varname):
        return self.CType(varname) + " *" + " " + varname 

    def CVar(self, direction, varname):
        if self.IsContainer():
            return "&" + varname
        if direction == "in":
            string = varname
        else:
            string = "&" + varname
        if self.IsArray():
            if direction == "in":
                string += ", " + varname + "_len"
            else:
                string += ", &" + varname + "_len"
        return string

    def CDeclareVar(self, direction, varname):
        if self.IsArray():
            return self.CType(varname) + " " + varname + " = NULL; int" + " " + varname + "_len = 0"
        elif self.IsStruct():
            return self.CType(varname) + " " + varname + " = { 0 }"
        else:
            return self.CType(varname) + " " + varname + " = 0"

    def CFree(self, varname, member="", in_array=False):
        strings = []
        if member != "":
            if not in_array:
                varname += ".member_" + str(member)
            else:
                varname += "[" + str(member) + "]"
        if not self.IsPrimitive():
            for x in self.subs:
                subfree = x.CFree(varname, str(self.subs.index(x)))
                for y in subfree:
                    strings.append(y)
            if self.IsArray():
                strings.append("if (" + varname + "); free(" + varname + ")");
        return strings;

    def CPack(self, varname, member = "", iterator="iter", in_array=False):
        strings = []
        param = varname
        if member != "":
            if not in_array:
                varname += "_member_" + str(member)
                param = param + "->member_" + str(member)
            else:
                param = param + "[" + str(member) + "]"
        if self.IsPrimitive():
            if self.signature == "s":
                strings.append("dbus_message_iter_append_basic(&" + iterator + ", " + self.DBusType() + ", " + param + " ? &" + param + " : &null_string)")
            else:
                strings.append("dbus_message_iter_append_basic(&" + iterator + ", " + self.DBusType() + ", &" + param + ")")
        if self.IsArray():
            strings.append("cdbus_pack_" + varname + "_array(&" + iterator + ", " + param + ", " + param + "_len)")
        if self.IsStruct(): 
            if member != "":
                param = "&" + param;
            strings.append("cdbus_pack_" + varname + "_struct(&" + iterator + ", " + param + ")")
                
        return strings

    def CPackFunctions(self, varname):
        strings = []
        for sub in self.subs:
            if self.IsArray():
                functions = sub.CPackFunctions(varname)
            else:
                functions = sub.CPackFunctions(varname + "_member_" + str(self.subs.index(sub)))
            for func in functions:
                    strings.append(func)

        if self.IsStruct():
            strings.append(self.CPackStructFunction(varname))
        if self.IsArray():
            strings.append(self.CPackArrayFunction(varname))
        return strings

    def CPackStructFunction(self, varname):
        string = "int cdbus_pack_" + varname + "_struct(DBusMessageIter *iter, " + self.CVarProto("in", varname) + ")\n"
        string += "{\n"
        string += "\tDBusMessageIter sub_iter;\n"
        string += "\tdbus_message_iter_open_container(iter, " + self.DBusType() + ", NULL, &sub_iter);\n"
        for x in self.subs:
            string += "\t" + ";\n\t".join(y for y in x.CPack(varname, str(self.subs.index(x)), "sub_iter")) + ";\n"
        string += "\tdbus_message_iter_close_container(iter, &sub_iter);\n"
        string += "\treturn 0;\n"
        string += "}\n"
        return string
        

    def CPackArrayFunction(self, varname):
        string = "int cdbus_pack_" + varname + "_array(DBusMessageIter *iter, " + self.CVarProto("in", varname) + ")\n"
        string += "{\n"
        string += "\tDBusMessageIter sub_iter;\n"
        string += "\tint __i = 0;\n"
        string += "\tdbus_message_iter_open_container(iter, " + self.DBusType() + ", \"" + self.SubSignature() + "\", &sub_iter);\n"
        string += "\tfor(__i = 0 ; __i < " + varname + "_len ; __i++) {\n"
        for x in self.subs:
            string += "\t\t" + ";\n\t\t".join(y for y in x.CPack(varname, "__i", "sub_iter", True)) + ";\n"
        string += "\t}\n"
        string += "\tdbus_message_iter_close_container(iter, &sub_iter);\n"
        string += "\treturn 0;\n"
        string += "}\n"
        return string


    def CUnpack(self, varname, member = "", iterator="iter", in_array=False):
        strings = []
        param = varname
        if member != "":
            if not in_array:
                varname += "_member_" + str(member)
                param += "->member_" + str(member)
            else:
                param = "((*" + varname + ")" + "[" + str(member) + "])"
        strings.append("if (dbus_message_iter_get_arg_type(&" + iterator + ") == " + self.DBusType() + ") {");
        if self.IsPrimitive():
            strings.append("\tDBusBasicValue val;")
            strings.append("\tdbus_message_iter_get_basic(&" + iterator + ", &val);")
            if self.DBusType() == "DBUS_TYPE_BYTE":
                strings.append("\t" + param + " = val.byt;")
            elif self.DBusType() == "DBUS_TYPE_BOOLEAN":
                strings.append("\t" + param + " = val.bool_val;")
            elif self.DBusType() == "DBUS_TYPE_INT16":
                strings.append("\t" + param + " = val.i16;")
            elif self.DBusType() == "DBUS_TYPE_UINT16":
                strings.append("\t" + param + " = val.u16;")
            elif self.DBusType() == "DBUS_TYPE_INT32":
                strings.append("\t" + param + " = val.i32;")
            elif self.DBusType() == "DBUS_TYPE_UINT32":
                strings.append("\t" + param + " = val.u32;")
            elif self.DBusType() == "DBUS_TYPE_INT64":
                strings.append("\t" + param + " = val.i64;")
            elif self.DBusType() == "DBUS_TYPE_UINT64":
                strings.append("\t" + param + " = val.u64;")
            elif self.DBusType() == "DBUS_TYPE_DOUBLE":
                strings.append("\t" + param + " = val.dbl;")
            elif self.DBusType() == "DBUS_TYPE_STRING":
                strings.append("\t" + param + " = val.str;")
            elif self.DBusType() == "DBUS_TYPE_UNIX_FD":
                    strings.append("\t" + param + " = val.fd;")
        if self.IsArray():
            strings.append("\tcdbus_unpack_" + varname + "_array(&" + iterator + ", &" + param + ", &" + param + "_len);")
        if self.IsStruct():
            strings.append("\tcdbus_unpack_" + varname + "_struct(&" + iterator + ", &" + param + ");")
        strings.append("}");
        strings.append("dbus_message_iter_next(&" + iterator + ");")
        return strings

    def CUnpackFunctions(self, varname):
        strings = []
        for sub in self.subs:
            if self.IsArray():
                functions = sub.CUnpackFunctions(varname)
            else:
                functions = sub.CUnpackFunctions(varname + "_member_" + str(self.subs.index(sub)))
            for func in functions:
                    strings.append(func)

        if self.IsStruct():
            strings.append(self.CUnpackStructFunction(varname))
        if self.IsArray():
            strings.append(self.CUnpackArrayFunction(varname))
        return strings

    def CUnpackStructFunction(self, varname):
        string = "int cdbus_unpack_" + varname + "_struct(DBusMessageIter *iter, " + self.CVarProto("out", varname) + ")\n"
        string += "{\n"
        string += "\tDBusMessageIter sub_iter;\n"
        string += "\tdbus_message_iter_recurse(iter, &sub_iter);\n"
        for x in self.subs:
            string += "\t" + ";\n\t".join(y for y in x.CUnpack(varname, str(self.subs.index(x)), "sub_iter")) + ";\n"
        string += "\treturn 0;\n"
        string += "}\n"
        return string
        

    def CUnpackArrayFunction(self, varname):
        string = "int cdbus_unpack_" + varname + "_array(DBusMessageIter *iter, " + self.CVarProto("out", varname) + ")\n"
        string += "{\n"
        string += "\tDBusMessageIter sub_iter;\n"
        string += "\t*" + varname + "_len = 0;\n"
        string += "\tdbus_message_iter_recurse(iter, &sub_iter);\n"
        string += "\tdo {\n"
        string += "\t\t(*" + varname + "_len)++;\n"
        string += "\t} while(dbus_message_iter_next(&sub_iter));\n"
        string += "\t*" + varname + " = malloc(sizeof(**" + varname + ") * (*" + varname + "_len));\n"  
        string += "\t*" + varname + "_len = 0;\n"
        string += "\tdbus_message_iter_recurse(iter, &sub_iter);\n"
        string += "\twhile(1) {\n"
        string += "\t\tint dont_stop;\n"
        string += "\t\tdont_stop = dbus_message_iter_has_next(&sub_iter);\n"
        for x in self.subs:
            string += "\t\t" + "\n\t\t".join(y for y in x.CUnpack(varname, "(*" + varname + "_len)++", "sub_iter", True)) + "\n"
        string += "\t\tif (!dont_stop) break;\n"
        string += "\t}\n"
        string += "\treturn 0;\n"
        string += "}\n"
        return string

    def CTypeDef(self, varname, in_array=False):
        strings = []
        subtypes = [] 
        if self.IsArray():
            in_array = True
        for sub in self.subs:
            if in_array:
                subtypes = sub.CTypeDef(varname, sub.IsArray())
            else:
                subtypes = sub.CTypeDef(varname + "_member_" + str(self.subs.index(sub)), sub.IsArray())
            for types in subtypes:
                strings.append(types);
        
        if self.IsStruct():
             strings.append(self.CTypeDefStruct(varname))

        return strings
        

    def CTypeDefStruct(self, varname):
        string = "struct " + varname + "_t  {\n"
        for sub in self.subs:
            string += "\t" + sub.CType(varname + "_member_" + str(self.subs.index(sub))) + " member_" + str(self.subs.index(sub)) + ";\n"
            if sub.IsArray():
                string += "\tint member_" +  str(self.subs.index(sub)) + "_len;\n"
        string += "};\n"
        return string


    def __str__(self):
        if self.IsPrimitive():
            return self.signature
        if self.IsStruct():
            return "(" + "".join([str(x) for x in self.subs]) + ")"
        if self.IsArray():
            return self.signature + str(self.subs[0])


class DBusAttribute:
    def __init__(self, name, signature, direction = "in"):
        self.name = name
        self.direction = direction
        self.type = signature
        self.subAttributes = []
        
    def CVarProto(self):
        return self.type.CVarProto(self.direction, self.name)

    def CDeclareVar(self):
        return self.type.CDeclareVar(self.direction, self.name)

    def CVar(self):
        return self.type.CVar(self.direction, self.name)

    def CUnpack(self):
        return self.type.CUnpack(self.name)

    def CPack(self):
        return self.type.CPack(self.name)

    def CFree(self):
        return self.type.CFree(self.name)

class DBusMethod:
    def __init__(self, name, interface, attributes):
        self.name = name
        self.attributes = attributes
        self.interface = interface

    def CFunctionPointer(self):
        string = "int (*" + self.name + ")"
        attributes = [x.CVarProto() for x in self.attributes]
        string += "(DBusConnection *cnx, DBusMessage *msg, void *data"
        if attributes:
            string += ", "
        string += ', '.join(attributes) + ");"
        return string;

    def CFreeFunctionPointer(self):
        string = "void (*" + self.name + "_free)"
        attributes = [x.CVarProto() for x in self.attributes]
        string += "(DBusConnection *cnx, DBusMessage *msg, void *data"
        if attributes:
            string += ", "
        string += ', '.join(attributes) + ");"
        return string;    

    def CallCFunctionWithRet(self):
        string = "ret = -1 ; if (" + self.interface.COps() + '.' + self.name + ") ret = " 
        string += self.interface.COps() + '.' + self.name
        string += "(cnx, msg, data"
        if self.attributes:
            string += ", " 
        string += ', '.join(x.CVar() for x in self.attributes) + ")"
        return string

    def CallCFreeFunction(self):
        string = "if (" + self.interface.COps() + '.' + self.name + "_free) " 
        string += self.interface.COps() + '.' + self.name + "_free";
        string += "(cnx, msg, data"
        if self.attributes:
            string += ", " 
        string += ', '.join(x.CVar() for x in self.attributes) + ")"
        return string


    def CProxyName(self):
        string = self.CName() + "_proxy"
        return string

    def CProxyPrototype(self):
        string = "int "
        string += self.CProxyName()
        string += "(DBusConnection *cnx, DBusMessage *msg, void *data);\n"
        return string
    
    def CProxy(self):
        string = "int "
        string += self.CName() + "_proxy"
        string += "(DBusConnection *cnx, DBusMessage *msg, void *data)\n"
        string += "{\n"
        string += "\tint ret;\n"
        string += "\tDBusMessage * reply;\n"
        string += "\t" + ";\n\t".join(x.CDeclareVar() for x in self.attributes) + ";\n"

        # Unpack the variables 
        string += "\n\tDBusMessageIter iter;\n"
        string += "\tdbus_message_iter_init(msg, &iter);\n"
        for x in self.attributes:
            if x.direction == "in":
                string += "\t" + "\n\t".join(y for y in x.CUnpack()) + "\n"
        string += "\n"

        # Call the real functions
        string += "\t" + self.CallCFunctionWithRet() + ";\n"
        string += "\n"

        string += "\tif (ret < 0) {\n"

        # ret < 0 : Send a generic error message and exit
        string += "\t\treply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, \"method_call failed\");\n"
        string += "\t\tif (!reply) {\n"
        string += "\t\t\tret = -1;\n"
        string += "\t\t\tgoto free;\n"
        string += "\t\t}\n"

        string += "\t} else {\n"

        # else, pack the variables and send the reply message
        string += "\t\treply = dbus_message_new_method_return(msg);\n"
        string += "\t\tif (!reply) {\n"
        string += "\t\t\tret = -1;\n"
        string += "\t\t\tgoto free;\n"
        string += "\t\t}\n"
        string += "\t\tdbus_message_iter_init_append(reply, &iter);\n"
        for x in self.attributes:
            if x.direction == "out":
                string += "\t\t" + ";\n\t".join(y for y in x.CPack()) + ";\n"
        string += "\n"
        string += "\t\t" + self.CallCFreeFunction() + ";\n"
        string += "\t}\n"

        string += "\tdbus_connection_send(cnx, reply, NULL);\n"
        string += "\tdbus_message_unref(reply);\n"

        string += "\n"
        
        # Free the allocated variables
        string += "free:\n"
        for x in self.attributes:
            attrfree = x.CFree()
            if len(attrfree) != 0:
                string += "\t" + ";\n\t".join(y for y in attrfree) + ";\n" 
        string += "\treturn ret;\n"
        string += "}\n"
        return string

    def CName(self):
        return self.interface.CName() + '_' + self.name;

    def CTableHeader(self):
        return "extern struct cdbus_arg_entry_t " + self.CTableName() + "[];\n"

    def CTable(self):
        string = "struct cdbus_arg_entry_t " + self.CTableName() + "[] = {\n"
        for attr in self.attributes:
            string += "\t{\"" + attr.name + "\", CDBUS_DIRECTION_" + attr.direction.upper() + ", \"" + attr.type.DBusSignature() + "\"},\n"
        string += "\t{NULL, 0, NULL},\n"
        string += "};\n"
        return string

    def CTableName(self):
        return self.CName() + "_method_table"


class DBusSignal:
    def __init__(self, name, interface, obj, attributes):
        self.name = name
        self.attributes = attributes
        self.interface = interface
        self.object = obj

    def CName(self):
        string = self.interface.CName() + '_' 
        string += self.name
        return string
    
    def CPrototype(self):
        string = "int " 
        string += self.CName()
        attributes = [x.CVarProto() for x in self.attributes]
        string += "(DBusConnection *cnx, const char * object_path"
        if attributes:
            string += ", " 
        string += ', '.join(attributes) + ");\n"
        return string

    def CFunction(self):
        string = "int "
        string += self.CName()
        string += "(DBusConnection *cnx, const char * object_path"
        attributes = [x.CVarProto() for x in self.attributes]
        if attributes:
            string += ", "
        string += ', '.join(attributes)
        string += ")\n"
        string += "{\n"
        string += "\tDBusMessage * msg;\n"
        string += "\tDBusMessageIter iter;\n"
        string += "\n"

        string += "\tmsg = dbus_message_new_signal((object_path ? object_path :\"" + self.object.name + "\"), \"" + self.interface.name + "\", \"" + self.name + "\");\n"
        string += "\tif (!msg) {\n"
        string += "\t\treturn -1;\n"
        string += "\t}\n"

        # pack the variables and send the message
        string += "\tdbus_message_iter_init_append(msg, &iter);\n"
        for x in self.attributes:
            string += "\t" + ";\n\t".join(y for y in x.CPack()) + ";\n"
        string += "\n"

        string += "\tdbus_connection_send(cnx, msg, NULL);\n"
        string += "\tdbus_message_unref(msg);\n"

        string += "\n"
        
        string += "\treturn 0;\n"
        string += "}\n"
        return string

    def CTableHeader(self):
        return "extern struct cdbus_arg_entry_t " + self.CTableName() + "[];\n"

    def CTable(self):
        string = "struct cdbus_arg_entry_t " + self.CTableName() + "[] = {\n"
        for attr in self.attributes:
            string += "\t{\"" + attr.name + "\", CDBUS_DIRECTION_" + attr.direction.upper() + ", \"" + attr.type.DBusSignature() + "\"},\n"
        string += "\t{NULL, 0, NULL},\n"
        string += "};\n"
        return string

    def CTableName(self):
        return self.CName() + "_signal_table"

class DBusInterface:
    def __init__(self,name):
        self.name = name
        self.methods = {}
        self.signals = {}

    def AddMethod(self, method):
        self.methods[method.name] = method

    def AddSignal(self, signal):
        self.signals[signal.name] = signal

    def CName(self):
        return self.name.replace('.', '_')

    def CTableHeader(self):
        return "extern struct cdbus_message_entry_t " + self.CTableName() + "[];\n"

    def COps(self):
        return self.CName() + "_ops"

    def COpsPrototype(self):
        string = "extern struct "
        string += self.COps() + " {\n";
        for (name, method) in self.methods.items():
            string += "\t" + method.CFunctionPointer() + "\n"
            string += "\t" + method.CFreeFunctionPointer() + "\n"
        string += "} "+ self.COps() + ";\n"
        return string

    def CTable(self):
        string = "struct cdbus_message_entry_t " + self.CTableName() + "[] = {\n"
        for (name, method) in self.methods.items():
            string += "\t{\"" + name + "\", " + method.CProxyName() + ", " + method.CTableName() + "},\n"
        for (name, signal) in self.signals.items():
            string += "\t{\"" + name + "\", NULL, " + signal.CTableName() +"},\n"
        string += "\t{NULL, NULL},\n"
        string += "};\n"
        return string

    def CTableName(self):
        return self.CName() + "_interface_table"

class DBusObject:
    def __init__(self,name):
        self.interfaces = {}
        self.name = name

    def AddInterface(self,interface):
        self.interfaces[interface.name] = interface

    def Interface(self,name):
        return self.interfaces[name]

    def CName(self):
        return self.name.replace('/', '_')[1:]

    def CTableHeader(self):
        return "extern struct cdbus_interface_entry_t " + self.CName() + "_object_table[];\n"

    def CTable(self):
        string = "struct cdbus_interface_entry_t " + self.CName() + "_object_table[] = {\n"
        string += "\t" + ",\n\t".join("{\"" + key + "\", " + self.interfaces[key].CTableName() + "}"  for key in self.interfaces) + ",\n"
        string += "\t{NULL, NULL},\n"
        string += "};\n"
        return string

    def CHeaderFileName(self):
        return self.CName() + ".h"

    def CFileName(self):
        return self.CName() + ".c"

    def CHeader(self):
        string = "/* Generated with xml2cdbus.py, do not touch */\n"
        string += "#ifndef __" + self.CName().upper() + "_H\n"
        string += "#define __" + self.CName().upper() + "_H\n"
        string += "\n"
        string += "#include \"libcdbus.h\"\n"
        string += "\n"
        string += "/* Generated types */\n"
        string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                for attr in msg.attributes:
                    for typestring in attr.type.CTypeDef(attr.name):
                        string += typestring
            for msg in itf.signals.values():
                for attr in msg.attributes:
                    for typestring in attr.type.CTypeDef(attr.name):
                        string += typestring
        string += "\n"
        string += "/* Functions implemented by the library user */\n"
        string += "\n"
        for itf in self.interfaces.values():
            string += itf.COpsPrototype()
        string += "\n"
        string += "/* Public functions */\n"
        string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.signals.values():
                string += msg.CPrototype()
        string += "\n"
        string += "/* Private declarations, you should don't have to touch it */\n"
        string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                string += msg.CProxyPrototype()
            string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                string += msg.CTableHeader();
            for msg in itf.signals.values():
                string += msg.CTableHeader();
            string += "\n"
            string += itf.CTableHeader()
        string += self.CTableHeader()
        string += "#endif"
        return string

    def CFile(self):
        string = "/* Generated with xml2cdbus.py, do not touch */\n"
        string += "\n"
        string += "#include <stdlib.h>\n"
        string += "#include \"" + self.CHeaderFileName() + "\"\n"
        string += "\n"
        string += "static char * null_string __attribute__((unused)) = \"\";\n";
        string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                string += msg.CTable();
            for msg in itf.signals.values():
                string += msg.CTable();
            string += itf.CTable()
        string += "\n"
        string += self.CTable()
        string += "\n"
        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                for attr in msg.attributes:
                    if attr.direction == "in":
                        for funcstring in attr.type.CUnpackFunctions(attr.name):
                            string += funcstring + "\n"
                    if attr.direction == "out":
                        for funcstring in attr.type.CPackFunctions(attr.name):
                            string += funcstring + "\n"
            for msg in itf.signals.values():
                for attr in msg.attributes:
                    for funcstring in attr.type.CPackFunctions(attr.name):
                        string += funcstring + "\n"

        for itf in self.interfaces.values():
            for msg in itf.methods.values():
                string += msg.CProxy() + "\n"
            for msg in itf.signals.values():
                string += msg.CFunction() + "\n"

        return string

objects = {}

current_node = ""
current_interface = ""
current_method = ""
current_signal = ""
current_args = []

def args2attribute(method, args, force_direction_in=False):
    attributes = []
    for arg in args:
        if force_direction_in:
            attributes.append(DBusAttribute(method + '_' + arg['name'],
                                            DBusSignature(arg['type']),
                                            "in"))
        else:
            attributes.append(DBusAttribute(method + '_' + arg['name'],
                                            DBusSignature(arg['type']),
                                            arg['direction']))

    return attributes

def add_method():
    global current_interface, current_node, objects
    global current_method, current_args
    dbusinterface = objects[current_node].Interface(current_interface)
    attributes = args2attribute(dbusinterface.CName() + '_' + current_method, current_args)
    method = DBusMethod(current_method, dbusinterface, attributes)
    dbusinterface.AddMethod(method)

def add_signal():
    global current_interface, current_node, objects
    global current_signal, current_args
    dbusinterface = objects[current_node].Interface(current_interface)
    attributes = args2attribute(dbusinterface.CName() + '_'  + current_signal, current_args, True)
    signal = DBusSignal(current_signal, dbusinterface, objects[current_node], attributes)
    dbusinterface.AddSignal(signal)
    
def start_element_handler(name, attr):
    global current_interface, current_node, objects
    global current_method, current_signal, current_args
    if name == "node":
        current_node = attr['name']
        dbusobject = DBusObject(attr['name'])
        objects[attr['name']] = dbusobject
    if name == "interface":
        current_interface = attr['name']
        dbusinterface = DBusInterface(attr['name'])
        objects[current_node].AddInterface(dbusinterface)
    if name == "method":
        current_args = []
        current_method = attr['name']
    if name == "signal":
        current_args = []
        current_signal = attr['name']
    if name == "arg":
        current_args.append(attr)

def end_element_handler(name):
    global current_node
    if name == "method":
        add_method()
    if name == "signal":
        add_signal()
    if name == "node":
        f = open(objects[current_node].CHeaderFileName(), "w")
        f.write(objects[current_node].CHeader())
        f.close()
        f = open(objects[current_node].CFileName(), "w")
        f.write(objects[current_node].CFile())
        f.close()

def usage():
    print("Usage:\t" + sys.argv[0] + " <introspection_file.xml>")
    print("Usage:\t" + sys.argv[0] + " -h")
    print
    print("Options:")
    print("\t-h\tthis message")

def main():
    try: 
        opts, args = getopt.getopt(sys.argv[1:], "h", ["help"])
    except getopt.GetOptError as err:
        print(err)
        usage()
    for o,a in opts:
        if o == "-h":
            usage()
            exit(0)
    if len(args) == 0:
        usage()
        exit(1)
    
    introspectionFile = open(args[0], "rb")
    
    parser = xml.parsers.expat.ParserCreate()
    parser.StartElementHandler = start_element_handler
    parser.EndElementHandler = end_element_handler
    try:
        parser.ParseFile(introspectionFile)
    except xml.parsers.expat.ExpatError as err:
        print(err)
        exit(1)

if __name__ == "__main__":
    main()
