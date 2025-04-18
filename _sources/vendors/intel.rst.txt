.. _intel:

Intel notes
###########

Intel/Altera
============

.. NOTE::

  * CYC1000
  * C10LP-RefKit
  * DE0
  * de0nano

Loading a bitstream
-------------------

SVF and RBF files are supported.

``sof`` to ``svf`` generation:

.. code-block:: bash

    quartus_cpf -c -q 12.0MHz -g 3.3 -n p project_name.sof project_name.svf

``sof`` to ``rbf`` generation:

.. code-block:: bash

    quartus_cpf  --option=bitstream_compression=off -c project_name.sof project_name.rbf

.. WARNING::
  As mentioned in ``cyclone`` handbooks, real-time decompression is not supported by FPGA in JTAG mode.
  Keep in mind to disable this option.

You can have Quartus automatically generate SVF and RBF files by adding these lines to the ``qsf`` file, or include them in a ``tcl`` file in FuseSoC

.. code-block:: 

    set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION OFF
    set_global_assignment -name GENERATE_RBF_FILE ON
    set_global_assignment -name GENERATE_SVF_FILE ON

file load:

.. code-block:: bash

    openFPGALoader -b boardname project_name.svf
    # or
    openFPGALoader -b boardname project_name.rbf

with ``boardname`` = ``de0``, ``cyc1000``, ``c10lp-refkit``, ``de0nano``, ``de0nanoSoc`` or ``qmtechCycloneV``.

SPI flash
---------

RPD and RBF are supported. POF is only supported for MAX10 (internal flash).

``pof`` to ``rpd``:

.. code-block:: bash

    quartus_cpf -c project_name.pof project_name.rpd

``sof`` to ``rpd``:

.. code-block:: bash

    # CYC1000
    quartus_cpf -o auto_create_rpd=on -c -d EPCQ16A -s 10CL025YU256C8G project_name.svf project_name.jic
    # C10LP-RefKit
    quartus_cpf -o auto_create_rpd=on -c -d EPCQ16A -s 10CL055YU484C8G project_name.svf project_name.jic

file load:

.. code-block:: bash

    openFPGALoader -b boardname -r project_name_auto.rpd
    # or
    openFPGALoader -b boardname -r project_name.rbf

with ``boardname`` = ``cyc1000``, ``c10lp-refkit``.

MAX10: FPGA Programming Guide
=============================

Supported Boards:

* step-max10_v1
* analogMax

Supported File Types:

* ``svf``
* ``svf``
* ``bin`` (arbitrary binary files)

Using ``svf``
-------------

.. note::

    This method is required to load a bitstream into *SRAM*.

.. code-block:: bash

    openFPGALoader [-b boardname] -c cablename the_svf_file.svf

Parameters:

* ``boardname``: One of the boards supported by ``openFPGALoader`` (optional).
* ``cablename``: One of the supported cables (see ``--list-cables``).

Using ``pof``
-------------

When writing the bitstream to internal flash, using a ``pof`` file is the fastest approach.

.. code-block:: bash

    openFPGALoader [-b boardname] [--flash-sector] -c cablename the_pof_file.pof

Parameters:

* ``boardname``: One of the boards supported by ``openFPGALoader`` (optional).
* ``cablename``: One of the supported cables (see ``--list-cables``).
* ``--flash-sector``: Specifies which internal flash sectors to erase/update instead of modifying the entire flash. One
  or more section may be provided, with ``,`` as separator. When this option isn't provided a full internal flash erase/
  update is performed

Accepted Flash Sectors:

* ``UFM0``, ``UFM1``: User Flash Memory sections.
* ``CFM0``, ``CFM1``, ``CFM2``: Configuration Flash Memory sectors.

Example:

.. code-block:: bash

    openFPGALoader -c usb-blaster --flash-sector UFM1,CFM0,CFM2 the_pof_file.pof

This command updates ``UFM1``, ``CFM0``, and ``CFM2``, while leaving other sectors unchanged.

Using an arbitrary binary file
------------------------------

This command updates only *User Flash Memory* sectors without modifying ``CFMx``. Unlike Altera Quartus, it supports
any binary format without limitations (not limited to a ``.bin``.

.. note:: This approach is useful to updates, for example, a softcore CPU firmware.

.. code-block:: bash

    openFPGALoader [-b boardname] -c cablename the_bin_file.bin

* ``boardname``: One of the boards supported by ``openFPGALoader`` (optional).
* ``cablename``: One of the supported cables (see ``--list-cables``).

Behavior:

``UFM0`` and ``UFM1`` will be erased before writing the binary file.

.. note:: Depending on the internal flash configuration, ``CFM1`` and ``CFM2`` may also store arbitrary data. However, currently, ``openFPGALoader`` only supports writing to ``UFMx``.

Intel/Altera (Old Boards)
=========================

.. NOTE::

  * Cyclone II (FPGA) (Tested OK: EP2C5T144C8N)
  * Max II (CPLD) (Tested OK: EPM240T100C5N)

Loading a Serial Vector Format (.svf)
-------------------------------------

SVF files are supported.

To load the file:

 .. code-block:: bash

    openFPGALoader -c usb-blaster project_name.svf

 
 
