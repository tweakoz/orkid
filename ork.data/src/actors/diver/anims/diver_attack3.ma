//Maya ASCII 2008 scene
//Name: diver_attack3.ma
//Last modified: Fri, Aug 15, 2008 02:46:28 PM
//Codeset: 1252
file -rdi 1 -ns "D_" -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
file -r -ns "D_" -dr 1 -rfn "D_RN" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
requires maya "2008";
requires "Mayatomr" "9.0.1.2m - 3.6.1.6 ";
requires "COLLADA" "3.05B";
currentUnit -l centimeter -a degree -t ntsc;
fileInfo "application" "maya";
fileInfo "product" "Maya Unlimited 2008";
fileInfo "version" "2008";
fileInfo "cutIdentifier" "200708022245-704165";
fileInfo "osv" "Microsoft Windows XP Service Pack 2 (Build 2600)\n";
createNode transform -s -n "persp";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 80.922056562060163 67.289094187591672 345.10922251592012 ;
	setAttr ".r" -type "double3" -2.1383527291881763 -1070.6000000005149 -2.5186280756618114e-017 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 361.60276955516957;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".tp" -type "double3" 0.13336041401070986 63.330162048339844 1.5007535259840061 ;
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
createNode lightLinker -n "lightLinker1";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 223 ".phl";
	setAttr ".phl[1757]" 0;
	setAttr ".phl[1758]" 0;
	setAttr ".phl[1759]" 0;
	setAttr ".phl[1760]" 0;
	setAttr ".phl[1761]" 0;
	setAttr ".phl[1762]" 0;
	setAttr ".phl[1763]" 0;
	setAttr ".phl[1764]" 0;
	setAttr ".phl[1765]" 0;
	setAttr ".phl[1766]" 0;
	setAttr ".phl[1767]" 0;
	setAttr ".phl[1768]" 0;
	setAttr ".phl[1769]" 0;
	setAttr ".phl[1770]" 0;
	setAttr ".phl[1771]" 0;
	setAttr ".phl[1772]" 0;
	setAttr ".phl[1773]" 0;
	setAttr ".phl[1774]" 0;
	setAttr ".phl[1775]" 0;
	setAttr ".phl[1776]" 0;
	setAttr ".phl[1777]" 0;
	setAttr ".phl[1778]" 0;
	setAttr ".phl[1779]" 0;
	setAttr ".phl[1780]" 0;
	setAttr ".phl[1781]" 0;
	setAttr ".phl[1782]" 0;
	setAttr ".phl[1783]" 0;
	setAttr ".phl[1784]" 0;
	setAttr ".phl[1785]" 0;
	setAttr ".phl[1786]" 0;
	setAttr ".phl[1787]" 0;
	setAttr ".phl[1788]" 0;
	setAttr ".phl[1789]" 0;
	setAttr ".phl[1790]" 0;
	setAttr ".phl[1791]" 0;
	setAttr ".phl[1792]" 0;
	setAttr ".phl[1793]" 0;
	setAttr ".phl[1794]" 0;
	setAttr ".phl[1795]" 0;
	setAttr ".phl[1796]" 0;
	setAttr ".phl[1797]" 0;
	setAttr ".phl[1798]" 0;
	setAttr ".phl[1799]" 0;
	setAttr ".phl[1800]" 0;
	setAttr ".phl[1801]" 0;
	setAttr ".phl[1802]" 0;
	setAttr ".phl[1803]" 0;
	setAttr ".phl[1804]" 0;
	setAttr ".phl[1805]" 0;
	setAttr ".phl[1806]" 0;
	setAttr ".phl[1807]" 0;
	setAttr ".phl[1808]" 0;
	setAttr ".phl[1809]" 0;
	setAttr ".phl[1810]" 0;
	setAttr ".phl[1811]" 0;
	setAttr ".phl[1812]" 0;
	setAttr ".phl[1813]" 0;
	setAttr ".phl[1814]" 0;
	setAttr ".phl[1815]" 0;
	setAttr ".phl[1816]" 0;
	setAttr ".phl[1817]" 0;
	setAttr ".phl[1818]" 0;
	setAttr ".phl[1819]" 0;
	setAttr ".phl[1820]" 0;
	setAttr ".phl[1821]" 0;
	setAttr ".phl[1822]" 0;
	setAttr ".phl[1823]" 0;
	setAttr ".phl[1824]" 0;
	setAttr ".phl[1825]" 0;
	setAttr ".phl[1826]" 0;
	setAttr ".phl[1827]" 0;
	setAttr ".phl[1828]" 0;
	setAttr ".phl[1829]" 0;
	setAttr ".phl[1830]" 0;
	setAttr ".phl[1831]" 0;
	setAttr ".phl[1832]" 0;
	setAttr ".phl[1833]" 0;
	setAttr ".phl[1834]" 0;
	setAttr ".phl[1835]" 0;
	setAttr ".phl[1836]" 0;
	setAttr ".phl[1837]" 0;
	setAttr ".phl[1838]" 0;
	setAttr ".phl[1839]" 0;
	setAttr ".phl[1840]" 0;
	setAttr ".phl[1841]" 0;
	setAttr ".phl[1842]" 0;
	setAttr ".phl[1843]" 0;
	setAttr ".phl[1844]" 0;
	setAttr ".phl[1845]" 0;
	setAttr ".phl[1846]" 0;
	setAttr ".phl[1847]" 0;
	setAttr ".phl[1848]" 0;
	setAttr ".phl[1849]" 0;
	setAttr ".phl[1850]" 0;
	setAttr ".phl[1851]" 0;
	setAttr ".phl[1852]" 0;
	setAttr ".phl[1853]" 0;
	setAttr ".phl[1854]" 0;
	setAttr ".phl[1855]" 0;
	setAttr ".phl[1856]" 0;
	setAttr ".phl[1857]" 0;
	setAttr ".phl[1858]" 0;
	setAttr ".phl[1859]" 0;
	setAttr ".phl[1860]" 0;
	setAttr ".phl[1861]" 0;
	setAttr ".phl[1862]" 0;
	setAttr ".phl[1863]" 0;
	setAttr ".phl[1864]" 0;
	setAttr ".phl[1865]" 0;
	setAttr ".phl[1866]" 0;
	setAttr ".phl[1867]" 0;
	setAttr ".phl[1868]" 0;
	setAttr ".phl[1869]" 0;
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
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 7
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[1750]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[1751]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[1752]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[1753]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[1754]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[1755]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[1756]" ""
		"D_RN" 392
		2 "|D_:Diver|D_:DiverShape" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShape" "intermediateObject" " 0"
		2 "|D_:Diver|D_:DiverShapeOrig" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShapeOrig" "intermediateObject" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder" "rotate" 
		" -type \"double3\" 5.719368 2.378072 -45.126456"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder" "rotateX" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder" "rotateY" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder" "rotateZ" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder" "segmentScaleCompensate" 
		" 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotate" " -type \"double3\" 0 0 -63.839357"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -65.32632"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -54.48984"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -58.159773"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" -3.341901 -3.173265 -56.359169"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -73.966144"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -18.948085 -18.366744 -9.399391"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" 0 0 -39.542557"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle" "rotate" " -type \"double3\" 0 0 0"
		
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle" "segmentScaleCompensate" 
		" 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder" "rotate" 
		" -type \"double3\" -182.272934 -194.74086 133.259662"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder" "rotateX" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder" "rotateY" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder" "rotateZ" 
		" -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder" "segmentScaleCompensate" 
		" 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -51.590749"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -67.298744"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 0 0 -60.240931"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotate" " -type \"double3\" 0 0 -56.936986"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -38.133329"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -74.810674"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" -12.49808 -16.016385 -6.617514"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotate" " -type \"double3\" 4.651465 11.870873 -38.285707"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "translate" " -type \"double3\" 0 17.062296 -3.26722"
		
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "rotate" " -type \"double3\" 30.142098 0 0"
		
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "segmentScaleCompensate" 
		" 1"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translate" " -type \"double3\" 2.996434 0 -2.44855"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotate" " -type \"double3\" 0 25.461881 0"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translate" " -type \"double3\" -4.264988 0 11.090879"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotate" " -type \"double3\" 0 -29.070586 0"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 9.451348 0 0"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -6.915389 0 0"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" 0 -5.546422 -0.606095"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" 1.291091 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" 0.141136 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotate" " -type \"double3\" 0.552161 0.00841513 0.0197946"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateZ" " -av"
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
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translate" " -type \"double3\" 0.00438577 0.779007 -0.00780013"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translate" " -type \"double3\" -0.000464093 0.69785 -0.013092"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotate" " -type \"double3\" 0.682281 -0.0265363 -0.0343613"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"Mask" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotate" " -type \"double3\" 0 0 -45.645013"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotate" " -type \"double3\" 0 -32.439137 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotate" " -type \"double3\" 1.720886 -8.107016 -1.123119"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotate" " -type \"double3\" -8.137897 -12.215538 47.399331"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" 0 30.009168 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotate" " -type \"double3\" -1.247462 9.699656 7.130177"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotate" " -type \"double3\" 0 7.279812 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl" "rotateZ" " -av"
		2 "D_:leftControl" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:skinCluster1" "nodeState" " 0"
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[1757]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[1758]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[1759]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[1760]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[1761]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[1762]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[1763]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.visibility" 
		"D_RN.placeHolderList[1764]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[1765]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[1766]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[1767]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[1768]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[1769]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[1770]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[1771]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.visibility" 
		"D_RN.placeHolderList[1772]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[1773]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[1774]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[1775]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[1776]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[1777]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[1778]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[1779]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.visibility" 
		"D_RN.placeHolderList[1780]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[1781]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[1782]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[1783]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[1784]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[1785]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[1786]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[1787]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.visibility" 
		"D_RN.placeHolderList[1788]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[1789]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[1790]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[1791]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[1792]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[1793]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[1794]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[1795]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[1796]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[1797]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[1798]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[1799]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[1800]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[1801]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[1802]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[1803]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[1804]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[1805]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[1806]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[1807]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[1808]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[1809]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[1810]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[1811]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[1812]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[1813]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[1814]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[1815]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[1816]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[1817]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[1818]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[1819]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[1820]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[1821]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[1822]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[1823]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[1824]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[1825]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[1826]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[1827]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[1828]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[1829]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[1830]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[1831]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[1832]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[1833]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[1834]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[1835]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[1836]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[1837]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[1838]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[1839]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[1840]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[1841]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[1842]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[1843]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[1844]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[1845]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[1846]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[1847]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[1848]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[1849]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[1850]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[1851]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[1852]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[1853]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[1854]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[1855]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[1856]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[1857]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[1858]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[1859]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[1860]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[1861]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[1862]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[1863]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[1864]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[1865]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[1866]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[1867]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[1868]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[1869]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[1870]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[1871]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[1872]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[1873]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[1874]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[1875]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[1876]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[1877]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[1878]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[1879]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[1880]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[1881]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[1882]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[1883]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[1884]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[1885]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleX" 
		"D_RN.placeHolderList[1886]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleY" 
		"D_RN.placeHolderList[1887]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleZ" 
		"D_RN.placeHolderList[1888]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[1889]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[1890]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[1891]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[1892]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[1893]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[1894]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[1895]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[1896]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[1897]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[1898]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[1899]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[1900]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[1901]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[1902]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[1903]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[1904]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[1905]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[1906]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[1907]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[1908]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[1909]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateX" 
		"D_RN.placeHolderList[1910]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateY" 
		"D_RN.placeHolderList[1911]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateZ" 
		"D_RN.placeHolderList[1912]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleX" 
		"D_RN.placeHolderList[1913]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleY" 
		"D_RN.placeHolderList[1914]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleZ" 
		"D_RN.placeHolderList[1915]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[1916]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[1917]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[1918]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[1919]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[1920]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateX" 
		"D_RN.placeHolderList[1921]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateY" 
		"D_RN.placeHolderList[1922]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateZ" 
		"D_RN.placeHolderList[1923]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[1924]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[1925]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[1926]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[1927]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[1928]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[1929]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[1930]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleX" 
		"D_RN.placeHolderList[1931]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleY" 
		"D_RN.placeHolderList[1932]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleZ" 
		"D_RN.placeHolderList[1933]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[1934]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[1935]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[1936]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[1937]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[1938]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[1939]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[1940]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[1941]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[1942]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[1943]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[1944]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[1945]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[1946]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[1947]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[1948]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[1949]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[1950]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[1951]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[1952]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[1953]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[1954]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[1955]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[1956]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[1957]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[1958]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[1959]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[1960]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[1961]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[1962]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[1963]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[1964]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[1965]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[1966]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[1967]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[1968]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[1969]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[1970]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[1971]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[1972]" "";
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
	setAttr ".b" -type "string" "playbackOptions -min 24 -max 48 -ast 24 -aet 48 ";
	setAttr ".st" 6;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 2.9964341932866638 3 2.9964341932866638 
		6 2.9964341932866638 12 2.9964341932866638 14 2.9964341932866638 17 2.9964341932866638 
		23 2.9964341932866638 24 2.9964341932866638 26 2.9964341932866638 29 2.9964341932866638 
		48 2.9964341932866638;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 -3.3324006396428079 6 -3.0979293082018526 
		12 -2.443248271110396 14 -3.1332755013940177 17 0.89485962002420916 23 1.0575875076492225 
		24 0.94843091555263859 26 0.77605774002244587 29 -0.10596504615380897 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTL -n "D_:HeadControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.0004640926137400277 3 -0.0004640926137400277 
		6 -0.2223283191419872 12 -0.20061182822539517 14 -0.12572649328257687 17 -0.001763634085434701 
		23 0.018154976827080917 24 0.017197041819449126 26 -0.0016566471434083935 32 -0.00051594414975139971 
		48 -0.0004640926137400277;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.0043857699112451395 3 0.0043857699112451395 
		6 0.0043857699112451395 12 0.0043857699112451395 14 0.088787809879480928 17 0.2065683583629917 
		23 0 24 -0.01118339460549877 26 0.0018195757564697128 30 0.0048757774650823021 48 
		0.0043857699112451395;
