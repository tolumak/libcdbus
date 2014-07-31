#!/usr/bin/env python

# D-Bus C Bindings library: test program
#
# Copyright 2011-2014 S.I.S.E. S.A.
# Author: Michel Lafon-Puyo <michel.lafonpuyo@gmail.com>
#
# This file is part of libcdbus
#
# libcdbus is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Foobar is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Foobar.  If not, see <http://www.gnu.org/licenses/>

import dbus

bus = dbus.SessionBus()
proxy = bus.get_object("fr.sise.test", "/fr/sise/test")
interface = dbus.Interface(proxy, dbus_interface="fr.sise.test")

print(interface.Hello("toto"))
