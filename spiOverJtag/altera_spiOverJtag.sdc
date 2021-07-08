#*********************
# Create Clock
#*********************

set jtag_t_period 83
create_clock -name {altera_reserved_tck} -period 83ns [get_ports {altera_reserved_tck}]
set_clock_groups -asynchronous -group {altera_reserved_tck}
#*********************
# Create Generated Clock
#*********************
derive_pll_clocks
#*********************
# Set Clock Uncertainty
#*********************
derive_clock_uncertainty
