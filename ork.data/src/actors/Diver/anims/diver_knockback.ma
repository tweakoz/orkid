//Maya ASCII 2008 scene
//Name: diver_knockback.ma
//Last modified: Mon, Aug 25, 2008 05:05:16 PM
//Codeset: 1252
file -rdi 1 -ns "D_" -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
file -r -ns "D_" -dr 1 -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
requires maya "2008";
requires "Mayatomr" "9.0.1.4m - 3.6.51.0 ";
requires "COLLADA" "3.05B";
currentUnit -l centimeter -a degree -t ntsc;
fileInfo "application" "maya";
fileInfo "product" "Maya Unlimited 2008";
fileInfo "version" "2008 Service Pack 1";
fileInfo "cutIdentifier" "200802242333-718075";
fileInfo "osv" "Microsoft Windows XP Service Pack 2 (Build 2600)\n";
createNode transform -s -n "persp";
	setAttr ".v" no;
	setAttr ".t" -type "double3" -397.51350519288292 54.905358712472335 36.573534226497145 ;
	setAttr ".r" -type "double3" -3.3383527176161496 279.39999999981609 3.6513117363651651e-015 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 385.3345271032822;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".tp" -type "double3" -14.405334940537376 4.93538676393631 -96.398888339143596 ;
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -s -n "top";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 5000.1000000000004 0 ;
	setAttr ".r" -type "double3" -89.999999999999986 0 0 ;
createNode camera -s -n "topShape" -p "top";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 0 5000.1000000000004 ;
createNode camera -s -n "frontShape" -p "front";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 5000.1000000000004 0 0 ;
	setAttr ".r" -type "double3" 0 89.999999999999986 0 ;
createNode camera -s -n "sideShape" -p "side";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "side";
	setAttr ".den" -type "string" "side_depth";
	setAttr ".man" -type "string" "side_mask";
	setAttr ".hc" -type "string" "viewSet -s %camera";
	setAttr ".o" yes;
createNode colladaDocument -n "colladaDocuments";
	setAttr ".doc[0].fn" -type "string" "";
createNode colladaDocument -n "colladaDocuments1";
	setAttr ".doc[0].fn" -type "string" "";