createNode animCurveTL -n "D_:LShoulderFK_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.0025643796042819182 3 0.0025643796042819182 
		6 0.0025643796042819182 12 0.0025643796042819182 14 0.0025643796042819182 17 0.0025643796042819182 
		23 0.0025643796042819182 24 0.0025643796042819182 26 0.0025643796042819182 29 0.0025643796042819182 
		48 0.0025643796042819182;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -6.915388563106255 3 -6.915388563106255 
		6 -6.915388563106255 12 -6.915388563106255 14 -6.915388563106255 17 -6.915388563106255 
		23 -6.915388563106255 24 -6.915388563106255 26 -6.915388563106255 29 -6.915388563106255 
		48 -6.915388563106255;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 9.4513480500108429 3 9.4513480500108429 
		6 9.4513480500108429 12 9.4513480500108429 14 9.4513480500108429 17 9.4513480500108429 
		23 9.4513480500108429 24 9.4513480500108429 26 9.4513480500108429 29 9.4513480500108429 
		48 9.4513480500108429;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -4.2649879960397659 3 -4.2649879960397659 
		6 -4.2649879960397659 12 -4.2649879960397659 14 -4.2649879960397659 17 -4.2649879960397659 
		23 -4.2649879960397659 24 -4.2649879960397659 26 -4.2649879960397659 29 -4.2649879960397659 
		48 -4.2649879960397659;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -5.5464221608478859 3 -5.5464221608478859 
		6 -8.4682659077720537 12 -8.1822703050438186 14 -8.279340096080805 17 -9.8502119919713795 
		23 -9.6075576180299258 24 -9.1393679711726641 26 -8.9548110078392806 29 -16.031954221167204 
		48 -5.5464221608478859;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 -3.5527136788005009e-015 1 -3.5527136788005009e-015 
		3 -3.5527136788005009e-015 8 -3.5527136788005009e-015 9 -3.5527136788005009e-015 
		12 -3.5527136788005009e-015 14 -3.5527136788005009e-015 15 -3.5527136788005009e-015 
		17 -3.5527136788005009e-015 23 -3.5527136788005009e-015 24 -3.5527136788005009e-015 
		26 -3.5527136788005009e-015 27 -3.5527136788005009e-015 28 -3.5527136788005009e-015 
		30 -3.5527136788005009e-015 32 -3.5527136788005009e-015 33 -3.5527136788005009e-015 
		34 -3.5527136788005009e-015 39 -3.5527136788005009e-015 40 -3.5527136788005009e-015 
		42 -3.5527136788005009e-015 43 -3.5527136788005009e-015 45 -3.5527136788005009e-015 
		50 -3.5527136788005009e-015;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTL -n "D_:HeadControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.6978503469639965 3 0.6978503469639965 
		6 -1.2239821206085151 12 -1.0358695168853707 14 -0.3871977093440494 17 0.68659345755725432 
		23 0.8591324717631903 24 0.85083464604204961 26 0.04904068404977923 32 0.77581885877918322 
		48 0.6978503469639965;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.77900741554026665 3 0.77900741554026665 
		6 0.77900741554026665 12 0.77900741554026665 14 0.99421336399870763 17 1.2945268970682531 
		23 0 24 0.014017521495813735 26 0.14029958486561539 30 0.86604334003138672 48 0.77900741554026665;
