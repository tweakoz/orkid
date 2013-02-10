//Maya ASCII 2008 scene
//Name: diver_death.ma
//Last modified: Wed, Aug 27, 2008 03:31:53 PM
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
	setAttr ".t" -type "double3" 37.194930866739611 73.057264632698804 304.1667089090293 ;
	setAttr ".r" -type "double3" 2.0616472839399962 364.59999999980397 4.9856761586702464e-017 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 337.82599492005039;
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
	setAttr -s 242 ".phl";
	setAttr ".phl[1]" 0;
	setAttr ".phl[2]" 0;
	setAttr ".phl[3]" 0;
	setAttr ".phl[4]" 0;
	setAttr ".phl[5]" 0;
	setAttr ".phl[6]" 0;
	setAttr ".phl[7]" 0;
	setAttr ".phl[8]" 0;
	setAttr ".phl[9]" 0;
	setAttr ".phl[10]" 0;
	setAttr ".phl[11]" 0;
	setAttr ".phl[12]" 0;
	setAttr ".phl[13]" 0;
	setAttr ".phl[14]" 0;
	setAttr ".phl[15]" 0;
	setAttr ".phl[16]" 0;
	setAttr ".phl[17]" 0;
	setAttr ".phl[18]" 0;
	setAttr ".phl[19]" 0;
	setAttr ".phl[20]" 0;
	setAttr ".phl[21]" 0;
	setAttr ".phl[22]" 0;
	setAttr ".phl[23]" 0;
	setAttr ".phl[24]" 0;
	setAttr ".phl[25]" 0;
	setAttr ".phl[26]" 0;
	setAttr ".phl[27]" 0;
	setAttr ".phl[28]" 0;
	setAttr ".phl[29]" 0;
	setAttr ".phl[30]" 0;
	setAttr ".phl[31]" 0;
	setAttr ".phl[32]" 0;
	setAttr ".phl[33]" 0;
	setAttr ".phl[34]" 0;
	setAttr ".phl[35]" 0;
	setAttr ".phl[36]" 0;
	setAttr ".phl[37]" 0;
	setAttr ".phl[38]" 0;
	setAttr ".phl[39]" 0;
	setAttr ".phl[40]" 0;
	setAttr ".phl[41]" 0;
	setAttr ".phl[42]" 0;
	setAttr ".phl[43]" 0;
	setAttr ".phl[44]" 0;
	setAttr ".phl[45]" 0;
	setAttr ".phl[46]" 0;
	setAttr ".phl[47]" 0;
	setAttr ".phl[48]" 0;
	setAttr ".phl[49]" 0;
	setAttr ".phl[50]" 0;
	setAttr ".phl[51]" 0;
	setAttr ".phl[52]" 0;
	setAttr ".phl[53]" 0;
	setAttr ".phl[54]" 0;
	setAttr ".phl[55]" 0;
	setAttr ".phl[56]" 0;
	setAttr ".phl[57]" 0;
	setAttr ".phl[58]" 0;
	setAttr ".phl[59]" 0;
	setAttr ".phl[60]" 0;
	setAttr ".phl[61]" 0;
	setAttr ".phl[62]" 0;
	setAttr ".phl[63]" 0;
	setAttr ".phl[64]" 0;
	setAttr ".phl[65]" 0;
	setAttr ".phl[66]" 0;
	setAttr ".phl[67]" 0;
	setAttr ".phl[68]" 0;
	setAttr ".phl[69]" 0;
	setAttr ".phl[70]" 0;
	setAttr ".phl[71]" 0;
	setAttr ".phl[72]" 0;
	setAttr ".phl[73]" 0;
	setAttr ".phl[74]" 0;
	setAttr ".phl[75]" 0;
	setAttr ".phl[76]" 0;
	setAttr ".phl[77]" 0;
	setAttr ".phl[78]" 0;
	setAttr ".phl[79]" 0;
	setAttr ".phl[80]" 0;
	setAttr ".phl[81]" 0;
	setAttr ".phl[82]" 0;
	setAttr ".phl[83]" 0;
	setAttr ".phl[84]" 0;
	setAttr ".phl[85]" 0;
	setAttr ".phl[86]" 0;
	setAttr ".phl[87]" 0;
	setAttr ".phl[88]" 0;
	setAttr ".phl[89]" 0;
	setAttr ".phl[90]" 0;
	setAttr ".phl[91]" 0;
	setAttr ".phl[92]" 0;
	setAttr ".phl[93]" 0;
	setAttr ".phl[94]" 0;
	setAttr ".phl[95]" 0;
	setAttr ".phl[96]" 0;
	setAttr ".phl[97]" 0;
	setAttr ".phl[98]" 0;
	setAttr ".phl[99]" 0;
	setAttr ".phl[100]" 0;
	setAttr ".phl[101]" 0;
	setAttr ".phl[102]" 0;
	setAttr ".phl[103]" 0;
	setAttr ".phl[104]" 0;
	setAttr ".phl[105]" 0;
	setAttr ".phl[106]" 0;
	setAttr ".phl[107]" 0;
	setAttr ".phl[108]" 0;
	setAttr ".phl[109]" 0;
	setAttr ".phl[110]" 0;
	setAttr ".phl[111]" 0;
	setAttr ".phl[112]" 0;
	setAttr ".phl[113]" 0;
	setAttr ".phl[114]" 0;
	setAttr ".phl[115]" 0;
	setAttr ".phl[116]" 0;
	setAttr ".phl[117]" 0;
	setAttr ".phl[118]" 0;
	setAttr ".phl[119]" 0;
	setAttr ".phl[120]" 0;
	setAttr ".phl[121]" 0;
	setAttr ".phl[122]" 0;
	setAttr ".phl[123]" 0;
	setAttr ".phl[124]" 0;
	setAttr ".phl[125]" 0;
	setAttr ".phl[126]" 0;
	setAttr ".phl[127]" 0;
	setAttr ".phl[128]" 0;
	setAttr ".phl[129]" 0;
	setAttr ".phl[130]" 0;
	setAttr ".phl[131]" 0;
	setAttr ".phl[132]" 0;
	setAttr ".phl[133]" 0;
	setAttr ".phl[134]" 0;
	setAttr ".phl[135]" 0;
	setAttr ".phl[136]" 0;
	setAttr ".phl[137]" 0;
	setAttr ".phl[138]" 0;
	setAttr ".phl[139]" 0;
	setAttr ".phl[140]" 0;
	setAttr ".phl[141]" 0;
	setAttr ".phl[142]" 0;
	setAttr ".phl[143]" 0;
	setAttr ".phl[144]" 0;
	setAttr ".phl[145]" 0;
	setAttr ".phl[146]" 0;
	setAttr ".phl[147]" 0;
	setAttr ".phl[148]" 0;
	setAttr ".phl[149]" 0;
	setAttr ".phl[150]" 0;
	setAttr ".phl[151]" 0;
	setAttr ".phl[152]" 0;
	setAttr ".phl[153]" 0;
	setAttr ".phl[154]" 0;
	setAttr ".phl[155]" 0;
	setAttr ".phl[156]" 0;
	setAttr ".phl[157]" 0;
	setAttr ".phl[158]" 0;
	setAttr ".phl[159]" 0;
	setAttr ".phl[160]" 0;
	setAttr ".phl[161]" 0;
	setAttr ".phl[162]" 0;
	setAttr ".phl[163]" 0;
	setAttr ".phl[164]" 0;
	setAttr ".phl[165]" 0;
	setAttr ".phl[166]" 0;
	setAttr ".phl[167]" 0;
	setAttr ".phl[168]" 0;
	setAttr ".phl[169]" 0;
	setAttr ".phl[170]" 0;
	setAttr ".phl[171]" 0;
	setAttr ".phl[172]" 0;
	setAttr ".phl[173]" 0;
	setAttr ".phl[174]" 0;
	setAttr ".phl[175]" 0;
	setAttr ".phl[176]" 0;
	setAttr ".phl[177]" 0;
	setAttr ".phl[178]" 0;
	setAttr ".phl[179]" 0;
	setAttr ".phl[180]" 0;
	setAttr ".phl[181]" 0;
	setAttr ".phl[182]" 0;
	setAttr ".phl[183]" 0;
	setAttr ".phl[184]" 0;
	setAttr ".phl[185]" 0;
	setAttr ".phl[186]" 0;
	setAttr ".phl[187]" 0;
	setAttr ".phl[188]" 0;
	setAttr ".phl[189]" 0;
	setAttr ".phl[190]" 0;
	setAttr ".phl[191]" 0;
	setAttr ".phl[192]" 0;
	setAttr ".phl[193]" 0;
	setAttr ".phl[194]" 0;
	setAttr ".phl[195]" 0;
	setAttr ".phl[196]" 0;
	setAttr ".phl[197]" 0;
	setAttr ".phl[198]" 0;
	setAttr ".phl[199]" 0;
	setAttr ".phl[200]" 0;
	setAttr ".phl[201]" 0;
	setAttr ".phl[202]" 0;
	setAttr ".phl[203]" 0;
	setAttr ".phl[204]" 0;
	setAttr ".phl[205]" 0;
	setAttr ".phl[206]" 0;
	setAttr ".phl[207]" 0;
	setAttr ".phl[208]" 0;
	setAttr ".phl[209]" 0;
	setAttr ".phl[210]" 0;
	setAttr ".phl[211]" 0;
	setAttr ".phl[212]" 0;
	setAttr ".phl[213]" 0;
	setAttr ".phl[214]" 0;
	setAttr ".phl[215]" 0;
	setAttr ".phl[216]" 0;
	setAttr ".phl[217]" 0;
	setAttr ".phl[218]" 0;
	setAttr ".phl[219]" 0;
	setAttr ".phl[220]" 0;
	setAttr ".phl[221]" 0;
	setAttr ".phl[222]" 0;
	setAttr ".phl[223]" 0;
	setAttr ".phl[224]" 0;
	setAttr ".phl[225]" 0;
	setAttr ".phl[226]" 0;
	setAttr ".phl[227]" 0;
	setAttr ".phl[228]" 0;
	setAttr ".phl[229]" 0;
	setAttr ".phl[230]" 0;
	setAttr ".phl[231]" 0;
	setAttr ".phl[232]" 0;
	setAttr ".phl[233]" 0;
	setAttr ".phl[234]" 0;
	setAttr ".phl[235]" 0;
	setAttr ".phl[236]" 0;
	setAttr ".phl[237]" 0;
	setAttr ".phl[238]" 0;
	setAttr ".phl[239]" 0;
	setAttr ".phl[240]" 0;
	setAttr ".phl[241]" 0;
	setAttr ".phl[242]" 0;
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 0
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
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateY" " -av"
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
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateY" " -av"
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
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -51.590749"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"segmentScaleCompensate" " 1"
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
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -38.133329"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -74.810674"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"segmentScaleCompensate" " 1"
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
		2 "|D_:controlcurvesGRP|D_:R_Foot" "BallRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 9.451348 0 0"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
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
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translate" " -type \"double3\" 0 0 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateX" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateY" " -av -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateZ" " -av -k 0"
		
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
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateZ" " -av"
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
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateX" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateY" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"translateZ" " -av -k 0"
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
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateX" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateY" " -av -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateZ" " -av -k 0"
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
		2 "D_:joints" "displayType" " 0"
		2 "D_:joints" "visibility" " 0"
		2 "D_:geometry" "displayType" " 2"
		2 "D_:geometry" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:skinCluster1" "nodeState" " 0"
		5 4 "D_RN" "|D_:Entity.translateX" "D_RN.placeHolderList[1]" ""
		5 4 "D_RN" "|D_:Entity.translateY" "D_RN.placeHolderList[2]" ""
		5 4 "D_RN" "|D_:Entity.translateZ" "D_RN.placeHolderList[3]" ""
		5 4 "D_RN" "|D_:Entity.rotateX" "D_RN.placeHolderList[4]" ""
		5 4 "D_RN" "|D_:Entity.rotateY" "D_RN.placeHolderList[5]" ""
		5 4 "D_RN" "|D_:Entity.rotateZ" "D_RN.placeHolderList[6]" ""
		5 4 "D_RN" "|D_:Entity.visibility" "D_RN.placeHolderList[7]" ""
		5 4 "D_RN" "|D_:Entity.scaleX" "D_RN.placeHolderList[8]" ""
		5 4 "D_RN" "|D_:Entity.scaleY" "D_RN.placeHolderList[9]" ""
		5 4 "D_RN" "|D_:Entity.scaleZ" "D_RN.placeHolderList[10]" ""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateX" "D_RN.placeHolderList[11]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateY" "D_RN.placeHolderList[12]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[13]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateX" "D_RN.placeHolderList[14]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateY" "D_RN.placeHolderList[15]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateZ" "D_RN.placeHolderList[16]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleX" "D_RN.placeHolderList[17]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleY" "D_RN.placeHolderList[18]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleZ" "D_RN.placeHolderList[19]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.visibility" "D_RN.placeHolderList[20]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[21]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[22]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[23]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[24]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[25]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[26]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[27]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.visibility" 
		"D_RN.placeHolderList[28]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[29]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[30]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[31]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[32]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[33]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[34]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[35]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.visibility" 
		"D_RN.placeHolderList[36]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[37]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[38]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[39]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[40]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[41]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[42]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[43]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.visibility" 
		"D_RN.placeHolderList[44]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[45]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[46]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[47]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[48]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[49]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[50]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[51]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.visibility" 
		"D_RN.placeHolderList[52]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[53]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[54]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[55]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[56]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[57]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[58]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[59]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[60]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[61]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[62]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[63]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[64]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[65]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[66]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[67]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[68]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[69]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[70]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[71]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[72]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[73]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[74]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[75]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[76]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[77]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[78]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[79]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[80]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[81]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[82]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[83]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[84]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[85]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[86]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[87]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[88]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[89]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[90]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[91]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[92]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[93]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[94]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[95]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[96]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[97]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[98]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[99]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[100]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[101]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[102]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[103]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[104]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[105]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[106]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[107]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[108]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[109]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[110]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[111]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[112]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[113]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[114]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[115]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[116]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[117]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[118]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[119]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[120]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[121]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[122]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionX" 
		"D_RN.placeHolderList[123]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionY" 
		"D_RN.placeHolderList[124]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localPositionZ" 
		"D_RN.placeHolderList[125]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleX" 
		"D_RN.placeHolderList[126]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleY" 
		"D_RN.placeHolderList[127]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee|D_:R_KneeShape.localScaleZ" 
		"D_RN.placeHolderList[128]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[129]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[130]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[131]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[132]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[133]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[134]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[135]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[136]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[137]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[138]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[139]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[140]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[141]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[142]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[143]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[144]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[145]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[146]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[147]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[148]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[149]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[150]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[151]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[152]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[153]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[154]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[155]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleX" 
		"D_RN.placeHolderList[156]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleY" 
		"D_RN.placeHolderList[157]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.scaleZ" 
		"D_RN.placeHolderList[158]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[159]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[160]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[161]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[162]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[163]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[164]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[165]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[166]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[167]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[168]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[169]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[170]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[171]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[172]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[173]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[174]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[175]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[176]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[177]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[178]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[179]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateX" 
		"D_RN.placeHolderList[180]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateY" 
		"D_RN.placeHolderList[181]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.translateZ" 
		"D_RN.placeHolderList[182]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleX" 
		"D_RN.placeHolderList[183]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleY" 
		"D_RN.placeHolderList[184]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.scaleZ" 
		"D_RN.placeHolderList[185]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[186]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[187]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[188]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[189]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[190]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateX" 
		"D_RN.placeHolderList[191]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateY" 
		"D_RN.placeHolderList[192]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.translateZ" 
		"D_RN.placeHolderList[193]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[194]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[195]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[196]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[197]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[198]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[199]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[200]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleX" 
		"D_RN.placeHolderList[201]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleY" 
		"D_RN.placeHolderList[202]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.scaleZ" 
		"D_RN.placeHolderList[203]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[204]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[205]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[206]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[207]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[208]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[209]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[210]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[211]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[212]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[213]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[214]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[215]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[216]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[217]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[218]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[219]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[220]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[221]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[222]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[223]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[224]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[225]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[226]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[227]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[228]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[229]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[230]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[231]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[232]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[233]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[234]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[235]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[236]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[237]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[238]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[239]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[240]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[241]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[242]" "";
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
		+ "                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n"
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
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"vertical2\\\" -ps 1 20 100 -ps 2 80 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Outliner\")) \n\t\t\t\t\t\"outlinerPanel\"\n\t\t\t\t\t\"$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\\\"Outliner\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\noutlinerEditor -e \\n    -showShapes 0\\n    -showAttributes 0\\n    -showConnected 0\\n    -showAnimCurvesOnly 0\\n    -showMuteInfo 0\\n    -autoExpand 0\\n    -showDagOnly 1\\n    -ignoreDagHierarchy 0\\n    -expandConnections 0\\n    -showUnitlessCurves 1\\n    -showCompounds 1\\n    -showLeafs 1\\n    -showNumericAttrsOnly 0\\n    -highlightActive 1\\n    -autoSelectNewObjects 0\\n    -doNotSelectNewObjects 0\\n    -dropIsParent 1\\n    -transmitFilters 0\\n    -setFilter \\\"defaultSetFilter\\\" \\n    -showSetMembers 1\\n    -allowMultiSelection 1\\n    -alwaysToggleSelect 0\\n    -directSelect 0\\n    -displayMode \\\"DAG\\\" \\n    -expandObjects 0\\n    -setsIgnoreFilters 1\\n    -editAttrName 0\\n    -showAttrValues 0\\n    -highlightSecondary 0\\n    -showUVAttrsOnly 0\\n    -showTextureNodesOnly 0\\n    -attrAlphaOrder \\\"default\\\" \\n    -sortOrder \\\"none\\\" \\n    -longNames 0\\n    -niceNames 1\\n    -showNamespace 1\\n    $editorName\"\n"
		+ "\t\t\t\t\t\"outlinerPanel -edit -l (localizedPanelLabel(\\\"Outliner\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\noutlinerEditor -e \\n    -showShapes 0\\n    -showAttributes 0\\n    -showConnected 0\\n    -showAnimCurvesOnly 0\\n    -showMuteInfo 0\\n    -autoExpand 0\\n    -showDagOnly 1\\n    -ignoreDagHierarchy 0\\n    -expandConnections 0\\n    -showUnitlessCurves 1\\n    -showCompounds 1\\n    -showLeafs 1\\n    -showNumericAttrsOnly 0\\n    -highlightActive 1\\n    -autoSelectNewObjects 0\\n    -doNotSelectNewObjects 0\\n    -dropIsParent 1\\n    -transmitFilters 0\\n    -setFilter \\\"defaultSetFilter\\\" \\n    -showSetMembers 1\\n    -allowMultiSelection 1\\n    -alwaysToggleSelect 0\\n    -directSelect 0\\n    -displayMode \\\"DAG\\\" \\n    -expandObjects 0\\n    -setsIgnoreFilters 1\\n    -editAttrName 0\\n    -showAttrValues 0\\n    -highlightSecondary 0\\n    -showUVAttrsOnly 0\\n    -showTextureNodesOnly 0\\n    -attrAlphaOrder \\\"default\\\" \\n    -sortOrder \\\"none\\\" \\n    -longNames 0\\n    -niceNames 1\\n    -showNamespace 1\\n    $editorName\"\n"
		+ "\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 500 -size 5000 -divisions 1 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 0 -max 80 -ast 0 -aet 80 ";
	setAttr ".st" 6;
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
	setAttr -s 15 ".ktv[0:14]"  0 -4.2649879960397659 3 -4.2649879960397659 
		5 -4.2649879960397659 8 -4.2649879960397659 10 -4.2649879960397659 14 -4.2649879960397659 
		23 -4.2649879960397659 31 -4.2649879960397659 37 -4.2649879960397659 42 -4.2366366499435646 
		49 2.9329621506256558 53 0.17026614182016564 58 2.9329621506256558 62 2.3818226389570509 
		66 2.9329621506256558;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 2.570958799865275 6 4.8186138897561719 
		14 -3.0175012971916497 18 -7.5370058225813299 23 -7.6218855213669485 31 -6.0852533607687347 
		37 -3.9686287014659301 46 0 50 0 55 0 59 0 63 0;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 0 21 0 23 0 27 
		0 30 0 37 0 40 0 41 0 60 0;