createNode lightLinker -n "lightLinker1";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
	setAttr ".cdl" 4;
	setAttr -s 5 ".dli[1:4]"  1 2 3 4;
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 238 ".phl";
	setAttr ".phl[1870]" 0;
	setAttr ".phl[1871]" 0;
	setAttr ".phl[1872]" 0;
	setAttr ".phl[1873]" 0;
	setAttr ".phl[1874]" 0;
	setAttr ".phl[1875]" 0;
	setAttr ".phl[1876]" 0;
	setAttr ".phl[1877]" 0;
	setAttr ".phl[1878]" 0;
	setAttr ".phl[1879]" 0;
	setAttr ".phl[1880]" 0;
	setAttr ".phl[1881]" 0;
	setAttr ".phl[1882]" 0;
	setAttr ".phl[1883]" 0;
	setAttr ".phl[1884]" 0;
	setAttr ".phl[1885]" 0;
	setAttr ".phl[1886]" 0;
	setAttr ".phl[1887]" 0;
	setAttr ".phl[1888]" 0;
	setAttr ".phl[1889]" 0;
	setAttr ".phl[1890]" 0;
	setAttr ".phl[1891]" 0;
	setAttr ".phl[1892]" 0;
	setAttr ".phl[1893]" 0;
	setAttr ".phl[1894]" 0;
	setAttr ".phl[1895]" 0;
	setAttr ".phl[1896]" 0;
	setAttr ".phl[1897]" 0;
	setAttr ".phl[1898]" 0;
	setAttr ".phl[1899]" 0;
	setAttr ".phl[1900]" 0;
	setAttr ".phl[1901]" 0;
	setAttr ".phl[1902]" 0;
	setAttr ".phl[1903]" 0;
	setAttr ".phl[1904]" 0;
	setAttr ".phl[1905]" 0;
	setAttr ".phl[1906]" 0;
	setAttr ".phl[1907]" 0;
	setAttr ".phl[1908]" 0;
	setAttr ".phl[1909]" 0;
	setAttr ".phl[1910]" 0;
	setAttr ".phl[1911]" 0;
	setAttr ".phl[1912]" 0;
	setAttr ".phl[1913]" 0;
	setAttr ".phl[1914]" 0;
	setAttr ".phl[1915]" 0;
	setAttr ".phl[1916]" 0;
	setAttr ".phl[1917]" 0;
	setAttr ".phl[1918]" 0;
	setAttr ".phl[1919]" 0;
	setAttr ".phl[1920]" 0;
	setAttr ".phl[1921]" 0;
	setAttr ".phl[1922]" 0;
	setAttr ".phl[1923]" 0;
	setAttr ".phl[1924]" 0;
	setAttr ".phl[1925]" 0;
	setAttr ".phl[1926]" 0;
	setAttr ".phl[1927]" 0;
	setAttr ".phl[1928]" 0;
	setAttr ".phl[1929]" 0;
	setAttr ".phl[1930]" 0;
	setAttr ".phl[1931]" 0;
	setAttr ".phl[1932]" 0;
	setAttr ".phl[1933]" 0;
	setAttr ".phl[1934]" 0;
	setAttr ".phl[1935]" 0;
	setAttr ".phl[1936]" 0;
	setAttr ".phl[1937]" 0;
	setAttr ".phl[1938]" 0;
	setAttr ".phl[1939]" 0;
	setAttr ".phl[1940]" 0;
	setAttr ".phl[1941]" 0;
	setAttr ".phl[1942]" 0;
	setAttr ".phl[1943]" 0;
	setAttr ".phl[1944]" 0;
	setAttr ".phl[1945]" 0;
	setAttr ".phl[1946]" 0;
	setAttr ".phl[1947]" 0;
	setAttr ".phl[1948]" 0;
	setAttr ".phl[1949]" 0;
	setAttr ".phl[1950]" 0;
	setAttr ".phl[1951]" 0;
	setAttr ".phl[1952]" 0;
	setAttr ".phl[1953]" 0;
	setAttr ".phl[1954]" 0;
	setAttr ".phl[1955]" 0;
	setAttr ".phl[1956]" 0;
	setAttr ".phl[1957]" 0;
	setAttr ".phl[1958]" 0;
	setAttr ".phl[1959]" 0;
	setAttr ".phl[1960]" 0;
	setAttr ".phl[1961]" 0;
	setAttr ".phl[1962]" 0;
	setAttr ".phl[1963]" 0;
	setAttr ".phl[1964]" 0;
	setAttr ".phl[1965]" 0;
	setAttr ".phl[1966]" 0;
	setAttr ".phl[1967]" 0;
	setAttr ".phl[1968]" 0;
	setAttr ".phl[1969]" 0;
	setAttr ".phl[1970]" 0;
	setAttr ".phl[1971]" 0;
	setAttr ".phl[1972]" 0;
	setAttr ".phl[1973]" 0;
	setAttr ".phl[1974]" 0;
	setAttr ".phl[1975]" 0;
	setAttr ".phl[1976]" 0;
	setAttr ".phl[1977]" 0;
	setAttr ".phl[1978]" 0;
	setAttr ".phl[1979]" 0;
	setAttr ".phl[1980]" 0;
	setAttr ".phl[1981]" 0;
	setAttr ".phl[1982]" 0;
	setAttr ".phl[1983]" 0;
	setAttr ".phl[1984]" 0;
	setAttr ".phl[1985]" 0;
	setAttr ".phl[1986]" 0;
	setAttr ".phl[1987]" 0;
	setAttr ".phl[1988]" 0;
	setAttr ".phl[1989]" 0;
	setAttr ".phl[1990]" 0;
	setAttr ".phl[1991]" 0;
	setAttr ".phl[1992]" 0;
	setAttr ".phl[1993]" 0;
	setAttr ".phl[1994]" 0;
	setAttr ".phl[1995]" 0;
	setAttr ".phl[1996]" 0;
	setAttr ".phl[1997]" 0;
	setAttr ".phl[1998]" 0;
	setAttr ".phl[1999]" 0;
	setAttr ".phl[2000]" 0;
	setAttr ".phl[2001]" 0;
	setAttr ".phl[2002]" 0;
	setAttr ".phl[2003]" 0;
	setAttr ".phl[2004]" 0;
	setAttr ".phl[2005]" 0;
	setAttr ".phl[2006]" 0;
	setAttr ".phl[2007]" 0;
	setAttr ".phl[2008]" 0;
	setAttr ".phl[2009]" 0;
	setAttr ".phl[2010]" 0;
	setAttr ".phl[2011]" 0;
	setAttr ".phl[2012]" 0;
	setAttr ".phl[2013]" 0;
	setAttr ".phl[2014]" 0;
	setAttr ".phl[2015]" 0;
	setAttr ".phl[2016]" 0;
	setAttr ".phl[2017]" 0;
	setAttr ".phl[2018]" 0;
	setAttr ".phl[2019]" 0;
	setAttr ".phl[2020]" 0;
	setAttr ".phl[2021]" 0;
	setAttr ".phl[2022]" 0;
	setAttr ".phl[2023]" 0;
	setAttr ".phl[2024]" 0;
	setAttr ".phl[2025]" 0;
	setAttr ".phl[2026]" 0;
	setAttr ".phl[2027]" 0;
	setAttr ".phl[2028]" 0;
	setAttr ".phl[2029]" 0;
	setAttr ".phl[2030]" 0;
	setAttr ".phl[2031]" 0;
	setAttr ".phl[2032]" 0;
	setAttr ".phl[2033]" 0;
	setAttr ".phl[2034]" 0;
	setAttr ".phl[2035]" 0;
	setAttr ".phl[2036]" 0;
	setAttr ".phl[2037]" 0;
	setAttr ".phl[2038]" 0;
	setAttr ".phl[2039]" 0;
	setAttr ".phl[2040]" 0;
	setAttr ".phl[2041]" 0;
	setAttr ".phl[2042]" 0;
	setAttr ".phl[2043]" 0;
	setAttr ".phl[2044]" 0;
	setAttr ".phl[2045]" 0;
	setAttr ".phl[2046]" 0;
	setAttr ".phl[2047]" 0;
	setAttr ".phl[2048]" 0;
	setAttr ".phl[2049]" 0;
	setAttr ".phl[2050]" 0;
	setAttr ".phl[2051]" 0;
	setAttr ".phl[2052]" 0;
	setAttr ".phl[2053]" 0;
	setAttr ".phl[2054]" 0;
	setAttr ".phl[2055]" 0;
	setAttr ".phl[2056]" 0;
	setAttr ".phl[2057]" 0;
	setAttr ".phl[2058]" 0;
	setAttr ".phl[2059]" 0;
	setAttr ".phl[2060]" 0;
	setAttr ".phl[2061]" 0;
	setAttr ".phl[2062]" 0;
	setAttr ".phl[2063]" 0;
	setAttr ".phl[2064]" 0;
	setAttr ".phl[2065]" 0;
	setAttr ".phl[2066]" 0;
	setAttr ".phl[2067]" 0;
	setAttr ".phl[2068]" 0;
	setAttr ".phl[2069]" 0;
	setAttr ".phl[2070]" 0;
	setAttr ".phl[2071]" 0;
	setAttr ".phl[2072]" 0;
	setAttr ".phl[2073]" 0;
	setAttr ".phl[2074]" 0;
	setAttr ".phl[2075]" 0;
	setAttr ".phl[2076]" 0;
	setAttr ".phl[2077]" 0;
	setAttr ".phl[2078]" 0;
	setAttr ".phl[2079]" 0;
	setAttr ".phl[2080]" 0;
	setAttr ".phl[2081]" 0;
	setAttr ".phl[2082]" 0;
	setAttr ".phl[2083]" 0;
	setAttr ".phl[2084]" 0;
	setAttr ".phl[2085]" 0;
	setAttr ".phl[2086]" 0;
	setAttr ".phl[2087]" 0;
	setAttr ".phl[2088]" 0;
	setAttr ".phl[2089]" 0;
	setAttr ".phl[2090]" 0;
	setAttr ".phl[2091]" 0;
	setAttr ".phl[2092]" 0;
	setAttr ".phl[2093]" 0;
	setAttr ".phl[2094]" 0;
	setAttr ".phl[2095]" 0;
	setAttr ".phl[2096]" 0;
	setAttr ".phl[2097]" 0;
	setAttr ".phl[2098]" 0;
	setAttr ".phl[2099]" 0;
	setAttr ".phl[2100]" 0;
	setAttr ".phl[2101]" 0;
	setAttr ".phl[2102]" 0;
	setAttr ".phl[2103]" 0;
	setAttr ".phl[2104]" 0;
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 3
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[1867]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[1868]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[1869]" ""
		"D_RN" 447
		2 "|D_:Entity" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "translateX" " -av"
		2 "|D_:Entity" "translateY" " -av"
		2 "|D_:Entity" "translateZ" " -av"
		2 "|D_:Entity" "rotate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity" "rotateX" " -av"
		2 "|D_:Entity" "rotateY" " -av"
		2 "|D_:Entity" "rotateZ" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity|D_:DiverGlobal" "translateX" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translateY" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "translateZ" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotate" " -type \"double3\" 0 0 0"
		2 "|D_:Entity|D_:DiverGlobal" "rotateX" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotateY" " -av"
		2 "|D_:Entity|D_:DiverGlobal" "rotateZ" " -av"
		2 "|D_:Diver|D_:DiverShape" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShape" "intermediateObject" " 0"
		2 "|D_:Diver|D_:DiverShapeOrig" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShapeOrig" "intermediateObject" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotate" " -type \"double3\" 0 0 -29.654061"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.526539"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -38.280067"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -44.659082"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" -4.493922 -0.794832 -23.031843"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -34.510714"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -11.893513 -1.755435 -7.719601"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" 0 0 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -27.828372"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.329396"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 0 0 -48.302668"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotate" " -type \"double3\" 0 0 -40.679342"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -16.176127"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -46.14001"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" -12.588356 -15.891604 -1.571825"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotate" " -type \"double3\" 0.0399682 0.102002 -0.328974"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translate" " -type \"double3\" 2.660671 26.91789 39.614157"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotate" " -type \"double3\" -64.538252 5.216297 -13.361557"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translate" " -type \"double3\" -2.737355 42.409751 36.473635"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotate" " -type \"double3\" -71.194103 0.279726 14.855081"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 32.279063 70.715487 7.304526"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -27.884308 70.715487 -10.822975"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" 0 -5.546422 -0.606095"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" -83.003409 3.956485 1.309488"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translate" " -type \"double3\" 0 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateX" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateY" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateZ" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" 14.487638 3.921283 0.944695"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleX" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleY" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleZ" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotate" " -type \"double3\" 9.493116 4.553871 0.885768"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translate" " -type \"double3\" 0.00256438 -0.0454696 0.00556348"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotate" " -type \"double3\" -0.701781 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translate" " -type \"double3\" 0.00438577 0.779007 -0.00780013"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translate" " -type \"double3\" -0.000464093 0.69785 -0.013092"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotate" " -type \"double3\" 19.070604 5.852263 -0.979487"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"Mask" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotate" " -type \"double3\" -13.768606 -49.871913 8.9968"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotate" " -type \"double3\" 0 -14.533183 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotate" " -type \"double3\" -16.385946 3.202822 -12.252089"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotate" " -type \"double3\" -6.997065 64.973708 -13.65116"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" 0 34.239669 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotate" " -type \"double3\" -47.378075 7.630678 2.918488"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotate" " -type \"double3\" -6.17895 7.279812 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "scaleZ" " -k 0"
		2 "D_:leftControl" "visibility" " 1"
		2 "D_:joints" "displayType" " 0"
		2 "D_:joints" "visibility" " 0"
		2 "D_:geometry" "displayType" " 2"
		2 "D_:geometry" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:skinCluster1" "nodeState" " 0"
		5 4 "D_RN" "|D_:Entity.translateX" "D_RN.placeHolderList[1870]" ""
		5 4 "D_RN" "|D_:Entity.translateY" "D_RN.placeHolderList[1871]" ""
		5 4 "D_RN" "|D_:Entity.translateZ" "D_RN.placeHolderList[1872]" ""
		5 4 "D_RN" "|D_:Entity.rotateX" "D_RN.placeHolderList[1873]" ""
		5 4 "D_RN" "|D_:Entity.rotateY" "D_RN.placeHolderList[1874]" ""
		5 4 "D_RN" "|D_:Entity.rotateZ" "D_RN.placeHolderList[1875]" ""
		5 4 "D_RN" "|D_:Entity.visibility" "D_RN.placeHolderList[1876]" ""
		5 4 "D_RN" "|D_:Entity.scaleX" "D_RN.placeHolderList[1877]" ""
		5 4 "D_RN" "|D_:Entity.scaleY" "D_RN.placeHolderList[1878]" ""
		5 4 "D_RN" "|D_:Entity.scaleZ" "D_RN.placeHolderList[1879]" ""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateX" "D_RN.placeHolderList[1880]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateY" "D_RN.placeHolderList[1881]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[1882]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateX" "D_RN.placeHolderList[1883]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateY" "D_RN.placeHolderList[1884]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateZ" "D_RN.placeHolderList[1885]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleX" "D_RN.placeHolderList[1886]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleY" "D_RN.placeHolderList[1887]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleZ" "D_RN.placeHolderList[1888]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.visibility" "D_RN.placeHolderList[1889]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[1890]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[1891]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[1892]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[1893]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[1894]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[1895]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[1896]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.visibility" 
		"D_RN.placeHolderList[1897]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[1898]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[1899]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[1900]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[1901]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[1902]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[1903]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[1904]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.visibility" 
		"D_RN.placeHolderList[1905]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[1906]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[1907]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[1908]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[1909]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[1910]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[1911]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[1912]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.visibility" 
		"D_RN.placeHolderList[1913]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[1914]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[1915]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[1916]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[1917]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[1918]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[1919]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[1920]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.visibility" 
		"D_RN.placeHolderList[1921]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[1922]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[1923]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[1924]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[1925]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[1926]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[1927]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[1928]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[1929]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[1930]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[1931]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[1932]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[1933]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[1934]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[1935]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[1936]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[1937]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[1938]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[1939]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[1940]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[1941]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[1942]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[1943]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[1944]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[1945]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[1946]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[1947]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[1948]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[1949]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[1950]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[1951]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[1952]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[1953]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[1954]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[1955]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[1956]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[1957]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[1958]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[1959]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[1960]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[1961]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[1962]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[1963]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[1964]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[1965]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[1966]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[1967]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[1968]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[1969]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[1970]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[1971]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[1972]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[1973]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[1974]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[1975]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[1976]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[1977]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[1978]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[1979]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[1980]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[1981]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[1982]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[1983]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[1984]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[1985]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[1986]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[1987]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[1988]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[1989]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[1990]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[1991]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[1992]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[1993]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[1994]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[1995]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[1996]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[1997]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[1998]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[1999]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[2000]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[2001]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[2002]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[2003]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[2004]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[2005]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[2006]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[2007]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[2008]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[2009]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[2010]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[2011]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[2012]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[2013]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[2014]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[2015]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[2016]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[2017]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[2018]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleX" 
		"D_RN.placeHolderList[2019]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleY" 
		"D_RN.placeHolderList[2020]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleZ" 
		"D_RN.placeHolderList[2021]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[2022]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[2023]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[2024]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[2025]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[2026]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[2027]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[2028]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[2029]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[2030]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2031]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[2032]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[2033]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[2034]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[2035]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[2036]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[2037]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2038]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[2039]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[2040]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[2041]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[2042]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateX" 
		"D_RN.placeHolderList[2043]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateY" 
		"D_RN.placeHolderList[2044]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateZ" 
		"D_RN.placeHolderList[2045]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleX" 
		"D_RN.placeHolderList[2046]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleY" 
		"D_RN.placeHolderList[2047]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleZ" 
		"D_RN.placeHolderList[2048]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[2049]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[2050]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[2051]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[2052]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateX" 
		"D_RN.placeHolderList[2053]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateY" 
		"D_RN.placeHolderList[2054]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateZ" 
		"D_RN.placeHolderList[2055]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[2056]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[2057]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2058]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[2059]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[2060]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2061]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[2062]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleX" 
		"D_RN.placeHolderList[2063]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleY" 
		"D_RN.placeHolderList[2064]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleZ" 
		"D_RN.placeHolderList[2065]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[2066]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[2067]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[2068]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[2069]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[2070]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[2071]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[2072]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[2073]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[2074]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[2075]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[2076]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[2077]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[2078]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2079]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[2080]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[2081]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2082]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[2083]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[2084]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[2085]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[2086]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[2087]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[2088]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[2089]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[2090]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[2091]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[2092]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[2093]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[2094]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[2095]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[2096]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[2097]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[2098]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[2099]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[2100]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[2101]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[2102]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[2103]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[2104]" "";
	setAttr ".ptag" -type "string" "";
