# ---------------------------------------------------
## UltraZed-EV SOM Clock Output #4 - 300MHz OSC Y4 on Bank 66
## ---------------------------------------------------
set_property PACKAGE_PIN AC8 [ get_ports "user_sys_clk_p" ]; # SYS_CLK_P
set_property PACKAGE_PIN AC7 [ get_ports "user_sys_clk_n" ]; # SYS_CLK_N
##
set_property IOSTANDARD LVDS [ get_ports "user_sys_clk_p" ];
set_property IOSTANDARD LVDS [ get_ports "user_sys_clk_n" ];

create_clock -add -name sysclock -period 3.333333333333333 [get_ports {"user_sys_clk_p","user_sys_clk_n"}];

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


# PL User Push Switches
#
set_property PACKAGE_PIN AA13 [get_ports {PL_PB1}];	# HP_DP_18_P
set_property PACKAGE_PIN AB13 [get_ports {PL_PB2}];	# HP_DP_18_N
set_property PACKAGE_PIN AA15 [get_ports {PL_PB3}];	# HP_DP_19_P
set_property PACKAGE_PIN AB15 [get_ports {PL_PB4}];	# HP_DP_19_N

set_property IOSTANDARD LVCMOS18 [get_ports {PL_PB1}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_PB2}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_PB3}]
set_property IOSTANDARD LVCMOS18 [get_ports {PL_PB4}]


# PL User DIP Switches
#
set_property PACKAGE_PIN AF13 [get_ports {SW1}];	# HP_SE_00
set_property PACKAGE_PIN AG19 [get_ports {SW2}];	# HP_SE_01
set_property PACKAGE_PIN AC13 [get_ports {SW3}];	# HP_SE_02
set_property PACKAGE_PIN AC19 [get_ports {SW4}];	# HP_SE_03
set_property PACKAGE_PIN AF1 [get_ports {SW5}];		# HP_SE_04
set_property PACKAGE_PIN AH4 [get_ports {SW6}];		# HP_SE_05
set_property PACKAGE_PIN AG9 [get_ports {SW7}];		# HP_SE_06
set_property PACKAGE_PIN AE10 [get_ports {SW8}];	# HP_SE_07

set_property IOSTANDARD LVCMOS18 [get_ports {SW1}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW2}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW3}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW4}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW5}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW6}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW7}]
set_property IOSTANDARD LVCMOS18 [get_ports {SW8}]


# PL PMODs
#
set_property PACKAGE_PIN B15 [get_ports {PLPMOD1_D0}];  # HD_SE_00_P
set_property PACKAGE_PIN A15 [get_ports {PLPMOD1_D1}];  # HD_SE_00_N
set_property PACKAGE_PIN A17 [get_ports {PLPMOD1_D2}];  # HD_SE_01_P
set_property PACKAGE_PIN A16 [get_ports {PLPMOD1_D3}];  # HD_SE_01_N
set_property PACKAGE_PIN J16 [get_ports {PLPMOD1_D4}];  # HD_SE_02_P
set_property PACKAGE_PIN H16 [get_ports {PLPMOD1_D5}];  # HD_SE_02_N
set_property PACKAGE_PIN K15 [get_ports {PLPMOD1_D6}];  # HD_SE_03_P
set_property PACKAGE_PIN K14 [get_ports {PLPMOD1_D7}];  # HD_SE_03_N

set_property PACKAGE_PIN G16 [get_ports {PLPMOD2_D0}];  # HD_SE_04_GC_P
set_property PACKAGE_PIN G15 [get_ports {PLPMOD2_D1}];  # HD_SE_04_GC_N
set_property PACKAGE_PIN E15 [get_ports {PLPMOD2_D2}];  # HD_SE_05_GC_P
set_property PACKAGE_PIN D15 [get_ports {PLPMOD2_D3}];  # HD_SE_05_GC_N
set_property PACKAGE_PIN F16 [get_ports {PLPMOD2_D4}];  # HD_SE_06_GC_P
set_property PACKAGE_PIN F15 [get_ports {PLPMOD2_D5}];  # HD_SE_06_GC_N
set_property PACKAGE_PIN E17 [get_ports {PLPMOD2_D6}];  # HD_SE_07_GC_P
set_property PACKAGE_PIN D17 [get_ports {PLPMOD2_D7}];  # HD_SE_07_GC_N