createNode animCurveTL -n "D_:LShoulderFK_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.045469619462510075 3 -0.045469619462510075 
		6 -0.045469619462510075 12 -0.045469619462510075 14 -0.045469619462510075 17 -0.045469619462510075 
		23 -0.045469619462510075 24 -0.045469619462510075 26 -0.045469619462510075 29 -0.045469619462510075 
		48 -0.045469619462510075;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -2.448549867634112 3 -2.448549867634112 
		6 -2.448549867634112 12 -2.448549867634112 14 -2.448549867634112 17 -2.448549867634112 
		23 -2.448549867634112 24 -2.448549867634112 26 -2.448549867634112 29 -2.448549867634112 
		48 -2.448549867634112;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.60609539862080597 3 -0.60609539862080597 
		6 5.3176594638382007 12 4.7378311126175934 14 3.9162305723886548 17 6.185650427009822 
		23 1.0265989638453747 24 0.55232286637598538 26 -3.432455187419408 29 13.98482058085308 
		48 -0.60609539862080597;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 -1.1102230246251565e-016 1 -1.1102230246251565e-016 
		3 -1.1102230246251565e-016 8 -1.1102230246251565e-016 9 -1.1102230246251565e-016 
		12 -1.1102230246251565e-016 14 -1.1102230246251565e-016 15 -1.1102230246251565e-016 
		17 -1.1102230246251565e-016 23 -1.1102230246251565e-016 24 -1.1102230246251565e-016 
		26 -1.1102230246251565e-016 27 -1.1102230246251565e-016 28 -1.1102230246251565e-016 
		30 -1.1102230246251565e-016 32 -1.1102230246251565e-016 33 -1.1102230246251565e-016 
		34 -1.1102230246251565e-016 39 -1.1102230246251565e-016 40 -1.1102230246251565e-016 
		42 -1.1102230246251565e-016 43 -1.1102230246251565e-016 45 -1.1102230246251565e-016 
		50 -1.1102230246251565e-016;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTL -n "D_:HeadControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.013092045525845527 3 -0.013092045525845527 
		6 2.4431368637240443 12 2.2027165236292214 14 1.3736710638485055 17 0.0012950032916183574 
		23 -0.21922127557076529 24 -0.20861610505258427 26 -0.17308046219469644 32 0.0087532119295020659 
		48 -0.013092045525845527;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.0078001293990800948 3 -0.0078001293990800948 
		6 -0.0078001293990800948 12 -0.0078001293990800948 14 1.0273969053461172 17 2.4719837591152425 
		23 0 24 -0.14057693039395425 26 0.011435102663905336 30 -0.0086716120291154847 48 
		-0.0078001293990800948;
