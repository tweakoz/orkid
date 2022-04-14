//Maya ASCII 2008 scene
//Name: diver_jog.ma
//Last modified: Tue, Aug 12, 2008 10:14:05 AM
//Codeset: 1252
file -rdi 1 -ns "D_" -rfn "D_RN" -shd "renderLayersByName" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
file -r -ns "D_" -dr 1 -rfn "D_RN" -shd "renderLayersByName" "V:/projects/w8/data/src//actors/Diver/ref/diver.ma";
requires maya "2008";
requires "Mayatomr" "9.0.1.2m - 3.6.1.6 ";
currentUnit -l centimeter -a degree -t ntsc;
fileInfo "application" "maya";
fileInfo "product" "Maya Unlimited 2008";
fileInfo "version" "2008";
fileInfo "cutIdentifier" "200708022245-704165";
fileInfo "osv" "Microsoft Windows XP Service Pack 2 (Build 2600)\n";
createNode transform -s -n "persp";
	setAttr ".v" no;
	setAttr ".t" -type "double3" -0.28234045327189339 81.097518989167895 439.19000177902427 ;
	setAttr ".r" -type "double3" -8.1052663818413642 -719.4000000004761 -1.1803486830914602e-016 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999993;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 448.15648282790994;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -s -n "top";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 100.1 0 ;
	setAttr ".r" -type "double3" -89.999999999999986 0 0 ;
createNode camera -s -n "topShape" -p "top";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 100.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 0 100.1 ;
createNode camera -s -n "frontShape" -p "front";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 100.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 100.1 0 0 ;
	setAttr ".r" -type "double3" 0 89.999999999999986 0 ;
createNode camera -s -n "sideShape" -p "side";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 100.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "side";
	setAttr ".den" -type "string" "side_depth";
	setAttr ".man" -type "string" "side_mask";
	setAttr ".hc" -type "string" "viewSet -s %camera";
	setAttr ".o" yes;