createNode animCurveTL -n "D_:HeadControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 0 21 0 23 0 27 
		0 30 0 37 0 40 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.0025643796042819182 3 0.0025643796042819182 
		14 0.0025643796042819182 23 0.0025643796042819182 31 0.0025643796042819182 37 0.0025643796042819182 
		42 0.0025643799999999998 45 0.0025643799999999998 49 0.0025643799999999998 53 0.074943845691361588 
		58 0.0025643799999999998 62 0.0201378648930092 66 0.0025643799999999998;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.0043857699112451395 3 0.0043857699112451395 
		14 0.0043857699112451395 23 0.0043857699112451395 31 0.0043857699112451395 37 0.0043857699112451395 
		42 0.0043857699999999998 47 0.0043857699999999998 51 0.0043857699999999998 55 0.0043857699999999998 
		60 0.0043857699999999998 64 0.0043857699999999998 68 0.0043857699999999998;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:LShoulderFK_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 9.6398884921149373e-005 
		21 0 23 -0.00086758995264350715 27 -0.00039169406562842093 30 0 37 2.2298141813211126e-005 
		40 3.98341756740479e-006 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.0004640926137400277 3 -0.0004640926137400277 
		14 -0.0004640926137400277 23 -0.0004640926137400277 31 -0.0004640926137400277 37 
		-0.0004640926137400277 42 -0.000464093 45 -0.000464093 49 -0.000464093 53 -0.000464093 
		58 -0.000464093 62 -0.000464093 66 -0.000464093;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 -6.915388563106255 3 -10.361483503993909 
		14 -10.361483503993909 23 -10.361483503993909 31 -10.361483503993909 37 -10.361483503993909 
		42 -27.884307763405538 45 -27.884307763405538 49 -47.979899062356225 58 -53.146302994186925 
		62 -53.488197359491394 66 -53.146302994186925;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 9.4513480500108429 3 9.4513480500108429 
		14 9.4513480500108429 23 16.935618064394315 31 22.168625116003071 37 28.489028969382524 
		39 30.580490526201789 42 32.893980678211001 43 32.279062992200437 47 22.594109437639585 
		51 26.311088730336238 56 22.594109437639585 60 24.130857362698979 64 22.594109437639585;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 2.9964341932866638 3 2.9964341932866638 
		14 2.9964341932866638 23 -2.9143273822617282 27 -2.5107734460388946 32 -4.4217184274303474 
		35 -0.25676055315424051 39 0.44145365521609348 47 2.9959092296987571 51 3.2398709200391274 
		56 2.9959092296987571 60 3.8431751575801378 64 2.9959092296987571;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 5 3.0987438686990925 8 1.9545829080011412 
		10 0 14 0 23 0 31 0 37 0 42 8.2539292407924982 49 0.13987482996382994 53 2.5395311654873542 
		58 1.1716157713320143 62 3.0051349117401926 66 2.995084657595287;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -5.5464221608478859 3 -8.9788621991684163 
		6 -12.906587702667249 14 -10.169086553574763 18 -7.9767596452382721 23 -6.4408396933601999 
		31 -4.2719822015213849 37 -2.4623502176498988 46 -31.036513367873908 50 -25.420773616093797 
		55 -31.036513367873908 59 -28.726399901327355 63 -31.036513367873908;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 -3.5527136788005009e-015 3 -3.5527136788005009e-015 
		5 -3.5527136788005009e-015 10 -3.5527136788005009e-015 15 -3.5527136788005009e-015 
		19 -3.5527136788005009e-015 21 -3.5527136788005009e-015 23 -3.5527136788005009e-015 
		27 -3.5527136788005009e-015 30 -3.5527136788005009e-015 37 -3.5527136788005009e-015 
		40 -3.5527136788005009e-015 41 -3.5527136788005009e-015 60 -3.5527136788005009e-015;
