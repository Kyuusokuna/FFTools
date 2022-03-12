#include "Craft_Action_to_Icon_id.h"

#define COMMON_ACTION_ICONS \
[CA_Muscle_memory] =     1994,\
[CA_Reflect] =           1982,\
[CA_Trained_Eye] =       1981,\
[CA_Careful_Synthesis] = 1986,\
[CA_Byregots_Blessing] = 1975,\
[CA_Trained_Finesse] =   1997,\
[CA_Observe] =           1954,\
[CA_Masters_Mend] =      1952,\
[CA_Waste_Not] =         1992,\
[CA_Waste_Not2] =        1993,\
[CA_Manipulation] =      1985,\
[CA_Veneration] =        1995,\
[CA_Great_Strides] =     1955,\
[CA_Innovation] =        1987


u32 Craft_Action_to_Icon_id[NUM_JOBS][NUM_ACTIONS] = {
	[Craft_Job_CRP] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1501,
		[CA_Prudent_Synthesis] =  1520,
		[CA_Focused_Synthesis] =  1536,
		[CA_Groundwork] =         1518,
		[CA_Delicate_Synthesis] = 1503,
		[CA_Basic_Touch] =        1502,
		[CA_Standard_Touch] =     1516,
		[CA_Advanced_Touch] =     1519,
		[CA_Prudent_Touch] =      1535,
		[CA_Focused_Touch] =      1537,
		[CA_Preparatory_Touch] =  1507,
	},
	[Craft_Job_BSM] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1551,
		[CA_Prudent_Synthesis] =  1570,
		[CA_Focused_Synthesis] =  1585,
		[CA_Groundwork] =         1568,
		[CA_Delicate_Synthesis] = 1553,
		[CA_Basic_Touch] =        1552,
		[CA_Standard_Touch] =     1566,
		[CA_Advanced_Touch] =     1569,
		[CA_Prudent_Touch] =      1584,
		[CA_Focused_Touch] =      1586,
		[CA_Preparatory_Touch] =  1557,
	},
	[Craft_Job_ARM] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1601,
		[CA_Prudent_Synthesis] =  1621,
		[CA_Focused_Synthesis] =  1636,
		[CA_Groundwork] =         1618,
		[CA_Delicate_Synthesis] = 1603,
		[CA_Basic_Touch] =        1602,
		[CA_Standard_Touch] =     1616,
		[CA_Advanced_Touch] =     1620,
		[CA_Prudent_Touch] =      1635,
		[CA_Focused_Touch] =      1637,
		[CA_Preparatory_Touch] =  1607,
	},
	[Craft_Job_GSM] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1651,
		[CA_Prudent_Synthesis] =  1670,
		[CA_Focused_Synthesis] =  1687,
		[CA_Groundwork] =         1667,
		[CA_Delicate_Synthesis] = 1653,
		[CA_Basic_Touch] =        1652,
		[CA_Standard_Touch] =     1665,
		[CA_Advanced_Touch] =     1669,
		[CA_Prudent_Touch] =      1686,
		[CA_Focused_Touch] =      1688,
		[CA_Preparatory_Touch] =  1657,
	},
	[Craft_Job_LTW] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1701,
		[CA_Prudent_Synthesis] =  1720,
		[CA_Focused_Synthesis] =  1735,
		[CA_Groundwork] =         1718,
		[CA_Delicate_Synthesis] = 1703,
		[CA_Basic_Touch] =        1702,
		[CA_Standard_Touch] =     1716,
		[CA_Advanced_Touch] =     1719,
		[CA_Prudent_Touch] =      1734,
		[CA_Focused_Touch] =      1736,
		[CA_Preparatory_Touch] =  1707,
	},
	[Craft_Job_WVR] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1751,
		[CA_Prudent_Synthesis] =  1770,
		[CA_Focused_Synthesis] =  1785,
		[CA_Groundwork] =         1767,
		[CA_Delicate_Synthesis] = 1753,
		[CA_Basic_Touch] =        1752,
		[CA_Standard_Touch] =     1765,
		[CA_Advanced_Touch] =     1769,
		[CA_Prudent_Touch] =      1784,
		[CA_Focused_Touch] =      1786,
		[CA_Preparatory_Touch] =  1757,
	},
	[Craft_Job_ALC] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1801,
		[CA_Prudent_Synthesis] =  1821,
		[CA_Focused_Synthesis] =  1836,
		[CA_Groundwork] =         1818,
		[CA_Delicate_Synthesis] = 1803,
		[CA_Basic_Touch] =        1802,
		[CA_Standard_Touch] =     1816,
		[CA_Advanced_Touch] =     1820,
		[CA_Prudent_Touch] =      1835,
		[CA_Focused_Touch] =      1837,
		[CA_Preparatory_Touch] =  1807,
	},
	[Craft_Job_CUL] = {
		COMMON_ACTION_ICONS,
		[CA_Basic_Synthesis] =    1851,
		[CA_Prudent_Synthesis] =  1870,
		[CA_Focused_Synthesis] =  1887,
		[CA_Groundwork] =         1867,
		[CA_Delicate_Synthesis] = 1853,
		[CA_Basic_Touch] =        1852,
		[CA_Standard_Touch] =     1865,
		[CA_Advanced_Touch] =     1869,
		[CA_Prudent_Touch] =      1886,
		[CA_Focused_Touch] =      1888,
		[CA_Preparatory_Touch] =  1857,
	},
};