//Maya ASCII 2008 scene
//Name: diver_idle_act2.ma
//Last modified: Mon, Aug 25, 2008 11:32:38 AM
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
	setAttr ".t" -type "double3" 43.700329733876188 96.424522100216336 260.20317896016866 ;
	setAttr ".r" -type "double3" -9.3383527296305466 12.199999999975988 2.0337777271019718e-016 ;
createNode camera -s -n "perspShape" -p "persp";
	setAttr -k off ".v" no;
	setAttr ".fl" 34.999999999999986;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 276.13455750949288;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".tp" -type "double3" 0.13336041401070986 63.330162048339844 1.5007535259840061 ;
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -s -n "top";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 1.3074966051082839 5000.1011786268054 3.1568149640537424 ;
	setAttr ".r" -type "double3" -89.999999999999986 0 0 ;
createNode camera -s -n "topShape" -p "top";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 241.91431310177924;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 1.3074966051082839 47.291083971224168 5000.3239475150294 ;
createNode camera -s -n "frontShape" -p "front";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 244.47891311405269;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 5000.3246904243961 47.291083971224168 3.1568149640537531 ;
	setAttr ".r" -type "double3" 0 89.999999999999986 0 ;
createNode camera -s -n "sideShape" -p "side";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".ncp" 1;
	setAttr ".fcp" 100000;
	setAttr ".coi" 5000.1000000000004;
	setAttr ".ow" 228.86438399180477;
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
createNode displayLayer -n "defaultLayer";
createNode renderLayerManager -n "renderLayerManager";
createNode renderLayer -n "defaultRenderLayer";
	setAttr ".g" yes;
createNode reference -n "D_RN";
	setAttr -s 224 ".phl";
	setAttr ".phl[2334]" 0;
	setAttr ".phl[2335]" 0;
	setAttr ".phl[2336]" 0;
	setAttr ".phl[2337]" 0;
	setAttr ".phl[2338]" 0;
	setAttr ".phl[2339]" 0;
	setAttr ".phl[2340]" 0;
	setAttr ".phl[2341]" 0;
	setAttr ".phl[2342]" 0;
	setAttr ".phl[2343]" 0;
	setAttr ".phl[2344]" 0;
	setAttr ".phl[2345]" 0;
	setAttr ".phl[2346]" 0;
	setAttr ".phl[2347]" 0;
	setAttr ".phl[2348]" 0;
	setAttr ".phl[2349]" 0;
	setAttr ".phl[2350]" 0;
	setAttr ".phl[2351]" 0;
	setAttr ".phl[2352]" 0;
	setAttr ".phl[2353]" 0;
	setAttr ".phl[2354]" 0;
	setAttr ".phl[2355]" 0;
	setAttr ".phl[2356]" 0;
	setAttr ".phl[2357]" 0;
	setAttr ".phl[2358]" 0;
	setAttr ".phl[2359]" 0;
	setAttr ".phl[2360]" 0;
	setAttr ".phl[2361]" 0;
	setAttr ".phl[2362]" 0;
	setAttr ".phl[2363]" 0;
	setAttr ".phl[2364]" 0;
	setAttr ".phl[2365]" 0;
	setAttr ".phl[2366]" 0;
	setAttr ".phl[2367]" 0;
	setAttr ".phl[2368]" 0;
	setAttr ".phl[2369]" 0;
	setAttr ".phl[2370]" 0;
	setAttr ".phl[2371]" 0;
	setAttr ".phl[2372]" 0;
	setAttr ".phl[2373]" 0;
	setAttr ".phl[2374]" 0;
	setAttr ".phl[2375]" 0;
	setAttr ".phl[2376]" 0;
	setAttr ".phl[2377]" 0;
	setAttr ".phl[2378]" 0;
	setAttr ".phl[2379]" 0;
	setAttr ".phl[2380]" 0;
	setAttr ".phl[2381]" 0;
	setAttr ".phl[2382]" 0;
	setAttr ".phl[2383]" 0;
	setAttr ".phl[2384]" 0;
	setAttr ".phl[2385]" 0;
	setAttr ".phl[2386]" 0;
	setAttr ".phl[2387]" 0;
	setAttr ".phl[2388]" 0;
	setAttr ".phl[2389]" 0;
	setAttr ".phl[2390]" 0;
	setAttr ".phl[2391]" 0;
	setAttr ".phl[2392]" 0;
	setAttr ".phl[2393]" 0;
	setAttr ".phl[2394]" 0;
	setAttr ".phl[2395]" 0;
	setAttr ".phl[2396]" 0;
	setAttr ".phl[2397]" 0;
	setAttr ".phl[2398]" 0;
	setAttr ".phl[2399]" 0;
	setAttr ".phl[2400]" 0;
	setAttr ".phl[2401]" 0;
	setAttr ".phl[2402]" 0;
	setAttr ".phl[2403]" 0;
	setAttr ".phl[2404]" 0;
	setAttr ".phl[2405]" 0;
	setAttr ".phl[2406]" 0;
	setAttr ".phl[2407]" 0;
	setAttr ".phl[2408]" 0;
	setAttr ".phl[2409]" 0;
	setAttr ".phl[2410]" 0;
	setAttr ".phl[2411]" 0;
	setAttr ".phl[2412]" 0;
	setAttr ".phl[2413]" 0;
	setAttr ".phl[2414]" 0;
	setAttr ".phl[2415]" 0;
	setAttr ".phl[2416]" 0;
	setAttr ".phl[2417]" 0;
	setAttr ".phl[2418]" 0;
	setAttr ".phl[2419]" 0;
	setAttr ".phl[2420]" 0;
	setAttr ".phl[2421]" 0;
	setAttr ".phl[2422]" 0;
	setAttr ".phl[2423]" 0;
	setAttr ".phl[2424]" 0;
	setAttr ".phl[2425]" 0;
	setAttr ".phl[2426]" 0;
	setAttr ".phl[2427]" 0;
	setAttr ".phl[2428]" 0;
	setAttr ".phl[2429]" 0;
	setAttr ".phl[2430]" 0;
	setAttr ".phl[2431]" 0;
	setAttr ".phl[2432]" 0;
	setAttr ".phl[2433]" 0;
	setAttr ".phl[2434]" 0;
	setAttr ".phl[2435]" 0;
	setAttr ".phl[2436]" 0;
	setAttr ".phl[2437]" 0;
	setAttr ".phl[2438]" 0;
	setAttr ".phl[2439]" 0;
	setAttr ".phl[2440]" 0;
	setAttr ".phl[2441]" 0;
	setAttr ".phl[2442]" 0;
	setAttr ".phl[2443]" 0;
	setAttr ".phl[2444]" 0;
	setAttr ".phl[2445]" 0;
	setAttr ".phl[2446]" 0;
	setAttr ".phl[2447]" 0;
	setAttr ".phl[2448]" 0;
	setAttr ".phl[2449]" 0;
	setAttr ".phl[2450]" 0;
	setAttr ".phl[2451]" 0;
	setAttr ".phl[2452]" 0;
	setAttr ".phl[2453]" 0;
	setAttr ".phl[2454]" 0;
	setAttr ".phl[2455]" 0;
	setAttr ".phl[2456]" 0;
	setAttr ".phl[2457]" 0;
	setAttr ".phl[2458]" 0;
	setAttr ".phl[2459]" 0;
	setAttr ".phl[2460]" 0;
	setAttr ".phl[2461]" 0;
	setAttr ".phl[2462]" 0;
	setAttr ".phl[2463]" 0;
	setAttr ".phl[2464]" 0;
	setAttr ".phl[2465]" 0;
	setAttr ".phl[2466]" 0;
	setAttr ".phl[2467]" 0;
	setAttr ".phl[2468]" 0;
	setAttr ".phl[2469]" 0;
	setAttr ".phl[2470]" 0;
	setAttr ".phl[2471]" 0;
	setAttr ".phl[2472]" 0;
	setAttr ".phl[2473]" 0;
	setAttr ".phl[2474]" 0;
	setAttr ".phl[2475]" 0;
	setAttr ".phl[2476]" 0;
	setAttr ".phl[2477]" 0;
	setAttr ".phl[2478]" 0;
	setAttr ".phl[2479]" 0;
	setAttr ".phl[2480]" 0;
	setAttr ".phl[2481]" 0;
	setAttr ".phl[2482]" 0;
	setAttr ".phl[2483]" 0;
	setAttr ".phl[2484]" 0;
	setAttr ".phl[2485]" 0;
	setAttr ".phl[2486]" 0;
	setAttr ".phl[2487]" 0;
	setAttr ".phl[2488]" 0;
	setAttr ".phl[2489]" 0;
	setAttr ".phl[2490]" 0;
	setAttr ".phl[2491]" 0;
	setAttr ".phl[2492]" 0;
	setAttr ".phl[2493]" 0;
	setAttr ".phl[2494]" 0;
	setAttr ".phl[2495]" 0;
	setAttr ".phl[2496]" 0;
	setAttr ".phl[2497]" 0;
	setAttr ".phl[2498]" 0;
	setAttr ".phl[2499]" 0;
	setAttr ".phl[2500]" 0;
	setAttr ".phl[2501]" 0;
	setAttr ".phl[2502]" 0;
	setAttr ".phl[2503]" 0;
	setAttr ".phl[2504]" 0;
	setAttr ".phl[2505]" 0;
	setAttr ".phl[2506]" 0;
	setAttr ".phl[2507]" 0;
	setAttr ".phl[2508]" 0;
	setAttr ".phl[2509]" 0;
	setAttr ".phl[2510]" 0;
	setAttr ".phl[2511]" 0;
	setAttr ".phl[2512]" 0;
	setAttr ".phl[2513]" 0;
	setAttr ".phl[2514]" 0;
	setAttr ".phl[2515]" 0;
	setAttr ".phl[2516]" 0;
	setAttr ".phl[2517]" 0;
	setAttr ".phl[2518]" 0;
	setAttr ".phl[2519]" 0;
	setAttr ".phl[2520]" 0;
	setAttr ".phl[2521]" 0;
	setAttr ".phl[2522]" 0;
	setAttr ".phl[2523]" 0;
	setAttr ".phl[2524]" 0;
	setAttr ".phl[2525]" 0;
	setAttr ".phl[2526]" 0;
	setAttr ".phl[2527]" 0;
	setAttr ".phl[2528]" 0;
	setAttr ".phl[2529]" 0;
	setAttr ".phl[2530]" 0;
	setAttr ".phl[2531]" 0;
	setAttr ".phl[2532]" 0;
	setAttr ".phl[2533]" 0;
	setAttr ".phl[2534]" 0;
	setAttr ".phl[2535]" 0;
	setAttr ".phl[2536]" 0;
	setAttr ".phl[2537]" 0;
	setAttr ".phl[2538]" 0;
	setAttr ".phl[2539]" 0;
	setAttr ".phl[2540]" 0;
	setAttr ".phl[2541]" 0;
	setAttr ".phl[2542]" 0;
	setAttr ".phl[2543]" 0;
	setAttr ".phl[2544]" 0;
	setAttr ".phl[2545]" 0;
	setAttr ".phl[2546]" 0;
	setAttr ".phl[2547]" 0;
	setAttr ".phl[2548]" 0;
	setAttr ".phl[2549]" 0;
	setAttr ".phl[2550]" 0;
	setAttr ".ed" -type "dataReferenceEdits" 
		"D_RN"
		"D_RN" 7
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[2327]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[2328]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[2329]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[2330]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[2331]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[2332]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[2333]" ""
		"D_RN" 379
		2 "|D_:Diver" "visibility" " 1"
		2 "|D_:Diver|D_:DiverShape" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShape" "intermediateObject" " 0"
		2 "|D_:Diver|D_:DiverShapeOrig" "visibility" " -k 0 1"
		2 "|D_:Diver|D_:DiverShapeOrig" "intermediateObject" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotate" " -type \"double3\" 0 0 -36.447187"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotate" " -type \"double3\" 0 0 -42.374435"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotate" " -type \"double3\" 0 0 -49.675424"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotate" " -type \"double3\" 0 0 -52.631685"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotate" " -type \"double3\" -4.544922 -0.68954 -21.556453"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotate" " -type \"double3\" 0 0 -30.250319"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotate" " -type \"double3\" -13.295361 -5.056349 -8.0534"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotate" " -type \"double3\" 0 0 -6.895673"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotate" " -type \"double3\" 0 0 -49.207258"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotate" " -type \"double3\" 0 0 -37.069649"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotate" " -type \"double3\" 0 0 -58.684035"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotate" " -type \"double3\" 0 0 -34.60943"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotate" " -type \"double3\" 0 0 -34.787072"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotate" " -type \"double3\" 0 0 -74.810674"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotate" " -type \"double3\" -12.471697 -16.052852 -8.092112"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateX" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateY" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"rotateZ" " -av"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1" 
		"segmentScaleCompensate" " 1"
		2 "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2" 
		"rotate" " -type \"double3\" 5.512847 14.069184 -45.375658"
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
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translate" " -type \"double3\" 9.451348 0 0"
		
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:L_Knee" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translate" " -type \"double3\" -6.915389 0 0"
		
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:R_Knee" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translate" " -type \"double3\" 0.0269367 -10.091368 -2.060157"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotate" " -type \"double3\" 2.332557 -0.0200671 -0.0100336"
		
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl" "rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotate" " -type \"double3\" 7.264694 -0.125503 2.523678"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateX" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateY" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control" "rotateZ" " -av"
		
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotate" " -type \"double3\" 6.320346 0.00841513 0.0197946"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translate" " -type \"double3\" -0.219719 -8.65432 -2.833738"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotate" " -type \"double3\" 2.528437 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translate" " -type \"double3\" 0.00321059 -1.306611 0.443048"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle" 
		"translateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle" 
		"translate" " -type \"double3\" -0.00190451 -2.107386 0.592046"
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
		"rotate" " -type \"double3\" -15.325383 1.087294 -2.560455"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scale" " -type \"double3\" 1 1 1"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl" 
		"scaleZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translate" " -type \"double3\" 0 0 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateX" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateY" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"translateZ" " -k 0"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotate" " -type \"double3\" 15.684454 -5.380935 -55.542243"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK" 
		"rotate" " -type \"double3\" 20.560333 -29.152968 -10.546356"
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
		"rotate" " -type \"double3\" -0.0960434 -2.366711 56.630598"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateX" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateY" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK" 
		"rotateZ" " -av"
		2 "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK" 
		"rotate" " -type \"double3\" 0 37.836727 0"
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
		2 "D_:joints" "visibility" " 1"
		2 "D_:geometry" "displayType" " 2"
		2 "D_:geometry" "visibility" " 1"
		2 "D_:rightControl" "visibility" " 1"
		2 "D_:torso" "visibility" " 1"
		2 "D_:skinCluster1" "nodeState" " 0"
		5 4 "D_RN" "|D_:Entity.visibility" "D_RN.placeHolderList[2334]" ""
		5 4 "D_RN" "|D_:Entity.translateX" "D_RN.placeHolderList[2335]" ""
		5 4 "D_RN" "|D_:Entity.translateY" "D_RN.placeHolderList[2336]" ""
		5 4 "D_RN" "|D_:Entity.translateZ" "D_RN.placeHolderList[2337]" ""
		5 4 "D_RN" "|D_:Entity.rotateX" "D_RN.placeHolderList[2338]" ""
		5 4 "D_RN" "|D_:Entity.rotateY" "D_RN.placeHolderList[2339]" ""
		5 4 "D_RN" "|D_:Entity.rotateZ" "D_RN.placeHolderList[2340]" ""
		5 4 "D_RN" "|D_:Entity.scaleX" "D_RN.placeHolderList[2341]" ""
		5 4 "D_RN" "|D_:Entity.scaleY" "D_RN.placeHolderList[2342]" ""
		5 4 "D_RN" "|D_:Entity.scaleZ" "D_RN.placeHolderList[2343]" ""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateX" "D_RN.placeHolderList[2344]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateY" "D_RN.placeHolderList[2345]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.translateZ" "D_RN.placeHolderList[2346]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateX" "D_RN.placeHolderList[2347]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateY" "D_RN.placeHolderList[2348]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.rotateZ" "D_RN.placeHolderList[2349]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleX" "D_RN.placeHolderList[2350]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleY" "D_RN.placeHolderList[2351]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.scaleZ" "D_RN.placeHolderList[2352]" 
		""
		5 4 "D_RN" "|D_:Entity|D_:DiverGlobal.visibility" "D_RN.placeHolderList[2353]" 
		""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateX" 
		"D_RN.placeHolderList[2354]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateY" 
		"D_RN.placeHolderList[2355]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.rotateZ" 
		"D_RN.placeHolderList[2356]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1.visibility" 
		"D_RN.placeHolderList[2357]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateX" 
		"D_RN.placeHolderList[2358]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateY" 
		"D_RN.placeHolderList[2359]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_mid_1|D_:l_mid_2.rotateZ" 
		"D_RN.placeHolderList[2360]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateX" 
		"D_RN.placeHolderList[2361]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateY" 
		"D_RN.placeHolderList[2362]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.rotateZ" 
		"D_RN.placeHolderList[2363]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1.visibility" 
		"D_RN.placeHolderList[2364]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateX" 
		"D_RN.placeHolderList[2365]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateY" 
		"D_RN.placeHolderList[2366]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_pink_1|D_:l_pink_2.rotateZ" 
		"D_RN.placeHolderList[2367]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateX" 
		"D_RN.placeHolderList[2368]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateY" 
		"D_RN.placeHolderList[2369]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.rotateZ" 
		"D_RN.placeHolderList[2370]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1.visibility" 
		"D_RN.placeHolderList[2371]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateX" 
		"D_RN.placeHolderList[2372]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateY" 
		"D_RN.placeHolderList[2373]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_point_1|D_:l_point_2.rotateZ" 
		"D_RN.placeHolderList[2374]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateX" 
		"D_RN.placeHolderList[2375]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateY" 
		"D_RN.placeHolderList[2376]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.rotateZ" 
		"D_RN.placeHolderList[2377]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1.visibility" 
		"D_RN.placeHolderList[2378]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateX" 
		"D_RN.placeHolderList[2379]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateY" 
		"D_RN.placeHolderList[2380]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:l_clavicle|D_:l_shoulder|D_:l_elbow|D_:l_wrist|D_:l_hand|D_:l_thumb_1|D_:l_thumb_2.rotateZ" 
		"D_RN.placeHolderList[2381]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateX" 
		"D_RN.placeHolderList[2382]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateY" 
		"D_RN.placeHolderList[2383]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.rotateZ" 
		"D_RN.placeHolderList[2384]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1.visibility" 
		"D_RN.placeHolderList[2385]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateX" 
		"D_RN.placeHolderList[2386]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateY" 
		"D_RN.placeHolderList[2387]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.rotateZ" 
		"D_RN.placeHolderList[2388]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_mid_1|D_:r_mid_2.visibility" 
		"D_RN.placeHolderList[2389]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateX" 
		"D_RN.placeHolderList[2390]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateY" 
		"D_RN.placeHolderList[2391]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.rotateZ" 
		"D_RN.placeHolderList[2392]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1.visibility" 
		"D_RN.placeHolderList[2393]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateX" 
		"D_RN.placeHolderList[2394]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateY" 
		"D_RN.placeHolderList[2395]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.rotateZ" 
		"D_RN.placeHolderList[2396]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_pink_1|D_:r_pink_2.visibility" 
		"D_RN.placeHolderList[2397]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateX" 
		"D_RN.placeHolderList[2398]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateY" 
		"D_RN.placeHolderList[2399]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.rotateZ" 
		"D_RN.placeHolderList[2400]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1.visibility" 
		"D_RN.placeHolderList[2401]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateX" 
		"D_RN.placeHolderList[2402]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateY" 
		"D_RN.placeHolderList[2403]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.rotateZ" 
		"D_RN.placeHolderList[2404]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_point_1|D_:r_point_2.visibility" 
		"D_RN.placeHolderList[2405]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateX" 
		"D_RN.placeHolderList[2406]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateY" 
		"D_RN.placeHolderList[2407]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.rotateZ" 
		"D_RN.placeHolderList[2408]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1.visibility" 
		"D_RN.placeHolderList[2409]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateX" 
		"D_RN.placeHolderList[2410]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateY" 
		"D_RN.placeHolderList[2411]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.rotateZ" 
		"D_RN.placeHolderList[2412]" ""
		5 4 "D_RN" "|D_:rootGRP|D_:Root|D_:spine0|D_:spine1|D_:r_clavicle|D_:r_shoulder|D_:r_elbow|D_:r_wrist|D_:r_hand|D_:r_thumb_1|D_:r_thumb_2.visibility" 
		"D_RN.placeHolderList[2413]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.ToeRoll" "D_RN.placeHolderList[2414]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.BallRoll" "D_RN.placeHolderList[2415]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateX" "D_RN.placeHolderList[2416]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateY" "D_RN.placeHolderList[2417]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.translateZ" "D_RN.placeHolderList[2418]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateX" "D_RN.placeHolderList[2419]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateY" "D_RN.placeHolderList[2420]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.rotateZ" "D_RN.placeHolderList[2421]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleX" "D_RN.placeHolderList[2422]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleY" "D_RN.placeHolderList[2423]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.scaleZ" "D_RN.placeHolderList[2424]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Foot.visibility" "D_RN.placeHolderList[2425]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.ToeRoll" "D_RN.placeHolderList[2426]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.BallRoll" "D_RN.placeHolderList[2427]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateX" "D_RN.placeHolderList[2428]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateY" "D_RN.placeHolderList[2429]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.translateZ" "D_RN.placeHolderList[2430]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateX" "D_RN.placeHolderList[2431]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateY" "D_RN.placeHolderList[2432]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.rotateZ" "D_RN.placeHolderList[2433]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleX" "D_RN.placeHolderList[2434]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleY" "D_RN.placeHolderList[2435]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.scaleZ" "D_RN.placeHolderList[2436]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Foot.visibility" "D_RN.placeHolderList[2437]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateX" "D_RN.placeHolderList[2438]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateY" "D_RN.placeHolderList[2439]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.translateZ" "D_RN.placeHolderList[2440]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleX" "D_RN.placeHolderList[2441]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleY" "D_RN.placeHolderList[2442]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.scaleZ" "D_RN.placeHolderList[2443]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:L_Knee.visibility" "D_RN.placeHolderList[2444]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateX" "D_RN.placeHolderList[2445]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateY" "D_RN.placeHolderList[2446]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.translateZ" "D_RN.placeHolderList[2447]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleX" "D_RN.placeHolderList[2448]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleY" "D_RN.placeHolderList[2449]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.scaleZ" "D_RN.placeHolderList[2450]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:R_Knee.visibility" "D_RN.placeHolderList[2451]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateX" "D_RN.placeHolderList[2452]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateY" "D_RN.placeHolderList[2453]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.translateZ" "D_RN.placeHolderList[2454]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateX" "D_RN.placeHolderList[2455]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateY" "D_RN.placeHolderList[2456]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.rotateZ" "D_RN.placeHolderList[2457]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleX" "D_RN.placeHolderList[2458]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleY" "D_RN.placeHolderList[2459]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.scaleZ" "D_RN.placeHolderList[2460]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl.visibility" "D_RN.placeHolderList[2461]" 
		""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateX" 
		"D_RN.placeHolderList[2462]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateY" 
		"D_RN.placeHolderList[2463]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.translateZ" 
		"D_RN.placeHolderList[2464]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleX" 
		"D_RN.placeHolderList[2465]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleY" 
		"D_RN.placeHolderList[2466]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.scaleZ" 
		"D_RN.placeHolderList[2467]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateX" 
		"D_RN.placeHolderList[2468]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateY" 
		"D_RN.placeHolderList[2469]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.rotateZ" 
		"D_RN.placeHolderList[2470]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control.visibility" 
		"D_RN.placeHolderList[2471]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleX" 
		"D_RN.placeHolderList[2472]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleY" 
		"D_RN.placeHolderList[2473]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.scaleZ" 
		"D_RN.placeHolderList[2474]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateX" 
		"D_RN.placeHolderList[2475]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateY" 
		"D_RN.placeHolderList[2476]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.rotateZ" 
		"D_RN.placeHolderList[2477]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control.visibility" 
		"D_RN.placeHolderList[2478]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateX" 
		"D_RN.placeHolderList[2479]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateY" 
		"D_RN.placeHolderList[2480]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.rotateZ" 
		"D_RN.placeHolderList[2481]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateX" 
		"D_RN.placeHolderList[2482]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateY" 
		"D_RN.placeHolderList[2483]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.translateZ" 
		"D_RN.placeHolderList[2484]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:TankControl.visibility" 
		"D_RN.placeHolderList[2485]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleX" 
		"D_RN.placeHolderList[2486]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleY" 
		"D_RN.placeHolderList[2487]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2488]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateX" 
		"D_RN.placeHolderList[2489]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateY" 
		"D_RN.placeHolderList[2490]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.translateZ" 
		"D_RN.placeHolderList[2491]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:L_Clavicle.visibility" 
		"D_RN.placeHolderList[2492]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleX" 
		"D_RN.placeHolderList[2493]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleY" 
		"D_RN.placeHolderList[2494]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.scaleZ" 
		"D_RN.placeHolderList[2495]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateX" 
		"D_RN.placeHolderList[2496]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateY" 
		"D_RN.placeHolderList[2497]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.translateZ" 
		"D_RN.placeHolderList[2498]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:R_Clavicle.visibility" 
		"D_RN.placeHolderList[2499]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.Mask" 
		"D_RN.placeHolderList[2500]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateX" 
		"D_RN.placeHolderList[2501]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateY" 
		"D_RN.placeHolderList[2502]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.rotateZ" 
		"D_RN.placeHolderList[2503]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:HeadControl.visibility" 
		"D_RN.placeHolderList[2504]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleX" 
		"D_RN.placeHolderList[2505]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleY" 
		"D_RN.placeHolderList[2506]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2507]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateX" 
		"D_RN.placeHolderList[2508]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateY" 
		"D_RN.placeHolderList[2509]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2510]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK.visibility" 
		"D_RN.placeHolderList[2511]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateX" 
		"D_RN.placeHolderList[2512]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateY" 
		"D_RN.placeHolderList[2513]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.rotateZ" 
		"D_RN.placeHolderList[2514]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK.visibility" 
		"D_RN.placeHolderList[2515]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleX" 
		"D_RN.placeHolderList[2516]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleY" 
		"D_RN.placeHolderList[2517]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.scaleZ" 
		"D_RN.placeHolderList[2518]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateX" 
		"D_RN.placeHolderList[2519]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateY" 
		"D_RN.placeHolderList[2520]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.rotateZ" 
		"D_RN.placeHolderList[2521]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:LShoulderFK|D_:LElbowFK|D_:L_Wrist.visibility" 
		"D_RN.placeHolderList[2522]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleX" 
		"D_RN.placeHolderList[2523]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleY" 
		"D_RN.placeHolderList[2524]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.scaleZ" 
		"D_RN.placeHolderList[2525]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateX" 
		"D_RN.placeHolderList[2526]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateY" 
		"D_RN.placeHolderList[2527]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.rotateZ" 
		"D_RN.placeHolderList[2528]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK.visibility" 
		"D_RN.placeHolderList[2529]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleX" 
		"D_RN.placeHolderList[2530]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleY" 
		"D_RN.placeHolderList[2531]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.scaleZ" 
		"D_RN.placeHolderList[2532]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateX" 
		"D_RN.placeHolderList[2533]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateY" 
		"D_RN.placeHolderList[2534]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.rotateZ" 
		"D_RN.placeHolderList[2535]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK.visibility" 
		"D_RN.placeHolderList[2536]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleX" 
		"D_RN.placeHolderList[2537]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleY" 
		"D_RN.placeHolderList[2538]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.scaleZ" 
		"D_RN.placeHolderList[2539]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateX" 
		"D_RN.placeHolderList[2540]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateY" 
		"D_RN.placeHolderList[2541]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.rotateZ" 
		"D_RN.placeHolderList[2542]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:Spine0Control|D_:Spine1Control|D_:RShoulderFK|D_:RElbowFK|D_:R_Wrist.visibility" 
		"D_RN.placeHolderList[2543]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleX" 
		"D_RN.placeHolderList[2544]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleY" 
		"D_RN.placeHolderList[2545]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.scaleZ" 
		"D_RN.placeHolderList[2546]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateX" 
		"D_RN.placeHolderList[2547]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateY" 
		"D_RN.placeHolderList[2548]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.rotateZ" 
		"D_RN.placeHolderList[2549]" ""
		5 4 "D_RN" "|D_:controlcurvesGRP|D_:RootControl|D_:HipControl.visibility" 
		"D_RN.placeHolderList[2550]" "";
	setAttr ".ptag" -type "string" "";