createNode animCurveTL -n "D_:HeadControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 0 21 0 23 0 27 
		0 30 0 37 0 40 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.045469619462510075 3 -0.045469619462510075 
		14 -0.045469619462510075 23 -0.045469619462510075 31 -0.045469619462510075 37 -0.045469619462510075 
		42 -0.045469599999999999 45 -0.045469599999999999 49 -0.045469599999999999 53 0.21803612137654435 
		58 -0.045469599999999999 62 0.063969137789151503 66 -0.045469599999999999;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.77900741554026665 3 0.77900741554026665 
		14 0.77900741554026665 23 0.77900741554026665 31 0.77900741554026665 37 0.77900741554026665 
		42 0.779007 47 0.779007 51 0.779007 55 0.779007 60 0.779007 64 0.779007 68 0.779007;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:LShoulderFK_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 0.0014274947274785987 
		21 0 23 -0.012847452797006685 27 -0.0058002873856235222 30 0 37 0.00033019554319472664 
		40 5.8987275555905963e-005 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.6978503469639965 3 0.6978503469639965 
		14 0.6978503469639965 23 0.6978503469639965 31 0.6978503469639965 37 0.6978503469639965 
		42 0.69785 45 0.69785 49 0.69785 53 0.69785 58 0.69785 62 0.69785 66 0.69785;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 0 14 0 23 0 31 0 37 0 42 70.715486919989587 
		45 70.715486919989587 49 70.715486919989587 58 30.015588885700126 62 27.322213473129601 
		66 30.015588885700126;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 0 3 0 14 0 23 10.384210720116435 31 17.950014208016544 
		37 25.529416780268154 39 32.708694517104334 42 70.715486919989587 43 70.715486919989587 
		47 70.715486919989587 51 70.715486919989587 56 70.715486919989587 60 70.715486919989587 
		64 70.715486919989587;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 8.5192618927852131 27 12.634035964550499 
		32 10.67847714678499 35 18.031415553808621 39 25.349797349715026 47 -0.87945984651022258 
		51 1.445769318956267 56 -0.87945984651022258 60 1.6965649795532736 64 3.1321336364758761;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 11.090879111775891 3 11.090879111775891 
		5 14.639189868929449 8 21.80913482405597 10 24.305542269574282 14 24.305542269574282 
		23 24.305542269574282 31 24.305542269574282 37 24.305542269574282 42 37.289284046160624 
		49 41.136712699604097 53 39.702632807459494 58 35.070173551134694 62 35.889625128608209 
		66 35.070173551134694;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTL -n "D_:RootControl_translateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.60609539862080597 3 -0.59325607362217769 
		6 0.66736031933801909 14 11.039823839939913 18 16.061437308330362 23 17.871415330230473 
		31 18.180669669991232 37 10.999605118414404 46 -0.606095 50 -0.606095 55 -0.606095 
		59 -0.606095 63 -0.606095;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 -1.1102230246251565e-016 3 -1.1102230246251565e-016 
		5 -1.1102230246251565e-016 10 -1.1102230246251565e-016 15 -1.1102230246251565e-016 
		19 -1.1102230246251565e-016 21 -1.1102230246251565e-016 23 -1.1102230246251565e-016 
		27 -1.1102230246251565e-016 30 -1.1102230246251565e-016 37 -1.1102230246251565e-016 
		40 -1.1102230246251565e-016 41 -1.1102230246251565e-016 60 -1.1102230246251565e-016;
