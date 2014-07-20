==========================
``perfmon_`` munin plugin
==========================

Intro
------

This is a new plugin for win32, in order to query PerfCounters.

Usage
-----

For now it can only emit 1 plugin value.

Just link (or copy) it as ``perfmon_myPerfCounter`` with an optional ``.exe`` extension.

Then in the munin-node plugins confiugration file, you have to add the counter path as :

::

    [perfmon_myPerfCounter]
    env.counter_path \Disque logique(C:)\Taille de file d'attente du disque actuelle

Remarks
-------

Note that the path has to be localized. Starting with Windows 2008/Vista, there is a 
possibility to use the English names of the paths

You can run the plugin with the special "list" command in order to retrieve
all the paths available on your system.
