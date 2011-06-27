#!/usr/bin/env python2

import dbus

bus = dbus.SessionBus()
proxy = bus.get_object("fr.sise.test", "/fr/sise/test")
interface = dbus.Interface(proxy, dbus_interface="fr.sise.test")

print(interface.Hello("toto"))
