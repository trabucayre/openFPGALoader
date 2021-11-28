.. _intel:

Intel notes
###########

Intel/Altera
============

.. NOTE::

  * CYC1000
  * DE0
  * de0nano

Loading a bitstream
-------------------

SVF and RBF files are supported.

``sof`` to ``svf`` generation:

.. code-block:: bash

    quartus_cpf -c -q -g 3.3 -n 12.0MHz p project_name.sof project_name.svf

``sof`` to ``rbf`` generation:

.. code-block:: bash

    quartus_cpf  --option=bitstream_compression=off -c project_name.sof project_name.rbf

.. WARNING::
  As mentionned in ``cyclone`` handbooks, real-time decompression is not supported by FPGA in JTAG mode.
  Keep in mind to disable this option.

file load:

.. code-block:: bash

    openFPGALoader -b boardname project_name.svf
    # or
    openFPGALoader -b boardname project_name.rbf

with ``boardname`` = ``de0``, ``cyc1000``, ``de0nano``, ``de0nanoSoc`` or ``qmtechCycloneV``.

SPI flash
---------

RPD and RBF are supported.

``sof`` to ``rpd``:

.. code-block:: bash

    quartus_cpf -o auto_create_rpd=on -c -d EPCQ16A -s 10CL025YU256C8G project_name.svf project_name.jic

file load:

.. code-block:: bash

    openFPGALoader -b cyc1000 -r project_name_auto.rpd
    # or
    openFPGALoader -b cyc1000 -r project_name.rbf