set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D0}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D1}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D2}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D3}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D4}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D5}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D6}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD1_D7}]

set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D0}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D1}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D2}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D3}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D4}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D5}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D6}]
set_property IOSTANDARD LVCMOS33 [get_ports {PLPMOD2_D7}]


# PL HDMI Interface
#
set_property PACKAGE_PIN D5  [get_ports {U9_IN_D0N}];		# GTH0_TX_N
set_property PACKAGE_PIN D6  [get_ports {U9_IN_D0P}];		# GTH0_TX_P
set_property PACKAGE_PIN C7  [get_ports {U9_IN_D1N}];		# GTH1_TX_N
set_property PACKAGE_PIN C8  [get_ports {U9_IN_D1P}];		# GTH1_TX_P
set_property PACKAGE_PIN B5  [get_ports {U9_IN_D2N}];		# GTH2_TX_N
set_property PACKAGE_PIN B6  [get_ports {U9_IN_D2P}];		# GTH2_TX_P

set_property PACKAGE_PIN AE15 [get_ports {U9_IN_CLKN}];	# HP_DP_23_N
set_property PACKAGE_PIN AD15 [get_ports {U9_IN_CLKP}];	# HP_DP_23_P
set_property PACKAGE_PIN A14  [get_ports {U9_OE}];		# HD_SE_13_P
set_property PACKAGE_PIN E13  [get_ports {U9_SCL_SRC}];	# HD_SE_17_GC_N
set_property PACKAGE_PIN E14  [get_ports {U9_SDA_SRC}];	# HD_SE_17_GC_P
set_property PACKAGE_PIN F13  [get_ports {U9_SCL_CTL}];	# HD_SE_18_GC_N
set_property PACKAGE_PIN G13  [get_ports {U9_SDA_CTL}];	# HD_SE_18_GC_P
set_property PACKAGE_PIN A13  [get_ports {U8_CEC_A}];		# HD_SE_13_N
set_property PACKAGE_PIN C14  [get_ports {U8_HPD_A}];		# HD_SE_14_P


set_property PACKAGE_PIN D1  [get_ports {J12_D0N}];			# GTH0_RX_N
set_property PACKAGE_PIN D2  [get_ports {J12_D0P}];			# GTH0_RX_P
set_property PACKAGE_PIN C3  [get_ports {J12_D1N}];			# GTH1_RX_N
set_property PACKAGE_PIN C4  [get_ports {J12_D1P}];			# GTH1_RX_P
set_property PACKAGE_PIN B1  [get_ports {J12_D2N}];			# GTH2_RX_N
set_property PACKAGE_PIN B2  [get_ports {J12_D2P}];			# GTH2_RX_P
set_property PACKAGE_PIN B9  [get_ports {J12_CLKN}];			# GTH_REFCLK1_N
set_property PACKAGE_PIN B10 [get_ports {J12_CLKP}];			# GTH_REFCLK1_P

set_property PACKAGE_PIN G14  [get_ports {HDMI_RX_PWR_DET}];	# HD_SE_15_N
set_property PACKAGE_PIN H14  [get_ports {HDMI_RX_HPD}];		# HD_SE_15_P
set_property PACKAGE_PIN B14  [get_ports {U11_CEC_A}];		# HD_SE_14_N
set_property PACKAGE_PIN E12  [get_ports {U11_SCL_A}];		# HD_SE_19_GC_N
set_property PACKAGE_PIN F12  [get_ports {U11_SDA_A}];		# HD_SE_19_GC_P


# PL 3G-SDI Interface
#
set_property PACKAGE_PIN A7  [get_ports {U13_SDIN}];		# GTH3_TX_N
set_property PACKAGE_PIN A8  [get_ports {U13_SDIP}];		# GTH3_TX_P
set_property PACKAGE_PIN A3  [get_ports {U12_SDON}];		# GTH3_RX_N
set_property PACKAGE_PIN A4  [get_ports {U12_SDOP}];		# GTH3_RX_P