createNode lightLinker -n "lightLinker1";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 182 ".phl";
	setAttr ".phl[23]" 0;
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
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 8
		1 |D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl 
		"blendD:NeckControlpoint1" "blendD:NeckControlpoint1" " -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK1" 
		"rotate" " -type \"double3\" 0 0 -30.414345"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK1|D_:RElbowFK1" 
		"rotate" " -type \"double3\" 0 -55.116582 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"translateX" " -av -k 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"blendD:NeckControlpoint1" " -k 1 0"
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist|D_:R_Wrist_parentConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.translateX" 
		""
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist|D_:R_Wrist_parentConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.translateX" 
		"D_RN.placeHolderList[23]" "D_RN.placeHolderList[24]" "D_:R_Wrist.tx"
		5 3 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.blendD:NeckControlpoint1" 
		"D_RN.placeHolderList[29]" ""
		"D_RN" 423
		1 |D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control "blendPoint1" "blendPoint1" 
		" -ci 1 -k 1 -bt \"aDBL\" -dv 1 -smn 0 -smx 1 -at \"double\""
		2 "|D_:DiverGlobal" "translate" " -type \"double3\" 0 0 0"
		2 "|D_:DiverGlobal" "translateZ" " -av"
		2 "|D_:DiverGlobal" "scale" " -type \"double3\" 3.383 3.383 3.383"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translate" " -type \"double3\" 0 0 -0.906044"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotate" " -type \"double3\" 0 2.9382 -2.736432"
		
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "ToeRoll" " -av -k 1 0"
		2 "|D_:controlcurvesGRP|D_:L_Foot" "BallRoll" " -av -k 1 11.296306"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translate" " -type \"double3\" 0 6.186311 3.08656"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotate" " -type \"double3\" 54.520408 0.945172 -0.41197"
		
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Foot" "BallRoll" " -av -k 1 0.18125"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 6.474345 3.548296 7.887485"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -0.463634 3.548296 7.887485"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" 1.491742 -2.366867 2.110619"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" 0 6.70771 -4.97682"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "scale" " -type \"double3\" 1 1 1"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "visibility" " 1"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateX" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateY" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "translateZ" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" -0.0638709 -1.633923 3.332875"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scale" " -type \"double3\" 1 1 1"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleX" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleY" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "scaleZ" " -k 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "blendPoint1" " -k 1 0"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1" 
		"rotate" " -type \"double3\" 4.824701 0 3.018956"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"translate" " -type \"double3\" 0.0360573 -0.802296 0.0708124"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"rotate" " -type \"double3\" -3.363206 0.474162 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle" 
		"translate" " -type \"double3\" -0.00620172 -0.306546 0.0744406"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"rotate" " -type \"double3\" 31.433562 17.543594 -39.720088"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" 0 -38.996276 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist" 
		"rotate" " -type \"double3\" 6.883856 32.262435 -42.583642"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle" 
		"translate" " -type \"double3\" -0.0653239 -0.140732 0.0388105"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"rotate" " -type \"double3\" -13.491819 -0.669931 41.980131"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" 0 72.40885 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotate" " -type \"double3\" -17.818998 34.200367 21.280794"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"rotate" " -type \"double3\" 9.922878 -5.780368 1.680764"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl" 
		"Mask" " -av -k 1 3.449601"
		2 "|D_:controlcurvesGRP|D_:HipControl" "visibility" " 1"
		2 "|D_:controlcurvesGRP|D_:HipControl" "rotate" " -type \"double3\" 0 6.100915 -10.18324"
		
		2 "|D_:controlcurvesGRP|D_:HipControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:HipControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:HipControl" "rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"translate" " -type \"double3\" 1.487404 -0.126626 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotate" " -type \"double3\" 0 0 -107.561443"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"translate" " -type \"double3\" 1.483734 -0.168835 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -126.441592"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"translate" " -type \"double3\" 1.551112 -0.18994 -1.096668"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -121.844858"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"translate" " -type \"double3\" 1.307841 -0.0633132 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -92.298803"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"translate" " -type \"double3\" 1.14705 -0.126626 0.989902"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" 0 0 -83.104977"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"translate" " -type \"double3\" 1.495442 -0.147731 0"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -121.104035"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"translate" " -type \"double3\" -0.338243 -0.34634 1.160395"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -37.0911 -9.528616 -19.791464"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"translate" " -type \"double3\" 1.169286 -0.720928 0.323178"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" -27.019641 51.942652 -49.994308"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"translate" " -type \"double3\" -1.487428 0.1266 2.25757e-006"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -104.890208"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"translate" " -type \"double3\" -1.483735 0.1688 3.17606e-007"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -110.604434"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"translate" " -type \"double3\" -1.551143 0.1899 1.096671"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 0 0 -99.606656"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"translate" " -type \"double3\" -1.3078 0.0633 6.73632e-006"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotate" " -type \"double3\" 0 0 -101.374783"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"translate" " -type \"double3\" -1.147039 0.1266 -0.989898"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -78.352908"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"translate" " -type \"double3\" -1.49547 0.1477 6.76661e-006"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -115.218003"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"translate" " -type \"double3\" 0.636749 0.401059 -1.176979"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" -28.485617 -8.583143 -12.934254"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"visibility" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"translate" " -type \"double3\" -1.169293 0.7209 -0.323173"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotate" " -type \"double3\" -8.79714 12.867589 -62.388976"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "translate" " -type \"double3\" 0 5.04354 -0.965776"
		
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "rotate" " -type \"double3\" 83.32908 0 0"
		
		2 "|D_:rootGRP|D_:Root|D_:hip|D_:r_hip|D_:r_knee" "segmentScaleCompensate" 
		" 1"
		2 "D_:joints" "visibility" " 0"
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateX" 
		""
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateY" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateY" 
		""
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateZ" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateZ" 
		""
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" ""
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateY" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" ""
		3 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateZ" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" ""
		5 4 "D_RN" "|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[30]" ""
		
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[31]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[32]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[33]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[34]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[35]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[36]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[37]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[38]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[39]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[40]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[41]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[42]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[43]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[44]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[45]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[46]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[47]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[48]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[49]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[50]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[51]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[52]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[53]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[54]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[55]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[56]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[57]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[58]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[59]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[60]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[61]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[62]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[63]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[64]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[65]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[66]" 
		""
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" "D_RN.placeHolderList[67]" 
		"D_RN.placeHolderList[68]" "D_:Spine0Control.tx"
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateY" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" "D_RN.placeHolderList[69]" 
		"D_RN.placeHolderList[70]" "D_:Spine0Control.ty"
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control_pointConstraint1.constraintTranslateZ" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" "D_RN.placeHolderList[71]" 
		"D_RN.placeHolderList[72]" "D_:Spine0Control.tz"
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[73]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[74]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[75]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.scaleX" 
		"D_RN.placeHolderList[76]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.scaleY" 
		"D_RN.placeHolderList[77]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.scaleZ" 
		"D_RN.placeHolderList[78]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.rotateX" 
		"D_RN.placeHolderList[79]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.rotateY" 
		"D_RN.placeHolderList[80]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1.rotateZ" 
		"D_RN.placeHolderList[81]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.translateX" 
		"D_RN.placeHolderList[82]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.translateY" 
		"D_RN.placeHolderList[83]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[84]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[85]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[86]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[87]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.scaleX" 
		"D_RN.placeHolderList[88]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.scaleY" 
		"D_RN.placeHolderList[89]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.scaleZ" 
		"D_RN.placeHolderList[90]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:TankControl.visibility" 
		"D_RN.placeHolderList[91]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[92]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[93]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[94]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[95]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[96]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[97]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[98]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[99]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[100]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[101]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[102]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[103]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[104]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[105]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[106]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[107]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[108]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[109]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[110]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[111]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[112]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[113]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[114]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:L_Clavicle|D_:LShoulderFK|D_:RElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[115]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[116]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[117]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[118]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[119]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[120]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[121]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[122]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[123]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[124]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[125]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[126]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[127]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[128]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[129]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[130]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[131]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[132]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[133]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[134]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[135]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:R_Clavicle|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[136]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[137]" ""
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateX" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateX" 
		"D_RN.placeHolderList[138]" "D_RN.placeHolderList[139]" "D_:HeadControl.tx"
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateY" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateY" 
		"D_RN.placeHolderList[140]" "D_RN.placeHolderList[141]" "D_:HeadControl.ty"
		5 0 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl|D_:NeckControl_pointConstraint1.constraintTranslateZ" 
		"|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.translateZ" 
		"D_RN.placeHolderList[142]" "D_RN.placeHolderList[143]" "D_:HeadControl.tz"
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[144]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[145]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine0Control1|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[146]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.scaleX" "D_RN.placeHolderList[147]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.scaleY" "D_RN.placeHolderList[148]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.scaleZ" "D_RN.placeHolderList[149]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.rotateX" "D_RN.placeHolderList[150]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.rotateY" "D_RN.placeHolderList[151]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:HipControl.rotateZ" "D_RN.placeHolderList[152]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[153]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[154]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[155]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.radius" 
		"D_RN.placeHolderList[156]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[157]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[158]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[159]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.radius" 
		"D_RN.placeHolderList[160]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[161]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[162]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[163]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.radius" 
		"D_RN.placeHolderList[164]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[165]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[166]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[167]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.radius" 
		"D_RN.placeHolderList[168]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[169]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[170]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[171]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.radius" 
		"D_RN.placeHolderList[172]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[173]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[174]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[175]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.radius" 
		"D_RN.placeHolderList[176]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[177]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[178]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[179]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.radius" 
		"D_RN.placeHolderList[180]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[181]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[182]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[183]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.radius" 
		"D_RN.placeHolderList[184]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[185]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[186]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[187]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[188]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[189]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[190]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[191]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[192]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[193]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[194]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[195]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[196]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[197]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[198]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[199]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[200]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[201]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[202]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[203]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[204]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[205]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[206]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[207]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[208]" "";
	setAttr ".ptag" -type "string" "";