createNode animCurveTL -n "D_:LShoulderFK_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 0 1 0 3 0 8 0 9 0 12 0 14 0 15 0 17 0 
		23 0 24 0 26 0 27 0 28 0 30 0 32 0 33 0 34 0 39 0 40 0 42 0 43 0 45 0 50 0;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.0055634826900936782 3 0.0055634826900936782 
		6 0.0055634826900936782 12 0.0055634826900936782 14 0.0055634826900936782 17 0.0055634826900936782 
		23 0.0055634826900936782 24 0.0055634826900936782 26 0.0055634826900936782 29 0.0055634826900936782 
		48 0.0055634826900936782;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 11.090879111775891 3 11.090879111775891 
		6 11.090879111775891 12 11.090879111775891 14 11.090879111775891 17 11.090879111775891 
		23 11.090879111775891 24 11.090879111775891 26 11.090879111775891 29 11.090879111775891 
		48 11.090879111775891;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1.2910907900570192 3 1.2910907900570192 
		6 6.7107729622276091 12 6.1802842011304104 14 6.7219734322782037 17 7.3539696058171868 
		23 4.1990964166554638 24 3.6953680622914464 26 -4.4211435863121507 29 8.534070020308679 
		48 1.2910907900570192;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.14113566014355422 3 0.69207225963260233 
		6 7.0369168373321251 12 6.3038404774546652 14 6.9930744094161756 17 9.7577028893174234 
		23 4.7581310353064143 24 3.9588989410452466 26 -13.123969676854243 29 27.588998749815474 
		48 0.14113566014355422;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.55216102146285018 3 -0.24681157624483049 
		6 2.581269640112529 12 6.4016188090559787 14 3.2368693547767968 17 -2.5110602779363917 
		23 -1.4477280184387631 24 -1.1638290400849216 26 -6.7076322517107867 29 13.113922601189737 
		48 0.55216102146285018;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.68228131693915994 3 8.1746845323201178 
		6 0.50849453094489327 12 -4.6933145281026372 14 -4.3722505633887838 17 -2.0673394483740553 
		23 -0.22872350041173811 24 -0.022864937559866141 26 -18.879803595875995 29 13.803397349050439 
		48 0.68228131693915994;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -8.1378969009838897 3 -31.227524313582872 
		6 -29.56311394193904 12 0 14 1.3788082175624059 17 1.6602543934210372 23 28.630082369699526 
		24 26.039627185275119 26 -50.059710362608968 33 -11.008899693634804 48 -8.1378969009838897;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 8 ".ktv[0:7]"  0 0 3 0 6 0 12 0 21 0 26 0 31 0 50 0;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -1.2474624697910552 3 -31.404592866445768 
		6 -17.727005055686011 12 11.729910985588104 14 17.703140901344693 17 21.88541190061822 
		23 16.183051812994066 24 13.910158536744369 26 -42.12076444465464 35 -14.077489590795373 
		48 -1.2474624697910552;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 -5.8899350681013249 6 -11.631576586574139 
		12 -28.406816820733198 14 -41.411404248403329 17 -40.535978571891079 21 -9.0689988398072963 
		24 -2.5602398150601613 26 -55.632090537657724 31 -14.338803949628581 48 0;