lockNode;
createNode mentalrayItemsList -s -n "mentalrayItemsList";
createNode mentalrayGlobals -s -n "mentalrayGlobals";
createNode mentalrayOptions -s -n "miDefaultOptions";
	setAttr ".maxr" 2;
createNode mentalrayFramebuffer -s -n "miDefaultFramebuffer";
createNode script -n "uiConfigurationScriptNode";
	setAttr ".b" -type "string" (
		"// Maya Mel UI Configuration File.\n//\n//  This script is machine generated.  Edit at your own risk.\n//\n//\n\nglobal string $gMainPane;\nif (`paneLayout -exists $gMainPane`) {\n\n\tglobal int $gUseScenePanelConfig;\n\tint    $useSceneConfig = $gUseScenePanelConfig;\n\tint    $menusOkayInPanels = `optionVar -q allowMenusInPanels`;\tint    $nVisPanes = `paneLayout -q -nvp $gMainPane`;\n\tint    $nPanes = 0;\n\tstring $editorName;\n\tstring $panelName;\n\tstring $itemFilterName;\n\tstring $panelConfig;\n\n\t//\n\t//  get current state of the UI\n\t//\n\tsceneUIReplacement -update $gMainPane;\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Top View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"top\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n"
		+ "                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n"
		+ "                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Top View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"top\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n"
		+ "            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Side View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"side\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n"
		+ "                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Side View\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"side\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n"
		+ "            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Front View\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n"
		+ "                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Front View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Persp View\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -rendererName \"base_OpenGL_Renderer\" \n                -colorResolution 256 256 \n                -bumpResolution 512 512 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 1\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n"
		+ "                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Persp View\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n"
		+ "        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -rendererName \"base_OpenGL_Renderer\" \n"
		+ "            -colorResolution 256 256 \n            -bumpResolution 512 512 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 1\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"Outliner\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n"
		+ "                -showUnitlessCurves 1\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 1\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\toutlinerPanel -edit -l (localizedPanelLabel(\"Outliner\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 1\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 1\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n"
		+ "            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"graphEditor\" (localizedPanelLabel(\"Graph Editor\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"graphEditor\" -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels `;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 1\n"
		+ "                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n            animCurveEditor -e \n                -displayKeys 1\n"
		+ "                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Graph Editor\")) -mbv $menusOkayInPanels  $panelName;\n\n\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 1\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n"
		+ "                -expandConnections 1\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 1\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 1\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"GraphEd\");\n"
		+ "            animCurveEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 1\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -showResults \"off\" \n                -showBufferCurves \"off\" \n                -smoothness \"fine\" \n                -resultSamples 0.5\n                -resultScreenSamples 0\n                -resultUpdate \"delayed\" \n                -clipTime \"on\" \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\tif ($useSceneConfig) {\n\t\tscriptedPanel -e -to $panelName;\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"dopeSheetPanel\" (localizedPanelLabel(\"Dope Sheet\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"dopeSheetPanel\" -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n"
		+ "                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Dope Sheet\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t\t$editorName = ($panelName+\"OutlineEd\");\n            outlinerEditor -e \n                -showShapes 1\n                -showAttributes 1\n                -showConnected 1\n                -showAnimCurvesOnly 1\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 0\n                -ignoreDagHierarchy 0\n                -expandConnections 1\n                -showUnitlessCurves 0\n                -showCompounds 1\n                -showLeafs 1\n                -showNumericAttrsOnly 1\n                -highlightActive 0\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 1\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"0\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n"
		+ "                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\n\t\t\t$editorName = ($panelName+\"DopeSheetEd\");\n            dopeSheetEditor -e \n                -displayKeys 1\n                -displayTangents 0\n                -displayActiveKeys 0\n                -displayActiveKeyTangents 0\n                -displayInfinities 0\n                -autoFit 0\n                -snapTime \"integer\" \n                -snapValue \"none\" \n                -outliner \"dopeSheetPanel1OutlineEd\" \n                -showSummary 1\n                -showScene 0\n                -hierarchyBelow 0\n                -showTicks 1\n                -selectionWindow 0 0 0 0 \n                $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextScriptedPanel \"clipEditorPanel\" (localizedPanelLabel(\"Trax Editor\")) `;\n"
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
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `scriptedPanel -unParent  -type \"scriptEditorPanel\" -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels `;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tscriptedPanel -edit -l (localizedPanelLabel(\"Script Editor\")) -mbv $menusOkayInPanels  $panelName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n"
		+ "                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 0\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 0\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n"
		+ "            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"outlinerPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `outlinerPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            outlinerEditor -e \n                -showShapes 0\n                -showAttributes 0\n                -showConnected 0\n                -showAnimCurvesOnly 0\n                -showMuteInfo 0\n                -autoExpand 0\n                -showDagOnly 1\n                -ignoreDagHierarchy 0\n                -expandConnections 0\n                -showUnitlessCurves 1\n                -showCompounds 0\n                -showLeafs 1\n                -showNumericAttrsOnly 0\n                -highlightActive 1\n                -autoSelectNewObjects 0\n"
		+ "                -doNotSelectNewObjects 0\n                -dropIsParent 1\n                -transmitFilters 0\n                -setFilter \"defaultSetFilter\" \n                -showSetMembers 0\n                -allowMultiSelection 1\n                -alwaysToggleSelect 0\n                -directSelect 0\n                -displayMode \"DAG\" \n                -expandObjects 0\n                -setsIgnoreFilters 1\n                -editAttrName 0\n                -showAttrValues 0\n                -highlightSecondary 0\n                -showUVAttrsOnly 0\n                -showTextureNodesOnly 0\n                -attrAlphaOrder \"default\" \n                -sortOrder \"none\" \n                -longNames 0\n                -niceNames 1\n                -showNamespace 1\n                $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\toutlinerPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        outlinerEditor -e \n            -showShapes 0\n            -showAttributes 0\n"
		+ "            -showConnected 0\n            -showAnimCurvesOnly 0\n            -showMuteInfo 0\n            -autoExpand 0\n            -showDagOnly 1\n            -ignoreDagHierarchy 0\n            -expandConnections 0\n            -showUnitlessCurves 1\n            -showCompounds 0\n            -showLeafs 1\n            -showNumericAttrsOnly 0\n            -highlightActive 1\n            -autoSelectNewObjects 0\n            -doNotSelectNewObjects 0\n            -dropIsParent 1\n            -transmitFilters 0\n            -setFilter \"defaultSetFilter\" \n            -showSetMembers 0\n            -allowMultiSelection 1\n            -alwaysToggleSelect 0\n            -directSelect 0\n            -displayMode \"DAG\" \n            -expandObjects 0\n            -setsIgnoreFilters 1\n            -editAttrName 0\n            -showAttrValues 0\n            -highlightSecondary 0\n            -showUVAttrsOnly 0\n            -showTextureNodesOnly 0\n            -attrAlphaOrder \"default\" \n            -sortOrder \"none\" \n            -longNames 0\n            -niceNames 1\n"
		+ "            -showNamespace 1\n            $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel16\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel16\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n"
		+ "                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n"
		+ "                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel16\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n"
		+ "            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n"
		+ "            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel17\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel17\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n"
		+ "                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel17\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel18\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel18\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n"
		+ "                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel18\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n"
		+ "            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel19\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel19\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel19\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel22\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel22\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n"
		+ "                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel22\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel23\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel23\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel23\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel24\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel24\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"none\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n"
		+ "                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel24\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"none\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n"
		+ "            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel25\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel25\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n                -manipulators 1\n                -dimensions 0\n                -handles 1\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel25\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 1\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel26\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel26\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel26\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n"
		+ "            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel27\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel27\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n"
		+ "                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel27\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel28\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel28\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n"
		+ "                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel28\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n"
		+ "            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n"
		+ "            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel29\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel29\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel29\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n"
		+ "                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n"
		+ "                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n"
		+ "                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n"
		+ "            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n"
		+ "            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n"
		+ "\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n"
		+ "                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n"
		+ "                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n"
		+ "        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n"
		+ "            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n"
		+ "            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n"
		+ "                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n"
		+ "                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n"
		+ "                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n"
		+ "            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n"
		+ "            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n"
		+ "                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n"
		+ "                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n"
		+ "                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n"
		+ "                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n"
		+ "                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n"
		+ "                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n"
		+ "            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n"
		+ "            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n"
		+ "            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n"
		+ "                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n"
		+ "                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n"
		+ "            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n"
		+ "            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\n"
		+ "modelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n"
		+ "                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n"
		+ "                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n"
		+ "                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n"
		+ "            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n"
		+ "            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n"
		+ "            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n"
		+ "                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n"
		+ "                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n"
		+ "            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n"
		+ "            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\n"
		+ "modelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel30\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel30\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n"
		+ "                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n"
		+ "                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n"
		+ "\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel30\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n"
		+ "            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n"
		+ "            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel31\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel31\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n"
		+ "                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n"
		+ "                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel31\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n"
		+ "            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n"
		+ "            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel32\")) `;\n\tif (\"\" == $panelName) {\n"
		+ "\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel32\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n"
		+ "                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n"
		+ "                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel32\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n"
		+ "            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n"
		+ "            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel33\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel33\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel33\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel40\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel40\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n"
		+ "                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel40\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel41\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel41\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel41\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n"
		+ "            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel42\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel42\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n"
		+ "                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel42\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n"
		+ "            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel43\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel43\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel43\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel44\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel44\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel44\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n"
		+ "            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel46\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel46\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n"
		+ "                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n"
		+ "                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel46\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel47\")) `;\n"
		+ "\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel47\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n"
		+ "                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n"
		+ "                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel47\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n"
		+ "            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n"
		+ "            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n"
		+ "            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel48\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel48\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n"
		+ "                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n"
		+ "                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n"
		+ "                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel48\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n"
		+ "            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n"
		+ "            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel49\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel49\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"front\" \n"
		+ "                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n"
		+ "                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 1\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n"
		+ "                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel49\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"front\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n"
		+ "            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n"
		+ "            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 1\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel53\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel53\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n"
		+ "                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n"
		+ "                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel53\")) -mbv $menusOkayInPanels  $panelName;\n"
		+ "\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n"
		+ "            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n"
		+ "            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel54\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel54\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n"
		+ "                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n"
		+ "                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 1\n"
		+ "                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel54\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n"
		+ "            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n"
		+ "            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 1\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 1\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel55\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel55\")) -mbv $menusOkayInPanels `;\n"
		+ "\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 0\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n"
		+ "                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n"
		+ "                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 0\n                -deformers 0\n                -dynamics 1\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n                -nCloths 0\n                -nRigids 0\n                -dynamicConstraints 0\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 1\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel55\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n"
		+ "            -headsUpDisplay 0\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n"
		+ "            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n            -ikHandles 0\n            -deformers 0\n            -dynamics 1\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 0\n            -nRigids 0\n            -dynamicConstraints 0\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 1\n            -strokes 0\n            -shadows 0\n"
		+ "            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"Model Panel60\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"Model Panel60\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n                -wireframeOnShaded 1\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 1\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 0\n"
		+ "                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n"
		+ "                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 1\n                -nurbsSurfaces 1\n                -polymeshes 0\n                -subdivSurfaces 1\n                -planes 1\n                -lights 1\n                -cameras 1\n                -controlVertices 1\n                -hulls 1\n                -grid 1\n                -joints 1\n                -ikHandles 1\n                -deformers 1\n                -dynamics 1\n                -fluids 1\n                -hairSystems 1\n                -follicles 1\n                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 1\n                -manipulators 1\n                -dimensions 1\n                -handles 1\n                -pivots 1\n                -textures 1\n                -strokes 1\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n"
		+ "\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"Model Panel60\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 1\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 1\n            -xray 0\n            -jointXray 0\n            -activeComponentsXray 0\n            -displayTextures 0\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n"
		+ "            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 1\n            -nurbsSurfaces 1\n            -polymeshes 0\n            -subdivSurfaces 1\n            -planes 1\n            -lights 1\n            -cameras 1\n            -controlVertices 1\n            -hulls 1\n            -grid 1\n            -joints 1\n"
		+ "            -ikHandles 1\n            -deformers 1\n            -dynamics 1\n            -fluids 1\n            -hairSystems 1\n            -follicles 1\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 1\n            -manipulators 1\n            -dimensions 1\n            -handles 1\n            -pivots 1\n            -textures 1\n            -strokes 1\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\t$panelName = `sceneUIReplacement -getNextPanel \"modelPanel\" (localizedPanelLabel(\"\")) `;\n\tif (\"\" == $panelName) {\n\t\tif ($useSceneConfig) {\n\t\t\t$panelName = `modelPanel -unParent -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels `;\n\t\t\t$editorName = $panelName;\n            modelEditor -e \n                -camera \"persp\" \n                -useInteractiveMode 0\n                -displayLights \"default\" \n                -displayAppearance \"smoothShaded\" \n                -activeOnly 0\n"
		+ "                -wireframeOnShaded 0\n                -headsUpDisplay 1\n                -selectionHiliteDisplay 1\n                -useDefaultMaterial 0\n                -bufferMode \"double\" \n                -twoSidedLighting 1\n                -backfaceCulling 0\n                -xray 0\n                -jointXray 0\n                -activeComponentsXray 0\n                -displayTextures 1\n                -smoothWireframe 0\n                -lineWidth 1\n                -textureAnisotropic 0\n                -textureHilight 1\n                -textureSampling 2\n                -textureDisplay \"modulate\" \n                -textureMaxSize 4096\n                -fogging 0\n                -fogSource \"fragment\" \n                -fogMode \"linear\" \n                -fogStart 0\n                -fogEnd 100\n                -fogDensity 0.1\n                -fogColor 0.5 0.5 0.5 1 \n                -maxConstantTransparency 1\n                -colorResolution 4 4 \n                -bumpResolution 4 4 \n                -textureCompression 0\n"
		+ "                -transparencyAlgorithm \"frontAndBackCull\" \n                -transpInShadows 0\n                -cullingOverride \"none\" \n                -lowQualityLighting 0\n                -maximumNumHardwareLights 0\n                -occlusionCulling 0\n                -shadingModel 0\n                -useBaseRenderer 0\n                -useReducedRenderer 0\n                -smallObjectCulling 0\n                -smallObjectThreshold -1 \n                -interactiveDisableShadows 0\n                -interactiveBackFaceCull 0\n                -sortTransparent 1\n                -nurbsCurves 0\n                -nurbsSurfaces 0\n                -polymeshes 1\n                -subdivSurfaces 0\n                -planes 0\n                -lights 0\n                -cameras 0\n                -controlVertices 1\n                -hulls 1\n                -grid 0\n                -joints 0\n                -ikHandles 0\n                -deformers 0\n                -dynamics 0\n                -fluids 0\n                -hairSystems 0\n                -follicles 0\n"
		+ "                -nCloths 1\n                -nRigids 1\n                -dynamicConstraints 1\n                -locators 0\n                -manipulators 1\n                -dimensions 0\n                -handles 0\n                -pivots 0\n                -textures 0\n                -strokes 0\n                -shadows 0\n                $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\t}\n\t} else {\n\t\t$label = `panel -q -label $panelName`;\n\t\tmodelPanel -edit -l (localizedPanelLabel(\"\")) -mbv $menusOkayInPanels  $panelName;\n\t\t$editorName = $panelName;\n        modelEditor -e \n            -camera \"persp\" \n            -useInteractiveMode 0\n            -displayLights \"default\" \n            -displayAppearance \"smoothShaded\" \n            -activeOnly 0\n            -wireframeOnShaded 0\n            -headsUpDisplay 1\n            -selectionHiliteDisplay 1\n            -useDefaultMaterial 0\n            -bufferMode \"double\" \n            -twoSidedLighting 1\n            -backfaceCulling 0\n            -xray 0\n            -jointXray 0\n"
		+ "            -activeComponentsXray 0\n            -displayTextures 1\n            -smoothWireframe 0\n            -lineWidth 1\n            -textureAnisotropic 0\n            -textureHilight 1\n            -textureSampling 2\n            -textureDisplay \"modulate\" \n            -textureMaxSize 4096\n            -fogging 0\n            -fogSource \"fragment\" \n            -fogMode \"linear\" \n            -fogStart 0\n            -fogEnd 100\n            -fogDensity 0.1\n            -fogColor 0.5 0.5 0.5 1 \n            -maxConstantTransparency 1\n            -colorResolution 4 4 \n            -bumpResolution 4 4 \n            -textureCompression 0\n            -transparencyAlgorithm \"frontAndBackCull\" \n            -transpInShadows 0\n            -cullingOverride \"none\" \n            -lowQualityLighting 0\n            -maximumNumHardwareLights 0\n            -occlusionCulling 0\n            -shadingModel 0\n            -useBaseRenderer 0\n            -useReducedRenderer 0\n            -smallObjectCulling 0\n            -smallObjectThreshold -1 \n"
		+ "            -interactiveDisableShadows 0\n            -interactiveBackFaceCull 0\n            -sortTransparent 1\n            -nurbsCurves 0\n            -nurbsSurfaces 0\n            -polymeshes 1\n            -subdivSurfaces 0\n            -planes 0\n            -lights 0\n            -cameras 0\n            -controlVertices 1\n            -hulls 1\n            -grid 0\n            -joints 0\n            -ikHandles 0\n            -deformers 0\n            -dynamics 0\n            -fluids 0\n            -hairSystems 0\n            -follicles 0\n            -nCloths 1\n            -nRigids 1\n            -dynamicConstraints 1\n            -locators 0\n            -manipulators 1\n            -dimensions 0\n            -handles 0\n            -pivots 0\n            -textures 0\n            -strokes 0\n            -shadows 0\n            $editorName;\nmodelEditor -e -viewSelected 0 $editorName;\n\t\tif (!$useSceneConfig) {\n\t\t\tpanel -e -l $label $panelName;\n\t\t}\n\t}\n\n\n\tif ($useSceneConfig) {\n        string $configName = `getPanel -cwl (localizedPanelLabel(\"Current Layout\"))`;\n"
		+ "        if (\"\" != $configName) {\n\t\t\tpanelConfiguration -edit -label (localizedPanelLabel(\"Current Layout\")) \n\t\t\t\t-defaultImage \"\"\n\t\t\t\t-image \"\"\n\t\t\t\t-sc false\n\t\t\t\t-configString \"global string $gMainPane; paneLayout -e -cn \\\"single\\\" -ps 1 100 100 $gMainPane;\"\n\t\t\t\t-removeAllPanels\n\t\t\t\t-ap false\n\t\t\t\t\t(localizedPanelLabel(\"Persp View\")) \n\t\t\t\t\t\"modelPanel\"\n"
		+ "\t\t\t\t\t\"$panelName = `modelPanel -unParent -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels `;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t\t\"modelPanel -edit -l (localizedPanelLabel(\\\"Persp View\\\")) -mbv $menusOkayInPanels  $panelName;\\n$editorName = $panelName;\\nmodelEditor -e \\n    -cam `findStartUpCamera persp` \\n    -useInteractiveMode 0\\n    -displayLights \\\"default\\\" \\n    -displayAppearance \\\"smoothShaded\\\" \\n    -activeOnly 0\\n    -wireframeOnShaded 0\\n    -headsUpDisplay 1\\n    -selectionHiliteDisplay 1\\n    -useDefaultMaterial 0\\n    -bufferMode \\\"double\\\" \\n    -twoSidedLighting 1\\n    -backfaceCulling 0\\n    -xray 0\\n    -jointXray 0\\n    -activeComponentsXray 0\\n    -displayTextures 1\\n    -smoothWireframe 0\\n    -lineWidth 1\\n    -textureAnisotropic 0\\n    -textureHilight 1\\n    -textureSampling 2\\n    -textureDisplay \\\"modulate\\\" \\n    -textureMaxSize 4096\\n    -fogging 0\\n    -fogSource \\\"fragment\\\" \\n    -fogMode \\\"linear\\\" \\n    -fogStart 0\\n    -fogEnd 100\\n    -fogDensity 0.1\\n    -fogColor 0.5 0.5 0.5 1 \\n    -maxConstantTransparency 1\\n    -rendererName \\\"base_OpenGL_Renderer\\\" \\n    -colorResolution 256 256 \\n    -bumpResolution 512 512 \\n    -textureCompression 0\\n    -transparencyAlgorithm \\\"frontAndBackCull\\\" \\n    -transpInShadows 0\\n    -cullingOverride \\\"none\\\" \\n    -lowQualityLighting 0\\n    -maximumNumHardwareLights 1\\n    -occlusionCulling 0\\n    -shadingModel 0\\n    -useBaseRenderer 0\\n    -useReducedRenderer 0\\n    -smallObjectCulling 0\\n    -smallObjectThreshold -1 \\n    -interactiveDisableShadows 0\\n    -interactiveBackFaceCull 0\\n    -sortTransparent 1\\n    -nurbsCurves 1\\n    -nurbsSurfaces 1\\n    -polymeshes 1\\n    -subdivSurfaces 1\\n    -planes 1\\n    -lights 1\\n    -cameras 1\\n    -controlVertices 1\\n    -hulls 1\\n    -grid 1\\n    -joints 1\\n    -ikHandles 1\\n    -deformers 1\\n    -dynamics 1\\n    -fluids 1\\n    -hairSystems 1\\n    -follicles 1\\n    -nCloths 1\\n    -nRigids 1\\n    -dynamicConstraints 1\\n    -locators 1\\n    -manipulators 1\\n    -dimensions 1\\n    -handles 1\\n    -pivots 1\\n    -textures 1\\n    -strokes 1\\n    -shadows 0\\n    $editorName;\\nmodelEditor -e -viewSelected 0 $editorName\"\n"
		+ "\t\t\t\t$configName;\n\n            setNamedPanelLayout (localizedPanelLabel(\"Current Layout\"));\n        }\n\n        panelHistory -e -clear mainPanelHistory;\n        setFocus `paneLayout -q -p1 $gMainPane`;\n        sceneUIReplacement -deleteRemaining;\n        sceneUIReplacement -clear;\n\t}\n\n\ngrid -spacing 500 -size 5000 -divisions 1 -displayAxes yes -displayGridLines yes -displayDivisionLines yes -displayPerspectiveLabels no -displayOrthographicLabels no -displayAxesBold yes -perspectiveLabelPosition axis -orthographicLabelPosition edge;\n}\n");
	setAttr ".st" 3;
createNode script -n "sceneConfigurationScriptNode";
	setAttr ".b" -type "string" "playbackOptions -min 0 -max 172 -ast 0 -aet 172 ";
	setAttr ".st" 6;
createNode animCurveTL -n "D_:L_Foot_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 2.9964341932866638 8 2.9964341932866638 
		14 2.9964341932866638 32 2.9964341932866638 39 2.9964341932866638 48 2.9964341932866638 
		62 2.9964341932866638 64 2.9964341932866638 76 2.9964341932866638 82 2.9964341932866638 
		86 2.9964341932866638 91 2.9964341932866638 96 2.9964341932866638 100 2.9964341932866638 
		105 2.9964341932866638 108 2.9964341932866638 112 2.9964341932866638 114 2.9964341932866638 
		119 2.9964341932866638 123 2.9964341932866638 126 2.9964341932866638 130 2.9964341932866638 
		146 2.9964341932866638 154 2.9964341932866638 162 2.9964341932866638 166 2.9964341932866638 
		172 2.9964341932866638;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:RootControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0.021774381346857739 39 
		0.034890427548663686 48 0 62 -0.15290969423717365 64 -0.38638635800735788 76 -3.7450154893333218 
		82 -4.1345456794532875 86 -4.1980420310647766 91 -4.2270814781295609 96 -4.242285509345912 
		100 -4.2585288391021319 105 -4.2725052204113334 108 -4.1980420310647766 112 -3.9406907946170278 
		114 -3.5048587271205851 119 -2.6025003406144975 123 -1.8733339823116513 126 -1.1688154094166761 
		130 -0.61017749920329711 146 0.46874305292619312 154 0.34139865840542666 162 0.18833200388727592 
		166 0.11332013887059988 172 0;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTL -n "D_:Spine0Control_translateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.0004640926137400277 8 0.0051240951018666023 
		14 -0.0014392282846949424 32 -0.0019795585093731263 39 -0.0017888730806488661 48 
		-0.0014392282846949424 62 -0.0054963501737444759 64 -0.0014392282846949424 76 0.067500864787830753 
		82 0.028701587366028722 86 0.028701587366028722 91 0.028701587366028722 96 0.028701587366028722 
		100 0.028701587366028722 105 0.028701587366028722 108 0.028701587366028722 112 0.028701587366028722 
		114 0.028701587366028722 119 0.028701587366028722 123 0.028701587366028722 126 0.028701587366028722 
		130 0.028701587366028722 146 0.028701587366028722 154 0.028701587366028722 162 0.010368874491333294 
		166 0.0058357047757915184 172 -0.0004640926137400277;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Clavicle_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.0043857699112451395 8 0.01000714257622564 
		14 0.0036605033889653901 32 0.0031380118280148555 39 0.0033224018976759809 48 0.0036605033889653901 
		62 0.0036634617908931277 64 0.0036605033889653901 76 0.0036605033889653901 82 0.0036605033889653901 
		86 0.0036605033889653901 91 0.0036605033889653901 96 0.0036605033889653901 100 0.0036605033889653901 
		105 0.0036605033889653901 108 0.0036605033889653901 112 0.0036605033889653901 114 
		0.0036605033889653901 119 0.0036605033889653901 123 0.0036605033889653901 126 0.0036605033889653901 
		130 0.0036605033889653901 146 0.0062299546255203822 154 0.0036605033889653901 162 
		0.0041163851966176234 166 0.0042291120809822419 172 0.0043857699112451395;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:TankControl_translateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.0025643796042819182 8 -0.02244503960018427 
		14 -0.35785652658104006 32 -0.20952494150601192 39 -0.25733994772687036 48 -0.31158187315074987 
		62 -0.31428816448110963 64 -0.31428816448110963 76 3.1602247302806399 82 4.0949601803234144 
		86 4.6077013466418304 91 4.8379832685574939 96 4.7784838643841425 100 4.6597289696597883 
		105 4.6597289696597883 108 4.245631114677729 112 2.0165421384327442 114 -0.025108830144092487 
		119 -1.7890770552980726 123 -1.7890770552980726 126 -1.7890770552980726 130 -1.7890770552980726 
		146 -1.7890770552980726 154 -3.0864569485285895 162 0.0023863742172688307 166 0.0025643796042819182 
		172 0.0025643796042819182;
	setAttr -s 27 ".kit[16:26]"  1 1 9 9 9 9 9 9 
		9 9 10;
	setAttr -s 27 ".kot[16:26]"  1 1 9 9 9 9 9 9 
		9 9 10;
	setAttr -s 27 ".kix[16:26]"  0.03754144161939621 0.04811893031001091 
		0.16766357421875 1 1 1 0.52486437559127808 0.28533205389976501 0.12841871380805969 
		0.99999988079071045 1;
	setAttr -s 27 ".kiy[16:26]"  -0.99929505586624146 -0.99884164333343506 
		-0.98584431409835815 0 0 0 -0.85118591785430908 0.95842868089675903 0.99172008037567139 
		0.00053401663899421692 0;
	setAttr -s 27 ".kox[16:26]"  0.037541437894105911 0.048118934035301208 
		0.16766357421875 1 1 1 0.52486437559127808 0.28533205389976501 0.12841871380805969 
		0.99999988079071045 1;
	setAttr -s 27 ".koy[16:26]"  -0.99929511547088623 -0.99884164333343506 
		-0.98584431409835815 0 0 0 -0.85118591785430908 0.95842868089675903 0.99172008037567139 
		0.00053401663899421692 0;
createNode animCurveTL -n "D_:R_Knee_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -6.915388563106255 8 -6.915388563106255 
		14 -6.915388563106255 32 -6.915388563106255 39 -6.915388563106255 48 -6.915388563106255 
		62 -6.915388563106255 64 -6.915388563106255 76 -6.915388563106255 82 -6.915388563106255 
		86 -6.915388563106255 91 -6.915388563106255 96 -6.915388563106255 100 -6.915388563106255 
		105 -6.915388563106255 108 -6.915388563106255 112 -6.915388563106255 114 -6.915388563106255 
		119 -6.915388563106255 123 -6.915388563106255 126 -6.915388563106255 130 -6.915388563106255 
		146 -6.915388563106255 154 -6.915388563106255 162 -6.915388563106255 166 -6.915388563106255 
		172 -6.915388563106255;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Knee_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 9.4513480500108429 8 9.4513480500108429 
		14 9.4513480500108429 32 9.4513480500108429 39 9.4513480500108429 48 9.4513480500108429 
		62 9.4513480500108429 64 9.4513480500108429 76 9.4513480500108429 82 9.4513480500108429 
		86 9.4513480500108429 91 9.4513480500108429 96 9.4513480500108429 100 9.4513480500108429 
		105 9.4513480500108429 108 9.4513480500108429 112 9.4513480500108429 114 9.4513480500108429 
		119 9.4513480500108429 123 9.4513480500108429 126 9.4513480500108429 130 9.4513480500108429 
		146 9.4513480500108429 154 9.4513480500108429 162 9.4513480500108429 166 9.4513480500108429 
		172 9.4513480500108429;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:R_Foot_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -4.2649879960397659 8 -4.2649879960397659 
		14 -4.2649879960397659 32 -4.2649879960397659 39 -4.2649879960397659 48 -4.2649879960397659 
		62 -4.2649879960397659 64 -4.2649879960397659 76 -4.2649879960397659 82 -4.2649879960397659 
		86 -4.2649879960397659 91 -4.2649879960397659 96 -4.2649879960397659 100 -4.2649879960397659 
		105 -4.2649879960397659 108 -4.2649879960397659 112 -4.2649879960397659 114 -4.2649879960397659 
		119 -4.2649879960397659 123 -4.2649879960397659 126 -4.2649879960397659 130 -4.2649879960397659 
		146 -4.2649879960397659 154 -4.2649879960397659 162 -4.2649879960397659 166 -4.2649879960397659 
		172 -4.2649879960397659;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Foot_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:RootControl_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -5.5464221608478859 8 -6.0785691180955084 
		14 -9.6710347301763981 32 -10.245012267066961 39 -9.8664425761073229 48 -9.6830615276705601 
		62 -9.7737761523933955 64 -9.6710347301763981 76 -8.1172584562157581 82 -9.4317109617767816 
		86 -10.728037363386818 91 -11.021759745579608 96 -11.15100710030727 100 -11.229529871471206 
		105 -11.205935440643909 108 -10.672589443734955 112 -10.202540322998399 114 -9.7155318842311313 
		119 -8.701735653246077 123 -7.8911351301557637 126 -7.1565472963903982 130 -6.9009046104397962 
		146 -7.1239649372239162 154 -6.6526525833749659 162 -6.1310759431579367 166 -5.8977886155116863 
		172 -5.5464221608478859;
	setAttr -s 27 ".kit[0:26]"  9 9 9 1 1 1 10 9 
		9 9 9 10 10 10 9 10 9 10 10 10 10 10 1 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 1 1 1 10 9 
		9 9 9 10 10 10 9 10 9 10 10 10 10 10 1 10 10 
		10 10;
	setAttr -s 27 ".kix[3:26]"  0.60232383012771606 0.53542762994766235 
		0.86753731966018677 0.99974578619003296 0.2711607813835144 0.92883718013763428 0.1266477108001709 
		0.18540234863758087 0.6189691424369812 0.82209235429763794 1 0.43185591697692871 
		0.22650027275085449 0.20455516874790192 0.15362654626369476 0.16225878894329071 0.14931346476078033 
		0.22935427725315094 0.99880778789520264 0.99698573350906372 0.47320595383644104 0.46822249889373779 
		0.49529317021369934 0.4946819543838501;
	setAttr -s 27 ".kiy[3:26]"  0.7982519268989563 0.84458118677139282 
		-0.4973720908164978 0.022544508799910545 0.96253412961959839 0.37048807740211487 
		-0.9919477105140686 -0.98266273736953735 -0.78541529178619385 -0.56935417652130127 
		0 0.9019426703453064 0.97401106357574463 0.97885507345199585 0.98812896013259888 
		0.98674821853637695 0.98878991603851318 0.97334301471710205 0.048815246671438217 
		0.077585645020008087 0.88095188140869141 0.88361054658889771 0.86872589588165283 
		0.86907404661178589;
	setAttr -s 27 ".kox[3:26]"  0.60232377052307129 0.53542762994766235 
		0.86753737926483154 0.99974578619003296 0.2711607813835144 0.92883718013763428 0.1266477108001709 
		0.18540234863758087 0.6189691424369812 0.82209235429763794 1 0.43185591697692871 
		0.22650027275085449 0.20455516874790192 0.15362654626369476 0.16225878894329071 0.14931346476078033 
		0.22935427725315094 0.99880778789520264 0.99698573350906372 0.47320595383644104 0.46822249889373779 
		0.49529317021369934 0.49468198418617249;
	setAttr -s 27 ".koy[3:26]"  0.7982519268989563 0.84458112716674805 
		-0.4973720908164978 0.022544508799910545 0.96253412961959839 0.37048807740211487 
		-0.9919477105140686 -0.98266273736953735 -0.78541529178619385 -0.56935417652130127 
		0 0.9019426703453064 0.97401106357574463 0.97885507345199585 0.98812896013259888 
		0.98674821853637695 0.98878991603851318 0.97334301471710205 0.048815246671438217 
		0.077585570514202118 0.88095188140869141 0.88361054658889771 0.86872589588165283 
		0.86907410621643066;
createNode animCurveTL -n "D_:Spine0Control_translateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -3.5527136788005009e-015;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.6978503469639965 8 0.069122597726647944 
		14 -1.9522297325789053 32 -2.118638988776202 39 -2.0599122720110046 48 -1.9522297325789053 
		62 -2.0310411126957169 64 -1.9522297325789053 76 -0.5980485424501536 82 -0.23482755868589167 
		86 -0.085781980728227014 91 -0.068190180092681629 96 -0.072131065832949537 100 -0.076700090133247398 
		105 -0.085781980728227014 108 -0.085781980728227014 112 -0.085781980728227014 114 
		-0.085781980728227014 119 -0.085781980728227014 123 -0.085781980728227014 126 -0.085781980728227014 
		130 -0.085781980728227014 146 -0.085781980728227014 154 0.62963679346857027 162 0.71072880671446592 
		166 0.70212934318163089 172 0.6978503469639965;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Clavicle_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.77900741554026665 8 0.42449346921968989 
		14 -1.1920150998869177 32 -1.3250952958804088 39 -1.2781305822072231 48 -1.1920150998869177 
		62 -1.1912615858435367 64 -1.1920150998869177 76 -1.1920150998869177 82 -1.1920150998869177 
		86 -1.1920150998869177 91 -1.1920150998869177 96 -1.1920150998869177 100 -1.1920150998869177 
		105 -1.1920150998869177 108 -1.1920150998869177 112 -1.0071985832194117 114 -0.91777123644481207 
		119 -0.82834388967021255 123 -0.70314560418577288 126 -0.53025273375488102 130 -0.22023793160293559 
		146 0.32202600981839336 154 0.56076089689523512 162 0.67503733056458615 166 0.71671906766442217 
		172 0.77900741554026665;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 9 10 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 9 10 10 10 10;
createNode animCurveTL -n "D_:TankControl_translateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.045469619462510075 8 -3.0893335427079096 
		14 -9.0570471159157364 32 -8.793056189826526 39 -8.4481430298188087 48 -8.3384381401921672 
		62 -8.9026435343354802 64 -9.0226896251182094 76 -9.74089266889988 82 -9.7743176039685622 
		86 -9.7858411554643627 91 -9.7901525402340663 96 -9.8054957542512344 100 -9.7859346949224584 
		105 -9.7859346949224584 108 -9.7859346949224584 112 -8.306527289270532 114 -7.9220078525014772 
		119 -7.8926214578682776 123 -7.8926214578682776 126 -7.8926214578682776 130 -7.8926214578682776 
		146 -7.8926214578682776 154 -6.4308108627397456 162 -0.85628470315678329 166 -0.045469619462510075 
		172 -0.045469619462510075;
	setAttr -s 27 ".kit[22:26]"  3 9 9 9 10;
	setAttr -s 27 ".kot[22:26]"  3 9 9 9 10;
createNode animCurveTL -n "D_:R_Knee_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Knee_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:R_Foot_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Foot_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -2.448549867634112 8 -2.448549867634112 
		14 -2.448549867634112 32 -2.448549867634112 39 -2.448549867634112 48 -2.448549867634112 
		62 -2.448549867634112 64 -2.448549867634112 76 -2.448549867634112 82 -2.448549867634112 
		86 -2.448549867634112 91 -2.448549867634112 96 -2.448549867634112 100 -2.448549867634112 
		105 -2.448549867634112 108 -2.448549867634112 112 -2.448549867634112 114 -2.448549867634112 
		119 -2.448549867634112 123 -2.448549867634112 126 -2.448549867634112 130 -2.448549867634112 
		146 -2.448549867634112 154 -2.448549867634112 162 -2.448549867634112 166 -2.448549867634112 
		172 -2.448549867634112;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:RootControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.60609539862080597 8 -1.1347534322673241 
		14 -1.9988970923013114 32 -2.0700383322973903 39 -2.0449322075773124 48 -1.9988970923013114 
		62 -1.9984942831526356 64 -1.9988970923013114 76 -1.9988970923013114 82 -1.9988970923013114 
		86 -1.9988970923013114 91 -1.9707679871472954 96 -1.9452712922056392 100 -1.9198717525791396 
		105 -1.9244339029547559 108 -2.1482998665165747 112 -3.0073658182543395 114 -3.0329513983910479 
		119 -3.01252038481829 123 -2.9897950034681386 126 -2.9437433018358017 130 -2.822656744347861 
		146 -1.2720163210919209 154 -1.0000911876463705 162 -0.79412852008454304 166 -0.67914534046544495 
		172 -0.60609539862080597;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 1 10 10 10 10 9 9 9 10 10 3;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 1 10 10 10 10 9 9 9 10 10 3;
	setAttr -s 27 ".kix[16:26]"  0.8524739146232605 1 1 1 1 0.37042105197906494 
		0.4019264280796051 0.7447589635848999 0.77996844053268433 0.87097948789596558 1;
	setAttr -s 27 ".kiy[16:26]"  -0.52276968955993652 0 0 0 0 0.92886388301849365 
		0.91567188501358032 0.66733354330062866 0.62581890821456909 0.4913194477558136 0;
	setAttr -s 27 ".kox[16:26]"  0.85247403383255005 1 1 1 1 0.37042105197906494 
		0.4019264280796051 0.7447589635848999 0.77996844053268433 0.87097948789596558 1;
	setAttr -s 27 ".koy[16:26]"  -0.52276968955993652 0 0 0 0 0.92886388301849365 
		0.91567188501358032 0.66733354330062866 0.62581890821456909 0.4913194477558136 0;
createNode animCurveTL -n "D_:Spine0Control_translateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 -1.1102230246251565e-016;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTL -n "D_:R_Clavicle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.013092045525845527 8 0.091871657326451944 
		14 0.55893561111328205 32 0.5973869792437333 39 0.5838172854681043 48 0.55893561111328205 
		62 0.57498618988233263 64 0.55893561111328205 76 0.28270716547528518 82 0.44096916845580053 
		86 0.44096916845580053 91 0.44096916845580053 96 0.44096916845580053 100 0.44096916845580053 
		105 0.44096916845580053 108 0.44096916845580053 112 0.44096916845580053 114 0.44096916845580053 
		119 0.44096916845580053 123 0.44096916845580053 126 0.44096916845580053 130 0.44096916845580053 
		146 0.44096916845580053 154 0.44096916845580053 162 0.25540640383744828 166 0.15173234812171288 
		172 -0.013092045525845527;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Clavicle_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.0078001293990800948 8 0.059396672244655474 
		14 0.41765093503714784 32 0.44714446738039715 39 0.43673604197441435 48 0.41765093503714784 
		62 0.41748393957016028 64 0.41765093503714784 76 0.41765093503714784 82 0.41765093503714784 
		86 0.41765093503714784 91 0.41765093503714784 96 0.41765093503714784 100 0.41765093503714784 
		105 0.41765093503714784 108 0.41765093503714784 112 0.41765093503714784 114 0.41765093503714784 
		119 0.41765093503714784 123 0.41765093503714784 126 0.41765093503714784 130 0.41765093503714784 
		146 0.36325988948620735 154 0.41765093503714784 162 0.22603908937131625 166 0.1328876984950314 
		172 -0.0078001293990800948;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:TankControl_translateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.0055634826900936782 8 0.20164809402193373 
		14 -4.6043308940652867 32 -3.0529192854126372 39 -2.6261048202237807 48 -2.479717901041079 
		62 -2.0702328131147048 64 -2.0702328131147048 76 0.57739210733898749 82 0.89601000746788895 
		86 0.94165109872619812 91 0.99989553145213128 96 1.0145355687972519 100 0.96585760420508737 
		105 0.96585760420508737 108 0.96585760420508737 112 -0.50999323017209819 114 -0.95366950336716161 
		119 -1.1516356151035476 123 -1.1516356151035476 126 -1.1516356151035476 130 -1.1516356151035476 
		146 -1.1516356151035476 154 -1.0608277940165156 162 0.03451411191201402 166 0.0055634826900936782 
		172 0.0055634826900936782;
	setAttr -s 27 ".kit[26]"  10;
	setAttr -s 27 ".kot[26]"  10;
createNode animCurveTL -n "D_:R_Knee_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:L_Knee_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:R_Foot_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 11.090879111775891 8 11.090879111775891 
		14 11.090879111775891 32 11.090879111775891 39 11.090879111775891 48 11.090879111775891 
		62 11.090879111775891 64 11.090879111775891 76 11.090879111775891 82 11.090879111775891 
		86 11.090879111775891 91 11.090879111775891 96 11.090879111775891 100 11.090879111775891 
		105 11.090879111775891 108 11.090879111775891 112 11.090879111775891 114 11.090879111775891 
		119 11.090879111775891 123 11.090879111775891 126 11.090879111775891 130 11.090879111775891 
		146 11.090879111775891 154 11.090879111775891 162 11.090879111775891 166 11.090879111775891 
		172 11.090879111775891;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:L_Foot_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 -14.022394825740609 91 0 96 -4.6860250215569783 100 0 105 -8.2568533128562915 
		108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:HipControl_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:RootControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1.2910907900570192 8 1.361882187525411 
		14 1.8958112857146905 32 2.3347928750124178 39 2.3291133481756865 48 2.2556208186914328 
		62 1.9990312478449681 64 1.464039846142603 76 -2.229931229253669 82 -3.2501152153604815 
		86 -4.3912388947865493 91 -4.5221879768624262 96 -4.4871262913865246 100 -4.4480524360958951 
		105 -4.3912388947865493 108 -4.4760447585807857 112 -4.7696772124304445 114 -3.3763114063519448 
		119 -1.2257970592541483 123 -0.26610441130000534 126 0.3720083886548386 130 1.2914019759778785 
		146 1.2914019759778785 154 1.2912078449913136 162 1.2911342676050681 166 1.291116073965024 
		172 1.2910907900570192;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTA -n "D_:Spine0Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.14113566014355422 8 1.0229733449613381 
		14 6.008581431194151 32 7.2633620940941777 39 7.2667451306478439 48 7.1181530244164222 
		62 6.3135387180471003 64 6.33241001087322 76 7.9623744418133926 82 8.3654203430871501 
		86 8.5151752595438026 91 8.5387238926630395 96 8.5424478462321289 100 8.5442882371931095 
		105 8.5151752595438026 108 8.3819060461495738 112 7.9784337205672546 114 6.0627897397287747 
		119 3.2750576435958108 123 2.0509436401283225 126 1.2417528838319556 130 0.059994223814250847 
		146 -0.12364090810716429 154 0.029027452832475823 162 0.10365365387117553 166 0.1199272580700413 
		172 0.14113566014355422;
	setAttr -s 27 ".kit[2:26]"  1 10 10 1 10 9 9 9 
		9 10 10 10 9 9 9 9 9 9 9 9 1 9 10 10 10;
	setAttr -s 27 ".kot[2:26]"  1 10 10 1 10 9 9 9 
		9 10 10 10 9 9 9 9 9 9 9 9 1 9 10 10 10;
	setAttr -s 27 ".kix[2:26]"  0.9974856972694397 1 1 0.99997568130493164 
		1 0.99810397624969482 0.99825590848922729 0.99958139657974243 0.99994921684265137 
		1 1 1 0.99994349479675293 0.99919503927230835 0.98012930154800415 0.94332426786422729 
		0.97382688522338867 0.98863106966018677 0.98909211158752441 0.99936169385910034 0.99998974800109863 
		0.99997234344482422 1 1 1;
	setAttr -s 27 ".kiy[2:26]"  0.070868335664272308 0 0 -0.0069746877998113632 
		0 0.061549387872219086 0.059034742414951324 0.028932472690939903 0.010081881657242775 
		0 0 0 -0.010627289302647114 -0.040115851908922195 -0.19835957884788513 -0.33187249302864075 
		-0.22729097306728363 -0.15036188066005707 -0.14729806780815125 -0.035723090171813965 
		0.0045318105258047581 0.0074379919096827507 0 0 0;
	setAttr -s 27 ".kox[2:26]"  0.9974856972694397 1 1 0.99997574090957642 
		1 0.99810397624969482 0.99825590848922729 0.99958139657974243 0.99994921684265137 
		1 1 1 0.99994349479675293 0.99919503927230835 0.98012930154800415 0.94332426786422729 
		0.97382688522338867 0.98863106966018677 0.98909211158752441 0.99936169385910034 0.99998974800109863 
		0.99997234344482422 1 1 1;
	setAttr -s 27 ".koy[2:26]"  0.070868320763111115 0 0 -0.0069746887311339378 
		0 0.061549387872219086 0.059034742414951324 0.028932472690939903 0.010081881657242775 
		0 0 0 -0.010627289302647114 -0.040115851908922195 -0.19835957884788513 -0.33187249302864075 
		-0.22729097306728363 -0.15036188066005707 -0.14729806780815125 -0.035723090171813965 
		0.0045318091288208961 0.0074379919096827507 0 0 0;
createNode animCurveTA -n "D_:Spine1Control_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.55216102146285018 8 1.3101625102849834 
		14 5.322879352742218 32 6.4009294374711247 39 6.1961876640701687 48 6.309504083226126 
		62 5.8208131935134606 64 5.790631745611984 76 6.1344022602002362 82 6.2194073647617172 
		86 6.2509916887599593 91 6.2574602003077153 96 6.2602105513684494 100 6.2622433583609274 
		105 6.2509916887599593 108 6.1888030462330681 112 6.0178041299188774 114 5.2797398541960865 
		119 4.2089279423260431 123 3.7391338952044295 126 3.4286787044804128 130 2.8530152571469083 
		146 1.3963382085480871 154 0.94545479467346227 162 0.65011252374241613 166 0.60582835243070388 
		172 0.55216102146285018;
	setAttr -s 27 ".kit[3:26]"  10 10 1 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 1 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kix[5:26]"  0.99998313188552856 1 0.9999312162399292 
		0.99992227554321289 0.99998140335083008 0.99999761581420898 1 1 1 0.99998843669891357 
		0.9998478889465332 0.9968680739402771 1 1 1 1 0.99858748912811279 0.99913519620895386 
		0.99970191717147827 1 1 1;
	setAttr -s 27 ".kiy[5:26]"  -0.0058078332804143429 0 0.01172739639878273 
		0.012471609748899937 0.0061044958420097828 0.0022138191852718592 0 0 0 -0.0048066019080579281 
		-0.017439734190702438 -0.07908225804567337 0 0 0 0 -0.053131368011236191 -0.04158053919672966 
		-0.02441290020942688 0 0 0;
	setAttr -s 27 ".kox[5:26]"  0.99998313188552856 1 0.9999312162399292 
		0.99992227554321289 0.99998140335083008 0.99999761581420898 1 1 1 0.99998843669891357 
		0.9998478889465332 0.9968680739402771 1 1 1 1 0.99858748912811279 0.99913519620895386 
		0.99970191717147827 1 1 1;
	setAttr -s 27 ".koy[5:26]"  -0.0058078332804143429 0 0.01172739639878273 
		0.012471609748899937 0.0061044958420097828 0.0022138191852718592 0 0 0 -0.0048066019080579281 
		-0.017439734190702438 -0.07908225804567337 0 0 0 0 -0.053131368011236191 -0.04158053919672966 
		-0.02441290020942688 0 0 0;
createNode animCurveTA -n "D_:HeadControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.68228131693915994 8 -5.3028670621685663 
		14 -14.034267991523304 32 -15.291074629425964 39 -15.378244385141796 48 -15.339046668334383 
		62 -13.624633523954493 64 -14.034267991523304 76 -23.046740541895883 82 -24.336342253479241 
		86 -25.036865374730908 91 -25.306806760771064 96 -25.533268824428813 100 -25.716845849214391 
		105 -25.036865374730908 108 -20.787696783340504 112 -11.190420331863454 114 -8.9717528627392014 
		119 -10.625678649980289 123 -12.796196392613206 126 -11.280049250446355 130 -9.3955934193452997 
		146 -9.3955934193452997 154 -9.4235405648452488 162 -2.0479650981366531 166 1.0532915939074465 
		172 0.68228131693915994;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 10 9 9 9 1 10 10 1 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 10 9 9 9 1 10 10 1 10 10;
	setAttr -s 27 ".kix[21:26]"  0.99654287099838257 1 1 0.80191731452941895 
		1 1;
	setAttr -s 27 ".kiy[21:26]"  0.083080604672431946 0 0 0.59743505716323853 
		0 0;
	setAttr -s 27 ".kox[21:26]"  0.99654287099838257 1 1 0.80191725492477417 
		1 1;
	setAttr -s 27 ".koy[21:26]"  0.083080597221851349 0 0 0.5974351167678833 
		0 0;
createNode animCurveTA -n "D_:RShoulderFK_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 28 ".ktv[0:27]"  0 -8.1378969009838897 8 -3.0872899224569146 
		14 -0.21707855682264904 32 -0.047297253669210511 39 -0.17114876067113621 48 -0.21707855682264904 
		62 0.90695448465330275 64 2.1257216869319273 76 10.596746464219263 82 5.874560864207222 
		86 0.79688991006060661 91 0.19757412285610382 96 0.33183184055826181 100 0.48748892202821442 
		105 0.79688991006060661 108 0.79688991006060661 112 0.79688991006060661 114 0.79688991006060661 
		119 0.73176968088820482 123 1.0057374243857258 126 1.665347398650614 130 1.665347398650614 
		146 1.4441151899449562 151 0.3660219429915525 154 -0.76508324604244093 160 -3.7473493305345351 
		164 -5.8554760693006118 172 -8.1378969009838897;
	setAttr -s 28 ".kit[3:27]"  10 10 9 9 10 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 10 9 9 9 10;
	setAttr -s 28 ".kot[3:27]"  10 10 9 9 10 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 10 9 9 9 10;
createNode animCurveTA -n "D_:RElbowFK_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		33.382265309221026 82 65.164037022819087 86 73.771596353800547 91 73.771596353800547 
		96 73.771596353800547 100 73.771596353800547 105 73.771596353800547 108 73.771596353800547 
		112 73.771596353800547 114 73.771596353800547 119 76.216385186067498 123 76.216385186067498 
		126 74.13278326912382 130 74.13278326912382 146 73.326529257369799 151 72.981109263705264 
		157 51.553142286482199 164 6.9045839947039882 168 -1.4585950532708987 172 -0.51766437193801651 
		172.005 0;
	setAttr -s 29 ".kit[4:28]"  3 10 10 10 1 10 10 10 
		10 10 10 10 9 9 9 9 9 9 10 1 10 10 1 1 10;
	setAttr -s 29 ".kot[4:28]"  3 10 10 10 1 10 10 10 
		10 10 10 10 9 9 9 9 9 9 10 1 10 10 1 1 10;
	setAttr -s 29 ".kix[8:28]"  0.39876475930213928 0.4274793267250061 
		1 1 1 1 1 1 1 0.98368734121322632 0.99003589153289795 0.98807179927825928 0.98807179927825928 
		0.99977737665176392 1 0.96900641918182373 0.35173791646957397 0.36842209100723267 
		0.97596186399459839 0.9883846640586853 1;
	setAttr -s 29 ".kiy[8:28]"  0.91705328226089478 0.90402513742446899 
		0 0 0 0 0 0 0 0.17988666892051697 0.14081487059593201 -0.15399415791034698 -0.15399384498596191 
		-0.021102970466017723 0 -0.24703571200370789 -0.93609851598739624 -0.92965859174728394 
		-0.2179415225982666 0.15197305381298065 0;
	setAttr -s 29 ".kox[8:28]"  0.39876481890678406 0.4274793267250061 
		1 1 1 1 1 1 1 0.98368734121322632 0.99003589153289795 0.98807179927825928 0.98807179927825928 
		0.99977737665176392 1 0.96900641918182373 0.35173791646957397 0.36842209100723267 
		0.97596186399459839 0.9883846640586853 1;
	setAttr -s 29 ".koy[8:28]"  0.91705322265625 0.90402513742446899 0 
		0 0 0 0 0 0 0.17988666892051697 0.14081487059593201 -0.15399415791034698 -0.15399384498596191 
		-0.021102970466017723 0 -0.24703569710254669 -0.93609851598739624 -0.92965859174728394 
		-0.21794156730175018 0.15197306871414185 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 -1.2474624697910552 8 -1.2474624697910552 
		14 -1.2474624697910552 32 -1.2474624697910552 39 -1.2474624697910552 48 -1.2474624697910552 
		62 0.29362241265549344 64 -1.2474624697910552 76 -27.414404240585654 82 -41.215537957348651 
		86 -33.601590284047703 91 -32.702918616229859 96 -32.904237539930918 100 -33.137644720791954 
		105 -33.601590284047703 108 -33.601590284047703 112 -33.601590284047703 114 -33.601590284047703 
		119 -39.448029823489293 123 -39.448029823489293 126 -41.323795117132036 130 -41.323795117132036 
		146 -44.168997956981038 151 -42.369331871063899 154 -36.33877536501781 157 -21.584140349757202 
		160 -5.3820568094684784 164 -5.1640306978899542 172 -1.2474624697910552;
	setAttr -s 29 ".kit[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 10 10 10 10 
		10;
	setAttr -s 29 ".kot[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 10 10 10 10 
		10;
createNode animCurveTA -n "D_:LShoulderFK_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 4.0494175138666755 14 14.914238233912144 
		32 15.808692205872301 39 15.493034610702646 48 14.914238233912144 62 14.909173741818638 
		64 14.914238233912144 76 14.914238233912144 82 14.914238233912144 86 14.914238233912144 
		91 14.914238233912144 96 14.914238233912144 100 14.914238233912144 105 14.914238233912144 
		108 14.914238233912144 112 14.914238233912144 114 14.914238233912144 119 14.914238233912144 
		123 14.914238233912144 126 14.914238233912144 130 14.914238233912144 146 11.556995525743567 
		154 3.9094854316977661 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 10 10 
		10 10;
createNode animCurveTA -n "D_:RElbowFK_rotateX1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 21 ".ktv[0:20]"  0 -3.0066835182480065 10 1.1374191182828715 
		30 9.7784401262198095 56 9.7784401262198095 70 9.7784401262198095 82 70.01960233068354 
		88 76.142386497411039 92 76.142386497411039 111 76.142386497411039 114 74.695356450496135 
		120 73.857675502775365 123 73.88059457333425 128 72.138654893456462 132 72.808034528545932 
		136 74.218636759941546 140 75.609569994291377 151 76.67568999115349 154 61.072422343776232 
		160 24.567182104064017 164 9.9083242328450449 172 -3.0066835182480065;
	setAttr -s 21 ".kit[9:20]"  10 10 10 9 9 9 10 10 
		10 10 10 10;
	setAttr -s 21 ".kot[9:20]"  10 10 10 9 9 9 10 10 
		10 10 10 10;
createNode animCurveTA -n "D_:L_Wrist_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1.7208864831813682 8 1.7208864831813682 
		14 1.7208864831813682 32 1.7208864831813682 39 1.7208864831813682 48 1.7208864831813682 
		62 1.7208864831813682 64 1.7208864831813682 76 1.7208864831813682 82 1.7208864831813682 
		86 1.7208864831813682 91 1.687942581108048 96 1.6448431857858294 100 1.6087690455066268 
		105 1.7208864831813682 108 2.4684301657752807 112 1.7208864831813682 114 1.7208864831813682 
		119 1.7208864831813682 123 1.7208864831813682 126 1.7208864831813682 130 1.7208864831813682 
		146 1.7208864831813682 154 17.892953380581179 162 6.8808728487775825 166 4.3955302836862096 
		172 1.7208864831813682;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:TankControl_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.70178127080170383 8 -0.70178127080170383 
		14 5.5731146928729496 32 3.2916988589556002 39 1.4406145749350137 48 0.52102575533863371 
		62 1.3331630393216405 64 1.3331630393216405 76 -0.70178127080170383 82 -0.70178127080170383 
		86 -0.70178127080170383 91 -0.70178127080170383 96 -0.70178127080170383 100 -0.70178127080170383 
		105 -0.70178127080170383 108 -0.70178127080170383 112 2.756013351967725 114 2.756013351967725 
		119 2.7285293044922869 123 2.7285293044922869 126 2.7285293044922869 130 2.7285293044922869 
		146 2.7285293044922869 154 2.7506331681091303 162 -0.70178127080170383 166 -0.70178127080170383 
		172 -0.70178127080170383;
	setAttr -s 27 ".kit[26]"  10;
	setAttr -s 27 ".kot[26]"  10;
createNode animCurveTA -n "D_:R_Foot_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:L_Foot_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 25.46188064100717 8 25.46188064100717 
		14 25.46188064100717 32 25.46188064100717 39 25.46188064100717 48 25.46188064100717 
		62 25.46188064100717 64 25.46188064100717 76 25.46188064100717 82 25.46188064100717 
		86 25.46188064100717 91 25.46188064100717 96 25.46188064100717 100 25.46188064100717 
		105 25.46188064100717 108 25.46188064100717 112 25.46188064100717 114 25.46188064100717 
		119 25.46188064100717 123 25.46188064100717 126 25.46188064100717 130 25.46188064100717 
		146 25.46188064100717 154 25.46188064100717 162 25.46188064100717 166 25.46188064100717 
		172 25.46188064100717;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:HipControl_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 7.2798116407249278 8 7.2798116407249278 
		14 7.2798116407249278 32 7.2798116407249278 39 7.2798116407249278 48 7.2798116407249278 
		62 7.2798116407249278 64 7.2798116407249278 76 7.2798116407249278 82 7.2798116407249278 
		86 7.2798116407249278 91 7.2798116407249278 96 7.2798116407249278 100 7.2798116407249278 
		105 7.2798116407249278 108 7.2798116407249278 112 7.2798116407249278 114 7.2798116407249278 
		119 7.2798116407249278 123 7.2798116407249278 126 7.2798116407249278 130 7.2798116407249278 
		146 7.2798116407249278 154 7.2798116407249278 162 7.2798116407249278 166 7.2798116407249278 
		172 7.2798116407249278;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:RootControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 -0.016221339001375356 39 
		-0.025992446634916139 48 0 62 0.11581908828678225 64 0.28784762638139216 76 2.7575846292918484 
		82 3.4394603568563484 86 4.2021704207990505 91 4.3390212838654243 96 4.3518595440430641 
		100 4.3662506790558924 105 4.3883283941654385 108 4.3617825369811607 112 3.9197380965144499 
		114 3.0121888411894422 119 1.7439709219650215 123 1.1936620132768168 126 0.74715412048620744 
		130 0.29717372135648606 146 0.38149219793981354 154 0.11178413962732865 162 0.024409141420759863 
		166 0.013132487353277756 172 0;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
createNode animCurveTA -n "D_:Spine0Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 1.1992476847254072 14 -0.00032989798323019941 
		32 -0.13152864006158527 39 -0.11621930864731322 48 -0.00032989798323019941 62 0.46607280052755035 
		64 0.92292674003019926 76 4.4888556394175732 82 5.5989092065761943 86 6.8405562457380515 
		91 7.055998131075051 96 7.0707537740131663 100 7.0545252231107929 105 6.9189727033275705 
		108 6.7168423054317392 112 6.2884942584148371 114 5.1277637185018534 119 3.4770836953908684 
		123 2.7570764492932525 126 2.2822851289073487 130 1.5849692892580369 146 1.5849692892580369 
		154 -0.83677785471206489 162 -0.41108662815311076 166 -0.23948151120144376 172 0;
	setAttr -s 27 ".kit[3:26]"  10 10 9 1 9 9 9 9 
		10 10 10 1 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 1 9 9 9 9 
		10 10 10 1 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kix[6:26]"  0.99696624279022217 0.98887068033218384 
		0.99087584018707275 0.99250400066375732 0.9964262843132019 1 1 1 0.99968886375427246 
		0.99888986349105835 0.99052143096923828 0.97859573364257813 0.99062246084213257 0.99603015184402466 
		0.99617874622344971 0.99983334541320801 0.99860721826553345 0.99787342548370361 1 
		1 1;
	setAttr -s 27 ".kiy[6:26]"  0.077835395932197571 0.14877729117870331 
		0.13477778434753418 0.1222117617726326 0.084467068314552307 0 0 0 -0.024943763390183449 
		-0.04710732027888298 -0.13735879957675934 -0.20579184591770172 -0.13662765920162201 
		-0.08901602029800415 -0.087338291108608246 -0.01825263537466526 -0.052760757505893707 
		-0.065181873738765717 0 0 0;
	setAttr -s 27 ".kox[6:26]"  0.99696624279022217 0.98887068033218384 
		0.99087584018707275 0.99250400066375732 0.9964262843132019 1 1 1 0.99968886375427246 
		0.99888986349105835 0.99052143096923828 0.97859573364257813 0.99062246084213257 0.99603015184402466 
		0.99617874622344971 0.99983334541320801 0.99860721826553345 0.99787342548370361 1 
		1 1;
	setAttr -s 27 ".koy[6:26]"  0.077835381031036377 0.14877729117870331 
		0.13477778434753418 0.1222117617726326 0.084467068314552307 0 0 0 -0.024943763390183449 
		-0.04710732027888298 -0.13735879957675934 -0.20579184591770172 -0.13662765920162201 
		-0.08901602029800415 -0.087338291108608246 -0.01825263537466526 -0.052760757505893707 
		-0.065181873738765717 0 0 0;
createNode animCurveTA -n "D_:Spine1Control_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.0084151252342212438 8 0.0084151252342212438 
		14 0.0084151252342212438 32 0.0084151252342212438 39 0.0084151252342212438 48 0.0084151252342212438 
		62 0.010120248093051959 64 0.14743967933447696 76 1.7829130897717513 82 2.2216982054769687 
		86 2.3847323118583819 91 2.4079034643783266 96 2.4087319616417471 100 2.4080356635100606 
		105 2.3847323118583819 108 2.2955938215794252 112 2.0504914243095778 114 1.5706355267468772 
		119 0.90246043080003546 123 0.61283437326636503 126 0.39302589343586375 130 0.14094606909622662 
		146 0.028521433651789517 154 0.15535774056978169 162 0.055974352440912548 166 0.03524333435248371 
		172 0.0084151252342212438;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 1 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 1 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kix[7:26]"  0.99885290861129761 0.99818462133407593 
		0.99950385093688965 0.99994134902954102 1 1 1 0.99997293949127197 0.99968767166137695 
		0.9980047345161438 0.99633324146270752 0.9984511137008667 0.99927479028701782 0.99937766790390015 
		0.9999544620513916 1 0.99999964237213135 1 1 1;
	setAttr -s 27 ".kiy[7:26]"  0.04788554459810257 0.060228209942579269 
		0.031495552510023117 0.010832349769771099 0 0 0 -0.0073591023683547974 -0.024993350729346275 
		-0.063138306140899658 -0.085557639598846436 -0.055636312812566757 -0.03807801753282547 
		-0.035275157541036606 -0.0095422649756073952 0.00031441400642506778 0.00089839438442140818 
		0 0 0;
	setAttr -s 27 ".kox[7:26]"  0.99885284900665283 0.99818462133407593 
		0.99950385093688965 0.99994134902954102 1 1 1 0.99997293949127197 0.99968767166137695 
		0.9980047345161438 0.99633324146270752 0.9984511137008667 0.99927479028701782 0.99937766790390015 
		0.9999544620513916 1 0.99999964237213135 1 1 1;
	setAttr -s 27 ".koy[7:26]"  0.047885537147521973 0.060228209942579269 
		0.031495552510023117 0.010832349769771099 0 0 0 -0.0073591023683547974 -0.024993350729346275 
		-0.063138306140899658 -0.085557639598846436 -0.055636312812566757 -0.03807801753282547 
		-0.035275157541036606 -0.0095422649756073952 0.00031441400642506778 0.00089839438442140818 
		0 0 0;
createNode animCurveTA -n "D_:HeadControl_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.026536333767640648 8 0.24782726192192872 
		14 1.0317231955402553 32 1.0962579839039106 39 1.073483311790999 48 1.0317231955402553 
		62 2.1217496123003263 64 1.0317231955402553 76 -17.482650629700977 82 -17.026873292508608 
		86 -16.779290988625224 91 -16.958731863199429 96 -17.238264801331582 100 -17.474344031889444 
		105 -16.779290988625224 108 -12.044433898015841 112 3.686911201104369 114 11.044579723866741 
		119 18.631729539589596 123 21.010195811976068 126 20.587080503849204 130 20.163965195722344 
		146 20.163965195722344 154 20.748071921803287 162 1.6880543738907214 166 -1.1662553313878883 
		172 -0.026536333767640648;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 9 10 10 10 10 10 10 1 
		1 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 9 10 10 10 10 10 10 1 
		1 10;
	setAttr -s 27 ".kix[24:26]"  0.64521163702011108 0.98842203617095947 
		1;
	setAttr -s 27 ".kiy[24:26]"  -0.7640039324760437 0.15172961354255676 
		0;
	setAttr -s 27 ".kox[24:26]"  0.64521163702011108 0.98842203617095947 
		1;
	setAttr -s 27 ".koy[24:26]"  -0.76400387287139893 0.15172961354255676 
		0;
createNode animCurveTA -n "D_:RShoulderFK_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 28 ".ktv[0:27]"  0 -12.215538151373286 8 -8.6142138537890958 
		14 -2.780916001504353 32 -2.300685653778817 39 -2.468438026443887 48 -2.780916001504353 
		62 -2.780916001504353 64 -2.0635122446051728 76 3.1484641072323907 82 -0.2827277678456015 
		86 -4.5321110471995754 91 -5.0336643128968248 96 -4.9213071904684051 100 -4.7910414470629696 
		105 -4.5321110471995754 108 -4.5321110471995754 112 -4.5321110471995754 114 -4.0624520583466843 
		119 -2.1338128018670375 123 0.87983238482046677 126 2.9983088089561147 130 3.5853825466225104 
		146 3.3371398216074692 151 0.77164240785758675 154 -1.5909611008593019 160 -6.8644889753231748 
		164 -9.2104399559285781 172 -12.215538151373286;
	setAttr -s 28 ".kit[3:27]"  10 10 9 9 10 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 1 10 9 9 9 10;
	setAttr -s 28 ".kot[3:27]"  10 10 9 9 10 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 1 10 9 9 9 10;
	setAttr -s 28 ".kix[22:27]"  0.99978727102279663 1 0.91387683153152466 
		0.92881101369857788 0.97380876541137695 0.98120146989822388;
	setAttr -s 28 ".kiy[22:27]"  -0.020626166835427284 0 -0.40599146485328674 
		-0.37055361270904541 -0.22736856341362 -0.19298604130744934;
	setAttr -s 28 ".kox[22:27]"  0.99978727102279663 1 0.91387683153152466 
		0.92881101369857788 0.97380876541137695 0.98120152950286865;
	setAttr -s 28 ".koy[22:27]"  -0.020626166835427284 0 -0.40599146485328674 
		-0.37055361270904541 -0.22736856341362 -0.19298605620861053;
createNode animCurveTA -n "D_:RElbowFK_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 38.157091370305473 8 38.157091370305473 
		14 38.157091370305473 32 38.157091370305473 39 37.34312763877638 48 38.157091370305473 
		62 38.157091370305473 64 38.157091370305473 76 31.911724411839248 82 29.021009604930555 
		86 29.234520655753361 91 29.234520655753361 96 29.234520655753361 100 29.234520655753361 
		105 29.234520655753361 108 29.234520655753361 112 27.267620969651961 114 26.611987740951474 
		119 25.12819247678803 123 25.12819247678803 126 25.418931600878174 130 25.418931600878174 
		146 22.699514979957105 151 23.722457470762162 157 28.835968262168159 164 36.148677789468834 
		168 38.07050785967084 172 38.100985184071668 172.005 38.157091370305473;
	setAttr -s 29 ".kit[4:28]"  3 10 10 10 10 10 10 10 
		10 10 10 10 9 9 9 9 9 9 10 9 10 9 10 10 10;
	setAttr -s 29 ".kot[4:28]"  3 10 10 10 10 10 10 10 
		10 10 10 10 9 9 9 9 9 9 10 9 10 9 10 10 10;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 9.6996556887373035 8 9.6996556887373035 
		14 9.6996556887373035 32 9.6996556887373035 39 9.6996556887373035 48 9.6996556887373035 
		62 10.465148921198926 64 9.6996556887373035 76 -3.298082027366112 82 42.498506614399162 
		86 34.385770127831101 91 33.428226593503545 96 33.642733893911505 100 33.891431555370708 
		105 34.385770127831101 108 34.385770127831101 112 34.385770127831101 114 34.385770127831101 
		119 34.893118688949102 123 34.893118688949102 126 33.276347653779986 130 33.276347653779986 
		146 23.893482092230713 151 25.443770363374977 154 37.998820292698618 157 41.656667931330439 
		160 41.549243611934656 164 24.50441040667484 172 9.6996556887373035;
	setAttr -s 29 ".kit[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 10 10 10 10 
		10;
	setAttr -s 29 ".kot[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 10 10 10 10 
		10;
createNode animCurveTA -n "D_:LShoulderFK_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0.47040859168971055 14 -4.9935882520410031 
		32 -5.4434156058308032 39 -5.2846691236554761 48 -4.9935882520410031 62 -4.9910412824554484 
		64 -4.9935882520410031 76 -4.9935882520410031 82 -4.9935882520410031 86 -4.9935882520410031 
		91 -4.9935882520410031 96 -4.9935882520410031 100 -4.9935882520410031 105 -4.9935882520410031 
		108 -4.9935882520410031 112 -4.9935882520410031 114 -4.9935882520410031 119 -4.9935882520410031 
		123 -4.9935882520410031 126 -4.9935882520410031 130 -4.9935882520410031 146 -2.6220901209845948 
		154 -0.088858118845267328 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 10 10 
		10 10;
createNode animCurveTA -n "D_:RElbowFK_rotateY1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 21 ".ktv[0:20]"  0 35.461272798046053 10 35.478630819683922 
		30 35.528225163800123 56 35.528225163800123 70 35.528225163800123 82 33.701695722312998 
		88 30.202808568751603 92 30.202808568751603 111 30.202808568751603 114 29.891788094229543 
		120 24.738682939117741 123 25.133905895796051 128 24.077510138664618 132 23.291379408415171 
		136 24.458804901686126 140 23.996746103612789 151 20.736719460730011 154 19.607466629143385 
		160 25.205179579583739 164 29.468753098159141 172 35.461272798046053;
	setAttr -s 21 ".kit[9:20]"  10 10 9 9 9 9 9 10 
		10 10 10 10;
	setAttr -s 21 ".kot[9:20]"  10 10 9 9 9 9 9 10 
		10 10 10 10;
createNode animCurveTA -n "D_:L_Wrist_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -8.1070161891185535 8 -8.1070161891185535 
		14 -8.1070161891185535 32 -8.1070161891185535 39 -8.1070161891185535 48 -8.1070161891185535 
		62 -8.1070161891185535 64 -8.1070161891185535 76 -8.1070161891185535 82 -8.1070161891185535 
		86 -8.1070161891185535 91 -8.1155790754116328 96 -8.1267816092521841 100 -8.1361581168307247 
		105 -8.1070161891185535 108 -7.9127122070385978 112 -8.1070161891185535 114 -8.1070161891185535 
		119 -8.1070161891185535 123 -8.1070161891185535 126 -8.1070161891185535 130 -8.1070161891185535 
		146 -8.1070161891185535 154 -1.7752451290002651 162 -0.71968561392476682 166 -2.6108375189714881 
		172 -8.1070161891185535;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:TankControl_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 -0.38839604002663036 123 
		-0.38839604002663036 126 -0.38839604002663036 130 -0.38839604002663036 146 -0.38839604002663036 
		154 -0.17219045359482713 162 0 166 0 172 0;
	setAttr -s 27 ".kit[26]"  10;
	setAttr -s 27 ".kot[26]"  10;
createNode animCurveTA -n "D_:R_Foot_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -29.070586091195047 8 -29.070586091195047 
		14 -29.070586091195047 32 -29.070586091195047 39 -29.070586091195047 48 -29.070586091195047 
		62 -29.070586091195047 64 -29.070586091195047 76 -29.070586091195047 82 -29.070586091195047 
		86 -29.070586091195047 91 -29.070586091195047 96 -29.070586091195047 100 -29.070586091195047 
		105 -29.070586091195047 108 -29.070586091195047 112 -29.070586091195047 114 -29.070586091195047 
		119 -29.070586091195047 123 -29.070586091195047 126 -29.070586091195047 130 -29.070586091195047 
		146 -29.070586091195047 154 -29.070586091195047 162 -29.070586091195047 166 -29.070586091195047 
		172 -29.070586091195047;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:L_Foot_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:HipControl_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:RootControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 -0.0081106693272270936 
		39 -0.012996222948644397 48 0 62 0.07661385623613913 64 0.14392381319069603 76 1.0612014286119935 
		82 1.3236076984109055 86 1.4211068390478609 91 1.4373092080512062 96 1.440873010128574 
		100 1.4430247978234931 105 1.4211068390478609 108 1.3145802230963168 112 0.94574170076670272 
		114 0.67812419925482381 119 0.35404617391350823 123 0.21992982794380944 126 0.1332508659757192 
		130 0 146 -0.11242463544443704 154 -0.056212317722218907 162 0 166 0 172 0;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
createNode animCurveTA -n "D_:Spine0Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0.42136077696910518 14 2.384509025020439 
		32 2.5461265743476562 39 2.4890908763352364 48 2.384509025020439 62 2.3655996147430236 
		64 2.384509025020439 76 2.6900445481793112 82 2.7655951785888866 86 2.7936666173740909 
		91 2.8007205791545555 96 2.8048721781865473 100 2.8081077735783144 105 2.7936666173740909 
		108 2.708784860490165 112 2.4148896488744502 114 1.7591413178175059 119 0.8374570109602919 
		123 0.43682098495385951 126 0.17296966372409198 130 -0.21568606442232949 146 -0.35621685872787634 
		154 -0.070092089514241465 162 0.0076869507576346812 166 0.0049950495583664083 172 
		0;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 9 9 9 9 9 9 9 10 10 10;
createNode animCurveTA -n "D_:Spine1Control_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0.019794606311957463 8 0.019794606311957463 
		14 0.019794606311957463 32 0.019794606311957463 39 0.019794606311957463 48 0.019794606311957463 
		62 0.0040497143655241303 64 0.019794606311957463 76 0.28713593528792003 82 0.35324217959577225 
		86 0.37780448146337392 91 0.37872555186345097 96 0.37548834151020993 100 0.37256942393099657 
		105 0.37780448146337392 108 0.42268832151136559 112 0.54610455438830663 114 0.52077321217592887 
		119 0.45697435379675316 123 0.42558571084029839 126 0.40402165631424397 130 0.37527456134324078 
		146 0.37527456134324078 154 0.15603808723015417 162 0.012789910901032452 166 0.014801264321489904 
		172 0.019794606311957463;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTA -n "D_:HeadControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -0.034361262366484416 8 -0.65659961163767455 
		14 -2.4344233440797756 32 -2.5807839364693645 39 -2.5291324999667468 48 -2.4344233440797756 
		62 -2.8636735277849343 64 -2.4344233440797756 76 4.8681271352518838 82 9.7209215661266608 
		86 10.939773994172228 91 11.071099489784467 96 11.022472607418182 100 10.971382115442458 
		105 10.939773994172228 108 11.224216456820752 112 11.866665189229531 114 12.015184418759596 
		119 11.650176503801106 123 11.389089613270913 126 10.759682756864173 130 10.130275900457436 
		146 10.130275900457436 154 3.6047571510237302 162 -2.2170916333591832 166 0.77134374872987688 
		172 -0.034361262366484416;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 10 10 1 1 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 10 10 1 1 10 10;
	setAttr -s 27 ".kix[23:26]"  0.86651903390884399 0.86837244033813477 
		1 1;
	setAttr -s 27 ".kiy[23:26]"  -0.49914407730102539 0.4959125816822052 
		0 0;
	setAttr -s 27 ".kox[23:26]"  0.86651903390884399 0.86837249994277954 
		1 1;
	setAttr -s 27 ".koy[23:26]"  -0.499144047498703 0.4959125816822052 0 
		0;
createNode animCurveTA -n "D_:RShoulderFK_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 28 ".ktv[0:27]"  0 47.399331424850153 8 48.437231442074463 
		14 55.894954080394648 32 56.676213314308157 39 56.560315951683386 48 55.894954080394648 
		62 54.503715221862144 64 52.926270058433666 76 30.73664624148914 82 12.49339247397198 
		86 10.588708001629543 91 10.363898729503706 96 10.414260126780324 100 10.472648634723269 
		105 10.588708001629543 108 10.588708001629543 112 10.588708001629543 114 10.588708001629543 
		119 11.83880619674502 123 12.891280680165101 126 14.244569796415632 130 14.244569796415632 
		146 13.724730165516407 151 15.784551408698732 154 18.186946386091179 160 25.519092180798332 
		164 31.652531514607361 172 47.399331424850153;
	setAttr -s 28 ".kit[3:27]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 9 10 10 10;
	setAttr -s 28 ".kot[3:27]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 9 9 10 10 10;
createNode animCurveTA -n "D_:RElbowFK_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		52.493422121766912 82 102.75474479397191 86 108.47893381049069 91 108.47893381049069 
		96 108.47893381049069 100 108.47893381049069 105 108.47893381049069 108 108.47893381049069 
		112 108.47893381049069 114 108.47893381049069 119 107.80899768511901 123 106.24909908238985 
		126 103.0403252775633 130 103.0403252775633 146 109.29422178428854 151 106.61796683599405 
		157 76.713652229257463 164 16.784902451485269 168 0.94535216443085002 172 -1.1730701078319232 
		172.005 0;
	setAttr -s 29 ".kit[4:28]"  3 10 10 10 1 10 10 10 
		10 10 10 10 9 9 9 9 9 9 1 1 10 10 9 9 10;
	setAttr -s 29 ".kot[4:28]"  3 10 10 10 1 10 10 10 
		10 10 10 10 9 9 9 9 9 9 1 1 10 10 9 9 10;
	setAttr -s 29 ".kix[8:28]"  0.24545332789421082 0.32286500930786133 
		1 1 1 1 1 1 1 0.99874675273895264 0.99169027805328369 0.94187510013580322 0.97238391637802124 
		0.98686045408248901 0.99120289087295532 0.78044015169143677 0.2663939893245697 0.26719173789024353 
		0.64800745248794556 0.99244892597198486 1;
	setAttr -s 29 ".kiy[8:28]"  0.96940839290618896 0.94644498825073242 
		0 0 0 0 0 0 0 -0.050048299133777618 -0.1286485493183136 -0.33596324920654297 -0.23338717222213745 
		0.16157525777816772 -0.13235113024711609 -0.62523049116134644 -0.96386426687240601 
		-0.96364343166351318 -0.76163405179977417 -0.12265847623348236 0;
	setAttr -s 29 ".kox[8:28]"  0.24545334279537201 0.32286500930786133 
		1 1 1 1 1 1 1 0.99874675273895264 0.99169027805328369 0.94187510013580322 0.97238391637802124 
		0.98686045408248901 0.9912029504776001 0.78044015169143677 0.2663939893245697 0.26719173789024353 
		0.64800745248794556 0.99244892597198486 1;
	setAttr -s 29 ".koy[8:28]"  0.96940845251083374 0.94644498825073242 
		0 0 0 0 0 0 0 -0.050048299133777618 -0.1286485493183136 -0.33596324920654297 -0.23338717222213745 
		0.16157525777816772 -0.13235117495059967 -0.62523049116134644 -0.96386426687240601 
		-0.96364343166351318 -0.76163405179977417 -0.12265847623348236 0;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTA -n "D_:R_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 7.1301771055535337 8 7.1301771055535337 
		14 7.1301771055535337 32 7.1301771055535337 39 7.1301771055535337 48 7.1301771055535337 
		62 10.010161588056548 64 7.1301771055535337 76 -41.77069140575157 82 -40.958501187920788 
		86 -30.721762689549625 91 -29.513523909711985 96 -29.784191537893602 100 -30.09800092453494 
		105 -30.721762689549625 108 -30.721762689549625 112 -30.721762689549625 114 -30.721762689549625 
		119 -45.390326299165565 123 -45.390326299165565 126 -48.31333717000593 130 -48.31333717000593 
		146 -43.718102758838384 151 -45.787941734134812 154 -43.082104563978383 157 -24.944558975472702 
		160 -0.63708737055441111 164 -2.5180810404098857 172 7.1301771055535337;
	setAttr -s 29 ".kit[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 1 1 10 10 10 
		10;
	setAttr -s 29 ".kot[3:28]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 9 9 9 9 9 10 1 1 10 10 10 
		10;
	setAttr -s 29 ".kix[23:28]"  0.96723359823226929 0.7903171181678772 
		0.26064503192901611 1 1 0.84552764892578125;
	setAttr -s 29 ".kiy[23:28]"  -0.25388839840888977 0.61269795894622803 
		0.965434730052948 0 0 0.53393173217773438;
	setAttr -s 29 ".kox[23:28]"  0.96723353862762451 0.79031717777252197 
		0.26064503192901611 1 1 0.84552764892578125;
	setAttr -s 29 ".koy[23:28]"  -0.25388836860656738 0.6126980185508728 
		0.965434730052948 0 0 0.53393173217773438;
createNode animCurveTA -n "D_:LShoulderFK_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -45.645013483740549 8 -48.647566387140792 
		14 -55.085829846468975 32 -55.615864428023933 39 -55.428812386809845 48 -55.085829846468975 
		62 -55.082828735100534 64 -55.085829846468975 76 -55.085829846468975 82 -55.085829846468975 
		86 -55.085829846468975 91 -55.085829846468975 96 -55.085829846468975 100 -55.085829846468975 
		105 -55.085829846468975 108 -55.085829846468975 112 -55.085829846468975 114 -55.085829846468975 
		119 -55.085829846468975 123 -55.085829846468975 126 -55.085829846468975 130 -55.085829846468975 
		146 -52.657580781958607 154 -47.268817697892317 162 -45.645013483740549 166 -45.645013483740549 
		172 -45.645013483740549;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 9 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 9 9 10 
		10 10;
createNode animCurveTA -n "D_:RElbowFK_rotateZ1";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 21 ".ktv[0:20]"  0 -0.21806032426803065 10 0.77466151151423557 
		30 8.5295690988404704 56 8.5295690988404704 70 8.5295690988404704 82 105.00839413785793 
		88 108.55570767678326 92 108.55570767678326 111 108.55570767678326 114 106.40172049080978 
		120 100.92886952679835 123 100.48801511152101 128 98.607196075561845 132 99.557389916430793 
		136 101.1174000798963 140 101.58729598610178 151 100.63880081403761 154 79.968318242137883 
		160 31.556491009064594 164 10.796704720555624 172 -0.21806032426803065;
	setAttr -s 21 ".kit[0:20]"  9 9 9 9 9 9 9 9 
		9 10 10 10 10 10 10 10 10 10 10 10 10;
	setAttr -s 21 ".kot[0:20]"  9 9 9 9 9 9 9 9 
		9 10 10 10 10 10 10 10 10 10 10 10 10;
createNode animCurveTA -n "D_:L_Wrist_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -1.1231193682533216 8 -1.1231193682533216 
		14 -1.1231193682533216 32 -1.1231193682533216 39 -1.1231193682533216 48 -1.1231193682533216 
		62 -1.1231193682533216 64 -1.1231193682533216 76 -1.1231193682533216 82 -1.1231193682533216 
		86 -1.1231193682533216 91 -0.88686677823510496 96 -0.57778551146629942 100 -0.31908481020412599 
		105 -1.1231193682533216 108 -6.4840246146836007 112 -6.0370321696767011 114 -5.8248807612634268 
		119 -5.4592457555209055 123 -5.0737396635253234 126 -4.5932933898296158 130 -3.9815437174647723 
		146 -1.1231193682533216 154 -17.919827453089134 162 -11.765489280943502 166 -7.831685827158716 
		172 -1.1231193682533216;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 10 9 9 9 9 10 10 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 10 9 10 9 9 9 9 10 10 10 10 10;
createNode animCurveTA -n "D_:TankControl_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 1.5972344076474623 114 3.1944688152949232 
		119 8.0953112104783695 123 8.0953112104783695 126 8.0953112104783695 130 8.0953112104783695 
		146 8.0953112104783695 154 3.57930804894744 162 0 166 0 172 0;
	setAttr -s 27 ".kit[15:26]"  3 9 9 9 9 9 9 9 
		9 9 9 10;
	setAttr -s 27 ".kot[15:26]"  3 9 9 9 9 9 9 9 
		9 9 9 10;
createNode animCurveTA -n "D_:R_Foot_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Foot_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:HipControl_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTU -n "D_:Spine0Control_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 14 1 32 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleX1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Knee_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:R_Foot_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Foot_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:HipControl_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1.0000000000000002;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTU -n "D_:Spine0Control_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 14 1 32 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleY1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Knee_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:R_Foot_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Foot_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:HipControl_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1.0000000000000002;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RootControl_scaleZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
	setAttr -s 27 ".kot[3:26]"  10 10 9 10 9 9 9 9 
		10 10 10 9 9 9 10 10 10 10 9 9 9 10 10 10;
createNode animCurveTU -n "D_:Spine0Control_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:Spine1Control_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Clavicle_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RShoulderFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 14 1 32 1;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Clavicle_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:LShoulderFK_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:RElbowFK_scaleZ1";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:L_Wrist_scaleZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  0 1;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Knee_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Knee_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:R_Foot_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:HipControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:RootControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 9 9 5 5 5 5 9 9 9 5 5 5;
createNode animCurveTU -n "D_:Spine0Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 9 9 5 5 5 5 9 9 9 5 5 5;
createNode animCurveTU -n "D_:Spine1Control_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 9 9 5 5 5 5 9 9 9 5 5 5;
createNode animCurveTU -n "D_:HeadControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:RShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		151 1 154 1 160 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 9 5 9 9 9 
		5 5 5 9 5 9 9 9 9 9 9 5 5 9 9 5;
createNode animCurveTU -n "D_:RElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 29 ".ktv[0:28]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		151 1 157 1 164 1 168 1 172 1 172.005 1;
	setAttr -s 29 ".kit[4:28]"  3 9 9 9 9 9 9 9 
		9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9;
	setAttr -s 29 ".kot[0:28]"  5 5 5 5 3 5 5 5 
		5 5 5 5 5 5 5 5 9 9 9 9 9 9 5 5 5 
		5 5 5 5;
	setAttr ".pre" 3;
	setAttr ".pst" 3;
createNode animCurveTU -n "D_:R_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		151 1 157 1 164 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 9 9 9 9 9 9 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Clavicle_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:LShoulderFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:RElbowFK_visibility1";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 19 ".ktv[0:18]"  0 1 10 1 30 1 56 1 70 1 82 1 88 1 92 1 
		111 1 114 1 120 1 123 1 128 1 132 1 136 1 140 1 151 1 154 1 172 1;
	setAttr -s 19 ".kot[9:18]"  5 5 5 5 5 5 5 5 
		5 5;
createNode animCurveTU -n "D_:L_Wrist_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:TankControl_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[26]"  5;
createNode animCurveTU -n "D_:R_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Knee_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:R_Foot_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTU -n "D_:L_Foot_ToeRoll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0.70000000000000007 91 0 96 1.1 100 0 105 0 108 0 112 0 114 0 119 0 123 
		0 126 0 130 0 146 0 154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:R_Foot_ToeRoll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:L_Foot_BallRoll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:R_Foot_BallRoll";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:r_thumb_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  15 0 35 5.5128474764561419 51 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:r_mid_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 14 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_mid_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  15 0 51 0;
createNode animCurveTA -n "D_:r_pink_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 10 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_pink_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  11 0 51 0;
createNode animCurveTA -n "D_:r_point_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 19 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_point_2_rotateX";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  40 0 51 0;
	setAttr -s 2 ".kit[1]"  3;
	setAttr -s 2 ".kot[1]"  3;
createNode animCurveTA -n "D_:r_thumb_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -12.498080351386355 14 -12.591699818506463 
		34 -12.472287215735706 40 -12.498080351386355 51 -12.498080351386355;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
createNode animCurveTA -n "D_:r_thumb_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  15 0 35 14.069183974047057 51 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:r_mid_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 14 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_mid_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  15 0 51 0;
createNode animCurveTA -n "D_:r_pink_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 10 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_pink_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  11 0 51 0;
createNode animCurveTA -n "D_:r_point_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 19 0 40 0 51 0;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_point_2_rotateY";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  40 0 51 0;
	setAttr -s 2 ".kit[1]"  3;
	setAttr -s 2 ".kot[1]"  3;
createNode animCurveTA -n "D_:r_thumb_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -16.016384857458238 14 -15.886982882284432 
		34 -16.052036445763452 40 -16.016384857458238 51 -16.016384857458238;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
createNode animCurveTA -n "D_:r_thumb_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  15 0 35 -45.375657885501766 51 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:r_mid_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -51.590748582374587 14 -26.948283143385655 
		40 -51.590748582374587 51 -51.590748582374587;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_mid_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  15 -37.069649276763101 51 -37.069649276763101;
createNode animCurveTA -n "D_:r_pink_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -60.240931129090278 10 -39.222855826331802 
		40 -60.240931129090278 51 -60.240931129090278;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_pink_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  11 -34.609429738102065 51 -34.609429738102065;
createNode animCurveTA -n "D_:r_point_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -38.133329475338165 19 -14.744903600829174 
		40 -38.133329475338165 51 -38.133329475338165;
	setAttr -s 4 ".kit[1:3]"  3 10 10;
	setAttr -s 4 ".kot[1:3]"  3 10 10;
createNode animCurveTA -n "D_:r_point_2_rotateZ";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 -74.810673852666966 51 -74.810673852666966;
	setAttr -s 2 ".kit[1]"  3;
	setAttr -s 2 ".kot[1]"  3;
createNode animCurveTA -n "D_:r_thumb_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 5 ".ktv[0:4]"  0 -6.6175135861403929 14 -1.3849478221335367 
		34 -8.0591398066657263 40 -6.6175135861403929 51 -6.6175135861403929;
	setAttr -s 5 ".kit[1:4]"  3 9 10 10;
	setAttr -s 5 ".kot[1:4]"  3 9 10 10;
createNode animCurveTA -n "D_:l_thumb_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 0 50 0 70 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_mid_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 29 0 49 0 54 0;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_mid_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 0 50 0 70 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_pink_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 25 0 45 0 54 0;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_pink_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  26 0 46 0 66 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_point_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -3.3419010434934679 33 -4.5690139218344736 
		53 -3.332939459546882 54 -3.3419010434934679;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_point_2_rotateX";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  34 0 54 0 74 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -18.948084576790443 29 -11.832370071732285 
		49 -20.26581049047898 54 -18.948084576790443;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_thumb_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 0 50 0 70 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_mid_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 29 0 49 0 54 0;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_mid_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 0 50 0 70 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_pink_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 0 25 0 45 0 54 0;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_pink_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  26 0 46 0 66 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_point_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -3.173264837453968 33 -0.63980004428447868 
		53 -3.1917666866484797 54 -3.173264837453968;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_point_2_rotateY";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  34 0 54 0 74 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -18.366743592299766 29 -1.6114629724377807 
		49 -21.469575703990163 54 -18.366743592299766;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_thumb_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 0 50 -44.132318556041994 70 0;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_mid_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -63.839356559844809 29 -29.357774215547497 
		49 -70.224839643485552 54 -63.839356559844809;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_mid_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  30 -37.526539377836308 50 -68.553080365792368 
		70 -37.526539377836308;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_pink_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -54.489839590457279 25 -32.228026897427476 
		45 -70.9610687044146 54 -54.489839590457279;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_pink_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  26 -42.0780670191971 46 -66.895514618571369 
		66 -42.0780670191971;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_point_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -56.359168905796714 33 -20.859487275184183 
		53 -56.618422461541243 54 -56.359168905796714;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:l_point_2_rotateZ";
	setAttr ".tan" 3;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  34 -29.931065482458106 54 -73.966144308812432 
		74 -29.931065482458106;
	setAttr -s 3 ".kit[1:2]"  9 3;
	setAttr -s 3 ".kot[1:2]"  9 3;
createNode animCurveTA -n "D_:l_thumb_1_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 4 ".ktv[0:3]"  0 -9.399391337933455 29 -7.7050417731401053 
		49 -9.7131600152444992 54 -9.399391337933455;
	setAttr -s 4 ".kit[1:3]"  3 9 10;
	setAttr -s 4 ".kot[1:3]"  3 9 10;
createNode animCurveTA -n "D_:LElbowFK_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 7.7434694019375794 14 19.711882458521458 
		32 20.697190534552739 39 20.349470039704098 48 19.711882458521458 62 19.70630354102877 
		64 19.711882458521458 76 19.711882458521458 82 19.711882458521458 86 19.711882458521458 
		91 20.937723863078745 96 21.734086934995577 100 22.852648229300865 105 25.544252708930269 
		108 26.425130041452125 112 26.139107096318241 114 26.038999249642885 119 24.717771758687796 
		123 22.850052484825245 126 20.92462133453969 130 18.63211220533017 146 18.445114196842326 
		154 11.330685750146797 162 -3.2699029697139945 166 -3.2486196985509586 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 1 10 10 10 9 9 9 9 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 1 10 10 10 9 9 9 9 10 10 10 
		10 10;
	setAttr -s 27 ".kix[14:26]"  0.97029447555541992 1 1 1 0.98322206735610962 
		0.96202719211578369 0.95366162061691284 0.99789977073669434 1 0.81514346599578857 
		1 1 0.96208572387695313;
	setAttr -s 27 ".kiy[14:26]"  0.2419268935918808 0 0 0 -0.18241269886493683 
		-0.2729533314704895 -0.30088105797767639 -0.064776986837387085 0 -0.57925915718078613 
		0 0 0.27274727821350098;
	setAttr -s 27 ".kox[14:26]"  0.97029447555541992 1 1 1 0.98322206735610962 
		0.96202719211578369 0.95366162061691284 0.99789977073669434 1 0.81514346599578857 
		1 1 0.96208572387695313;
	setAttr -s 27 ".koy[14:26]"  0.2419268786907196 0 0 0 -0.18241269886493683 
		-0.2729533314704895 -0.30088105797767639 -0.064776986837387085 0 -0.57925915718078613 
		0 0 0.27274727821350098;
createNode animCurveTA -n "D_:LElbowFK_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 -32.439136911498778 8 -31.729612813043925 
		14 -29.323536455735557 32 -29.125454523479959 39 -29.195358698032219 48 -29.323536455735557 
		62 -29.324658016367067 64 -29.323536455735557 76 -29.323536455735557 82 -29.323536455735557 
		86 -29.323536455735557 91 -26.809889488889631 96 -25.13912106660343 100 -23.159800020556315 
		105 -18.66655749042858 108 -16.393583991534804 112 -12.048445385500125 114 -11.209646933733683 
		119 -18.207827151927336 123 -23.213562747403049 126 -24.268256080430447 130 -24.567751099101098 
		146 -27.754028808895789 154 -21.78006806177601 162 -32.572021058138652 166 -32.525245948014501 
		172 -32.439136911498778;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 9 9 9 9 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 9 9 9 9 10 10 10 
		10 10;
createNode animCurveTA -n "D_:LElbowFK_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 -4.1234408158048081 14 -10.121172516375969 
		32 -10.614939995244255 39 -10.440686810336919 48 -10.121172516375969 62 -10.118376753271278 
		64 -10.121172516375969 76 -10.121172516375969 82 -10.121172516375969 86 -10.121172516375969 
		91 -11.857171883439516 96 -13.010125062448191 100 -14.376658952124513 105 -17.487320178632721 
		108 -19.091169440207103 112 -22.205165278434819 114 -22.906336115592332 119 -18.415887136199579 
		123 -15.203909222659096 126 -15.242818652543502 130 -15.28016380451413 146 -15.185137876643795 
		154 -9.1300969747266514 162 4.4093077774980785 166 4.2814597635163043 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 9 9 9 9 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 9 9 9 9 10 10 10 
		10 10;
createNode animCurveTU -n "D_:Entity_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTL -n "D_:Entity_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:Entity_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:Entity_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:Entity_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:Entity_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:Entity_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:Entity_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:Entity_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:Entity_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:DiverGlobal_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 5 5 5 5 5 5 5 5 5;
createNode animCurveTL -n "D_:DiverGlobal_translateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:DiverGlobal_translateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 2.7755575615628914e-017 8 0 14 0 32 0 
		39 0 48 0 62 0 64 0 76 0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 
		123 0 126 0 130 0 146 0 154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTL -n "D_:DiverGlobal_translateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTA -n "D_:DiverGlobal_rotateZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleX";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleY";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:DiverGlobal_scaleZ";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:HeadControl_Mask";
	setAttr ".tan" 10;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 0 8 0 14 0 32 0 39 0 48 0 62 0 64 0 76 
		0 82 0 86 0 91 0 96 0 100 0 105 0 108 0 112 0 114 0 119 0 123 0 126 0 130 0 146 0 
		154 0 162 0 166 0 172 0;
	setAttr -s 27 ".kit[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
	setAttr -s 27 ".kot[0:26]"  9 9 9 10 10 9 10 9 
		9 9 9 10 10 10 9 10 10 10 10 10 10 10 10 10 10 
		10 10;
createNode animCurveTU -n "D_:LElbowFK_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 27 ".ktv[0:26]"  0 1 8 1 14 1 32 1 39 1 48 1 62 1 64 1 76 
		1 82 1 86 1 91 1 96 1 100 1 105 1 108 1 112 1 114 1 119 1 123 1 126 1 130 1 146 1 
		154 1 162 1 166 1 172 1;
	setAttr -s 27 ".kot[3:26]"  5 5 9 5 9 9 9 9 
		5 5 5 9 5 5 5 9 9 9 9 5 5 5 5 5;
createNode animCurveTU -n "D_:r_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 40 1 51 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTU -n "D_:r_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 40 1 51 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTU -n "D_:r_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 40 1 51 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTU -n "D_:r_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 3 ".ktv[0:2]"  0 1 40 1 51 1;
	setAttr -s 3 ".kot[0:2]"  5 5 5;
createNode animCurveTU -n "D_:l_thumb_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_point_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_mid_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:l_pink_1_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr -s 2 ".ktv[0:1]"  0 1 54 1;
	setAttr -s 2 ".kot[0:1]"  5 5;
createNode animCurveTU -n "D_:r_point_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  51 1;
	setAttr ".kot[0]"  5;
createNode animCurveTU -n "D_:r_thumb_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  51 1;
	setAttr ".kot[0]"  5;
createNode animCurveTU -n "D_:r_mid_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  51 1;
	setAttr ".kot[0]"  5;
createNode animCurveTU -n "D_:r_pink_2_visibility";
	setAttr ".tan" 9;
	setAttr ".wgt" no;
	setAttr ".ktv[0]"  51 1;
	setAttr ".kot[0]"  5;
select -ne :time1;
	setAttr -k on ".cch";
	setAttr -cb on ".ihi";
	setAttr -k on ".nds";
	setAttr -cb on ".bnm";
	setAttr -k on ".o" 35;
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
connectAttr "D_:Entity_visibility.o" "D_RN.phl[2334]";
connectAttr "D_:Entity_translateX.o" "D_RN.phl[2335]";
connectAttr "D_:Entity_translateY.o" "D_RN.phl[2336]";
connectAttr "D_:Entity_translateZ.o" "D_RN.phl[2337]";
connectAttr "D_:Entity_rotateX.o" "D_RN.phl[2338]";
connectAttr "D_:Entity_rotateY.o" "D_RN.phl[2339]";
connectAttr "D_:Entity_rotateZ.o" "D_RN.phl[2340]";
connectAttr "D_:Entity_scaleX.o" "D_RN.phl[2341]";
connectAttr "D_:Entity_scaleY.o" "D_RN.phl[2342]";
connectAttr "D_:Entity_scaleZ.o" "D_RN.phl[2343]";
connectAttr "D_:DiverGlobal_translateX.o" "D_RN.phl[2344]";
connectAttr "D_:DiverGlobal_translateY.o" "D_RN.phl[2345]";
connectAttr "D_:DiverGlobal_translateZ.o" "D_RN.phl[2346]";
connectAttr "D_:DiverGlobal_rotateX.o" "D_RN.phl[2347]";
connectAttr "D_:DiverGlobal_rotateY.o" "D_RN.phl[2348]";
connectAttr "D_:DiverGlobal_rotateZ.o" "D_RN.phl[2349]";
connectAttr "D_:DiverGlobal_scaleX.o" "D_RN.phl[2350]";
connectAttr "D_:DiverGlobal_scaleY.o" "D_RN.phl[2351]";
connectAttr "D_:DiverGlobal_scaleZ.o" "D_RN.phl[2352]";
connectAttr "D_:DiverGlobal_visibility.o" "D_RN.phl[2353]";
connectAttr "D_:l_mid_1_rotateX.o" "D_RN.phl[2354]";
connectAttr "D_:l_mid_1_rotateY.o" "D_RN.phl[2355]";
connectAttr "D_:l_mid_1_rotateZ.o" "D_RN.phl[2356]";
connectAttr "D_:l_mid_1_visibility.o" "D_RN.phl[2357]";
connectAttr "D_:l_mid_2_rotateX.o" "D_RN.phl[2358]";
connectAttr "D_:l_mid_2_rotateY.o" "D_RN.phl[2359]";
connectAttr "D_:l_mid_2_rotateZ.o" "D_RN.phl[2360]";
connectAttr "D_:l_pink_1_rotateX.o" "D_RN.phl[2361]";
connectAttr "D_:l_pink_1_rotateY.o" "D_RN.phl[2362]";
connectAttr "D_:l_pink_1_rotateZ.o" "D_RN.phl[2363]";
connectAttr "D_:l_pink_1_visibility.o" "D_RN.phl[2364]";
connectAttr "D_:l_pink_2_rotateX.o" "D_RN.phl[2365]";
connectAttr "D_:l_pink_2_rotateY.o" "D_RN.phl[2366]";
connectAttr "D_:l_pink_2_rotateZ.o" "D_RN.phl[2367]";
connectAttr "D_:l_point_1_rotateX.o" "D_RN.phl[2368]";
connectAttr "D_:l_point_1_rotateY.o" "D_RN.phl[2369]";
connectAttr "D_:l_point_1_rotateZ.o" "D_RN.phl[2370]";
connectAttr "D_:l_point_1_visibility.o" "D_RN.phl[2371]";
connectAttr "D_:l_point_2_rotateX.o" "D_RN.phl[2372]";
connectAttr "D_:l_point_2_rotateY.o" "D_RN.phl[2373]";
connectAttr "D_:l_point_2_rotateZ.o" "D_RN.phl[2374]";
connectAttr "D_:l_thumb_1_rotateX.o" "D_RN.phl[2375]";
connectAttr "D_:l_thumb_1_rotateY.o" "D_RN.phl[2376]";
connectAttr "D_:l_thumb_1_rotateZ.o" "D_RN.phl[2377]";
connectAttr "D_:l_thumb_1_visibility.o" "D_RN.phl[2378]";
connectAttr "D_:l_thumb_2_rotateX.o" "D_RN.phl[2379]";
connectAttr "D_:l_thumb_2_rotateY.o" "D_RN.phl[2380]";
connectAttr "D_:l_thumb_2_rotateZ.o" "D_RN.phl[2381]";
connectAttr "D_:r_mid_1_rotateX.o" "D_RN.phl[2382]";
connectAttr "D_:r_mid_1_rotateY.o" "D_RN.phl[2383]";
connectAttr "D_:r_mid_1_rotateZ.o" "D_RN.phl[2384]";
connectAttr "D_:r_mid_1_visibility.o" "D_RN.phl[2385]";
connectAttr "D_:r_mid_2_rotateX.o" "D_RN.phl[2386]";
connectAttr "D_:r_mid_2_rotateY.o" "D_RN.phl[2387]";
connectAttr "D_:r_mid_2_rotateZ.o" "D_RN.phl[2388]";
connectAttr "D_:r_mid_2_visibility.o" "D_RN.phl[2389]";
connectAttr "D_:r_pink_1_rotateX.o" "D_RN.phl[2390]";
connectAttr "D_:r_pink_1_rotateY.o" "D_RN.phl[2391]";
connectAttr "D_:r_pink_1_rotateZ.o" "D_RN.phl[2392]";
connectAttr "D_:r_pink_1_visibility.o" "D_RN.phl[2393]";
connectAttr "D_:r_pink_2_rotateX.o" "D_RN.phl[2394]";
connectAttr "D_:r_pink_2_rotateY.o" "D_RN.phl[2395]";
connectAttr "D_:r_pink_2_rotateZ.o" "D_RN.phl[2396]";
connectAttr "D_:r_pink_2_visibility.o" "D_RN.phl[2397]";
connectAttr "D_:r_point_1_rotateX.o" "D_RN.phl[2398]";
connectAttr "D_:r_point_1_rotateY.o" "D_RN.phl[2399]";
connectAttr "D_:r_point_1_rotateZ.o" "D_RN.phl[2400]";
connectAttr "D_:r_point_1_visibility.o" "D_RN.phl[2401]";
connectAttr "D_:r_point_2_rotateX.o" "D_RN.phl[2402]";
connectAttr "D_:r_point_2_rotateY.o" "D_RN.phl[2403]";
connectAttr "D_:r_point_2_rotateZ.o" "D_RN.phl[2404]";
connectAttr "D_:r_point_2_visibility.o" "D_RN.phl[2405]";
connectAttr "D_:r_thumb_1_rotateX.o" "D_RN.phl[2406]";
connectAttr "D_:r_thumb_1_rotateY.o" "D_RN.phl[2407]";
connectAttr "D_:r_thumb_1_rotateZ.o" "D_RN.phl[2408]";
connectAttr "D_:r_thumb_1_visibility.o" "D_RN.phl[2409]";
connectAttr "D_:r_thumb_2_rotateX.o" "D_RN.phl[2410]";
connectAttr "D_:r_thumb_2_rotateY.o" "D_RN.phl[2411]";
connectAttr "D_:r_thumb_2_rotateZ.o" "D_RN.phl[2412]";
connectAttr "D_:r_thumb_2_visibility.o" "D_RN.phl[2413]";
connectAttr "D_:L_Foot_ToeRoll.o" "D_RN.phl[2414]";
connectAttr "D_:L_Foot_BallRoll.o" "D_RN.phl[2415]";
connectAttr "D_:L_Foot_translateX.o" "D_RN.phl[2416]";
connectAttr "D_:L_Foot_translateY.o" "D_RN.phl[2417]";
connectAttr "D_:L_Foot_translateZ.o" "D_RN.phl[2418]";
connectAttr "D_:L_Foot_rotateX.o" "D_RN.phl[2419]";
connectAttr "D_:L_Foot_rotateY.o" "D_RN.phl[2420]";
connectAttr "D_:L_Foot_rotateZ.o" "D_RN.phl[2421]";
connectAttr "D_:L_Foot_scaleX.o" "D_RN.phl[2422]";
connectAttr "D_:L_Foot_scaleY.o" "D_RN.phl[2423]";
connectAttr "D_:L_Foot_scaleZ.o" "D_RN.phl[2424]";
connectAttr "D_:L_Foot_visibility.o" "D_RN.phl[2425]";
connectAttr "D_:R_Foot_ToeRoll.o" "D_RN.phl[2426]";
connectAttr "D_:R_Foot_BallRoll.o" "D_RN.phl[2427]";
connectAttr "D_:R_Foot_translateX.o" "D_RN.phl[2428]";
connectAttr "D_:R_Foot_translateY.o" "D_RN.phl[2429]";
connectAttr "D_:R_Foot_translateZ.o" "D_RN.phl[2430]";
connectAttr "D_:R_Foot_rotateX.o" "D_RN.phl[2431]";
connectAttr "D_:R_Foot_rotateY.o" "D_RN.phl[2432]";
connectAttr "D_:R_Foot_rotateZ.o" "D_RN.phl[2433]";
connectAttr "D_:R_Foot_scaleX.o" "D_RN.phl[2434]";
connectAttr "D_:R_Foot_scaleY.o" "D_RN.phl[2435]";
connectAttr "D_:R_Foot_scaleZ.o" "D_RN.phl[2436]";
connectAttr "D_:R_Foot_visibility.o" "D_RN.phl[2437]";
connectAttr "D_:L_Knee_translateX.o" "D_RN.phl[2438]";
connectAttr "D_:L_Knee_translateY.o" "D_RN.phl[2439]";
connectAttr "D_:L_Knee_translateZ.o" "D_RN.phl[2440]";
connectAttr "D_:L_Knee_scaleX.o" "D_RN.phl[2441]";
connectAttr "D_:L_Knee_scaleY.o" "D_RN.phl[2442]";
connectAttr "D_:L_Knee_scaleZ.o" "D_RN.phl[2443]";
connectAttr "D_:L_Knee_visibility.o" "D_RN.phl[2444]";
connectAttr "D_:R_Knee_translateX.o" "D_RN.phl[2445]";
connectAttr "D_:R_Knee_translateY.o" "D_RN.phl[2446]";
connectAttr "D_:R_Knee_translateZ.o" "D_RN.phl[2447]";
connectAttr "D_:R_Knee_scaleX.o" "D_RN.phl[2448]";
connectAttr "D_:R_Knee_scaleY.o" "D_RN.phl[2449]";
connectAttr "D_:R_Knee_scaleZ.o" "D_RN.phl[2450]";
connectAttr "D_:R_Knee_visibility.o" "D_RN.phl[2451]";
connectAttr "D_:RootControl_translateX.o" "D_RN.phl[2452]";
connectAttr "D_:RootControl_translateY.o" "D_RN.phl[2453]";
connectAttr "D_:RootControl_translateZ.o" "D_RN.phl[2454]";
connectAttr "D_:RootControl_rotateX.o" "D_RN.phl[2455]";
connectAttr "D_:RootControl_rotateY.o" "D_RN.phl[2456]";
connectAttr "D_:RootControl_rotateZ.o" "D_RN.phl[2457]";
connectAttr "D_:RootControl_scaleX.o" "D_RN.phl[2458]";
connectAttr "D_:RootControl_scaleY.o" "D_RN.phl[2459]";
connectAttr "D_:RootControl_scaleZ.o" "D_RN.phl[2460]";
connectAttr "D_:RootControl_visibility.o" "D_RN.phl[2461]";
connectAttr "D_:Spine0Control_translateX.o" "D_RN.phl[2462]";
connectAttr "D_:Spine0Control_translateY.o" "D_RN.phl[2463]";
connectAttr "D_:Spine0Control_translateZ.o" "D_RN.phl[2464]";
connectAttr "D_:Spine0Control_scaleX.o" "D_RN.phl[2465]";
connectAttr "D_:Spine0Control_scaleY.o" "D_RN.phl[2466]";
connectAttr "D_:Spine0Control_scaleZ.o" "D_RN.phl[2467]";
connectAttr "D_:Spine0Control_rotateX.o" "D_RN.phl[2468]";
connectAttr "D_:Spine0Control_rotateY.o" "D_RN.phl[2469]";
connectAttr "D_:Spine0Control_rotateZ.o" "D_RN.phl[2470]";
connectAttr "D_:Spine0Control_visibility.o" "D_RN.phl[2471]";
connectAttr "D_:Spine1Control_scaleX.o" "D_RN.phl[2472]";
connectAttr "D_:Spine1Control_scaleY.o" "D_RN.phl[2473]";
connectAttr "D_:Spine1Control_scaleZ.o" "D_RN.phl[2474]";
connectAttr "D_:Spine1Control_rotateX.o" "D_RN.phl[2475]";
connectAttr "D_:Spine1Control_rotateY.o" "D_RN.phl[2476]";
connectAttr "D_:Spine1Control_rotateZ.o" "D_RN.phl[2477]";
connectAttr "D_:Spine1Control_visibility.o" "D_RN.phl[2478]";
connectAttr "D_:TankControl_rotateX.o" "D_RN.phl[2479]";
connectAttr "D_:TankControl_rotateY.o" "D_RN.phl[2480]";
connectAttr "D_:TankControl_rotateZ.o" "D_RN.phl[2481]";
connectAttr "D_:TankControl_translateX.o" "D_RN.phl[2482]";
connectAttr "D_:TankControl_translateY.o" "D_RN.phl[2483]";
connectAttr "D_:TankControl_translateZ.o" "D_RN.phl[2484]";
connectAttr "D_:TankControl_visibility.o" "D_RN.phl[2485]";
connectAttr "D_:L_Clavicle_scaleX.o" "D_RN.phl[2486]";
connectAttr "D_:L_Clavicle_scaleY.o" "D_RN.phl[2487]";
connectAttr "D_:L_Clavicle_scaleZ.o" "D_RN.phl[2488]";
connectAttr "D_:L_Clavicle_translateX.o" "D_RN.phl[2489]";
connectAttr "D_:L_Clavicle_translateY.o" "D_RN.phl[2490]";
connectAttr "D_:L_Clavicle_translateZ.o" "D_RN.phl[2491]";
connectAttr "D_:L_Clavicle_visibility.o" "D_RN.phl[2492]";
connectAttr "D_:R_Clavicle_scaleX.o" "D_RN.phl[2493]";
connectAttr "D_:R_Clavicle_scaleY.o" "D_RN.phl[2494]";
connectAttr "D_:R_Clavicle_scaleZ.o" "D_RN.phl[2495]";
connectAttr "D_:R_Clavicle_translateX.o" "D_RN.phl[2496]";
connectAttr "D_:R_Clavicle_translateY.o" "D_RN.phl[2497]";
connectAttr "D_:R_Clavicle_translateZ.o" "D_RN.phl[2498]";
connectAttr "D_:R_Clavicle_visibility.o" "D_RN.phl[2499]";
connectAttr "D_:HeadControl_Mask.o" "D_RN.phl[2500]";
connectAttr "D_:HeadControl_rotateX.o" "D_RN.phl[2501]";
connectAttr "D_:HeadControl_rotateY.o" "D_RN.phl[2502]";
connectAttr "D_:HeadControl_rotateZ.o" "D_RN.phl[2503]";
connectAttr "D_:HeadControl_visibility.o" "D_RN.phl[2504]";
connectAttr "D_:LShoulderFK_scaleX.o" "D_RN.phl[2505]";
connectAttr "D_:LShoulderFK_scaleY.o" "D_RN.phl[2506]";
connectAttr "D_:LShoulderFK_scaleZ.o" "D_RN.phl[2507]";
connectAttr "D_:LShoulderFK_rotateX.o" "D_RN.phl[2508]";
connectAttr "D_:LShoulderFK_rotateY.o" "D_RN.phl[2509]";
connectAttr "D_:LShoulderFK_rotateZ.o" "D_RN.phl[2510]";
connectAttr "D_:LShoulderFK_visibility.o" "D_RN.phl[2511]";
connectAttr "D_:LElbowFK_rotateX.o" "D_RN.phl[2512]";
connectAttr "D_:LElbowFK_rotateY.o" "D_RN.phl[2513]";
connectAttr "D_:LElbowFK_rotateZ.o" "D_RN.phl[2514]";
connectAttr "D_:LElbowFK_visibility.o" "D_RN.phl[2515]";
connectAttr "D_:L_Wrist_scaleX.o" "D_RN.phl[2516]";
connectAttr "D_:L_Wrist_scaleY.o" "D_RN.phl[2517]";
connectAttr "D_:L_Wrist_scaleZ.o" "D_RN.phl[2518]";
connectAttr "D_:L_Wrist_rotateX.o" "D_RN.phl[2519]";
connectAttr "D_:L_Wrist_rotateY.o" "D_RN.phl[2520]";
connectAttr "D_:L_Wrist_rotateZ.o" "D_RN.phl[2521]";
connectAttr "D_:L_Wrist_visibility.o" "D_RN.phl[2522]";
connectAttr "D_:RShoulderFK_scaleX.o" "D_RN.phl[2523]";
connectAttr "D_:RShoulderFK_scaleY.o" "D_RN.phl[2524]";
connectAttr "D_:RShoulderFK_scaleZ.o" "D_RN.phl[2525]";
connectAttr "D_:RShoulderFK_rotateX.o" "D_RN.phl[2526]";
connectAttr "D_:RShoulderFK_rotateY.o" "D_RN.phl[2527]";
connectAttr "D_:RShoulderFK_rotateZ.o" "D_RN.phl[2528]";
connectAttr "D_:RShoulderFK_visibility.o" "D_RN.phl[2529]";
connectAttr "D_:RElbowFK_scaleX.o" "D_RN.phl[2530]";
connectAttr "D_:RElbowFK_scaleY.o" "D_RN.phl[2531]";
connectAttr "D_:RElbowFK_scaleZ.o" "D_RN.phl[2532]";
connectAttr "D_:RElbowFK_rotateX.o" "D_RN.phl[2533]";
connectAttr "D_:RElbowFK_rotateY.o" "D_RN.phl[2534]";
connectAttr "D_:RElbowFK_rotateZ.o" "D_RN.phl[2535]";
connectAttr "D_:RElbowFK_visibility.o" "D_RN.phl[2536]";
connectAttr "D_:R_Wrist_scaleX.o" "D_RN.phl[2537]";
connectAttr "D_:R_Wrist_scaleY.o" "D_RN.phl[2538]";
connectAttr "D_:R_Wrist_scaleZ.o" "D_RN.phl[2539]";
connectAttr "D_:R_Wrist_rotateX.o" "D_RN.phl[2540]";
connectAttr "D_:R_Wrist_rotateY.o" "D_RN.phl[2541]";
connectAttr "D_:R_Wrist_rotateZ.o" "D_RN.phl[2542]";
connectAttr "D_:R_Wrist_visibility.o" "D_RN.phl[2543]";
connectAttr "D_:HipControl_scaleX.o" "D_RN.phl[2544]";
connectAttr "D_:HipControl_scaleY.o" "D_RN.phl[2545]";
connectAttr "D_:HipControl_scaleZ.o" "D_RN.phl[2546]";
connectAttr "D_:HipControl_rotateX.o" "D_RN.phl[2547]";
connectAttr "D_:HipControl_rotateY.o" "D_RN.phl[2548]";
connectAttr "D_:HipControl_rotateZ.o" "D_RN.phl[2549]";
connectAttr "D_:HipControl_visibility.o" "D_RN.phl[2550]";
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
connectAttr "D_:RElbowFK_scaleX1.o" "D_RN.phl[2327]";
connectAttr "D_:RElbowFK_scaleY1.o" "D_RN.phl[2328]";
connectAttr "D_:RElbowFK_scaleZ1.o" "D_RN.phl[2329]";
connectAttr "D_:RElbowFK_rotateX1.o" "D_RN.phl[2330]";
connectAttr "D_:RElbowFK_rotateY1.o" "D_RN.phl[2331]";
connectAttr "D_:RElbowFK_rotateZ1.o" "D_RN.phl[2332]";
connectAttr "D_:RElbowFK_visibility1.o" "D_RN.phl[2333]";
connectAttr "lightLinker1.msg" ":lightList1.ln" -na;
// End of diver_idle_act2.ma
