..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2019-2023 Intel Corporation.

Timer Sample Application
==============================

The Timer sample application demonstrates the use of a timer in a FGEN application. This application
is implemented as a command that can be executed at the FGEN cli application. For more details
about the FGEN cli application see :doc:`cli`.

Running the Application
-----------------------

After :ref:`building FGEN <building-fgen>`, run the example:

.. code-block:: console

    $ ./buildir/examples/timer/timer

Launch the timer test:

.. code-block:: console

    fgen-cli:/> timer

Launch the perf test:

.. code-block:: console

    fgen-cli:/> perf

Explanation
-----------

The following sections provide some explanation of the code.

Initialization
~~~~~~~~~~~~~~

In addition to CLI initialization, the timer subsystem must be initialized, by calling the
fgen_timer_subsystem_init() function.

Initialize CLI:

.. code-block:: c

  if (setup_cli() < 0)
        return -1;

Initialize FGEN timer library:

.. code-block:: c

    fgen_timer_subsystem_init();

Managing Timers
~~~~~~~~~~~~~~~

The call to fgen_timer_init() is necessary before doing any other operation on the timer structure.

Initialize timer structure:

.. code-block:: c

    fgen_timer_init(&tim0);

Then, the two timers are configured:

The first timer is for 2 seconds. The SINGLE flag means that the timer expires only once and must be
reloaded manually if required. The callback function is single_timer(). The second timer expires
every half a second. Since the PERIODICAL flag is provided, the timer is reloaded automatically by
the timer subsystem. The callback function is periodical_timer().

Load single use timer for 2 seconds:

.. code-block:: c

    fgen_timer_reset(&tim0, fgen_get_timer_hz() * 2, SINGLE, fgen_id(), single_timer, NULL);

Load second timer, every 1/2 second:

.. code-block:: c

    fgen_timer_reset(&tim0, fgen_get_timer_hz() / 2, PERIODICAL, fgen_id(), periodical_timer, &count);

The timer is stopped using the fgen_timer_stop() function.