# PL SFP+ Interface
#
set_property PACKAGE_PIN H5  [get_ports {SFP1_TD_N}];		# GTH4_TX_N
set_property PACKAGE_PIN H6  [get_ports {SFP1_TD_P}];		# GTH4_TX_P
set_property PACKAGE_PIN H1  [get_ports {SFP1_RD_N}];		# GTH4_RX_N
set_property PACKAGE_PIN H2  [get_ports {SFP1_RD_P}];		# GTH4_RX_P
set_property PACKAGE_PIN J15 [get_ports {SFP1_TX_DIS}];	# HD_SE_09_P
set_property PACKAGE_PIN J14 [get_ports {LOS1}];		# HD_SE_09_N
set_property PACKAGE_PIN C17 [get_ports {SFP1_SDA}];		# HD_SE_10_P
set_property PACKAGE_PIN B16 [get_ports {SFP1_SCL}];		# HD_SE_10_N

set_property PACKAGE_PIN G7  [get_ports {SFP2_TD_N}];		# GTH5_TX_N
set_property PACKAGE_PIN G8  [get_ports {SFP2_TD_P}];		# GTH5_TX_P
set_property PACKAGE_PIN G3  [get_ports {SFP2_RD_N}];		# GTH5_RX_N
set_property PACKAGE_PIN G4  [get_ports {SFP2_RD_P}];		# GTH5_RX_P
set_property PACKAGE_PIN L15 [get_ports {SFP2_TX_DIS}];	# HD_SE_11_P
set_property PACKAGE_PIN L14 [get_ports {LOS2}];		# HD_SE_11_N
set_property PACKAGE_PIN B12 [get_ports {SFP2_SDA}];		# HD_SE_12_P
set_property PACKAGE_PIN A12 [get_ports {SFP2_SCL}];		# HD_SE_12_N


# GTH Differential Clocks From the IDT 8T49N241 Device
#
set_property PACKAGE_PIN D9  [get_ports {GTH_REFCLK0_N}]
set_property PACKAGE_PIN D10 [get_ports {GTH_REFCLK0_P}]
set_property PACKAGE_PIN H9  [get_ports {GTH_REFCLK2_N}]
set_property PACKAGE_PIN H10 [get_ports {GTH_REFCLK2_P}]
set_property PACKAGE_PIN J7  [get_ports {GTH_REFCLK5_N}]
set_property PACKAGE_PIN J8  [get_ports {GTH_REFCLK5_P}]


# GTH SMA Clocks
#
set_property PACKAGE_PIN F9  [get_ports {GTH_REFCLK3_N}]
set_property PACKAGE_PIN F10  [get_ports {GTH_REFCLK3_P}]
set_property PACKAGE_PIN N7  [get_ports {GTH_REFCLK7_N}]
set_property PACKAGE_PIN N8  [get_ports {GTH_REFCLK7_P}]


# PL LVDS Touch Panel Interface
#
set_property PACKAGE_PIN AH12 [get_ports {TP_D0_P}];		# HP_DP_42_P
set_property PACKAGE_PIN AJ12 [get_ports {TP_D0_N}];		# HP_DP_42_N
set_property PACKAGE_PIN AE9 [get_ports {TP_D1_P}];		# HP_DP_43_P
set_property PACKAGE_PIN AE8 [get_ports {TP_D1_N}];		# HP_DP_43_N
set_property PACKAGE_PIN AH3 [get_ports {TP_D2_P}];		# HP_DP_44_P
set_property PACKAGE_PIN AH2 [get_ports {TP_D2_N}];		# HP_DP_44_N
set_property PACKAGE_PIN AK3 [get_ports {TP_D3_P}];		# HP_DP_45_P
set_property PACKAGE_PIN AK2 [get_ports {TP_D3_N}];		# HP_DP_45_N
set_property PACKAGE_PIN AG1 [get_ports {TP_CLK_P}];		# HP_DP_46_P
set_property PACKAGE_PIN AH1 [get_ports {TP_CLK_N}];		# HP_DP_46_N
set_property PACKAGE_PIN H13  [get_ports {TP_SCL}];		# HD_SE_21_P
set_property PACKAGE_PIN H12  [get_ports {TP_SDA}];		# HD_SE_21_N
set_property PACKAGE_PIN C12  [get_ports {TP_IRQ_N}];		# HD_SE_20_N