createNode animCurveTA -n "D_:RElbowFK_rotateX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 8 0 12 0 14 0 17 0 23 0 24 0 26 0 34 
		0 48 0;
	setAttr -s 10 ".kit[8:9]"  3 9;
	setAttr -s 10 ".kot[8:9]"  3 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1.7208864831813682 3 -13.083520364718757 
		6 -12.041866205994069 12 -9.1334029403422203 14 -39.28980141638489 17 9.7595433290119651 
		23 8.3404807787620499 24 7.5194929545537335 26 -37.375046726695686 33 -2.7208377739466028 
		48 1.7208864831813682;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.70178127080170383 3 -0.70178127080170383 
		6 -0.70178127080170383 12 -0.70178127080170383 14 -0.70178127080170383 17 -0.70178127080170383 
		23 -0.70178127080170383 24 -0.70178127080170383 26 -0.70178127080170383 29 -0.70178127080170383 
		48 -0.70178127080170383;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 25.46188064100717 3 25.46188064100717 
		6 25.46188064100717 12 25.46188064100717 14 25.46188064100717 17 25.46188064100717 
		23 25.46188064100717 24 25.46188064100717 26 25.46188064100717 29 25.46188064100717 
		48 25.46188064100717;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 7.2798116407249278 3 7.2798116407249278 
		6 7.2798116407249278 12 7.2798116407249278 14 7.2798116407249278 17 7.2798116407249278 
		23 7.2798116407249278 24 7.2798116407249278 26 7.2798116407249278 29 7.2798116407249278 
		48 7.2798116407249278;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 14.978885445917465 12 13.512723754070652 
		14 4.37825134718552 17 -10.287321339772435 23 -7.0419533119415094 24 -6.0704863202104349 
		26 -4.9672020059200372 29 0.67823535838451587 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 -15.519353879891728 6 -1.3275751344799125 
		12 0.43911083100859044 14 -4.6953687695225286 17 -26.786645867284204 23 -14.425142293411804 
		24 -12.112234968839445 26 2.8156595637252981 29 -0.45651618937639249 48 0;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.0084151252342212438 3 -9.146379277402275 
		6 23.328130629492417 12 37.502119200757399 14 19.168241987262988 17 -11.496386143500516 
		23 -8.4896216589493658 24 -7.3686910127433771 26 -2.2975500837639893 29 -0.23135828804069952 
		48 0.0084151252342212438;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.026536333767640648 3 17.867985781271368 
		6 -3.6805348369846524 12 -10.436197896953781 14 -2.4071787139041412 17 10.545329969105893 
		23 14.46994506981528 24 13.06946332577232 26 -2.2470801530170683 29 -0.53820375120514385 
		48 -0.026536333767640648;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -12.215538151373286 3 -31.252931700017971 
		6 54.955346482415862 12 86.531846854234956 14 47.140523070011923 17 -23.200546724371513 
		23 -13.158777908477457 24 -12.485711239629094 26 -1.2019652039221289 33 -51.678205375077241 
		48 -12.215538151373286;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 8 ".ktv[0:7]"  0 37.514767526546166 3 54.732955703610493 
		6 23.021882453055216 12 22.298635988167046 21 28.194980385960882 26 90.35447811884373 
		31 26.977404537144519 50 45.06635863439579;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 9.6996556887373035 3 7.4201537909504109 
		6 13.534439789097171 12 19.343886293181377 14 9.327960060140013 17 -8.9618226141303285 
		23 -13.201584863432085 24 -10.878348724702626 26 23.538370128796831 35 -2.7818842508266384 
		48 9.6996556887373035;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 -7.9128925062975943 6 32.177323474434608 
		12 37.206154171488151 14 4.7589972145583816 17 -41.428547209885501 21 -28.922400815589892 
		24 -17.17491645420278 26 -2.5591890682529406 31 41.012959596554253 48 0;
createNode animCurveTA -n "D_:RElbowFK_rotateY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 30.009168092349935 8 41.082604720095077 
		12 45.91552123099089 14 37.871587023568871 17 24.077256088453758 23 22.917202170505856 
		24 23.193297610225798 26 98.556519060984144 34 30.009168092349935 48 30.009168092349935;
	setAttr -s 10 ".kit[8:9]"  3 9;
	setAttr -s 10 ".kot[8:9]"  3 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -8.1070161891185535 3 -5.381559025576971 
		6 -5.5733251542707229 12 -6.1087665007493506 14 -17.572680096677246 17 -17.682482750599309 
		23 -15.350858468108086 24 -14.408438518637087 26 -30.439797064327191 33 7.1169827432946491 
		48 -8.1070161891185535;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -29.070586091195047 3 -29.070586091195047 
		6 -29.070586091195047 12 -29.070586091195047 14 -29.070586091195047 17 -29.070586091195047 
		23 -29.070586091195047 24 -29.070586091195047 26 -29.070586091195047 29 -29.070586091195047 
		48 -29.070586091195047;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 -2.298743226019559 12 -2.0737378830285138 
		14 -1.1813001935077878 17 0.28300182942071378 23 0.35821618566827723 24 0.32237944469735946 
		26 0.26378839000356785 29 -0.036018388557778752 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 -0.067410229514656195 6 2.5113267799070798 
		12 2.2726227392411551 14 0.85154630942543985 17 3.8908799799339526 23 2.0768844953480974 
		24 1.7419453632104505 26 0.38941956157408536 29 -0.03964838012931076 48 0;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0.019794606311957463 3 -1.7384311034072828 
		6 2.1358054930136769 12 12.142183871224363 14 7.7798638683514163 17 -1.7163766228557575 
		23 -1.7919926640201518 24 -1.5935115002341933 26 -0.0078304423014717659 29 0.11254093334052898 
		48 0.019794606311957463;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -0.034361262366484416 3 8.2513108740154006 
		6 0.16759423231577583 12 -5.6249975569894746 14 0.24284136882116233 17 10.278152509271717 
		23 2.9424225248897824 24 2.1947163721871008 26 0.66454809956509642 29 0.41149915197691167 
		48 -0.034361262366484416;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 47.399331424850153 3 19.66553651781026 
		6 -19.783636869498771 12 0 14 2.9473567159173859 17 7.6692329248117872 23 35.500858513851583 
		24 38.405302912559989 26 42.968974227803223 33 40.580638869950334 48 47.399331424850153;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 8 ".ktv[0:7]"  0 0 3 0 6 0 12 0 21 0 26 0 31 0 50 0;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 7.1301771055535337 3 -19.611849060951659 
		6 14.609088394037585 12 38.368181032452576 14 30.756524457399536 17 14.601885237418179 
		23 21.076387496623411 24 19.881592587802121 26 24.170685786369873 35 -12.908621174619324 
		48 7.1301771055535337;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -45.645013483740549 3 -24.097526981138639 
		6 -26.916959651929364 12 -32.249014234716086 14 -6.0798397783978748 17 22.919683258782062 
		21 -21.968026535488487 24 -35.13878355281097 26 -41.833129162003111 31 -39.834498573535882 
		48 -45.645013483740549;