createNode animCurveTL -n "D_:HeadControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 0 21 0 23 0 27 
		0 30 0 37 0 40 0 41 0 60 0;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.0055634826900936782 3 0.0055634826900936782 
		14 0.0055634826900936782 23 0.0055634826900936782 31 0.0055634826900936782 37 0.0055634826900936782 
		42 0.00556348 45 0.00556348 49 0.00556348 53 1.3636952210414313 58 0.00556348 62 
		0.45534269170346903 66 0.00556348;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.0078001293990800948 3 -0.0078001293990800948 
		14 -0.0078001293990800948 23 -0.0078001293990800948 31 -0.0078001293990800948 37 
		-0.0078001293990800948 42 -0.0078001299999999997 47 -0.0078001299999999997 51 -0.0078001299999999997 
		55 -0.0078001299999999997 60 -0.0078001299999999997 64 -0.0078001299999999997 68 
		-0.0078001299999999997;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:LShoulderFK_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 0 3 0 5 0 10 0 15 0 19 -0.00098804087687659426 
		21 0 23 0.0088923674306018441 27 0.0040146702798645661 30 0 37 -0.00022854490973657081 
		40 -4.0828057995653889e-005 41 0 60 0;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.013092045525845527 3 -0.013092045525845527 
		14 -0.013092045525845527 23 -0.013092045525845527 31 -0.013092045525845527 37 -0.013092045525845527 
		42 -0.013092 45 -0.013092 49 -0.013092 53 -0.013092 58 -0.013092 62 -0.013092 66 
		-0.013092;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 0 14 0 23 0 31 0 37 0 42 -21.659810076259703 
		45 -21.659810076259703 49 6.5186188811778578 58 -1.1584119150679841 62 -1.6664506744064058 
		66 -1.1584119150679841;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 0 3 0 14 30.404816438374283 23 22.380006892199535 
		31 16.86572423135641 37 11.316127045673976 39 8.8243993265817 42 1.6810069718094027 
		43 1.6810069718094027 47 1.6810069718094027 51 1.6810069718094027 56 1.6810069718094027 
		60 1.6810069718094027 64 1.6810069718094027;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -2.448549867634112 3 -2.448549867634112 
		14 -2.448549867634112 23 21.872356180252392 27 30.557598703035712 32 44.644748161686422 
		35 37.840873852688333 39 44.129286800489808 47 42.445421918691181 51 41.006751006011406 
		56 42.445421918691181 60 37.887623170689906 64 37.61919391360825;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 5 5.4830900057277345 8 1.9300473996457406 
		10 0 14 0 23 0 31 0 37 0 42 -51.510891530673923 49 -39.893734705007475 53 -79.429646904823059 
		58 -47.103970911780529 62 -60.805102510784081 66 -64.260593957478363;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 0 14 0 23 0 31 0 37 0 42 -7.955963018273553 
		46 -7.955963018273553 50 -7.955963018273553 55 -7.955963018273553 59 -7.955963018273553 
		63 -7.955963018273553;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1.2910907900570192 3 5.8549926735825615 
		6 10.080228777067186 14 -1.8697897672785431 18 -3.5063445634742854 23 -12.184570295457666 
		31 -19.66021814919463 37 -35.683103718938057 46 -60.625450949398697 50 -66.752718094449648 
		55 -60.625450949398697 59 -63.118914373684319 63 -60.625450949398697;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.14113566014355422 3 15.082254199725005 
		14 0.14113566014355422 23 0.14113566014355422 31 0.14113566014355422 37 0.14113566014355422 
		42 11.877760912006144 43 9.853685343470115 47 6.3705016295424093 51 7.1833804730571273 
		56 6.3705016295424093 60 6.7065782596714181 64 6.3705016295424093;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.55216102146285018 3 10.783628041331253 
		14 -5.9807148200540388 23 -7.2790696766519538 31 -7.4164887447130186 37 -7.3702767387535459 
		42 11.797607222658783 44 9.853685343470115 48 6.3705016295424093 52 7.4221744008992525 
		57 6.3705016295424093 61 6.8053052266615772 65 6.3705016295424093;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 0.68228131693915994 3 23.066056228006989 
		7 -16.84183482931865 9 -12.460547012966863 12 -10.282504909951696 20 -8.856834158341325 
		27 -9.9038091621905142 31 -10.619209686467576 37 -12.227250795655738 42 20.402897778371152 
		45 11.067367619159388 49 -29.538640174932738 53 1.9668485980125785 58 -29.538640174932738 
		62 -23.4798950219344 66 -29.538640174932738;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		3 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 9 3 9 9 9 9 9 
		3 9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -0.70178127080170383 3 -0.70178127080170383 
		14 -0.70178127080170383 23 -0.70178127080170383 31 -0.70178127080170383 37 -0.70178127080170383 
		42 -0.701781 45 -0.701781 49 -0.701781 53 -0.701781 58 -0.701781 62 -0.701781 66 
		-0.701781;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 -34.653799053373191 14 -44.347162192471863 
		37 -46.650553748665146 47 -11.658940641286739 51 -26.437083662190155 55 -19.11053225713891 
		60 -26.437083662190155 64 -26.887127132400757 68 -26.437083662190155;
	setAttr -s 10 ".kit[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
	setAttr -s 10 ".kot[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 33.23861725407226 14 26.052333247413557 
		23 26.052333247413557 31 10.428954474749512 37 9.230292990968783 48 0 52 0 56 0 61 
		0 65 0 69 0;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1.7208864831813682 3 -40.882328072226137 
		14 -100.17320511700215 23 -122.31974385588582 31 -36.241738632416521 37 -48.880749844411035 
		49 -45.606688528262566 53 -94.961599170900712 55 -82.917286553101306 57 -67.565630360176897 
		62 -94.961599170900712 64 -92.77406348221173 66 -88.96447525406532 70 -94.961599170900712;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -18.948084576790443 3 -18.948084576790443 
		14 -27.45065052875605 23 -24.544551714461132 31 -27.45065052875605 37 -4.5155907859905815 
		42 -11.893513 51 -11.893513 55 -11.893513 59 -11.893513 64 -11.893513 68 -11.893513 
		72 -11.893513;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -3.3419010434934679 3 -3.3419010434934679 
		14 0 23 0 31 0 37 -0.79642129542031892 42 -4.493922 51 -4.493922 55 -4.493922 59 
		-4.493922 64 -4.493922 68 -4.493922 72 -4.493922;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 -0.6872236728401484 23 -0.6144698441446641 
		31 -0.6872236728401484 37 0 42 0 51 0 55 0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 -4.7864383321874557 23 -4.2797158080168405 
		31 -4.7864383321874557 37 0 42 0 51 0 55 0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -8.1378969009838897 3 -22.758994202215636 
		14 -67.32554509588654 16 -64.622930400679991 18 -67.975977497246447 20 -65.373604139116452 
		23 -69.924420610946967 27 -68.22486765072216 31 -71.423558058716438 42 -24.970130078056531 
		46 10.809175549672981 50 -34.08843602875357 54 -28.418812160716907 59 -34.08843602875357 
		63 -33.159761841662778 67 -34.08843602875357;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 -3.7592510050250603 14 -0.85325820778614558 
		23 -0.85325820778614558 31 -0.85325820778614558 42 -0.21978469667797473 47 0 51 0 
		55 0 60 0 64 0 68 0;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 -1.2474624697910552 3 -55.150970056391202 
		14 -86.022996941867916 17 -78.539092741257846 19 -86.204464536190486 21 -67.819243808121655 
		23 -70.988476315289702 25 -47.23819041594664 27 -49.967517695453111 31 -36.66457759447853 
		42 -32.52942432472009 46 -31.735107987930775 48 -51.414106878329683 52 -101.7819935241337 
		54 -82.112970134233265 56 -59.709391665101094 61 -101.7819935241337 63 -95.411491630519052 
		65 -88.104868947082807 69 -101.7819935241337;
	setAttr -s 20 ".kit[0:19]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kot[0:19]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -12.498080351386355 3 -12.498080351386355 
		14 -29.636193451426365 19 -13.359071258087782 23 -13.983154883663699 27 -29.636193451426365 
		31 -29.636193451426365 37 0 42 -12.588356 49 -12.588356 53 -12.588356 57 -12.588356 
		62 -12.588356 66 -12.588356 70 -12.588356;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 4.6514645037620239 3 4.6514645037620239 
		14 0 19 0.042415231307892327 23 -0.0026509520021138584 27 0 31 0 37 0 42 0.039968200000000002 
		49 0.039968200000000002 53 0.039968200000000002 57 0.039968200000000002 62 0.039968200000000002 
		66 0.039968200000000002 70 0.039968200000000002;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 8.0625526097620117 19 0 23 4.0312763048810059 
		27 8.0625526097620117 31 8.0625526097620117 37 0 42 0 49 0 53 0 57 0 62 0 66 0 70 
		0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 0 19 0 23 0 27 0 31 0 37 0 42 
		0 49 0 53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 4.9194558255705436 19 0 23 2.4597279127852723 
		27 4.9194558255705436 31 4.9194558255705436 37 0 42 0 49 0 53 0 57 0 62 0 66 0 70 
		0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 0 19 0 23 0 27 0 31 0 37 0 42 
		0 49 0 53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 -37.13116320444874 19 0 23 -18.565581602224373 
		27 -37.13116320444874 31 -37.13116320444874 37 0 42 0 49 0 53 0 57 0 62 0 66 0 70 
		0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 -15.820450030314936 19 0 23 
		-7.9102250151574696 27 -15.820450030314936 31 -15.820450030314936 37 0 42 0 49 0 
		53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 21.419258770543713 27 -1.8367692037326067 
		32 -26.409725659584467 35 -26.790774371895317 39 -64.35302371045826 47 -54.646973439480767 
		51 -77.194023432777939 56 -53.013932763478259 60 -64.92838865428233 64 -55.226675766118333;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -29.070586091195047 3 -29.070586091195047 
		5 -26.258080246287751 8 -18.976952026636582 10 -16.366155870886189 14 -16.366155870886189 
		23 -16.366155870886189 31 -16.366155870886189 37 -16.366155870886189 42 -1.5079173671648725 
		49 -4.5919795683795686 53 -12.663708628436973 58 -29.229969910608542 62 -4.5747799845786847 
		66 -23.314502742108239;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 7.2798116407249278 3 7.2798116407249278 
		14 7.2798116407249278 23 7.2798116407249278 31 7.2798116407249278 37 7.2798116407249278 
		42 7.2798116407249385 46 7.2798116407249385 50 7.2798116407249385 55 7.2798116407249385 
		59 7.2798116407249385 63 7.2798116407249385;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 -3.3943752565105507 6 -6.9413792668478012 
		14 -1.4245349833350784 18 -0.24049297502276876 23 1.865776543585473 31 1.3341991888781271 
		37 1.2304401553957625 46 1.239820085060547 50 3.1781889865140553 55 1.239820085060547 
		59 1.9193691470446623 63 1.239820085060547;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 1.302801324362874 14 0 23 0 31 0 
		37 0 42 0 43 0 47 0 51 0 56 0 60 0 64 0;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.0084151252342212438 3 -3.8303147759325125 
		14 -0.86238807422325769 23 -1.035452972801653 31 -1.053770325968763 37 -1.0476104808933782 
		42 -0.83625293676933532 44 0 48 0 52 0.13813106207525966 57 0 61 0.057108906219461235 
		65 0;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -0.026536333767640648 3 -9.1400575576498611 
		7 -19.302331750284015 9 -1.7953990623439513 12 -22.072176348991754 20 -2.6897129939423876 
		27 -20.6836774819763 31 -14.239192048743643 37 -13.704641300666516 42 -0.026536300000000054 
		45 -0.026536300000000016 49 -0.02653630000000003 53 -0.026536300000000027 58 -0.02653630000000003 
		62 -0.026536300000000027 66 -0.02653630000000003;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		1 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 9 3 9 9 9 9 9 
		1 9 9 9 9 9 9 9;
	setAttr -s 16 ".kix[8:15]"  0.98138993978500366 0.74505871534347534 
		1 1 1 1 1 1;
	setAttr -s 16 ".kiy[8:15]"  0.19202564656734467 0.66699886322021484 
		0 0 0 0 0 0;
	setAttr -s 16 ".kox[8:15]"  0.98138993978500366 0.74505871534347534 
		1 1 1 1 1 1;
	setAttr -s 16 ".koy[8:15]"  0.19202564656734467 0.66699886322021484 
		0 0 0 0 0 0;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 45 0 49 
		0 53 0 58 0 62 0 66 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 13.854716632275778 14 21.866826204823752 
		37 -47.441573565687634 47 -19.295124805239627 51 27.970644970134352 55 21.052104125551352 
		60 27.970644970134352 64 26.590636729495234 68 27.970644970134352;
	setAttr -s 10 ".kit[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
	setAttr -s 10 ".kot[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 -32.439136911498778 3 -75.728655994994938 
		14 -72.081814658584989 23 -72.081814658584989 31 -41.715414836240839 37 -32.607141977583161 
		48 -25.518185311929557 52 7.4020349556572906 56 -19.036349389954694 61 7.4020349556572906 
		65 -3.8898807682999852 69 7.4020349556572906;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 -8.1070161891185535 3 -12.129203839578368 
		14 -35.318146754689117 23 -42.113545384856216 31 -15.332482023549151 37 8.4508491362944831 
		49 -16.757981363675544 53 -30.844327868686506 55 10.937759331895412 57 -8.5300638478878383 
		62 -30.844327868686506 64 -11.833028805209052 66 -28.013393447575837 70 -30.844327868686506;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -18.366743592299766 3 -18.366743592299766 
		14 -6.731408680814388 23 -6.0187793419696707 31 -6.731408680814388 37 -4.3770492272789845 
		42 -1.755435 51 -1.755435 55 -1.755435 59 -1.755435 64 -1.755435 68 -1.755435 72 
		-1.755435;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -3.173264837453968 3 -3.173264837453968 
		14 0 23 0 31 0 37 -0.75623294447559453 42 -0.794832 51 -0.794832 55 -0.794832 59 
		-0.794832 64 -0.794832 68 -0.794832 72 -0.794832;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 -16.430638259804731 23 -14.691187352479663 
		31 -16.430638259804731 37 0 42 0 51 0 55 0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 -2.1220532667549841 23 -1.8973993353667433 
		31 -2.1220532667549841 37 0 42 0 51 0 55 0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 51 0 55 
		0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -12.215538151373286 3 -15.649624328500144 
		14 39.015174494458577 16 37.583896486366861 18 33.365365740440232 20 31.080795714294329 
		23 34.249154667293169 27 32.020998189138616 31 31.499919910334548 42 51.190340527347296 
		46 56.491487672248724 50 -54.248283482997898 54 -36.127286066840426 59 -54.248283482997898 
		63 -51.280096736031595 67 -54.248283482997898;
	setAttr -s 16 ".kit[0:15]"  3 1 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 1 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
	setAttr -s 16 ".kix[1:15]"  0.95846480131149292 1 0.80401182174682617 
		0.76146793365478516 0.99574452638626099 0.99753618240356445 0.98419409990310669 0.83110171556472778 
		0.75355792045593262 0.14340989291667938 0.16276535391807556 1 0.75013476610183716 
		1 0.93211621046066284;
	setAttr -s 16 ".kiy[1:15]"  0.28521108627319336 0 -0.59461343288421631 
		-0.64820259809494019 0.092156335711479187 0.070153720676898956 -0.17709293961524963 
		0.55612033605575562 0.65738153457641602 -0.98966342210769653 -0.98666483163833618 
		0 -0.66128498315811157 0 -0.36215922236442566;
	setAttr -s 16 ".kox[1:15]"  0.95846480131149292 1 0.80401182174682617 
		0.76146793365478516 0.99574452638626099 0.99753618240356445 0.98419409990310669 0.83110171556472778 
		0.75355792045593262 0.14340989291667938 0.16276535391807556 1 0.75013476610183716 
		1 0.93211621046066284;
	setAttr -s 16 ".koy[1:15]"  0.28521096706390381 0 -0.59461343288421631 
		-0.64820259809494019 0.092156335711479187 0.070153720676898956 -0.17709293961524963 
		0.55612033605575562 0.65738153457641602 -0.98966342210769653 -0.98666483163833618 
		0 -0.66128498315811157 0 -0.36215922236442566;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 30.009168092349935 3 78.660362739709029 
		14 30.038305996145393 23 30.038305996145393 31 30.038305996145393 42 25.258453436481609 
		47 39.864956406790128 51 10.997393031614397 55 23.421681574538972 60 10.997393031614397 
		64 16.292957860642307 68 10.997393031614397;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 9.6996556887373035 3 29.569732818562521 
		14 17.496170541777442 17 14.964486193658132 19 12.565049228483392 21 5.9870743844969976 
		23 13.275215535257608 25 9.7213252583584691 27 6.5621304767316708 31 20.512221059839995 
		42 12.052420760676513 46 10.433115202147139 48 19.274478570713924 52 41.928772306339141 
		54 -8.4211662250189008 56 12.25411914057131 61 41.928772306339141 63 22.614744045287512 
		65 35.405167231556277 69 41.928772306339141;
	setAttr -s 20 ".kit[0:19]"  3 9 1 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kot[0:19]"  3 9 1 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kix[2:19]"  0.85198217630386353 0.88852763175964355 
		0.64807367324829102 0.99570697546005249 0.89841145277023315 0.7511824369430542 0.72801578044891357 
		0.98212653398513794 0.94331967830657959 0.84599864482879639 0.34190624952316284 0.38232001662254333 
		0.24931076169013977 0.25662961602210999 0.79042530059814453 0.76045984029769897 0.51025736331939697 
		0.760459303855896;
	setAttr -s 20 ".kiy[2:19]"  -0.52357083559036255 -0.45882308483123779 
		-0.76157760620117188 0.092561475932598114 0.43915459513664246 -0.66009467840194702 
		0.68556034564971924 0.18822188675403595 -0.33188548684120178 0.53318500518798828 
		0.93973404169082642 -0.92402994632720947 -0.96842354536056519 0.96650981903076172 
		0.61255848407745361 -0.64938491582870483 0.86002177000045776 0.64938563108444214;
	setAttr -s 20 ".kox[2:19]"  0.85198217630386353 0.88852763175964355 
		0.64807367324829102 0.99570697546005249 0.89841145277023315 0.7511824369430542 0.72801578044891357 
		0.98212653398513794 0.94331967830657959 0.84599864482879639 0.34190624952316284 0.38232001662254333 
		0.24931076169013977 0.25662961602210999 0.79042530059814453 0.76045984029769897 0.51025736331939697 
		0.760459303855896;
	setAttr -s 20 ".koy[2:19]"  -0.52357077598571777 -0.45882308483123779 
		-0.76157760620117188 0.092561475932598114 0.43915459513664246 -0.66009467840194702 
		0.68556034564971924 0.18822188675403595 -0.33188548684120178 0.53318500518798828 
		0.93973404169082642 -0.92402994632720947 -0.96842354536056519 0.96650981903076172 
		0.61255848407745361 -0.64938491582870483 0.86002177000045776 0.64938563108444214;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -16.016384857458238 3 -16.016384857458238 
		14 -5.000312341799809 19 -16.864558819116564 23 -1.4461212514066717 27 -5.000312341799809 
		31 -5.000312341799809 37 0 42 -15.891604000000001 49 -15.891604000000001 53 -15.891604000000001 
		57 -15.891604000000001 62 -15.891604000000001 66 -15.891604000000001 70 -15.891604000000001;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 11.870872562983752 3 11.870872562983752 
		14 34.907240585230163 19 0.1082470164426929 23 17.446854583432735 27 34.907240585230163 
		31 34.907240585230163 37 0 42 0.10200200000000001 49 0.10200200000000001 53 0.10200200000000001 
		57 0.10200200000000001 62 0.10200200000000001 66 0.10200200000000001 70 0.10200200000000001;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 -13.701735265684137 19 0 23 
		-6.8508676328420677 27 -13.701735265684137 31 -13.701735265684137 37 0 42 0 49 0 
		53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 0 19 0 23 0 27 0 31 0 37 0 42 
		0 49 0 53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 -4.8298593669674332 19 0 23 
		-2.4149296834837166 27 -4.8298593669674332 31 -4.8298593669674332 37 0 42 0 49 0 
		53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 0 19 0 23 0 27 0 31 0 37 0 42 
		0 49 0 53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 24.137602630720838 19 0 23 12.068801315360416 
		27 24.137602630720838 31 24.137602630720838 37 0 42 0 49 0 53 0 57 0 62 0 66 0 70 
		0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 14 -15.987168955404782 19 0 23 
		-7.9935844777023908 27 -15.987168955404782 31 -15.987168955404782 37 0 42 0 49 0 
		53 0 57 0 62 0 66 0 70 0;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 25.46188064100717 3 25.46188064100717 
		14 25.46188064100717 23 0.3901002704621751 27 -3.594315631633926 32 -4.2776191829961974 
		35 -5.0675764185149763 39 -4.0186736509154439 47 15.56899876299296 51 12.877898365541254 
		56 14.523315456769369 60 16.411256630461022 64 18.504714186249604;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 5 -0.2652711733362334 8 -0.093375439353310344 
		10 0 14 0 23 0 31 0 37 0 42 32.829431609299213 49 17.220870324257572 53 24.634013438233971 
		58 41.11684299443467 62 52.767386001469021 66 62.946117168765603;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 46 0 50 
		0 55 0 59 0 63 0;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0.99037185596550503 6 1.4543060964823606 
		14 -4.9134174803668964 18 -8.5937036085706744 23 -5.5423774812588276 31 -10.121681390154546 
		37 -6.0342554451201984 46 0.93918826059982152 50 3.4800735764342199 55 0.93918826059982152 
		59 1.8299661886232397 63 0.93918826059982152;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 8.0188816005647041 14 0 23 0 31 0 
		37 0 42 0 43 0 47 0 51 0 56 0 60 0 64 0;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0.019794606311957463 3 2.694530269192561 
		14 -6.6675030570445193 23 -7.9965479092396903 31 -8.1372152720450845 37 -8.0899109796921707 
		42 0.99189379387377541 44 0 48 0 52 -0.16383960295566991 57 0 61 -0.06773784511184254 
		65 0;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 -0.034361262366484416 3 -4.0504787693781079 
		7 -1.9282028547712293 9 -4.3007685961839179 12 -1.3854946557432795 20 -4.6244666941396293 
		27 -1.6527200508040134 31 -2.5472717975418742 37 -1.9894491284567175 42 -0.034361300000000108 
		45 -0.034361300000000192 49 -0.034361300000000233 53 -0.034361300000000282 58 -0.034361300000000233 
		62 -0.034361300000000226 66 -0.034361300000000233;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		3 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 9 3 9 9 9 9 9 
		3 9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 31 0 37 0 42 0 45 0 49 
		0 53 0 58 0 62 0 66 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -45.645013483740549 3 -69.036101552521174 
		14 -33.215107312037269 37 13.686829976013033 47 -23.509799026229519 51 -26.49961449615429 
		55 -11.404324795983673 60 -26.49961449615429 64 -26.378531162336451 68 -26.49961449615429;
	setAttr -s 10 ".kit[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
	setAttr -s 10 ".kot[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 -27.777371966555087 14 -20.302518974328791 
		23 -20.302518974328791 31 -2.3395624468109935 37 -0.36161223038247775 48 0 52 0 56 
		0 61 0 65 0 69 0;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 -1.1231193682533216 3 -22.274210594278191 
		14 -37.533270457091142 23 1.9494451299646358 31 -20.602402366678941 37 -2.4293557876741687 
		49 -29.337763070546501 53 -28.482457373761967 55 -20.838296677770899 57 -21.70956201003094 
		62 -28.482457373761967 64 -29.626163795737785 66 -28.005974172713991 70 -28.482457373761967;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -9.399391337933455 3 -9.399391337933455 
		14 -18.272095970653059 23 -16.337696765747406 31 -18.272095970653059 37 -2.2400050339911322 
		42 -7.719601 51 -7.719601 55 -7.719601 59 -7.719601 64 -7.719601 68 -7.719601 72 
		-7.719601;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -39.542557426213627 3 -39.542557426213627 
		14 0 23 0 31 0 37 -9.4235388556823239 42 0 51 0 55 0 59 0 64 0 68 0 72 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -56.359168905796714 3 -56.359168905796714 
		14 -12.28182706398594 23 -10.98159545039662 31 -12.28182706398594 37 -13.431170392238732 
		42 -23.031843 51 -23.031843 55 -23.031843 59 -23.031843 64 -23.031843 68 -23.031843 
		72 -23.031843;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -73.966144308812432 3 -73.966144308812432 
		14 -42.755480161637585 23 -38.229115608763216 31 -42.755480161637585 37 -17.627156130299124 
		42 -34.510714 51 -34.510714 55 -34.510714 59 -34.510714 64 -34.510714 68 -34.510714 
		72 -34.510714;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -63.839356559844809 3 -63.839356559844809 
		14 -20.202447187910025 23 -18.063688839796214 31 -20.202447187910025 37 -15.213802283218049 
		42 -29.654061000000002 51 -29.654061000000002 55 -29.654061000000002 59 -29.654061000000002 
		64 -29.654061000000002 68 -29.654061000000002 72 -29.654061000000002;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -65.326320103044935 3 -65.326320103044935 
		14 -42.817389722702188 23 -38.284471070304605 31 -42.817389722702188 37 -15.568166059276678 
		42 -37.526539 51 -37.526539 55 -37.526539 59 -37.526539 64 -37.526539 68 -37.526539 
		72 -37.526539;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -54.489839590457279 3 -54.489839590457279 
		14 -39.974891070203824 23 -35.742897177489283 31 -39.974891070203824 37 -12.985683132038504 
		42 -38.280067 51 -38.280067 55 -38.280067 59 -38.280067 64 -38.280067 68 -38.280067 
		72 -38.280067;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 -58.159773063591629 3 -58.159773063591629 
		14 -43.910496841916462 23 -39.261854920850304 31 -43.910496841916462 37 -13.860279046125072 
		42 -44.659082 51 -44.659082 55 -44.659082 59 -44.659082 64 -44.659082 68 -44.659082 
		72 -44.659082;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 47.399331424850153 3 52.932028039455588 
		14 0.95134934704772045 16 2.1755911694035146 18 -0.13522591402819314 20 0.7060638217894234 
		23 -2.7615153844569673 27 -1.173780643146527 31 -6.5509746316580317 42 -6.8105896017635565 
		46 43.831588717232556 50 22.907096454299086 54 17.883256824890438 59 22.907096454299086 
		63 22.084200568243212 67 22.907096454299086;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 0 3 -4.5326320976986203 14 -1.2737083076742455 
		23 -1.2737083076742455 31 -1.2737083076742455 42 -0.31426939723750325 47 0 51 0 55 
		0 60 0 64 0 68 0;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 7.1301771055535337 3 8.6660441189924242 
		14 -19.379004371490147 17 -23.179486845829043 19 -21.915398949280984 21 -22.286381416386828 
		23 -20.080424949249732 25 -20.672074338447501 27 -22.150961895772966 31 -19.153587864168223 
		42 -4.3014113446874305 46 2.9099770196098693 48 29.68140688919064 52 22.501381336498007 
		54 14.764936416258712 56 18.621718277308613 61 22.501381336498007 63 25.083122511055439 
		65 22.95901047719418 69 22.501381336498007;
	setAttr -s 20 ".kit[0:19]"  3 9 1 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kot[0:19]"  3 9 1 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kix[2:19]"  0.72076910734176636 0.96648859977722168 
		0.99323564767837524 0.97234362363815308 0.97839444875717163 0.96517789363861084 0.99133414030075073 
		0.84873491525650024 0.79226666688919067 0.31952723860740662 0.50488448143005371 0.60920774936676025 
		0.89161121845245361 0.86552482843399048 0.90035641193389893 0.99821054935455322 0.97554713487625122 
		0.998210608959198;
	setAttr -s 20 ".kiy[2:19]"  -0.69317531585693359 -0.25670966506004333 
		0.11611642688512802 0.23355457186698914 0.20674678683280945 -0.2615947425365448 0.1313646137714386 
		0.52881854772567749 0.61017501354217529 0.94757711887359619 0.86318695545196533 -0.7930106520652771 
		-0.45280182361602783 0.50086599588394165 0.43515315651893616 0.059796269983053207 
		-0.21979016065597534 -0.059796378016471863;
	setAttr -s 20 ".kox[2:19]"  0.72076922655105591 0.96648859977722168 
		0.99323564767837524 0.97234362363815308 0.97839444875717163 0.96517789363861084 0.99133414030075073 
		0.84873491525650024 0.79226666688919067 0.31952723860740662 0.50488448143005371 0.60920774936676025 
		0.89161121845245361 0.86552482843399048 0.90035641193389893 0.99821054935455322 0.97554713487625122 
		0.998210608959198;
	setAttr -s 20 ".koy[2:19]"  -0.69317513704299927 -0.25670966506004333 
		0.11611642688512802 0.23355457186698914 0.20674678683280945 -0.2615947425365448 0.1313646137714386 
		0.52881854772567749 0.61017501354217529 0.94757711887359619 0.86318695545196533 -0.7930106520652771 
		-0.45280182361602783 0.50086599588394165 0.43515315651893616 0.059796269983053207 
		-0.21979016065597534 -0.059796378016471863;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -6.6175135861403929 3 -6.6175135861403929 
		14 -1.0385848073509598 19 -1.6680591288588742 23 -0.41503870594768261 27 -1.0385848073509598 
		31 -1.0385848073509598 37 0 42 -1.571825 49 -1.571825 53 -1.571825 57 -1.571825 62 
		-1.571825 66 -1.571825 70 -1.571825;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -38.285706776879671 3 -38.285706776879671 
		14 0 19 -0.34911525431273716 23 0.021819702986829202 27 0 31 0 37 0 42 -0.328974 
		49 -0.328974 53 -0.328974 57 -0.328974 62 -0.328974 66 -0.328974 70 -0.328974;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -38.133329475338165 3 -38.133329475338165 
		14 -20.826735573236689 19 -17.166501625775705 23 -9.3404614545678779 27 -20.826735573236689 
		31 -20.826735573236689 37 0 42 -16.176127 49 -16.176127 53 -16.176127 57 -16.176127 
		62 -16.176127 66 -16.176127 70 -16.176127;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -74.810673852666966 3 -74.810673852666966 
		14 -53.28039666000052 19 -48.964907185867062 23 -23.579891728679737 27 -53.28039666000052 
		31 -53.28039666000052 37 0 42 -46.140010000000004 49 -46.140010000000004 53 -46.140010000000004 
		57 -46.140010000000004 62 -46.140010000000004 66 -46.140010000000004 70 -46.140010000000004;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -51.590748582374587 3 -51.590748582374587 
		14 1.6896744022624171 19 -29.532148799143009 23 2.6905965614865255 27 1.6896744022624171 
		31 1.6896744022624171 37 0 42 -27.828372 49 -27.828372 53 -27.828372 57 -27.828372 
		62 -27.828372 66 -27.828372 70 -27.828372;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -67.298744458395092 3 -67.298744458395092 
		14 -71.352656140838533 19 -39.614868135630424 23 -33.200399323754226 27 -71.352656140838533 
		31 -71.352656140838533 37 0 42 -37.329396 49 -37.329396 53 -37.329396 57 -37.329396 
		62 -37.329396 66 -37.329396 70 -37.329396;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -60.240931129090278 3 -60.240931129090278 
		14 -56.095800534101684 19 -51.259972611207075 23 -33.829331939000319 27 -56.095800534101684 
		31 -56.095800534101684 37 0 42 -48.302668000000004 49 -48.302668000000004 53 -48.302668000000004 
		57 -48.302668000000004 62 -48.302668000000004 66 -48.302668000000004 70 -48.302668000000004;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 -56.936986476047892 3 -56.936986476047892 
		14 -63.055161885778702 19 -43.169912534711777 23 -28.829461526839811 27 -63.055161885778702 
		31 -63.055161885778702 37 0 42 -40.679342 49 -40.679342 53 -40.679342 57 -40.679342 
		62 -40.679342 66 -40.679342 70 -40.679342;
	setAttr -s 15 ".kit[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 9 3 9 9 9 9 3 
		9 9 9 9 9 9 9;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 -7.8465618197579943 27 
		-11.736469484674741 32 -30.162981527624652 35 -26.928815057248265 39 -18.424878731632436 
		47 -36.259762493767042 51 -29.094298879388912 56 -48.825654276848525 60 -45.178499673672306 
		64 -53.467991492174868;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 3 1 5 1 8 1 10 1 14 1 23 1 31 1 37 
		1 42 1 49 1 53 1 58 1 62 1 66 1;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 6 1 14 1 18 1 23 1 31 1 37 1 46 
		1 50 1 55 1 59 1 63 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 58 1 62 1 66 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1 3 1 14 1 23 1 31 1 37 1 39 1 42 1 43 
		1 47 1 51 1 56 1 60 1 64 1;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 27 1 32 1 35 1 39 1 47 
		1 51 1 56 1 60 1 64 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 3 1 5 1 8 1 10 1 14 1 23 1 31 1 37 
		1 42 1 49 1 53 1 58 1 62 1 66 1;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1.0000000000000002 3 1.0000000000000002 
		5 1.0000000000000002 10 1.0000000000000002 15 1.0000000000000002 19 1.0000000000000002 
		21 1.0000000000000002 23 1.0000000000000002 27 1.0000000000000002 30 1.0000000000000002 
		37 1.0000000000000002 40 1.0000000000000002 41 1.0000000000000002 60 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 6 1 14 1 18 1 23 1 31 1 37 1 46 
		1 50 1 55 1 59 1 63 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 58 1 62 1 66 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1 3 1 14 1 23 1 31 1 37 1 39 1 42 1 43 
		1 47 1 51 1 56 1 60 1 64 1;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 27 1 32 1 35 1 39 1 47 
		1 51 1 56 1 60 1 64 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 3 1 5 1 8 1 10 1 14 1 23 1 31 1 37 
		1 42 1 49 1 53 1 58 1 62 1 66 1;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1.0000000000000002 3 1.0000000000000002 
		5 1.0000000000000002 10 1.0000000000000002 15 1.0000000000000002 19 1.0000000000000002 
		21 1.0000000000000002 23 1.0000000000000002 27 1.0000000000000002 30 1.0000000000000002 
		37 1.0000000000000002 40 1.0000000000000002 41 1.0000000000000002 60 1.0000000000000002;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 6 1 14 1 18 1 23 1 31 1 37 1 46 
		1 50 1 55 1 59 1 63 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:HeadControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:TankControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:LElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  2 1 3 1 5 1 10 1 15 1 19 1 21 1 23 1 27 
		1 30 1 37 1 40 1 41 1 60 1;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 58 1 62 1 66 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1 3 1 14 1 23 1 31 1 37 1 39 1 42 1 43 
		1 47 1 51 1 56 1 60 1 64 1;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 27 1 32 1 35 1 39 1 47 
		1 51 1 56 1 60 1 64 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 1 3 1 5 1 8 1 10 1 14 1 23 1 31 1 37 
		1 42 1 49 1 53 1 58 1 62 1 66 1;
	setAttr -s 15 ".kit[2:14]"  9 9 3 3 9 9 3 9 
		9 9 9 9 9;
	setAttr -s 15 ".kot[2:14]"  5 5 3 3 5 5 3 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 46 1 50 
		1 55 1 59 1 63 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 5 3 5 5 3 5 5 
		5 5 5 5;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 6 1 14 1 18 1 23 1 31 1 37 1 46 
		1 50 1 55 1 59 1 63 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 5 5 5 5 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 43 1 47 
		1 51 1 56 1 60 1 64 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 5 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 44 1 48 
		1 52 1 57 1 61 1 65 1;
	setAttr -s 13 ".kit[0:12]"  3 9 9 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 5 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 16 ".ktv[0:15]"  0 1 3 1 7 1 9 1 12 1 20 1 27 1 31 1 37 
		1 42 1 45 1 49 1 53 1 58 1 62 1 66 1;
	setAttr -s 16 ".kit[0:15]"  3 9 3 9 9 9 9 9 
		3 9 9 9 9 9 9 9;
	setAttr -s 16 ".kot[0:15]"  3 5 3 5 5 5 5 5 
		3 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 53 1 58 1 62 1 66 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 47 1 51 
		1 55 1 60 1 64 1 68 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 14 1 37 1 47 1 51 1 55 1 60 1 64 
		1 68 1;
	setAttr -s 10 ".kit[0:9]"  3 9 3 9 9 9 9 9 
		9 9;
	setAttr -s 10 ".kot[0:9]"  3 5 3 5 5 5 5 5 
		5 5;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 48 1 52 1 56 
		1 61 1 65 1 69 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 5 3 5 5 3 5 5 
		5 5 5 5;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1 3 1 14 1 23 1 31 1 37 1 49 1 53 1 55 
		1 57 1 62 1 64 1 66 1 70 1;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5 5;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:l_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 51 1 55 
		1 59 1 64 1 68 1 72 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 53 1 58 1 62 1 66 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 42 1 46 1 50 1 54 
		1 59 1 63 1 67 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 5 3 5 5 5 5 5 
		5 5 5 5;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 42 1 47 1 51 1 55 
		1 60 1 64 1 68 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 9 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 5 3 5 5 5 5 5 
		5 5 5 5;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 20 ".ktv[0:19]"  0 1 3 1 14 1 17 1 19 1 21 1 23 1 25 1 27 
		1 31 1 42 1 46 1 48 1 52 1 54 1 56 1 61 1 63 1 65 1 69 1;
	setAttr -s 20 ".kit[0:19]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 20 ".kot[0:19]"  3 5 3 5 5 5 5 5 
		5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 49 1 53 
		1 57 1 62 1 66 1 70 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 3 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 3 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 12 ".ktv[0:11]"  0 1 3 1 14 1 23 1 31 1 37 1 42 1 45 1 49 
		1 58 1 62 1 66 1;
	setAttr -s 12 ".kit[0:11]"  3 9 3 9 9 3 9 9 
		9 9 9 9;
	setAttr -s 12 ".kot[0:11]"  3 5 3 5 5 3 5 5 
		5 5 5 5;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 14 ".ktv[0:13]"  0 1 3 1 14 1 23 1 31 1 37 1 39 1 42 1 43 
		1 47 1 51 1 56 1 60 1 64 1;
	setAttr -s 14 ".kit[0:13]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9 9;
	setAttr -s 14 ".kot[0:13]"  3 5 3 5 5 5 5 5 
		5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 1 3 1 14 1 23 1 27 1 32 1 35 1 39 1 47 
		1 51 1 56 1 60 1 64 1;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 5 3 5 5 5 5 5 
		5 5 5 5 5;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 5 0 8 -2.8000000000000003 10 0 
		14 0 23 0 31 0 37 0 42 0 49 0 53 0 58 0 62 0 66 0;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 0 23 0 27 0 32 0 35 0 39 0 47 
		0 51 0 56 0 60 0 64 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 15 ".ktv[0:14]"  0 0 3 0 5 0 8 0 10 0 14 0 23 0 31 0 37 
		0 42 0 49 0 53 0 58 0 62 0 66 0;
	setAttr -s 15 ".kit[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
	setAttr -s 15 ".kot[0:14]"  3 3 9 9 3 3 9 9 
		3 9 9 9 9 9 9;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 13 ".ktv[0:12]"  0 0 3 0 14 17.1 23 0 27 0 32 0 35 0 39 
		0 47 0 51 0 56 0 60 0 64 0;
	setAttr -s 13 ".kit[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
	setAttr -s 13 ".kot[0:12]"  3 9 3 9 9 9 9 9 
		9 9 9 9 9;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 9 ".ktv[0:8]"  0 0 7 0 37 0 48 50.7 52 0 56 39.2 61 0 
		65 8.6 69 0;
	setAttr -s 9 ".kit[0:8]"  3 3 3 9 9 9 9 9 
		9;
	setAttr -s 9 ".kot[0:8]"  3 3 3 9 9 9 9 9 
		9;