set_property IOSTANDARD LVDS [get_ports {TP_CLK_P}]
set_property IOSTANDARD LVDS [get_ports {TP_CLK_N}]
set_property IOSTANDARD LVDS [get_ports {TP_D0_P}]
set_property IOSTANDARD LVDS [get_ports {TP_D0_N}]
set_property IOSTANDARD LVDS [get_ports {TP_D1_P}]
set_property IOSTANDARD LVDS [get_ports {TP_D1_N}]
set_property IOSTANDARD LVDS [get_ports {TP_D2_P}]
set_property IOSTANDARD LVDS [get_ports {TP_D2_N}]
set_property IOSTANDARD LVDS [get_ports {TP_D3_P}]
set_property IOSTANDARD LVDS [get_ports {TP_D3_N}]
set_property IOSTANDARD LVCMOS33 [get_ports {TP_INT#}]
set_property IOSTANDARD LVCMOS33 [get_ports {TP_SCL}]
set_property IOSTANDARD LVCMOS33 [get_ports {TP_SDA}]


# FMC HPC Interface
#
set_property PACKAGE_PIN AH14 [get_ports {FMC_CLK0_M2C_N}];  	# HP_DP_14_GC_N
set_property PACKAGE_PIN AG14 [get_ports {FMC_CLK0_M2C_P}];  	# HP_DP_14_GC_P
set_property PACKAGE_PIN AJ7 [get_ports {FMC_CLK1_M2C_N}];  	# HP_DP_34_GC_N
set_property PACKAGE_PIN AH7 [get_ports {FMC_CLK1_M2C_P}];  	# HP_DP_34_GC_P
set_property PACKAGE_PIN AF17 [get_ports {FMC_LA00_CC_N}];		# HP_DP_12_GC_N
set_property PACKAGE_PIN AF16 [get_ports {FMC_LA00_CC_P}];  	# HP_DP_12_GC_P
set_property PACKAGE_PIN AE17 [get_ports {FMC_LA01_CC_N}];  	# HP_DP_13_GC_N
set_property PACKAGE_PIN AD17 [get_ports {FMC_LA01_CC_P}];  	# HP_DP_13_GC_P

set_property PACKAGE_PIN AH18 [get_ports {FMC_LA02_N}];  		# HP_DP_00_N
set_property PACKAGE_PIN AG18 [get_ports {FMC_LA02_P}];  		# HP_DP_00_P
set_property PACKAGE_PIN AF18 [get_ports {FMC_LA03_N}];  		# HP_DP_01_N
set_property PACKAGE_PIN AE18 [get_ports {FMC_LA03_P}];  		# HP_DP_01_P
set_property PACKAGE_PIN AJ17 [get_ports {FMC_LA04_N}];  		# HP_DP_02_N
set_property PACKAGE_PIN AH17 [get_ports {FMC_LA04_P}];  		# HP_DP_02_P
set_property PACKAGE_PIN AE19 [get_ports {FMC_LA05_N}];  		# HP_DP_03_N
set_property PACKAGE_PIN AD19 [get_ports {FMC_LA05_P}];  		# HP_DP_03_P
set_property PACKAGE_PIN AC18 [get_ports {FMC_LA06_N}];  		# HP_DP_04_N
set_property PACKAGE_PIN AC17 [get_ports {FMC_LA06_P}];  		# HP_DP_04_P
set_property PACKAGE_PIN AB16 [get_ports {FMC_LA07_N}];  		# HP_DP_05_N
set_property PACKAGE_PIN AA16 [get_ports {FMC_LA07_P}];  		# HP_DP_05_P
set_property PACKAGE_PIN AK16 [get_ports {FMC_LA08_N}];  		# HP_DP_06_N
set_property PACKAGE_PIN AJ16 [get_ports {FMC_LA08_P}];  		# HP_DP_06_P
set_property PACKAGE_PIN AK18 [get_ports {FMC_LA09_N}]; 		# HP_DP_07_N
set_property PACKAGE_PIN AK17 [get_ports {FMC_LA09_P}];  		# HP_DP_07_P
set_property PACKAGE_PIN AH16 [get_ports {FMC_LA10_N}];  		# HP_DP_08_N
set_property PACKAGE_PIN AG16 [get_ports {FMC_LA10_P}];  		# HP_DP_08_P
set_property PACKAGE_PIN AD16 [get_ports {FMC_LA11_N}];  		# HP_DP_09_N
set_property PACKAGE_PIN AC16 [get_ports {FMC_LA11_P}];  		# HP_DP_09_P
set_property PACKAGE_PIN AK14 [get_ports {FMC_LA12_N}];  		# HP_DP_10_N
set_property PACKAGE_PIN AJ14 [get_ports {FMC_LA12_P}];  		# HP_DP_10_P
set_property PACKAGE_PIN AK15 [get_ports {FMC_LA13_N}];  		# HP_DP_11_N
set_property PACKAGE_PIN AJ15 [get_ports {FMC_LA13_P}];  		# HP_DP_11_P
set_property PACKAGE_PIN AG15 [get_ports {FMC_LA14_N}];  		# HP_DP_15_GC_N
set_property PACKAGE_PIN AF15 [get_ports {FMC_LA14_P}];  		# HP_DP_15_GC_P
set_property PACKAGE_PIN AK12 [get_ports {FMC_LA15_N}];  		# HP_DP_16_N
set_property PACKAGE_PIN AK13 [get_ports {FMC_LA15_P}];  		# HP_DP_16_P
set_property PACKAGE_PIN AH13 [get_ports {FMC_LA16_N}];  		# HP_DP_17_N
set_property PACKAGE_PIN AG13 [get_ports {FMC_LA16_P}];  		# HP_DP_17_P
set_property PACKAGE_PIN AG5 [get_ports {FMC_LA17_CC_N}];  	# HP_DP_32_GC_N
set_property PACKAGE_PIN AG6 [get_ports {FMC_LA17_CC_P}];  	# HP_DP_32_GC_P
set_property PACKAGE_PIN AJ6 [get_ports {FMC_LA18_CC_N}];  	# HP_DP_33_GC_N
set_property PACKAGE_PIN AH6 [get_ports {FMC_LA18_CC_P}];  	# HP_DP_33_GC_P
set_property PACKAGE_PIN AG10 [get_ports {FMC_LA19_N}];  		# HP_DP_24_N
set_property PACKAGE_PIN AF10 [get_ports {FMC_LA19_P}];  		# HP_DP_24_P
set_property PACKAGE_PIN AK10 [get_ports {FMC_LA20_N}];  		# HP_DP_25_N
set_property PACKAGE_PIN AJ10 [get_ports {FMC_LA20_P}];  		# HP_DP_25_P
set_property PACKAGE_PIN AF7 [get_ports {FMC_LA21_N}];  		# HP_DP_26_N
set_property PACKAGE_PIN AF8 [get_ports {FMC_LA21_P}];  		# HP_DP_26_P
set_property PACKAGE_PIN AF11 [get_ports {FMC_LA22_N}];  		# HP_DP_27_N
set_property PACKAGE_PIN AF12 [get_ports {FMC_LA22_P}];  		# HP_DP_27_P
set_property PACKAGE_PIN AK5 [get_ports {FMC_LA23_N}];  		# HP_DP_28_N
set_property PACKAGE_PIN AJ5 [get_ports {FMC_LA23_P}];  		# HP_DP_28_P
set_property PACKAGE_PIN AK6 [get_ports {FMC_LA24_N}];  		# HP_DP_29_N
set_property PACKAGE_PIN AK7 [get_ports {FMC_LA24_P}];  		# HP_DP_29_P
set_property PACKAGE_PIN AF5 [get_ports {FMC_LA25_N}];  		# HP_DP_30_N
set_property PACKAGE_PIN AF6 [get_ports {FMC_LA25_P}];  		# HP_DP_30_P
set_property PACKAGE_PIN AK8 [get_ports {FMC_LA26_N}];  		# HP_DP_31_N
set_property PACKAGE_PIN AK9 [get_ports {FMC_LA26_P}];  		# HP_DP_31_P
set_property PACKAGE_PIN AH8 [get_ports {FMC_LA27_N}];  		# HP_DP_35_GC_N
set_property PACKAGE_PIN AG8 [get_ports {FMC_LA27_P}];  		# HP_DP_35_GC_P
set_property PACKAGE_PIN AK11 [get_ports {FMC_LA28_N}];  		# HP_DP_36_N
set_property PACKAGE_PIN AJ11 [get_ports {FMC_LA28_P}];  		# HP_DP_36_P
set_property PACKAGE_PIN AK4 [get_ports {FMC_LA29_N}];  		# HP_DP_37_N
set_property PACKAGE_PIN AJ4 [get_ports {FMC_LA29_P}];  		# HP_DP_37_P
set_property PACKAGE_PIN AJ1 [get_ports {FMC_LA30_N}];  		# HP_DP_38_N
set_property PACKAGE_PIN AJ2 [get_ports {FMC_LA30_P}];  		# HP_DP_38_P
set_property PACKAGE_PIN AF2 [get_ports {FMC_LA31_N}]; 		# HP_DP_39_N
set_property PACKAGE_PIN AF3 [get_ports {FMC_LA31_P}];  		# HP_DP_39_P
set_property PACKAGE_PIN AJ9 [get_ports {FMC_LA32_N}];  		# HP_DP_40_N
set_property PACKAGE_PIN AH9 [get_ports {FMC_LA32_P}];  		# HP_DP_40_P
set_property PACKAGE_PIN AH11 [get_ports {FMC_LA33_N}];  		# HP_DP_41_N
set_property PACKAGE_PIN AG11 [get_ports {FMC_LA33_P}];  		# HP_DP_41_P
set_property PACKAGE_PIN J12  [get_ports {FMC_SCL}];  		# HD_SE_22_N
set_property PACKAGE_PIN K13  [get_ports {FMC_SDA}];  		# HD_SE_22_P
set_property PACKAGE_PIN K12   [get_ports {FMC_PRSNT_M2C#}];  	# HD_SE_23_P
set_property PACKAGE_PIN K11  [get_ports {FMC_TRST#}];  		# HD_SE_23_N

set_property IOSTANDARD LVDS [get_ports {FMC_CLK0_M2C_N}]
set_property IOSTANDARD LVDS [get_ports {FMC_CLK0_M2C_P}]
set_property IOSTANDARD LVDS [get_ports {FMC_CLK1_M2C_N}]
set_property IOSTANDARD LVDS [get_ports {FMC_CLK1_M2C_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA00_CC_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA00_CC_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA01_CC_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA01_CC_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA02_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA02_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA03_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA03_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA04_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA04_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA05_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA05_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA06_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA06_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA07_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA07_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA08_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA08_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA09_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA09_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA10_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA10_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA11_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA11_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA12_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA12_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA13_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA13_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA14_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA14_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA15_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA15_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA16_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA16_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA17_CC_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA17_CC_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA18_CC_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA18_CC_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA19_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA19_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA20_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA20_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA21_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA21_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA22_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA22_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA23_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA23_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA24_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA24_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA25_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA25_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA26_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA26_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA27_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA27_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA28_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA28_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA29_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA29_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA30_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA30_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA31_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA31_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA32_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA32_N}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA33_P}]
set_property IOSTANDARD LVCMOS18 [get_ports {FMC_LA33_N}]
set_property IOSTANDARD LVCMOS33 [get_ports {FMC_SCL}]
set_property IOSTANDARD LVCMOS33 [get_ports {FMC_SDA}]
set_property IOSTANDARD LVCMOS33 [get_ports {FMC_PRSNT_M2C#}]
set_property IOSTANDARD LVCMOS33 [get_ports {FMC_TRST#}]

set_property PACKAGE_PIN P5  [get_ports {DP0_M2C_N}]; # GTH8_TX_N
set_property PACKAGE_PIN P6  [get_ports {DP0_M2C_P}]; # GTH8_TX_P
set_property PACKAGE_PIN N3  [get_ports {DP0_C2M_N}]; # GTH8_RX_N
set_property PACKAGE_PIN N4  [get_ports {DP0_C2M_P}]; # GTH8_RX_P
set_property PACKAGE_PIN M5  [get_ports {DP1_M2C_N}]; # GTH9_TX_N
set_property PACKAGE_PIN M6  [get_ports {DP1_M2C_P}]; # GTH9_TX_P
set_property PACKAGE_PIN M1  [get_ports {DP1_C2M_N}]; # GTH9_RX_N
set_property PACKAGE_PIN M2  [get_ports {DP1_C2M_P}]; # GTH9_RX_P
set_property PACKAGE_PIN L3  [get_ports {DP2_M2C_N}]; # GTH10_TX_N
set_property PACKAGE_PIN L4  [get_ports {DP2_M2C_P}]; # GTH10_TX_P
set_property PACKAGE_PIN K1  [get_ports {DP2_C2M_N}]; # GTH10_RX_N
set_property PACKAGE_PIN K2  [get_ports {DP2_C2M_P}]; # GTH10_RX_P
set_property PACKAGE_PIN K5  [get_ports {DP3_M2C_N}]; # GTH11_TX_N
set_property PACKAGE_PIN K6  [get_ports {DP3_M2C_P}]; # GTH11_TX_P
set_property PACKAGE_PIN J3  [get_ports {DP3_C2M_N}]; # GTH11_RX_N
set_property PACKAGE_PIN J4  [get_ports {DP3_C2M_P}]; # GTH11_RX_P
set_property PACKAGE_PIN W3  [get_ports {DP4_M2C_N}]; # GTH12_TX_N
set_property PACKAGE_PIN W4  [get_ports {DP4_M2C_P}]; # GTH12_TX_P
set_property PACKAGE_PIN V1  [get_ports {DP4_C2M_N}]; # GTH12_RX_N
set_property PACKAGE_PIN V2  [get_ports {DP4_C2M_P}]; # GTH12_RX_P
set_property PACKAGE_PIN V5  [get_ports {DP5_M2C_N}]; # GTH13_TX_N
set_property PACKAGE_PIN V6  [get_ports {DP5_M2C_P}]; # GTH13_TX_P
set_property PACKAGE_PIN U3  [get_ports {DP5_C2M_N}]; # GTH13_RX_N
set_property PACKAGE_PIN U4  [get_ports {DP5_C2M_P}]; # GTH13_RX_P
set_property PACKAGE_PIN T5  [get_ports {DP6_M2C_N}]; # GTH14_TX_N
set_property PACKAGE_PIN T6  [get_ports {DP6_M2C_P}]; # GTH14_TX_P
set_property PACKAGE_PIN T1  [get_ports {DP6_C2M_N}]; # GTH14_RX_N
set_property PACKAGE_PIN T2  [get_ports {DP6_C2M_P}]; # GTH14_RX_P
set_property PACKAGE_PIN R3  [get_ports {DP7_M2C_N}]; # GTH15_TX_N
set_property PACKAGE_PIN R4  [get_ports {DP7_M2C_P}]; # GTH15_TX_P
set_property PACKAGE_PIN P1  [get_ports {DP7_C2M_N}]; # GTH15_RX_N
set_property PACKAGE_PIN P2  [get_ports {DP7_C2M_P}]; # GTH15_RX_P

set_property PACKAGE_PIN L7  [get_ports {GBTCLK0_M2C_N}]; # GTH_REFCLK4_N
set_property PACKAGE_PIN L8  [get_ports {GBTCLK0_M2C_P}]; # GTH_REFCLK4_P
set_property PACKAGE_PIN R7  [get_ports {GBTCLK1_M2C_N}]; # GTH_REFCLK6_N
set_property PACKAGE_PIN R8  [get_ports {GBTCLK1_M2C_P}]; # GTH_REFCLK6_P