createNode animCurveTA -n "D_:RElbowFK_rotateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 8 0 12 0 14 0 17 0 23 0 24 0 26 0 34 
		0 48 0;
	setAttr -s 10 ".kit[8:9]"  3 9;
	setAttr -s 10 ".kot[8:9]"  3 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -1.1231193682533216 3 40.23554883135062 
		6 37.325507978512086 12 29.200213781225887 14 12.384772217323839 17 -6.2932153283355694 
		23 -21.591283632691724 24 -20.16528926926205 26 -13.038911103278592 33 13.518226743044115 
		48 -1.1231193682533216;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HeadControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 1 1 3 1 8 1 9 1 12 1 15 1 17 1 20 1 
		24 1 26 1 27 1 28 1 33 1 42 1 45 1 50 1;
	setAttr -s 17 ".kit[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
	setAttr -s 17 ".kot[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 8 1 12 1 14 1 17 1 23 1 24 1 27 1 30 
		1 32 1 34 1 39 1 40 1 43 1 50 1;
	setAttr -s 15 ".kit[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:TankControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1.0000000000000002 1 1.0000000000000002 
		3 1.0000000000000002 8 1.0000000000000002 9 1.0000000000000002 12 1.0000000000000002 
		14 1.0000000000000002 15 1.0000000000000002 17 1.0000000000000002 23 1.0000000000000002 
		24 1.0000000000000002 26 1.0000000000000002 27 1.0000000000000002 28 1.0000000000000002 
		30 1.0000000000000002 32 1.0000000000000002 33 1.0000000000000002 34 1.0000000000000002 
		39 1.0000000000000002 40 1.0000000000000002 42 1.0000000000000002 43 1.0000000000000002 
		45 1.0000000000000002 50 1.0000000000000002;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HeadControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 1 1 3 1 8 1 9 1 12 1 15 1 17 1 20 1 
		24 1 26 1 27 1 28 1 33 1 42 1 45 1 50 1;
	setAttr -s 17 ".kit[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
	setAttr -s 17 ".kot[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 8 1 12 1 14 1 17 1 23 1 24 1 27 1 30 
		1 32 1 34 1 39 1 40 1 43 1 50 1;
	setAttr -s 15 ".kit[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:TankControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1.0000000000000002 1 1.0000000000000002 
		3 1.0000000000000002 8 1.0000000000000002 9 1.0000000000000002 12 1.0000000000000002 
		14 1.0000000000000002 15 1.0000000000000002 17 1.0000000000000002 23 1.0000000000000002 
		24 1.0000000000000002 26 1.0000000000000002 27 1.0000000000000002 28 1.0000000000000002 
		30 1.0000000000000002 32 1.0000000000000002 33 1.0000000000000002 34 1.0000000000000002 
		39 1.0000000000000002 40 1.0000000000000002 42 1.0000000000000002 43 1.0000000000000002 
		45 1.0000000000000002 50 1.0000000000000002;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HeadControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 17 ".ktv[0:16]"  0 1 1 1 3 1 8 1 9 1 12 1 15 1 17 1 20 1 
		24 1 26 1 27 1 28 1 33 1 42 1 45 1 50 1;
	setAttr -s 17 ".kit[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
	setAttr -s 17 ".kot[0:16]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 8 1 12 1 14 1 17 1 23 1 24 1 27 1 30 
		1 32 1 34 1 39 1 40 1 43 1 50 1;
	setAttr -s 15 ".kit[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
	setAttr -s 24 ".kit[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 24 ".kot[0:23]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTU -n "D_:TankControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 3 
		5 5 3;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 32 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 33 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 8 ".ktv[0:7]"  0 1 3 1 6 1 12 1 21 1 26 1 31 1 50 1;
	setAttr -s 8 ".kot[0:7]"  5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 35 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 30 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 21 1 24 1 26 
		1 31 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:RElbowFK_visibility1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 8 1 12 1 14 1 17 1 23 1 24 1 26 1 34 
		1 48 1;
	setAttr -s 10 ".kit[8:9]"  3 9;
	setAttr -s 10 ".kot[0:9]"  5 5 5 5 5 5 5 5 
		3 5;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 33 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 9 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 29 1 48 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 5 5 5 5 5 5 3 
		5 5 3;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 13.9 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 29 0 48 0;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 9 9 9 3 
		9 9 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 4.6514645037620239 3 -18.749197807628825 
		6 -17.102701770063511 12 -12.505424507718381 14 -5.9047531811411718 17 4.5460472140928943 
		23 6.2508469705920158 24 6.1682089636916286 26 5.8925472885279548 37 0 48 4.6514645037620239;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 36 -6.125258946926361 48 0;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 37 0 48 0;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 36 0 48 0;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 37 0 48 0;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		-2.4172816964274655 36 3.7447492349304423 48 0;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		-4.0518344649779543 37 0.57682340605517601 48 0;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -12.498080351386355 3 -39.336187175023355 
		6 -37.447828804954362 12 -32.175233673727476 14 -24.604955395373889 17 -12.618982928442151 
		23 -10.663756199719584 24 -10.758533337065575 26 -11.074688394109522 36 26.121904014150079 
		48 -12.498080351386355;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 11.870872562983752 3 22.223059666244193 
		6 21.494668528470452 12 19.460885308807434 14 16.540823269311257 17 11.917507973994095 
		23 11.163323915044225 24 11.199882031193209 26 11.321831623744682 37 0 48 11.870872562983752;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 36 -9.9932552882157246 48 0;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 37 0 48 0;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 36 0 48 0;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 37 0 48 0;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		-4.4278525109838647 36 -19.116376890405181 48 0;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		-2.3749154658777236 37 0.0015917247923092558 48 0;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -16.016384857458238 3 -13.120461003258193 
		6 -13.324221353609911 12 -13.893152464813753 14 -14.710011469853754 17 -16.003339053636648 
		23 -16.214314716940084 24 -16.204087939198324 26 -16.169973725538849 36 -42.519891840602469 
		48 -16.016384857458238;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -38.285706776879671 3 -46.660258916787178 
		6 -46.071016324793121 12 -44.425757838989071 14 -42.063531225178018 17 -38.323433170739257 
		23 -37.713325052402759 24 -37.742899269663454 26 -37.841552159012011 37 0 48 -38.285706776879671;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -51.590748582374587 3 -105.76063242502967 
		6 -101.94918051330795 12 -91.307004005052178 14 -76.027196934118848 17 -51.834777676166198 
		23 -47.888359274017297 24 -48.079656900104432 26 -89.835430073881383 36 -14.97559803997812 
		48 -51.590748582374587;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -67.298744458395092 3 -96.634601901291617 
		6 -94.570499082509514 12 -88.807197672992515 14 -80.532373917702046 17 -67.430899122074734 
		23 -65.293704683262277 24 -65.397302474063835 26 -101.97410836930875 37 -42.333676012150349 
		48 -67.298744458395092;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -60.240931129090278 3 -121.90874155722138 
		6 -117.56972738043795 12 -105.4545138726298 14 -88.059751452761247 17 -60.518737528365797 
		23 -56.02607552692249 24 -56.243851618508231 26 -110.61349438246789 36 -30.035032255518793 
		48 -60.240931129090278;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -56.936986476047892 3 -79.438201561333329 
		6 -77.854991608327282 12 -73.434419198358952 14 -67.087456581937531 17 -57.038351860196336 
		23 -55.399079208579515 24 -55.478540871705817 26 -83.309489053057078 37 -23.041703642723245 
		48 -56.936986476047892;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -38.133329475338165 3 -87.835981851538122 
		6 -84.338849201832573 12 -74.574301583528154 14 -60.554575505613158 17 -38.357234208530997 
		23 -34.736265589399196 24 -34.911787449830705 26 -75.375136921911647 36 -2.5796717010454944 
		48 -38.133329475338165;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -74.810673852666966 3 -106.40912584431071 
		6 -104.18582441355959 12 -97.978015429501568 14 -89.064977183522601 17 -74.953021241542942 
		23 -72.650990852097692 24 -72.762578857129895 26 -106.47410257796642 37 -40.727635326214454 
		48 -74.810673852666966;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -6.6175135861403929 3 -5.3965862722667559 
		6 -5.4824920452614663 12 -5.7223545520329004 14 -6.06674395511103 17 -6.6120134491032747 
		23 -6.7009612125800304 24 -6.6966495825329178 26 -6.6822669617379997 36 -3.7765113031804414 
		48 -6.6175135861403929;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 -0.49752213032517595 
		17 -1.2655608308156205 23 -0.70564959005518235 24 -0.59503796687420385 26 -0.48689241847824666 
		35 -1.2703305073984699 48 0;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 34 -4.0658800076276016 48 0;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 35 0 48 0;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 34 -0.57463260803829586 48 0;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 6.4508605130583954 
		17 16.409232598997587 23 9.1494363405197312 24 7.7152486280119001 26 6.9904123838880956 
		35 -0.88515405350840337 48 0;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -3.3419010434934679 3 -4.0798082912944569 
		6 -4.0278883352082726 12 -3.8829196082758028 14 -2.8571375564585622 17 -1.2653740255965067 
		23 -2.131786298135383 24 -2.316173807463942 26 -0.16974103783985106 34 -4.7293232700880914 
		48 -3.3419010434934679;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 35 0 48 0;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -18.948084576790443 3 22.203343566012339 
		6 19.307884348847502 12 11.223304184499797 14 -7.6128729594747613 17 -37.150067380501213 
		23 -32.013092647542273 24 -30.260689900235878 26 -28.204673450459442 34 18.746302040739341 
		48 -18.948084576790443;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 13.821293435459177 
		17 35.157606706293826 23 19.603127843036084 24 16.530308514074488 26 13.525997298099018 
		35 -1.3423589436517207 48 0;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 34 -2.409528626273107 48 0;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 35 0 48 0;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 34 -0.75159132680914298 48 0;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 -2.9347961757536307 
		17 -7.4653224045775088 23 -4.1625037423328468 24 -3.5100251030156797 26 0.73048218640599216 
		35 0.33531105603656514 48 0;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -3.173264837453968 3 5.9717679485556099 
		6 5.3283135007624747 12 3.5316869406533051 14 0.45228118062195988 17 -4.4035451503759404 
		23 -4.5072558845793127 24 -4.36383200611448 26 -4.6653362076989531 34 0.70186255546045817 
		48 -3.173264837453968;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0 35 0 48 0;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -18.366743592299766 3 -44.599304589297255 
		6 -42.753553081840053 12 -37.59992295926638 14 -32.393757735342405 17 -24.064090538371545 
		23 -19.684634125493435 24 -19.289646066361996 26 -19.121912572628229 34 -22.366910011077838 
		48 -18.366743592299766;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -39.542557426213627 3 -39.542557426213627 
		6 -39.542557426213627 12 -39.542557426213627 14 -46.269927013535572 17 -56.655153975424049 
		23 -49.084174427703566 24 -47.588511846175642 26 -46.126194879909228 35 6.846456525843907 
		48 -39.542557426213627;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -63.839356559844809 3 -35.168157049760268 
		6 -37.185493813883369 12 -42.81821692665573 14 -62.602836143618966 17 -93.46488243585253 
		23 -82.389543407027688 24 -79.687695456146855 26 -108.28795555874375 34 1.0929785691442266 
		48 -63.839356559844809;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -65.326320103044935 3 -26.648288166695199 
		6 -29.369716538106083 12 -36.968375074728513 14 -67.234226208401225 17 -114.38812171688697 
		23 -95.422837546985079 24 -90.98295603399211 26 -109.3067578906325 35 -35.073307082545966 
		48 -65.326320103044935;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -54.489839590457279 3 -39.526546103327185 
		6 -40.579379698703406 12 -43.519057512614182 14 -67.748724644139571 17 -105.31972966855005 
		23 -83.891789289817453 24 -79.390459171534161 26 -103.01671411212746 34 -9.2177487959337618 
		48 -54.489839590457279;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -58.159773063591629 3 -24.547501800884554 
		6 -26.91249774463946 12 -33.515940230474556 14 -63.328418998450559 17 -109.72589501790239 
		23 -89.293688316958566 24 -84.654810044362051 26 -104.71284360910673 35 -28.352640975314678 
		48 -58.159773063591629;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -56.359168905796714 3 -5.8580014148219446 
		6 -9.4113184560168364 12 -19.332741776254405 14 -45.718130865471615 17 -87.013600057540771 
		23 -77.029913807345437 24 -74.15245187628868 26 -99.008428434349611 34 -4.0576614854765474 
		48 -56.359168905796714;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -73.966144308812432 3 -42.475339916232386 
		6 -44.691067144896699 12 -50.877728011718467 14 -75.356441538946427 17 -113.49635980065869 
		23 -98.238769968553967 24 -94.66017207623149 26 -110.08454401182286 35 -34.986262703783076 
		48 -73.966144308812432;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -9.399391337933455 3 2.1133971830122911 
		6 1.3033448678760056 12 -0.95844923962204209 14 -2.6199479070939304 17 -5.3133368889851189 
		23 -7.9368862531148325 24 -8.2488229077486341 26 -8.4579336898540269 34 -2.5256700675275963 
		48 -9.399391337933455;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		0.14498791794750221 32 0.55959998421041324 48 0;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 -32.439136911498778 3 -32.439136911498778 
		6 -32.439136911498778 12 -32.439136911498778 14 -53.649494185167519 17 -12.635272754941033 
		23 -47.704755710655839 24 -47.978351423924259 26 -83.35242994509494 32 -17.378993827902704 
		48 -32.439136911498778;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 0 3 0 6 0 12 0 14 0 17 0 23 0 24 0 26 
		-0.064226921695693356 32 -0.24789227459631347 48 0;