createNode animCurveTL -n "D_:R_KneeShape_localPositionX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 -8.3628095325263345;
createNode animCurveTL -n "D_:R_KneeShape_localPositionY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 24.703883880000003;
createNode animCurveTL -n "D_:R_KneeShape_localPositionZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 23.055145000000003;
createNode animCurveTL -n "D_:R_KneeShape_localScaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
createNode animCurveTL -n "D_:R_KneeShape_localScaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
createNode animCurveTL -n "D_:R_KneeShape_localScaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  26 1;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 0;
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
connectAttr "D_:Entity_translateX.o" "D_RN.phl[1]";
connectAttr "D_:Entity_translateY.o" "D_RN.phl[2]";
connectAttr "D_:Entity_translateZ.o" "D_RN.phl[3]";
connectAttr "D_:Entity_rotateX.o" "D_RN.phl[4]";
connectAttr "D_:Entity_rotateY.o" "D_RN.phl[5]";
connectAttr "D_:Entity_rotateZ.o" "D_RN.phl[6]";
connectAttr "D_:Entity_visibility.o" "D_RN.phl[7]";
connectAttr "D_:Entity_scaleX.o" "D_RN.phl[8]";
connectAttr "D_:Entity_scaleY.o" "D_RN.phl[9]";
connectAttr "D_:Entity_scaleZ.o" "D_RN.phl[10]";
connectAttr "D_:DiverGlobal_translateX.o" "D_RN.phl[11]";
connectAttr "D_:DiverGlobal_translateY.o" "D_RN.phl[12]";
connectAttr "D_:DiverGlobal_translateZ.o" "D_RN.phl[13]";
connectAttr "D_:DiverGlobal_rotateX.o" "D_RN.phl[14]";
connectAttr "D_:DiverGlobal_rotateY.o" "D_RN.phl[15]";
connectAttr "D_:DiverGlobal_rotateZ.o" "D_RN.phl[16]";
connectAttr "D_:DiverGlobal_scaleX.o" "D_RN.phl[17]";
connectAttr "D_:DiverGlobal_scaleY.o" "D_RN.phl[18]";
connectAttr "D_:DiverGlobal_scaleZ.o" "D_RN.phl[19]";
connectAttr "D_:DiverGlobal_visibility.o" "D_RN.phl[20]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[21]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[22]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[23]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[24]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[25]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[26]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[27]";
connectAttr "D_:l_mid_2_visibility.o" "D_RN.phl[28]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[29]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[30]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[31]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[32]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[33]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[34]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[35]";
connectAttr "D_:l_pink_2_visibility.o" "D_RN.phl[36]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[37]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[38]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[39]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[40]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[41]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[42]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[43]";
connectAttr "D_:l_point_2_visibility.o" "D_RN.phl[44]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[45]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[46]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[47]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[48]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[49]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[50]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[51]";
connectAttr "D_:l_thumb_2_visibility.o" "D_RN.phl[52]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[53]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[54]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[55]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[56]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[57]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[58]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[59]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[60]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[61]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[62]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[63]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[64]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[65]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[66]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[67]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[68]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[69]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[70]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[71]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[72]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[73]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[74]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[75]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[76]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[77]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[78]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[79]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[80]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[81]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[82]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[83]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[84]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[85]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[86]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[87]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[88]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[89]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[90]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[91]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[92]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[93]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[94]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[95]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[96]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[97]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[98]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[99]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[100]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[101]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[102]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[103]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[104]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[105]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[106]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[107]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[108]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[109]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[110]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[111]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[112]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[113]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[114]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[115]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[116]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[117]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[118]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[119]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[120]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[121]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[122]";
connectAttr "D_:R_KneeShape_localPositionX.o" "D_RN.phl[123]";
connectAttr "D_:R_KneeShape_localPositionY.o" "D_RN.phl[124]";
connectAttr "D_:R_KneeShape_localPositionZ.o" "D_RN.phl[125]";
connectAttr "D_:R_KneeShape_localScaleX.o" "D_RN.phl[126]";
connectAttr "D_:R_KneeShape_localScaleY.o" "D_RN.phl[127]";
connectAttr "D_:R_KneeShape_localScaleZ.o" "D_RN.phl[128]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[129]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[130]";
connectAttr "D_:RootControl_translateZ1.o" "D_RN.phl[131]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[132]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[133]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[134]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[135]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[136]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[137]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[138]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[139]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[140]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[141]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[142]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[143]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[144]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[145]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[146]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[147]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[148]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[149]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[150]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[151]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[152]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[153]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[154]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[155]";
connectAttr "D_:TankControl_scaleX.o" "D_RN.phl[156]";
connectAttr "D_:TankControl_scaleY.o" "D_RN.phl[157]";
connectAttr "D_:TankControl_scaleZ.o" "D_RN.phl[158]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[159]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[160]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[161]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[162]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[163]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[164]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[165]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[166]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[167]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[168]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[169]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[170]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[171]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[172]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[173]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[174]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[175]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[176]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[177]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[178]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[179]";
connectAttr "D_:HeadControl_translateX.o" "D_RN.phl[180]";
connectAttr "D_:HeadControl_translateY.o" "D_RN.phl[181]";
connectAttr "D_:HeadControl_translateZ.o" "D_RN.phl[182]";
connectAttr "D_:HeadControl_scaleX.o" "D_RN.phl[183]";
connectAttr "D_:HeadControl_scaleY.o" "D_RN.phl[184]";
connectAttr "D_:HeadControl_scaleZ.o" "D_RN.phl[185]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[186]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[187]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[188]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[189]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[190]";
connectAttr "D_:LShoulderFK_translateX.o" "D_RN.phl[191]";
connectAttr "D_:LShoulderFK_translateY.o" "D_RN.phl[192]";
connectAttr "D_:LShoulderFK_translateZ.o" "D_RN.phl[193]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[194]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[195]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[196]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[197]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[198]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[199]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[200]";
connectAttr "D_:LElbowFK_scaleX.o" "D_RN.phl[201]";
connectAttr "D_:LElbowFK_scaleY.o" "D_RN.phl[202]";
connectAttr "D_:LElbowFK_scaleZ.o" "D_RN.phl[203]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[204]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[205]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[206]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[207]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[208]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[209]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[210]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[211]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[212]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[213]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[214]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[215]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[216]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[217]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[218]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[219]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[220]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[221]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[222]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[223]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[224]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[225]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[226]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[227]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[228]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[229]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[230]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[231]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[232]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[233]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[234]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[235]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[236]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[237]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[238]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[239]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[240]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[241]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[242]";
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
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_death.ma
