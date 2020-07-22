# ---------------------------------------------------
## UltraZed-EV SOM Clock Output #4 - 300MHz OSC Y4 on Bank 66
## ---------------------------------------------------
set_property PACKAGE_PIN AC8 [ get_ports {user_sys_clk_p} ]; # SYS_CLK_P
set_property PACKAGE_PIN AC7 [ get_ports {user_sys_clk_n} ]; # SYS_CLK_N
##
set_property IOSTANDARD LVDS [ get_ports {user_sys_clk_p} ];
set_property IOSTANDARD LVDS [ get_ports {user_sys_clk_n} ];

create_clock -add -name sysclock -period 3.333 [get_ports {user_sys_clk_p}];

# PL User LEDs
#
set_property PACKAGE_PIN AC14 [get_ports {PL_LED1}];	# HP_DP_20_P
set_property PACKAGE_PIN AD14 [get_ports {PL_LED2}];	# HP_DP_20_N
set_property PACKAGE_PIN AE14 [get_ports {PL_LED3}];	# HP_DP_21_P
set_property PACKAGE_PIN AE13 [get_ports {PL_LED4}];	# HP_DP_21_N
set_property PACKAGE_PIN AA14 [get_ports {PL_LED5}];	# HP_DP_22_P
set_property PACKAGE_PIN AB14 [get_ports {PL_LED6}];	# HP_DP_22_N
set_property PACKAGE_PIN AG4 [get_ports {PL_LED7}];	# HP_DP_47_P
set_property PACKAGE_PIN AG3 [get_ports {PL_LED8}];	# HP_DP_47_N

set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED1}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED2}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED3}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED4}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED5}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED6}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED7}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_LED8}]