lockNode;
createNode reference -n "sharedReferenceNode";
	setAttr ".ed" -type "dataReferenceEdits" 
		"sharedReferenceNode";
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
		+ "                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n"
		+ "                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n"
		+ "                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dopeSheetPanel\" (localizedPanelLabel(\"Dope Sheet\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dopeSheetPanel\" -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n"
		+ "                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n"
		+ "                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n"
		+ "                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n"
		+ "                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"clipEditorPanel\" -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n                -snapValue \"none\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Trax Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = clipEditorNameFromPanel($panelName);\n            clipEditor -e \n                -displayKeys 0\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"none\" \n                -snapValue \"none\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperGraphPanel\" (localizedPanelLabel(\"Hypergraph Hierarchy\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperGraphPanel\" -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n"
		+ "                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypergraph Hierarchy\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"HyperGraphEd\");\n            hyperGraph -e \n                -graphLayoutStyle \"hierarchicalLayout\" \n                -orientation \"horiz\" \n                -zoom 1\n                -animateTransition 0\n                -showShapes 0\n                -showDeformers 0\n                -showExpressions 0\n                -showConstraints 0\n                -showUnderworld 0\n                -showInvisible 0\n                -transitionFrames 1\n                -freeform 0\n                -imagePosition 0 0 \n                -imageScale 1\n                -imageEnabled 0\n                -graphType \"DAG\" \n                -updateSelection 1\n                -updateNodeAdded 1\n                -useDrawOverrideColor 0\n                -limitGraphTraversal -1\n                -iconSize \"smallIcons\" \n                -showCachedConnections 0\n"
		+ "                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"hyperShadePanel\" (localizedPanelLabel(\"Hypershade\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"hyperShadePanel\" -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Hypershade\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"visorPanel\" (localizedPanelLabel(\"Visor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"visorPanel\" -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Visor\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"polyTexturePlacementPanel\" (localizedPanelLabel(\"UV Texture Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"polyTexturePlacementPanel\" -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"UV Texture Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"multiListerPanel\" (localizedPanelLabel(\"Multilister\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"multiListerPanel\" -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Multilister\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"renderWindowPanel\" (localizedPanelLabel(\"Render View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"renderWindowPanel\" -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Render View\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"blendShapePanel\" (localizedPanelLabel(\"Blend Shape\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\tblendShapePanel -unParent -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tblendShapePanel -edit -l (localizedPanelLabel(\"Blend Shape\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n"
		+ "\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynRelEdPanel\" (localizedPanelLabel(\"Dynamic Relationships\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynRelEdPanel\" -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dynamic Relationships\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"devicePanel\" (localizedPanelLabel(\"Devices\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\tdevicePanel -unParent -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels ;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tdevicePanel -edit -l (localizedPanelLabel(\"Devices\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"relationshipPanel\" (localizedPanelLabel(\"Relationship Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"relationshipPanel\" -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Relationship Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"referenceEditorPanel\" (localizedPanelLabel(\"Reference Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"referenceEditorPanel\" -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Reference Editor\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"componentEditorPanel\" (localizedPanelLabel(\"Component Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"componentEditorPanel\" -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Component Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dynPaintScriptedPanelType\" (localizedPanelLabel(\"Paint Effects\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dynPaintScriptedPanelType\" -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Paint Effects\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"webBrowserPanel\" (localizedPanelLabel(\"Web Browser\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"webBrowserPanel\" -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Web Browser\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"scriptEditorPanel\" (localizedPanelLabel(\"Script Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"single\\\" -ps 1 100 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 100 -size 100 -divisions 10 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 0 -max 24 -ast 0 -aet 24 ";
	setAttr ".st" 6;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine0Control1_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine0Control1_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine0Control1_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 11 ".ktv[0:10]"  0 1 7 1 10 1 15 1 22 1 25 1 31 1 36 1 40 
		1 46 1 52 1;
	setAttr -s 11 ".kit[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr -s 11 ".kot[0:10]"  3 9 9 9 3 3 3 3 
		9 9 9;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_thumb_2_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.5;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_mid_1_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.51758123775072173;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_mid_2_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.51777929065801409;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_pink_1_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.51592281818476815;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_pink_2_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.5043346543997419;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_point_1_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.5145160662748105;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_point_2_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.53450914938637284;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:l_thumb_1_radius";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  25 0.5;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 7 ".ktv[0:6]"  0 0 3 -1.1866208249125492 7 -1.6596727652653429 
		13 0 16 1.1719117898710461 18 1.6464233561971238 25 0;
	setAttr -s 7 ".kit[0:6]"  1 9 3 9 9 3 1;
	setAttr -s 7 ".kot[0:6]"  1 9 3 9 9 3 1;
	setAttr -s 7 ".kix[0:6]"  0.083595775067806244 0.13922081887722015 
		1 0.10535807907581329 0.10071482509374619 1 0.078981250524520874;
	setAttr -s 7 ".kiy[0:6]"  -0.99649977684020996 -0.99026137590408325 
		0 0.99443435668945313 0.99491536617279053 0 -0.99687618017196655;
	setAttr -s 7 ".kox[0:6]"  0.083595737814903259 0.13922081887722015 
		1 0.10535807907581329 0.10071482509374619 1 0.078981280326843262;
	setAttr -s 7 ".koy[0:6]"  -0.99649977684020996 -0.99026137590408325 
		0 0.99443435668945313 0.99491536617279053 0 -0.99687612056732178;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 7 ".ktv[0:6]"  0 0 3 -1.4760642211434962 7 -2.9521284422869924 
		13 0 16 -1.790211683390357 18 -2.9521284422869924 25 0;
	setAttr -s 7 ".kit[1:6]"  9 3 3 9 3 3;
	setAttr -s 7 ".kot[1:6]"  9 3 3 9 3 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0.73406369426751583 7 2.4056761399911544 
		13 0.78764427034695073 18 2.451003740449393 25 0.73406369426751583;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 5 0 8 0 10 0 13 1.0501666209874374e-015 
		16 0 18 0 22 0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 7 0 10 0 13 0 16 0 18 0 19 0 22 
		0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 3.7374630868666427 3 0 5 0 8 0 10 0 13 
		10.67752997493449 16 10.962790006058388 18 7.6481591803802065 22 5.1367907222161318 
		25 3.7374630868666427;
	setAttr -s 10 ".kit[5:9]"  9 9 9 9 3;
	setAttr -s 10 ".kot[5:9]"  9 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 11.650371934128051 3 10.176970051140328 
		7 5.9347570363807565 10 4.4189454817268263 13 3.7497422259639208 16 0 18 0 19 0 22 
		0 25 11.650371934128051;
	setAttr -s 10 ".kit[1:9]"  9 9 9 9 3 3 3 3 
		3;
	setAttr -s 10 ".kot[1:9]"  9 9 9 9 3 3 3 3 
		3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 7 ".ktv[0:6]"  0 9.4164884782754257 3 9.4164884782754257 
		10 -3.713947107965029 13 -3.9921816813255813 16 -2.1741059834016863 18 0.33369852971561775 
		25 9.4164884782754257;
	setAttr -s 7 ".kit[0:6]"  1 1 9 9 9 9 1;
	setAttr -s 7 ".kot[0:6]"  1 1 9 9 9 9 1;
	setAttr -s 7 ".ktl[0:6]" no no yes yes yes yes yes;
	setAttr -s 7 ".kix[0:6]"  0.094005793333053589 0.051538318395614624 
		0.024851858615875244 0.12880165874958038 0.038499247282743454 0.025874385610222816 
		0.037564307451248169;
	setAttr -s 7 ".kiy[0:6]"  0.99557167291641235 -0.99867105484008789 
		-0.99969112873077393 0.99167037010192871 0.99925863742828369 0.99966520071029663 
		0.9992942214012146;
	setAttr -s 7 ".kox[0:6]"  0.044312160462141037 0.024893205612897873 
		0.024851858615875244 0.12880165874958038 0.038499247282743454 0.025874385610222816 
		0.037564322352409363;
	setAttr -s 7 ".koy[0:6]"  0.99901777505874634 -0.99969011545181274 
		-0.99969112873077393 0.99167037010192871 0.99925863742828369 0.99966520071029663 
		0.9992942214012146;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 8 ".ktv[0:7]"  0 -4.4882789946720694 3 -3.0431869317290672 
		7 -0.1139968402453973 10 3.9193688171218199 13 10.450403915998972 16 10.188041056432432 
		22 -5.6710525538465522 25 -4.4882789946720694;
	setAttr -s 8 ".kit[0:7]"  1 9 9 9 9 9 9 1;
	setAttr -s 8 ".kot[0:7]"  1 9 9 9 9 9 9 1;
	setAttr -s 8 ".kix[0:7]"  0.10589123517274857 0.053266361355781555 
		0.033493794500827789 0.018928114324808121 0.031888458877801895 0.018605520948767662 
		0.020436819642782211 0.078392840921878815;
	setAttr -s 8 ".kiy[0:7]"  0.99437767267227173 0.99858039617538452 
		0.99943888187408447 0.99982088804244995 0.99949145317077637 -0.99982690811157227 
		-0.99979114532470703 0.99692255258560181;
	setAttr -s 8 ".kox[0:7]"  0.10589122027158737 0.053266361355781555 
		0.033493794500827789 0.018928114324808121 0.031888458877801895 0.018605520948767662 
		0.020436819642782211 0.078392781317234039;
	setAttr -s 8 ".koy[0:7]"  0.99437767267227173 0.99858039617538452 
		0.99943888187408447 0.99982088804244995 0.99949145317077637 -0.99982690811157227 
		-0.99979114532470703 0.99692255258560181;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -23.814719161186414 3 -23.476455397342285 
		5 0 8 0 10 0 13 93.699405014676231 16 119.96382471550849 18 94.662598770339486 22 
		14.954446854479496 25 -23.814719161186414;
	setAttr -s 10 ".kit[0:9]"  1 9 3 3 3 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  1 9 3 3 3 9 9 9 
		9 3;
	setAttr -s 10 ".kix[0:9]"  0.68167251348495483 0.37217706441879272 
		1 1 1 0.095088943839073181 0.99495160579681396 0.10848106443881989 0.11212866008281708 
		1;
	setAttr -s 10 ".kiy[0:9]"  -0.73165732622146606 0.92816174030303955 
		0 0 0 0.99546879529953003 0.10035619139671326 -0.9940984845161438 -0.99369364976882935 
		0;
	setAttr -s 10 ".kox[0:9]"  0.68167245388031006 0.37217706441879272 
		1 1 1 0.095088943839073181 0.99495160579681396 0.10848106443881989 0.11212866008281708 
		1;
	setAttr -s 10 ".koy[0:9]"  -0.73165738582611084 0.92816174030303955 
		0 0 0 0.99546879529953003 0.10035619139671326 -0.9940984845161438 -0.99369364976882935 
		0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 115.2797659590601 3 113.59764042807792 
		7 56.78788921401803 10 17.857064029063938 13 -24.559343644224555 16 -43.222805107840173 
		18 0 19 0 22 0 25 115.2797659590601;
	setAttr -s 10 ".kit[0:9]"  1 9 9 9 9 9 3 3 
		3 1;
	setAttr -s 10 ".kot[0:9]"  1 9 9 9 9 9 3 3 
		3 1;
	setAttr -s 10 ".kix[0:9]"  0.3156033456325531 0.22281597554683685 
		0.13829614222049713 0.13949000835418701 0.18439239263534546 0.36239489912986755 1 
		1 1 0.24306383728981018;
	setAttr -s 10 ".kiy[0:9]"  0.94889122247695923 -0.97486048936843872 
		-0.99039095640182495 -0.99022352695465088 -0.98285263776779175 0.93202471733093262 
		0 0 0 0.97001028060913086;
	setAttr -s 10 ".kox[0:9]"  0.3156033456325531 0.22281597554683685 
		0.13829614222049713 0.13949000835418701 0.18439239263534546 0.36239489912986755 1 
		1 1 0.24306386709213257;
	setAttr -s 10 ".koy[0:9]"  0.94889122247695923 -0.97486048936843872 
		-0.99039095640182495 -0.99022352695465088 -0.98285263776779175 0.93202471733093262 
		0 0 0 0.97001028060913086;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 -13.486307348051398 3 -9.725983608447974 
		5 -7.9713381174487328 8 -5.3946370920504823 10 0 13 0.63496697855908857 16 0.21763316505657512 
		18 0 22 0 25 -13.486307348051398;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 7 -0.74744941696963485 10 0 13 
		8.9693928844870943 16 7.8035398256202493 18 5.6307945973714428 19 4.582087080447609 
		22 0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 1 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 1 9 9 9 9 9 
		17 17;
	setAttr -s 10 ".kix[2:9]"  0.99136710166931152 0.76270866394042969 
		0.82654690742492676 0.94397091865539551 0.87166875600814819 0.80496847629547119 0.92852073907852173 
		1;
	setAttr -s 10 ".kiy[2:9]"  -0.1311153918504715 0.64674222469329834 
		0.56286787986755371 -0.33002841472625732 -0.49009555578231812 -0.59331756830215454 
		-0.37128067016601563 0;
	setAttr -s 10 ".kox[2:9]"  0.99136710166931152 0.76270866394042969 
		0.82654690742492676 0.94397091865539551 0.87166875600814819 0.80496847629547119 0 
		0;
	setAttr -s 10 ".koy[2:9]"  -0.1311153918504715 0.64674222469329834 
		0.56286787986755371 -0.33002841472625732 -0.49009555578231812 -0.59331756830215454 
		0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 5.8768016917806714 3 4.1964488591423237 
		5 3.4329979824460954 8 2.3216880589319646 10 0 13 -0.27324316189479103 16 -0.093616625893825592 
		18 0 22 0 25 5.8768016917806714;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 7 0.69854331973630246 10 0 13 -8.3825199952294742 
		16 -7.2719069082217427 18 -5.2447166704938182 19 -4.2675168192586641 22 0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 5 1 8 1 10 1 13 1 16 1 18 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 7 1 10 1 13 1 16 1 18 1 19 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 5 1 8 1 10 1 13 1 16 1 18 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 7 1 10 1 13 1 16 1 18 1 19 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 5 1 8 1 10 1 13 1 16 1 18 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 7 1 10 1 13 1 16 1 18 1 19 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 5 1 8 1 10 1 13 1 16 1 18 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 5 5 5 5 5 5 5 
		5 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 1 3 1 7 1 10 1 13 1 16 1 18 1 19 1 22 
		1 25 1;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 5 5 5 5 5 5 5 
		5 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 5 0 8 0 10 0 13 0 16 0 18 0 22 
		0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 7 0 10 0 13 0 16 0 18 0 19 0 22 
		0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 5 0 8 0 10 21.830500017690717 13 
		0 16 -2.1750005124829546 18 0 22 0 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 10 ".ktv[0:9]"  0 0 3 0 7 0 10 0 13 0 16 0 18 0 19 0 22 
		30.5 25 0;
	setAttr -s 10 ".kit[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr -s 10 ".kot[0:9]"  3 9 9 9 9 9 9 9 
		9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -3.0801111832072361 7 -8.0572511003833434 
		13 -3.5990647215603389 18 -0.089211488451144044 25 -3.0801111832072361;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 7 2.4531258580516435 13 6.653093400760711 
		18 8.1855901356935501 25 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 3.5482964349741364 7 3.5482964349741364 
		13 3.5482964349741364 18 3.5482964349741364 25 3.5482964349741364;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 3.5482964349741364 7 3.5482964349741346 
		13 3.548296434974135 18 3.5482964349741355 25 3.5482964349741364;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 7.8874849842219765 7 7.8874849842219765 
		13 7.8874849842219765 18 7.8874849842219765 25 7.8874849842219765;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 7.8874849842219765 7 7.8874849842219765 
		13 7.8874849842219765 18 7.8874849842219765 25 7.8874849842219765;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 7 0 13 0 18 0 25 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 22.023383027176749 7 0 13 -13.479171708519955 
		18 0 25 22.023383027176749;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 7 13.656406705446042 13 3.262090958078482 
		18 -11.517651586806673 25 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 7 0 13 0 18 0 25 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 19.186040572687354 7 0 13 -14.965745019187489 
		18 0 25 19.186040572687354;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 0 7 5.7305527218301977 13 5.7305527218301977 
		18 -5.1220196028718394 25 0;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.94030660390853882 0.97438281774520874 
		0.88845688104629517 0.97013276815414429 0.89754360914230347;
	setAttr -s 5 ".kiy[0:4]"  0.34032860398292542 0.22489583492279053 
		-0.4589601457118988 -0.24257448315620422 0.44092589616775513;
	setAttr -s 5 ".kox[0:4]"  0.94030654430389404 0.97438281774520874 
		0.88845688104629517 0.97013276815414429 0.89754348993301392;
	setAttr -s 5 ".koy[0:4]"  0.34032872319221497 0.22489583492279053 
		-0.4589601457118988 -0.24257448315620422 0.44092592597007751;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  1 -0.68688697485843608 8 0 14 -0.26513292809026229 
		19 0 26 -0.68688697485843608;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  2 -0.82894930692835722 9 4.4584146987800564 
		15 -4.2960546863918925 20 4.8247010018168472 27 -0.82894930692835722;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  1 -7.7197090727178592 8 0 14 11.983173230521055 
		19 0 26 -7.7197090727178592;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  2 -15.459162417016124 9 0 15 10.665692825758653 
		20 0 27 -15.459162417016124;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  1 1.8680135623590881 8 -2.3696607526622522 
		14 -4.3006530616091414 19 3.01895645314828 26 1.8680135623590881;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:Spine0Control1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  2 3.1070709397498799 9 -2.369660752662254 
		15 -4.0831535892485666 20 3.01895645314828 27 3.1070709397498799;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -27.01964136531959;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -37.091099892115828;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 51.942652070622316;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -9.5286162669966146;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -49.994308218224297;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -107.56144331697654;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -126.44159212033421;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -121.84485841982915;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -92.298803270737679;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -83.104976879379635;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -121.10403483939967;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -19.79146378847523;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 -54.05655437889979 9 -43.840370755512559 
		18 6.4084504279299415 22 -0.97669407575320621 30 -54.05655437889979;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -13.775448510702205 9 3.8687527262251504 
		16 50.253975706757089 22 17.800472732126018 28 -13.775448510702205;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 0 9 0 17 0 22 0 29 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 -25.259824170775119 9 -26.488173907498116 
		18 14.132352299825891 22 41.852836313426373 30 -25.259824170775119;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.67441457509994507 0.53320825099945068 
		0.34146237373352051 0.50288087129592896 0.43064287304878235;
	setAttr -s 5 ".kiy[0:4]"  -0.73835289478302002 0.84598398208618164 
		0.9398953914642334 -0.86435574293136597 -0.9025225043296814;
	setAttr -s 5 ".kox[0:4]"  0.67441463470458984 0.53320825099945068 
		0.34146237373352051 0.50288087129592896 0.43064284324645996;
	setAttr -s 5 ".koy[0:4]"  -0.73835283517837524 0.84598398208618164 
		0.9398953914642334 -0.86435574293136597 -0.90252244472503662;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -44.673610622706903 9 0.99601822553372987 
		16 39.961966675274837 22 1.2231233860022794 28 -44.673610622706903;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 -57.778548585398703 9 -63.387245556208264 
		17 -38.409103380695221 22 -42.309700703346188 29 -57.778548585398703;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.71145015954971313 0.78844887018203735 
		0.76233917474746704 0.7637636661529541 0.65072757005691528;
	setAttr -s 5 ".kiy[0:4]"  -0.70273655652999878 0.61510032415390015 
		0.64717769622802734 -0.64549601078033447 -0.7593112587928772;
	setAttr -s 5 ".kox[0:4]"  0.71145015954971313 0.78844887018203735 
		0.76233917474746704 0.7637636661529541 0.65072751045227051;
	setAttr -s 5 ".koy[0:4]"  -0.70273661613464355 0.61510032415390015 
		0.64717769622802734 -0.64549601078033447 -0.75931131839752197;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 -10.740917760156725 9 -9.5271792805073687 
		18 -39.552497650226883 22 -40.807633535205738 30 -10.740917760156725;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.81712812185287476 0.6527978777885437 
		0.62169498205184937 0.62252366542816162 0.72073441743850708;
	setAttr -s 5 ".kiy[0:4]"  0.57645618915557861 -0.75753211975097656 
		-0.78325945138931274 0.78260093927383423 0.69321143627166748;
	setAttr -s 5 ".kox[0:4]"  0.81712812185287476 0.6527978777885437 
		0.62169498205184937 0.62252366542816162 0.72073447704315186;
	setAttr -s 5 ".koy[0:4]"  0.57645612955093384 -0.75753211975097656 
		-0.78325945138931274 0.78260093927383423 0.69321131706237793;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -27.944839755341693 9 -35.385989974524016 
		16 -20.823397973930227 22 -46.531657738385093 28 -27.944839755341693;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 0 9 0 17 0 22 0 29 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -8.7971401573922208;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -28.485617385803991;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 12.867588697404495;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -8.583142820760413;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -62.388975689521537;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -104.89020811605528;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -110.60443389104233;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -99.60665648873848;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -101.37478254074362;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -78.352908365889178;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -115.21800320317323;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -12.934254295111689;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 42.265388664186069 9 30.456701313058513 
		18 -20.270974445556881 23 -1.5259437554557687 30 42.265388664186069;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 0 9 0 17 0 23 0 29 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 -10.803205230674127 9 -28.893212957839317 
		18 28.886777608521914 23 30.756869867931307 30 -10.803205230674127;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.29407897591590881 0.53033578395843506 
		0.40903472900390625 0.50005382299423218 0.28179660439491272;
	setAttr -s 5 ".kiy[0:4]"  -0.95578116178512573 0.84778773784637451 
		0.91251885890960693 -0.86599433422088623 -0.95947414636611938;
	setAttr -s 5 ".kox[0:4]"  0.2940790057182312 0.53033578395843506 
		0.40903472900390625 0.50005382299423218 0.2817966639995575;
	setAttr -s 5 ".koy[0:4]"  -0.95578110218048096 0.84778773784637451 
		0.91251885890960693 -0.86599433422088623 -0.95947414636611938;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 41.620609199886921 9 50.654361545255071 
		17 75.292183675902052 23 62.375672514424096 29 41.620609199886921;
	setAttr -s 5 ".kit[0:4]"  3 9 9 1 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 1 3;
	setAttr -s 5 ".kix[3:4]"  0.43714991211891174 1;
	setAttr -s 5 ".kiy[3:4]"  -0.89938861131668091 0;
	setAttr -s 5 ".kox[3:4]"  0.43714994192123413 1;
	setAttr -s 5 ".koy[3:4]"  -0.89938867092132568 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 32.922129761550678 9 34.597625603773828 
		18 22.658914861299856 23 21.782423428115955 30 32.922129761550678;
	setAttr -s 5 ".kit[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kot[0:4]"  1 9 9 9 1;
	setAttr -s 5 ".kix[0:4]"  0.91997402906417847 0.92415493726730347 
		0.90177357196807861 0.91266548633575439 0.89181512594223022;
	setAttr -s 5 ".kiy[0:4]"  0.39197930693626404 -0.38201770186424255 
		-0.43220877647399902 0.4087073802947998 0.45240002870559692;
	setAttr -s 5 ".kox[0:4]"  0.91997408866882324 0.92415493726730347 
		0.90177357196807861 0.91266548633575439 0.891815185546875;
	setAttr -s 5 ".koy[0:4]"  0.39197924733161926 -0.38201770186424255 
		-0.43220877647399902 0.4087073802947998 0.45239990949630737;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RElbowFK_rotateZ1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  4 0 9 0 17 0 23 0 29 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 24.214664684250785 9 4.0608765483956217 
		16 -25.791620796513278 23 1.4488748086882413 28 24.214664684250785;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -58.767566701644526 9 -17.912885391826617 
		16 27.535606346205668 23 -29.584122675474397 28 -58.767566701644526;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 23.449684327567784 9 50.235287591068492 
		16 42.823656373622782 23 39.73343136475556 28 23.449684327567784;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0 10 11.086141007897119 16 -0.016911332583479353 
		21 11.077744495416358 28 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  3 4.2335837063757022 16 -9.2852514161142174 
		28 4.2335837063757022;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  3 -3.8545576096239689 16 3.6181270212851824 
		28 -3.8545576096239689;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  5 0 12 9.8 18 0 23 9.8 30 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.40284009285740136 10 -0.0082753657521474691 
		16 0.41286186497764726 21 -0.0082753657521474691 28 0.40284009285740136;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.69383970005596174 10 -0.98297777847196466 
		16 0.73384019965719582 21 -0.98297777847196466 28 0.69383970005596174;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -0.049587362203235327 10 0.07624749756716459 
		16 -0.00095329156016320347 21 0.07624749756716459 28 -0.049587362203235327;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0 10 -3.7535780140979309 16 0 21 -3.7535780140979309 
		28 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 -2.0698932382803128 10 0 16 2.3120836553640776 
		21 0 28 -2.0698932382803128;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0 10 0 16 0 21 0 28 0;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:TankControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  3 1 10 1 21 1 28 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:TankControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  3 1 10 1 21 1 28 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:TankControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  3 1 10 1 21 1 28 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  3 1 10 1 21 1 28 1;
	setAttr -s 4 ".kot[0:3]"  5 5 5 5;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:DiverGlobal_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 0 25 185.55457216234069;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.035303236806917715 9 -0.0033393082234525889 
		16 -0.0033393082234525889 22 -0.0033393082234525889 28 0.035303236806917715;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.31822258295239547 9 -0.63869888993881974 
		16 -0.013819243960647959 22 -0.38739472521449142 28 0.31822258295239547;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.07126988441364944 9 0.07422190607982905 
		16 0.074221906079830383 22 0.074221906079829494 28 0.07126988441364944;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.043718243828962305 9 -0.020545545417686998 
		16 -0.066947265747242482 23 -0.047077050920017197 28 0.043718243828962305;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.42599942233575511 9 -0.46388741575742609 
		16 0.47820569341294006 23 -0.5426579599653123 28 0.42599942233575511;
	setAttr -s 5 ".kit[0:4]"  3 9 9 9 3;
	setAttr -s 5 ".kot[0:4]"  3 9 9 9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  3 0.0054181731410484733 9 0.04646324916282249 
		16 0.02973023417746631 23 0.041831951598461072 28 0.0054181731410484733;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode mute -n "DiverGlobal_translateZ";
	setAttr ".ihi" 0;
	setAttr ".h" 0;
	setAttr ".m" yes;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 20;
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
	setAttr -s 4 ".gn";
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
connectAttr "DiverGlobal_translateZ.o" "D_RN.phl[30]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[31]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[32]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[33]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[34]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[35]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[36]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[37]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[38]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[39]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[40]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[41]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[42]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[43]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[44]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[45]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[46]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[47]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[48]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[49]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[50]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[51]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[52]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[53]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[54]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[55]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[56]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[57]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[58]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[59]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[60]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[61]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[62]";
connectAttr "D_:RootControl_translateZ.o" "D_RN.phl[63]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[64]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[65]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[66]";
connectAttr "D_RN.phl[67]" "D_RN.phl[68]";
connectAttr "D_RN.phl[69]" "D_RN.phl[70]";
connectAttr "D_RN.phl[71]" "D_RN.phl[72]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[73]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[74]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[75]";
connectAttr "D_:Spine0Control1_scaleX.o" "D_RN.phl[76]";
connectAttr "D_:Spine0Control1_scaleY.o" "D_RN.phl[77]";
connectAttr "D_:Spine0Control1_scaleZ.o" "D_RN.phl[78]";
connectAttr "D_:Spine0Control1_rotateX.o" "D_RN.phl[79]";
connectAttr "D_:Spine0Control1_rotateY.o" "D_RN.phl[80]";
connectAttr "D_:Spine0Control1_rotateZ.o" "D_RN.phl[81]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[82]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[83]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[84]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[85]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[86]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[87]";
connectAttr "D_:TankControl_scaleX.o" "D_RN.phl[88]";
connectAttr "D_:TankControl_scaleY.o" "D_RN.phl[89]";
connectAttr "D_:TankControl_scaleZ.o" "D_RN.phl[90]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[91]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[92]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[93]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[94]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[95]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[96]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[97]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[98]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[99]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[100]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[101]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[102]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[103]";
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[104]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[105]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[106]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[107]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[108]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[109]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[110]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[111]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[112]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[113]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[114]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[115]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[116]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[117]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[118]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[119]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[120]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[121]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[122]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[123]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[124]";
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[125]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[126]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[127]";
connectAttr "D_:RElbowFK_rotateX1.o" "D_RN.phl[128]";
connectAttr "D_:RElbowFK_rotateY1.o" "D_RN.phl[129]";
connectAttr "D_:RElbowFK_rotateZ1.o" "D_RN.phl[130]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[131]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[132]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[133]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[134]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[135]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[136]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[137]";
connectAttr "D_RN.phl[138]" "D_RN.phl[139]";
connectAttr "D_RN.phl[140]" "D_RN.phl[141]";
connectAttr "D_RN.phl[142]" "D_RN.phl[143]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[144]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[145]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[146]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[147]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[148]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[149]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[150]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[151]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[152]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[153]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[154]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[155]";
connectAttr "D_:l_mid_1_radius.o" "D_RN.phl[156]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[157]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[158]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[159]";
connectAttr "D_:l_mid_2_radius.o" "D_RN.phl[160]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[161]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[162]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[163]";
connectAttr "D_:l_pink_1_radius.o" "D_RN.phl[164]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[165]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[166]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[167]";
connectAttr "D_:l_pink_2_radius.o" "D_RN.phl[168]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[169]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[170]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[171]";
connectAttr "D_:l_point_1_radius.o" "D_RN.phl[172]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[173]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[174]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[175]";
connectAttr "D_:l_point_2_radius.o" "D_RN.phl[176]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[177]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[178]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[179]";
connectAttr "D_:l_thumb_1_radius.o" "D_RN.phl[180]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[181]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[182]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[183]";
connectAttr "D_:l_thumb_2_radius.o" "D_RN.phl[184]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[185]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[186]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[187]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[188]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[189]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[190]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[191]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[192]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[193]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[194]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[195]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[196]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[197]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[198]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[199]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[200]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[201]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[202]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[203]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[204]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[205]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[206]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[207]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[208]";
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
connectAttr "D_RN.phl[23]" "D_RN.phl[24]";
connectAttr "sharedReferenceNode.sr" "D_RN.sr";
connectAttr "D_:DiverGlobal_translateZ.o" "DiverGlobal_translateZ.i";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_jog.ma