lockNode;
createNode mentalrayItemsList -s -n "mentalrayItemsList";
createNode mentalrayGlobals -s -n "mentalrayGlobals";
createNode mentalrayOptions -s -n "miDefaultOptions";
	setAttr ".maxr" 2;
createNode mentalrayFramebuffer -s -n "miDefaultFramebuffer";
createNode script -n "uiConfigurationScriptNode";
	setAttr ".b" -type "string" (
		"// Maya Mel UI Configuration File.\n//\n//  This script is machine generated.  Edit at your own risk.\n//\n//\n\nglobal string $gMainPane;\nif (`paneLayout -exists $gMainPane`) {\n\n\tglobal int $gUseScenePanelConfig;\n\tint    $useSceneConfig = $gUseScenePanelConfig;\n\tint    $menusOkayInPanels = `optionVar -q allowMenusInPanels`;\tint    $nVisPanes = `paneLayout -q -nvp $gMainPane`;\n\tint    $nPanes = 0;\n\tstring $editorName;\n\tstring $panelName;\n\tstring $itemFilterName;\n\tstring $panelConfig;\n\n\t//\n\t//  get current state of the UI\n\t//\n\tsceneUIReplacement -update $gMainPane;\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Top View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"top\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n"
		+ "                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n"
		+ "                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"top\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n"
		+ "            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Side View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"side\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n"
		+ "                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"side\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n"
		+ "            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Front View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"wireframe\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n"
		+ "                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"wireframe\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n"
		+ "            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Persp View\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n"
		+ "                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n"
		+ "            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n"
		+ "            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n"
		+ "            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"Outliner\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n                -showCompounds 1\n"
		+ "                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 1\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 1\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 1\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n"
		+ "            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"graphEditor\" (localizedPanelLabel(\"Graph Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"graphEditor\" -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n"
		+ "                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n"
		+ "                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n"
		+ "                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n"
		+ "                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dopeSheetPanel\" (localizedPanelLabel(\"Dope Sheet\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dopeSheetPanel\" -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n"
		+ "                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n"
		+ "                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n"
		+ "                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n"
		+ "                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"clipEditorPanel\" -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n                -snapValue \"none\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n"
		+ "                -snapValue \"none\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperGraphPanel\" (localizedPanelLabel(\"Hypergraph Hierarchy\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperGraphPanel\" -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n"
		+ "                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n"
		+ "                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperShadePanel\" (localizedPanelLabel(\"Hypershade\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperShadePanel\" -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"visorPanel\" (localizedPanelLabel(\"Visor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\t$panelName = `scriptedPanel -unParent  -type \"visorPanel\" -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"polyTexturePlacementPanel\" (localizedPanelLabel(\"UV Texture Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"polyTexturePlacementPanel\" -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"multiListerPanel\" (localizedPanelLabel(\"Multilister\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"multiListerPanel\" -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"renderWindowPanel\" (localizedPanelLabel(\"Render View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"renderWindowPanel\" -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"blendShapePanel\" (localizedPanelLabel(\"Blend Shape\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\tblendShapePanel -unParent -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tblendShapePanel -edit -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynRelEdPanel\" (localizedPanelLabel(\"Dynamic Relationships\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynRelEdPanel\" -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"devicePanel\" (localizedPanelLabel(\"Devices\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\tdevicePanel -unParent -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tdevicePanel -edit -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"relationshipPanel\" (localizedPanelLabel(\"Relationship Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"relationshipPanel\" -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"referenceEditorPanel\" (localizedPanelLabel(\"Reference Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n"
		+ "\t\t\t$panelName = `scriptedPanel -unParent  -type \"referenceEditorPanel\" -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"componentEditorPanel\" (localizedPanelLabel(\"Component Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"componentEditorPanel\" -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynPaintScriptedPanelType\" (localizedPanelLabel(\"Paint Effects\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynPaintScriptedPanelType\" -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"webBrowserPanel\" (localizedPanelLabel(\"Web Browser\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"webBrowserPanel\" -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"scriptEditorPanel\" (localizedPanelLabel(\"Script Editor\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"single\\\" -ps 1 100 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 500 -size 5000 -divisions 1 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 0 -max 60 -ast 0 -aet 60 ";
	setAttr ".st" 6;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Entity_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTL -n "D_:Entity_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:Entity_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:Entity_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:Entity_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTU -n "D_:Entity_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:Entity_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:Entity_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTL -n "D_:DiverGlobal_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTL -n "D_:DiverGlobal_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 2.7755575615628914e-017 30 0 35 0;
createNode animCurveTL -n "D_:DiverGlobal_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTA -n "D_:DiverGlobal_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 0 30 0 35 0;
createNode animCurveTU -n "D_:DiverGlobal_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTU -n "D_:DiverGlobal_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 30 1 35 1;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -0.606095;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -4.2649879960397659 5 -5.0939135741121353 
		11 -4.6119123367924733 16 -1.191101304430275 21 1.6453012346354736 26 -1.8003410216903348 
		30 -5.4618733140443592 35 -7.1593005426197083 41 1.6453012346354736 60 -4.2655129596276726;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 12 0 22 0 31 0 42 0 60 0;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:HeadControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.0025643799999999998 15 0.0025643799999999998 
		25 0.0025643799999999998 34 0.0025643799999999998 45 0.0025643799999999998 60 0.0025643799999999998;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.0043857699999999998 15 0.0043857699999999998 
		25 0.0043857699999999998 34 0.0043857699999999998 45 0.0043857699999999998 60 0.0043857699999999998;
createNode animCurveTL -n "D_:LShoulderFK_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.000464093 15 -0.000464093 25 -0.000464093 
		34 -0.000464093 45 -0.000464093 60 -0.000464093;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -27.884307763405538 11 -27.884307763405538 
		21 -27.884307763405538 30 -27.884307763405538 41 -27.884307763405538 60 -27.884307763405538;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 32.279062992200437 11 32.279062992200437 
		21 32.279062992200437 30 32.279062992200437 41 32.279062992200437 60 32.279062992200437;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 2.9964341932866638 11 6.5199609137778545 
		21 5.5168826591059066 26 6.0527154634094309 30 7.0780034400083629 36 6.3494301844621539 
		41 6.9019110363397829 46 2.7242831858456187 51 2.3564085262961223 60 2.9959092296987571;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 12.723746268316562 5 23.592926484311018 
		11 37.815154585530692 16 26.706649434006504 21 14.63384464622456 26 23.630184648962736 
		30 34.327720803117309 35 24.258442163114708 41 14.63384464622456 60 50.522608938728091;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -5.5464221608478859 12 -5.5464221608478859 
		22 -5.5464221608478859 31 -5.5464221608478859 42 -5.5464221608478859 60 -5.5464221608478859;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -3.5527136788005009e-015 11 -3.5527136788005009e-015 
		21 -3.5527136788005009e-015 30 -3.5527136788005009e-015 41 -3.5527136788005009e-015 
		60 -3.5527136788005009e-015;
createNode animCurveTL -n "D_:HeadControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.045469599999999999 15 -0.045469599999999999 
		25 -0.045469599999999999 34 -0.045469599999999999 45 -0.045469599999999999 60 -0.045469599999999999;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.779007 15 0.779007 25 0.779007 34 0.779007 
		45 0.779007 60 0.779007;
createNode animCurveTL -n "D_:LShoulderFK_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.69785 15 0.69785 25 0.69785 34 0.69785 
		45 0.69785 60 0.69785;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 70.715486919989587 11 70.715486919989587 
		21 70.715486919989587 30 70.715486919989587 41 70.715486919989587 60 70.715486919989587;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 70.715486919989587 11 70.715486919989587 
		21 70.715486919989587 30 70.715486919989587 41 70.715486919989587 60 70.715486919989587;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 9.6083316879075777 11 15.511024721927662 
		21 37.867503292661404 26 26.211379105271504 30 13.716904804513817 36 20.974196025725689 
		41 41.864415345146583 46 22.387373377004337 51 14.141856928062715 60 40.010733637858976;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 25.985741098956321 5 25.90293834770246 
		11 39.204760910875422 16 42.885648583907901 21 22.47112684910979 26 22.562000472211139 
		30 37.952373532057337 35 44.15868035232495 41 22.47112684910979 60 41.136712699604097;
createNode animCurveTL -n "D_:RootControl_translateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.606095 12 -0.606095 22 -0.606095 31 
		-0.606095 42 -0.606095 60 -0.606095;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -1.1102230246251565e-016 11 -1.1102230246251565e-016 
		21 -1.1102230246251565e-016 30 -1.1102230246251565e-016 41 -1.1102230246251565e-016 
		60 -1.1102230246251565e-016;
createNode animCurveTL -n "D_:HeadControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.00556348 15 0.00556348 25 0.00556348 
		34 0.00556348 45 0.00556348 60 0.00556348;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.0078001299999999997 15 -0.0078001299999999997 
		25 -0.0078001299999999997 34 -0.0078001299999999997 45 -0.0078001299999999997 60 
		-0.0078001299999999997;
createNode animCurveTL -n "D_:LShoulderFK_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.013092 15 -0.013092 25 -0.013092 34 
		-0.013092 45 -0.013092 60 -0.013092;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 26.858369875485081 11 26.858369875485081 
		21 26.858369875485081 30 26.858369875485081 41 26.858369875485081 60 -21.659810076259703;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 26.858369875485081 11 26.858369875485081 
		21 26.858369875485081 30 26.858369875485081 41 26.858369875485081 60 1.6810069718094027;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 25.110059705958012 11 24.242511342285496 
		21 39.547126710264038 26 37.45839722141676 30 26.617900409092876 36 28.951456855314127 
		41 42.279590745145654 46 38.2637109355124 51 33.340452805882435 60 46.162877802789652;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -5.6047902079194305 5 -14.87850912966031 
		11 -100.75630246976348 16 -40.163905516130676 21 -1.5662044552088346 26 -7.7104970925976772 
		30 -85.914790835386356 35 -43.642498137004488 41 -1.5662044552088346 60 -92.142317270095774;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 -7.955963018273553;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -79.539638518575202 12 -79.341068388733916 
		22 -80.076912983350567 31 -73.301766705500725 42 -73.251744433185408 60 -86.03049564358615;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -6.7684253819291431 13 2.0826741877272514 
		23 6.2900892161430404 32 2.0826741877272514 43 6.2900892161430404 60 17.103730177460317;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -15.862762789783957 15 0.10863904635086916 
		25 -0.99957711322552723 34 0.10863904635086916 45 -0.99957711322552723 60 13.799337746107811;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -8.0471042972886497 16 3.0634242215177987 
		26 7.8202872231710865 35 3.0634242215177987 46 7.8202872231710865 60 23.931978630215916;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.701781 15 -0.701781 25 -0.701781 34 
		-0.701781 45 -0.701781 60 -0.701781;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 60.604264828151074 16 -35.111009323726329 
		26 33.538236250033705 35 -35.111009323726329 46 33.538236250033705 60 -38.413995681368256;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 18 0 28 0 37 0 48 0 60 0;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1.720886 20 1.720886 30 8.3110648066146666 
		39 1.720886 50 8.3110648066146666 60 -37.163962980744763;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -11.893513 21 -11.893513 31 -11.893513 
		40 -11.893513 51 -11.893513 60 -11.893513;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -4.493922 21 -4.493922 31 -4.493922 40 
		-4.493922 51 -4.493922 60 -4.493922;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 78.629540283800281 16 36.924226879755381 
		26 -22.590727631162768 35 36.924226879755381 46 -22.590727631162768 60 2.8757973026950432;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 18 0 28 0 37 0 48 0 60 0;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -1.247462 20 -1.247462 30 -18.634547249942123 
		39 -1.247462 50 -18.634547249942123 60 -69.476750898961853;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -12.588356 22 -12.588356 32 -12.588356 
		41 -12.588356 52 -12.588356 60 -12.588356;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.039968200000000002 22 0.039968200000000002 
		32 0.039968200000000002 41 0.039968200000000002 52 0.039968200000000002 60 0.039968200000000002;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -4.9018716395975854 11 -0.28962494316845711 
		21 -95.845643804663837 26 -33.167478349950414 30 -16.139355368366804 36 -36.933795965122769 
		41 -111.88748741408216 46 -37.090943085919299 51 -43.638983144453917 60 -83.218370089439134;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -23.348433280742061 5 -18.367384634317279 
		11 -11.758332169834283 16 -2.8733471987286401 21 3.6429345669340933 26 -5.3358570620317805 
		30 -11.758332169834256 35 -4.6612407539810743 41 3.6429345669340933 60 -0.9456590963667092;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 7.2798116407249278 11 7.2798116407249278 
		21 7.2798116407249278 30 7.2798116407249278 41 7.2798116407249278 60 7.2798116407249385;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 3.178188986514062 12 2.4212647032268864 
		22 4.5991186461770805 31 3.1974933354295931 42 6.1661547677697541 60 3.1781889865140549;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 13 -9.3072559596347233 23 13.262384992817534 
		32 -9.3072559596347233 43 13.262384992817534 60 0;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.0084151300000001161 15 -9.9268699412134325 
		25 17.611365912858957 34 -9.9268699412134325 45 17.611365912858957 60 -1.9114352785588817;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.026536300000000068 16 -16.831642255397739 
		26 15.466780030201772 35 -16.831642255397739 46 15.466780030201772 60 -0.026536300000000075;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 15 0 25 0 34 0 45 0 60 0;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -12.319262581902757 16 -58.057770608758105 
		26 -11.25997773970434 35 -58.057770608758105 46 -11.25997773970434 60 -69.54968125454505;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -32.439137 18 -56.460434203558904 28 
		-24.335800126449083 37 -56.460434203558904 48 -24.335800126449083 60 -10.773467821346996;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -8.107016 20 -8.107016 30 1.1873549496295066 
		39 -8.107016 50 1.1873549496295066 60 4.0808238580019598;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -1.755435 21 -1.755435 31 -1.755435 40 
		-1.755435 51 -1.755435 60 -1.755435;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.794832 21 -0.794832 31 -0.794832 40 
		-0.794832 51 -0.794832 60 -0.794832;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 26.331423297808854 16 -13.861117497241917 
		26 59.650048820216952 35 -13.861117497241917 46 59.650048820216952 60 63.895138519893386;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 17.032479480022982 18 38.157091370305473 
		28 55.020938898954157 37 38.157091370305473 48 55.020938898954157 60 21.067109956317655;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 9.699656 20 9.699656 30 19.751319237226099 
		39 9.699656 50 19.751319237226099 60 -3.1335584812635862;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -15.891604000000001 22 -15.891604000000001 
		32 -15.891604000000001 41 -15.891604000000001 52 -15.891604000000001 60 -15.891604000000001;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.10200200000000001 22 0.10200200000000001 
		32 0.10200200000000001 41 0.10200200000000001 52 0.10200200000000001 60 0.10200200000000001;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 22 0 32 0 41 0 52 0 60 0;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 5.4057900633591123 11 -6.4373782266153894 
		21 7.6356333077705392 26 4.624710493613498 30 3.3048862604113971 36 12.203931682953234 
		41 17.550211356692362 46 0.94157743098591062 51 1.5593513271758253 60 8.5570733124446932;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 11.244804206570331 5 7.9481221614829769 
		11 5.6185470931613164 16 9.4623197908660845 21 13.027382155297827 26 8.7081019705395484 
		30 5.6185470931613057 35 8.8505984098453716 41 13.027382155297827 60 15.22515310007196;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 11 0 21 0 30 0 41 0 60 0;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 3.4800735764342119 12 7.5435954587252354 
		22 -4.4291236046582467 31 10.368291013882837 42 -4.2180568836026815 60 3.4800735764342163;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 13 -2.6237145509470965 23 3.1597884038357789 
		32 -2.6237145509470965 43 3.1597884038357789 60 0;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0.019794600000000152 15 -1.0062287441581943 
		25 -2.374695139318761 34 -1.0062287441581943 45 -2.374695139318761 60 2.2671858594973879;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.034361300000000101 16 -0.42317762107345447 
		26 -2.8185375719021319 35 -0.42317762107345447 46 -2.8185375719021319 60 -0.034361300000000067;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 15 0 25 0 34 0 45 0 60 0;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -56.474803594548057 16 3.6693670930958828 
		26 2.4156251833505498 35 3.6693670930958828 46 2.4156251833505498 60 12.031987391787304;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 18 0 28 0 37 0 48 0 60 0;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -1.123119 20 -1.123119 30 -12.689706337225967 
		39 -1.123119 50 -12.689706337225967 60 -10.931747806621852;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -7.719601 21 -7.719601 31 -7.719601 40 
		-7.719601 51 -7.719601 60 -7.719601;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 21 0 31 0 40 0 51 0 60 0;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -23.031843 21 -23.031843 31 -23.031843 
		40 -23.031843 51 -23.031843 60 -23.031843;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -34.510714 21 -34.510714 31 -34.510714 
		40 -34.510714 51 -34.510714 60 -34.510714;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -29.654061000000002 21 -29.654061000000002 
		31 -29.654061000000002 40 -29.654061000000002 51 -29.654061000000002 60 -29.654061000000002;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -37.526539 21 -37.526539 31 -37.526539 
		40 -37.526539 51 -37.526539 60 -37.526539;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -38.280067 21 -38.280067 31 -38.280067 
		40 -38.280067 51 -38.280067 60 -38.280067;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -44.659082 21 -44.659082 31 -44.659082 
		40 -44.659082 51 -44.659082 60 -44.659082;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 64.154859766663066 16 9.7361165806084333 
		26 -19.046484541404258 35 9.7361165806084333 46 -19.046484541404258 60 -9.8468186975466381;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 0 18 0 28 0 37 0 48 0 60 0;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 7.130177 20 7.130177 30 -0.12527311851531509 
		39 7.130177 50 -0.12527311851531509 60 6.0150771198115978;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -1.571825 22 -1.571825 32 -1.571825 41 
		-1.571825 52 -1.571825 60 -1.571825;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -0.328974 22 -0.328974 32 -0.328974 41 
		-0.328974 52 -0.328974 60 -0.328974;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -16.176127 22 -16.176127 32 -16.176127 
		41 -16.176127 52 -16.176127 60 -16.176127;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -46.140010000000004 22 -46.140010000000004 
		32 -46.140010000000004 41 -46.140010000000004 52 -46.140010000000004 60 -46.140010000000004;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -27.828372 22 -27.828372 32 -27.828372 
		41 -27.828372 52 -27.828372 60 -27.828372;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -37.329396 22 -37.329396 32 -37.329396 
		41 -37.329396 52 -37.329396 60 -37.329396;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -48.302668000000004 22 -48.302668000000004 
		32 -48.302668000000004 41 -48.302668000000004 52 -48.302668000000004 60 -48.302668000000004;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 -40.679342 22 -40.679342 32 -40.679342 
		41 -40.679342 52 -40.679342 60 -40.679342;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -14.389489480457701 11 -2.5322176663129738 
		21 -14.195173019007866 26 -15.423731274724688 30 -7.0346339352965863 36 -11.773841015186608 
		41 -16.188561062288727 46 -5.85552698549869 51 -8.4667008580889078 60 -17.589444894367343;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 5 1 11 1 16 1 21 1 26 1 30 1 35 1 41 
		1 60 1;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 12 1 22 1 31 1 42 1 60 1;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 11 1 21 1 26 1 30 1 36 1 41 1 46 1 
		51 1 60 1;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 5 1 11 1 16 1 21 1 26 1 30 1 35 1 41 
		1 60 1;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1.0000000000000002 11 1.0000000000000002 
		21 1.0000000000000002 30 1.0000000000000002 41 1.0000000000000002 60 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 12 1 22 1 31 1 42 1 60 1;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 11 1 21 1 26 1 30 1 36 1 41 1 46 1 
		51 1 60 1;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 5 1 11 1 16 1 21 1 26 1 30 1 35 1 41 
		1 60 1;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1.0000000000000002 11 1.0000000000000002 
		21 1.0000000000000002 30 1.0000000000000002 41 1.0000000000000002 60 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 12 1 22 1 31 1 42 1 60 1;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 11 1 21 1 26 1 30 1 36 1 41 1 46 1 
		51 1 60 1;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 5 1 11 1 16 1 21 1 26 1 30 1 35 1 41 
		1 60 1;
	setAttr -s 10 ".kot[0:9]"  5 5 5 5 5 5 5 5 
		5 5;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 12 1 22 1 31 1 42 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 13 1 23 1 32 1 43 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 15 1 25 1 34 1 45 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 16 1 26 1 35 1 46 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 15 1 25 1 34 1 45 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 15 1 25 1 34 1 45 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 16 1 26 1 35 1 46 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 18 1 28 1 37 1 48 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 20 1 30 1 39 1 50 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 21 1 31 1 40 1 51 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 15 1 25 1 34 1 45 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 16 1 26 1 35 1 46 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 18 1 28 1 37 1 48 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 20 1 30 1 39 1 50 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 22 1 32 1 41 1 52 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 6 ".ktv[0:5]"  0 1 11 1 21 1 30 1 41 1 60 1;
	setAttr -s 6 ".kot[0:5]"  5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 11 1 21 1 26 1 30 1 36 1 41 1 46 1 
		51 1 60 1;
	setAttr -s 10 ".kot[0:9]"  5 5 5 5 5 5 5 5 
		5 5;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 5 0 11 0 16 0 21 0 26 0 30 0 35 0 41 
		0 60 0;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 11 0 21 0 26 0 30 0 36 0 41 0 46 0 
		51 0 60 0;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 5 0 11 0 16 0 21 0 26 0 30 0 35 0 41 
		0 60 0;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 11 0 21 0 26 0 30 0 36 0 41 0 46 0 
		51 0 60 0;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 56;
select -ne :renderPartition;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 5 ".st";
	setAttr -cb on ".an";
	setAttr -cb on ".pt";
select -ne :renderGlobalsList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
select -ne :defaultShaderList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 5 ".s";
select -ne :postProcessList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 2 ".p";
select -ne :defaultRenderUtilityList1;
	setAttr -s 3 ".u";
select -ne :lightList1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 2 ".ln";
select -ne :defaultTextureList1;
	setAttr -cb on ".cch";
	setAttr -cb on ".ihi";
	setAttr -cb on ".nds";
	setAttr -cb on ".bnm";
	setAttr -s 3 ".tx";
select -ne :initialShadingGroup;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -av -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".mwc";
	setAttr -cb on ".an";
	setAttr -cb on ".il";
	setAttr -cb on ".vo";
	setAttr -cb on ".eo";
	setAttr -cb on ".fo";
	setAttr -cb on ".epo";
	setAttr ".ro" yes;
	setAttr -s 8 ".gn";
	setAttr -cb on ".mimt";
	setAttr -cb on ".miop";
	setAttr -cb on ".mise";
	setAttr -cb on ".mism";
	setAttr -cb on ".mice";
	setAttr -av -cb on ".micc";
	setAttr -cb on ".mica";
	setAttr -cb on ".micw";
	setAttr -cb on ".mirw";
select -ne :initialParticleSE;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".mwc";
	setAttr -cb on ".an";
	setAttr -cb on ".il";
	setAttr -cb on ".vo";
	setAttr -cb on ".eo";
	setAttr -cb on ".fo";
	setAttr -cb on ".epo";
	setAttr ".ro" yes;
	setAttr -cb on ".mimt";
	setAttr -cb on ".miop";
	setAttr -cb on ".mise";
	setAttr -cb on ".mism";
	setAttr -cb on ".mice";
	setAttr -cb on ".micc";
	setAttr -cb on ".mica";
	setAttr -cb on ".micw";
	setAttr -cb on ".mirw";
select -ne :defaultRenderGlobals;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".macc";
	setAttr -k on ".macd";
	setAttr -k on ".macq";
	setAttr -k on ".mcfr" 30;
	setAttr -cb on ".ifg";
	setAttr -k on ".clip";
	setAttr -k on ".edm";
	setAttr -k on ".edl";
	setAttr -cb on ".ren";
	setAttr -av -k on ".esr";
	setAttr -k on ".ors";
	setAttr -cb on ".sdf";
	setAttr -k on ".outf";
	setAttr -k on ".gama";
	setAttr ".fs" 0.5;
	setAttr ".ef" 5;
	setAttr -av -k on ".bfs";
	setAttr -k on ".be";
	setAttr -k on ".fec";
	setAttr -k on ".ofc";
	setAttr -cb on ".ofe";
	setAttr -cb on ".efe";
	setAttr -cb on ".umfn";
	setAttr -cb on ".peie";
	setAttr -cb on ".ifp";
	setAttr -k on ".comp";
	setAttr -k on ".cth";
	setAttr -k on ".soll";
	setAttr -k on ".rd";
	setAttr -k on ".lp";
	setAttr -k on ".sp";
	setAttr -k on ".shs";
	setAttr -k on ".lpr";
	setAttr -cb on ".gv";
	setAttr -cb on ".sv";
	setAttr -k on ".mm";
	setAttr -k on ".npu";
	setAttr -k on ".itf";
	setAttr -k on ".shp";
	setAttr -cb on ".isp";
	setAttr -k on ".uf";
	setAttr -k on ".oi";
	setAttr -k on ".rut";
	setAttr -av -k on ".mbf";
	setAttr -k on ".afp";
	setAttr -k on ".pfb";
	setAttr -cb on ".pfrm";
	setAttr -cb on ".pfom";
	setAttr -av -k on ".bll";
	setAttr -k on ".bls";
	setAttr -k on ".smv";
	setAttr -k on ".ubc";
	setAttr -k on ".mbc";
	setAttr -cb on ".mbt";
	setAttr -k on ".udbx";
	setAttr -k on ".smc";
	setAttr -k on ".kmv";
	setAttr -cb on ".isl";
	setAttr -cb on ".ism";
	setAttr -cb on ".imb";
	setAttr -k on ".rlen";
	setAttr -av -k on ".frts";
	setAttr -k on ".tlwd";
	setAttr -k on ".tlht";
	setAttr -k on ".jfc";
	setAttr -cb on ".rsb";
	setAttr -k on ".ope";
	setAttr -k on ".oppf";
	setAttr -cb on ".hbl";
select -ne :defaultLightSet;
	setAttr -k on ".cch";
	setAttr -k on ".nds";
	setAttr -k on ".mwc";
	setAttr ".ro" yes;
select -ne :hardwareRenderGlobals;
	setAttr -k on ".cch";
	setAttr -k on ".nds";
	setAttr ".ctrs" 256;
	setAttr ".btrs" 512;
	setAttr -k off ".eeaa";
	setAttr -k off ".engm";
	setAttr -k off ".mes";
	setAttr -k off ".emb";
	setAttr -k off ".mbbf";
	setAttr -k off ".mbs";
	setAttr -k off ".trm";
	setAttr -k off ".clmt";
	setAttr -k off ".twa";
	setAttr -k off ".twz";
	setAttr -k on ".hwcc";
	setAttr -k on ".hwdp";
	setAttr -k on ".hwql";
	setAttr ".hwfr" 30;
select -ne :defaultHardwareRenderGlobals;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".rp";
	setAttr -k on ".cai";
	setAttr -k on ".coi";
	setAttr -cb on ".bc";
	setAttr -av -k on ".bcb";
	setAttr -av -k on ".bcg";
	setAttr -av -k on ".bcr";
	setAttr -k on ".ei";
	setAttr -k on ".ex";
	setAttr -k on ".es";
	setAttr -av -k on ".ef";
	setAttr -k on ".bf";
	setAttr -k on ".fii";
	setAttr -k on ".sf";
	setAttr -k on ".gr";
	setAttr -k on ".li";
	setAttr -k on ".ls";
	setAttr -k on ".mb";
	setAttr -k on ".ti";
	setAttr -k on ".txt";
	setAttr -k on ".mpr";
	setAttr -k on ".wzd";
	setAttr ".fn" -type "string" "im";
	setAttr -k on ".if";
	setAttr ".res" -type "string" "ntsc_4d 646 485 1.333";
	setAttr -k on ".as";
	setAttr -k on ".ds";
	setAttr -k on ".lm";
	setAttr -k on ".fir";
	setAttr -k on ".aap";
	setAttr -k on ".gh";
	setAttr -cb on ".sd";
select -ne :ikSystem;
	setAttr -av ".gsn";
	setAttr -s 4 ".sol";
connectAttr "D_:Entity_translateX.o" "D_RN.phl[1870]";
connectAttr "D_:Entity_translateY.o" "D_RN.phl[1871]";
connectAttr "D_:Entity_translateZ.o" "D_RN.phl[1872]";
connectAttr "D_:Entity_rotateX.o" "D_RN.phl[1873]";
connectAttr "D_:Entity_rotateY.o" "D_RN.phl[1874]";
connectAttr "D_:Entity_rotateZ.o" "D_RN.phl[1875]";
connectAttr "D_:Entity_visibility.o" "D_RN.phl[1876]";
connectAttr "D_:Entity_scaleX.o" "D_RN.phl[1877]";
connectAttr "D_:Entity_scaleY.o" "D_RN.phl[1878]";
connectAttr "D_:Entity_scaleZ.o" "D_RN.phl[1879]";
connectAttr "D_:DiverGlobal_translateX.o" "D_RN.phl[1880]";
connectAttr "D_:DiverGlobal_translateY.o" "D_RN.phl[1881]";
connectAttr "D_:DiverGlobal_translateZ.o" "D_RN.phl[1882]";
connectAttr "D_:DiverGlobal_rotateX.o" "D_RN.phl[1883]";
connectAttr "D_:DiverGlobal_rotateY.o" "D_RN.phl[1884]";
connectAttr "D_:DiverGlobal_rotateZ.o" "D_RN.phl[1885]";
connectAttr "D_:DiverGlobal_scaleX.o" "D_RN.phl[1886]";
connectAttr "D_:DiverGlobal_scaleY.o" "D_RN.phl[1887]";
connectAttr "D_:DiverGlobal_scaleZ.o" "D_RN.phl[1888]";
connectAttr "D_:DiverGlobal_visibility.o" "D_RN.phl[1889]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[1890]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[1891]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[1892]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[1893]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[1894]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[1895]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[1896]";
connectAttr "D_:l_mid_2_visibility.o" "D_RN.phl[1897]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[1898]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[1899]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[1900]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[1901]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[1902]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[1903]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[1904]";
connectAttr "D_:l_pink_2_visibility.o" "D_RN.phl[1905]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[1906]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[1907]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[1908]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[1909]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[1910]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[1911]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[1912]";
connectAttr "D_:l_point_2_visibility.o" "D_RN.phl[1913]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[1914]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[1915]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[1916]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[1917]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[1918]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[1919]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[1920]";
connectAttr "D_:l_thumb_2_visibility.o" "D_RN.phl[1921]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[1922]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[1923]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[1924]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[1925]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[1926]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[1927]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[1928]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[1929]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[1930]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[1931]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[1932]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[1933]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[1934]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[1935]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[1936]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[1937]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[1938]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[1939]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[1940]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[1941]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[1942]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[1943]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[1944]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[1945]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[1946]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[1947]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[1948]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[1949]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[1950]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[1951]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[1952]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[1953]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[1954]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[1955]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[1956]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[1957]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[1958]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[1959]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[1960]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[1961]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[1962]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[1963]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[1964]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[1965]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[1966]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[1967]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[1968]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[1969]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[1970]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[1971]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[1972]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[1973]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[1974]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[1975]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[1976]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[1977]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[1978]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[1979]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[1980]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[1981]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[1982]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[1983]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[1984]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[1985]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[1986]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[1987]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[1988]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[1989]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[1990]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[1991]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[1992]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[1993]";
connectAttr "D_:RootControl_translateZ1.o" "D_RN.phl[1994]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[1995]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[1996]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[1997]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[1998]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[1999]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[2000]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[2001]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[2002]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[2003]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[2004]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[2005]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[2006]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[2007]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[2008]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[2009]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[2010]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[2011]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[2012]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[2013]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[2014]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[2015]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[2016]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[2017]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[2018]";
connectAttr "D_:TankControl_scaleX.o" "D_RN.phl[2019]";
connectAttr "D_:TankControl_scaleY.o" "D_RN.phl[2020]";
connectAttr "D_:TankControl_scaleZ.o" "D_RN.phl[2021]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[2022]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[2023]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[2024]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[2025]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[2026]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[2027]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[2028]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[2029]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[2030]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[2031]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[2032]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[2033]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[2034]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[2035]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[2036]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[2037]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[2038]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[2039]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[2040]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[2041]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[2042]";
connectAttr "D_:HeadControl_translateX.o" "D_RN.phl[2043]";
connectAttr "D_:HeadControl_translateY.o" "D_RN.phl[2044]";
connectAttr "D_:HeadControl_translateZ.o" "D_RN.phl[2045]";
connectAttr "D_:HeadControl_scaleX.o" "D_RN.phl[2046]";
connectAttr "D_:HeadControl_scaleY.o" "D_RN.phl[2047]";
connectAttr "D_:HeadControl_scaleZ.o" "D_RN.phl[2048]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[2049]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[2050]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[2051]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[2052]";
connectAttr "D_:LShoulderFK_translateX.o" "D_RN.phl[2053]";
connectAttr "D_:LShoulderFK_translateY.o" "D_RN.phl[2054]";
connectAttr "D_:LShoulderFK_translateZ.o" "D_RN.phl[2055]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[2056]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[2057]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[2058]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[2059]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[2060]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[2061]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[2062]";
connectAttr "D_:LElbowFK_scaleX.o" "D_RN.phl[2063]";
connectAttr "D_:LElbowFK_scaleY.o" "D_RN.phl[2064]";
connectAttr "D_:LElbowFK_scaleZ.o" "D_RN.phl[2065]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[2066]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[2067]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[2068]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[2069]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[2070]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[2071]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[2072]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[2073]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[2074]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[2075]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[2076]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[2077]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[2078]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[2079]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[2080]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[2081]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[2082]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[2083]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[2084]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[2085]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[2086]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[2087]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[2088]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[2089]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[2090]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[2091]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[2092]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[2093]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[2094]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[2095]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[2096]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[2097]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[2098]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[2099]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[2100]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[2101]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[2102]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[2103]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[2104]";
connectAttr ":defaultLightSet.msg" "lightLinker1.lnk[0].llnk";
connectAttr ":initialShadingGroup.msg" "lightLinker1.lnk[0].olnk";
connectAttr ":defaultLightSet.msg" "lightLinker1.lnk[1].llnk";
connectAttr ":initialParticleSE.msg" "lightLinker1.lnk[1].olnk";
connectAttr ":defaultLightSet.msg" "lightLinker1.slnk[0].sllk";
connectAttr ":initialShadingGroup.msg" "lightLinker1.slnk[0].solk";
connectAttr ":defaultLightSet.msg" "lightLinker1.slnk[1].sllk";
connectAttr ":initialParticleSE.msg" "lightLinker1.slnk[1].solk";
connectAttr "layerManager.dli[0]" "defaultLayer.id";
connectAttr "renderLayerManager.rlmi[0]" "defaultRenderLayer.rlid";
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[1867]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[1868]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[1869]";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_knockback.ma
