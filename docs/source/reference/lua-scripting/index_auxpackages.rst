##################
Auxiliary Packages
##################

.. include:: version.rst

.. toctree::
    :hidden:

    calendar <openfallout_aux_calendar>
    time <openfallout_aux_time>
    ui <openfallout_aux_ui>
    util <openfallout_aux_util>


**Auxiliary packages**

``openfallout_aux.*`` are built-in libraries that are itself implemented in Lua. They can not do anything that is not possible with the basic API, they only make it more convenient.
Sources can be found in ``resources/vfs/openfallout_aux``. In theory mods can override them, but it is not recommended.

.. include:: tables/aux_packages.rst