createNode animCurveTU -n "D_:LElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:LElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:LElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 24 ".ktv[0:23]"  0 1 1 1 3 1 8 1 9 1 12 1 14 1 15 1 17 1 
		23 1 24 1 26 1 27 1 28 1 30 1 32 1 33 1 34 1 39 1 40 1 42 1 43 1 45 1 50 1;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 32 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 34 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 35 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 34 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 35 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 34 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 35 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 34 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:l_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 35 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 36 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 37 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 36 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 37 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 36 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 37 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 36 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 3 1 6 1 12 1 14 1 17 1 23 1 24 1 26 
		1 37 1 48 1;
	setAttr -s 11 ".kot[0:10]"  5 5 5 5 5 5 5 5 
		5 5 5;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 23 0 26 0 29 0 35 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 9;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 9;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 48;
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
	setAttr -s 2 ".sol";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[1757]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[1758]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[1759]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[1760]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[1761]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[1762]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[1763]";
connectAttr "D_:l_mid_2_visibility.o" "D_RN.phl[1764]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[1765]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[1766]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[1767]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[1768]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[1769]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[1770]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[1771]";
connectAttr "D_:l_pink_2_visibility.o" "D_RN.phl[1772]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[1773]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[1774]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[1775]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[1776]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[1777]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[1778]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[1779]";
connectAttr "D_:l_point_2_visibility.o" "D_RN.phl[1780]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[1781]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[1782]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[1783]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[1784]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[1785]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[1786]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[1787]";
connectAttr "D_:l_thumb_2_visibility.o" "D_RN.phl[1788]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[1789]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[1790]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[1791]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[1792]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[1793]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[1794]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[1795]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[1796]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[1797]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[1798]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[1799]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[1800]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[1801]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[1802]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[1803]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[1804]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[1805]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[1806]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[1807]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[1808]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[1809]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[1810]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[1811]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[1812]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[1813]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[1814]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[1815]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[1816]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[1817]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[1818]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[1819]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[1820]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[1821]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[1822]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[1823]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[1824]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[1825]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[1826]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[1827]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[1828]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[1829]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[1830]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[1831]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[1832]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[1833]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[1834]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[1835]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[1836]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[1837]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[1838]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[1839]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[1840]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[1841]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[1842]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[1843]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[1844]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[1845]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[1846]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[1847]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[1848]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[1849]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[1850]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[1851]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[1852]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[1853]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[1854]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[1855]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[1856]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[1857]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[1858]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[1859]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[1860]";
connectAttr "D_:RootControl_translateZ.o" "D_RN.phl[1861]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[1862]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[1863]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[1864]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[1865]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[1866]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[1867]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[1868]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[1869]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[1870]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[1871]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[1872]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[1873]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[1874]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[1875]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[1876]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[1877]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[1878]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[1879]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[1880]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[1881]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[1882]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[1883]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[1884]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[1885]";
connectAttr "D_:TankControl_scaleX.o" "D_RN.phl[1886]";
connectAttr "D_:TankControl_scaleY.o" "D_RN.phl[1887]";
connectAttr "D_:TankControl_scaleZ.o" "D_RN.phl[1888]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[1889]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[1890]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[1891]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[1892]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[1893]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[1894]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[1895]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[1896]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[1897]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[1898]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[1899]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[1900]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[1901]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[1902]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[1903]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[1904]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[1905]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[1906]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[1907]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[1908]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[1909]";
connectAttr "D_:HeadControl_translateX.o" "D_RN.phl[1910]";
connectAttr "D_:HeadControl_translateY.o" "D_RN.phl[1911]";
connectAttr "D_:HeadControl_translateZ.o" "D_RN.phl[1912]";
connectAttr "D_:HeadControl_scaleX.o" "D_RN.phl[1913]";
connectAttr "D_:HeadControl_scaleY.o" "D_RN.phl[1914]";
connectAttr "D_:HeadControl_scaleZ.o" "D_RN.phl[1915]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[1916]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[1917]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[1918]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[1919]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[1920]";
connectAttr "D_:LShoulderFK_translateX.o" "D_RN.phl[1921]";
connectAttr "D_:LShoulderFK_translateY.o" "D_RN.phl[1922]";
connectAttr "D_:LShoulderFK_translateZ.o" "D_RN.phl[1923]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[1924]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[1925]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[1926]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[1927]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[1928]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[1929]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[1930]";
connectAttr "D_:LElbowFK_scaleX.o" "D_RN.phl[1931]";
connectAttr "D_:LElbowFK_scaleY.o" "D_RN.phl[1932]";
connectAttr "D_:LElbowFK_scaleZ.o" "D_RN.phl[1933]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[1934]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[1935]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[1936]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[1937]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[1938]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[1939]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[1940]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[1941]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[1942]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[1943]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[1944]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[1945]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[1946]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[1947]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[1948]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[1949]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[1950]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[1951]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[1952]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[1953]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[1954]";
connectAttr "D_:RElbowFK_rotateX1.o" "D_RN.phl[1955]";
connectAttr "D_:RElbowFK_rotateY1.o" "D_RN.phl[1956]";
connectAttr "D_:RElbowFK_rotateZ1.o" "D_RN.phl[1957]";
connectAttr "D_:RElbowFK_visibility1.o" "D_RN.phl[1958]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[1959]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[1960]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[1961]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[1962]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[1963]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[1964]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[1965]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[1966]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[1967]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[1968]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[1969]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[1970]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[1971]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[1972]";
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
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[1750]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[1751]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[1752]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[1753]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[1754]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[1755]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[1756]";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_attack3.ma
