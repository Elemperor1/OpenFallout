########
Packages
########

.. include:: version.rst

.. toctree::
    :hidden:

    ambient <openfallout_ambient>
    animation <openfallout_animation>
    async <openfallout_async>
    camera <openfallout_camera>
    content <openfallout_content>
    core <openfallout_core>
    debug <openfallout_debug>
    input <openfallout_input>
    markup <openfallout_markup>
    menu <openfallout_menu>
    nearby <openfallout_nearby>
    postprocessing <openfallout_postprocessing>
    self <openfallout_self>
    storage <openfallout_storage>
    types <openfallout_types>
    ui <openfallout_ui>
    util <openfallout_util>
    vfs <openfallout_vfs>
    world <openfallout_world>

**API packages**

API packages provide functions that can be called by scripts. I.e. it is a script-to-engine interaction.
A package can be loaded with ``require('<package name>')``.
It can not be overloaded even if there is a lua file with the same name.
The list of available packages is different for global and for local scripts.
Player scripts are local scripts that are attached to a player.

.. include:: tables/packages.rst
